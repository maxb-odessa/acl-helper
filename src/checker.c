/** \file */

#include "acl-helper.h"
#include "log.h"
#include "conf.h"
#include "tree.h"
#include "resolve.h"
#include "options.h"
#include "geoip2.h"
#include "ssl.h"
#include "misc.h"
#include "loop.h"
#include "source.h"

#include "checker.h"


static void *rmatch_dummy(int, node_t **, int, char **);
static void *rmatch_string(int, node_t **, int, char **);
static void *rmatch_shell(int, node_t **, int, char **);
#ifdef USE_REGEX
static void *rmatch_regex(int, node_t **, int, char **);
#endif
#ifdef USE_PCRE
static void *rmatch_pcre(int, node_t **, int, char **);
#endif
#ifdef USE_SSL
static void *rmatch_ssl(int, node_t **, int,  char **);
#endif
#ifdef USE_GEOIP2
static void *rmatch_geoip2(int, node_t **, int,  char **);
#endif
#ifdef USE_RESOLVE
static void *rmatch_resolve(int, node_t **, int, char **);
static void *rmatch_dresolve(int, node_t **, int, char **);
#endif
static void *rmatch_ip(int, node_t **, int, char **);

//! available checkers drivers
static cdriver_t checker_drivers[] = {

  // generic (plain) checkers
  {"dummy",         TYPE_DUMMY,       0,  rmatch_dummy},
  {"string",        TYPE_STRING,      0,  rmatch_string},
  {"istring",       TYPE_STRING,      1,  rmatch_string},
  {"ip",            TYPE_IP,          0,  rmatch_ip},
  {"resolve",       TYPE_IP,          0,  rmatch_resolve},
  {"dresolve",      TYPE_LIST,        1,  rmatch_dresolve},
  // shell pattern based checkers
  {"match",         TYPE_SHELL,       0,  rmatch_shell},
  {"imatch",        TYPE_SHELL,       1,  rmatch_shell},
  // regex based checkers
#ifdef USE_REGEX
  {"regex",         TYPE_REGEX,       0,  rmatch_regex},
  {"iregex",        TYPE_REGEX,       1,  rmatch_regex},
#endif
  // pcre based checkers
#ifdef USE_PCRE
  {"pcre",          TYPE_PCRE,        0,  rmatch_pcre},
  {"ipcre",         TYPE_PCRE,        1,  rmatch_pcre},
#endif
  // special ssl based checkers
#ifdef USE_SSL
  {"ssl",           TYPE_SSL,         0,  rmatch_ssl},
#endif
  // geoip2 checker
#ifdef USE_GEOIP2
  {"geoip2",        TYPE_IP,         0,  rmatch_geoip2},
#endif

  // terminator!
  {NULL,},
};


//! private configured checkers data
static struct checker *checkers;



//! parse checker config line and create new checker in list
//! \param str checker definition line
//! \return 0 if ok, !0 otherwise
int checker_config(char *str) {

  // split string into tokens (we expect 7 tokens)
  char *array[8] = {0,};
  int n = parse_string(str, array, ":", 8);
  if (n < 8) {
    wlog(L_ERR, "not enough args for 'checker'");
    return 1;
  }

  // go to last checker in list to add new one
  struct checker *cp = checkers;
  while (cp && cp->next)
    cp = cp->next;

  // allocate space for new checker
  struct checker *new_cp = calloc(1, sizeof(struct checker));
  assert(new_cp);

  // add new checker to the list
  if (! cp)
    cp = checkers = new_cp;
  else
    cp = cp->next = new_cp;

  // store name
  cp->name = array[0];

  // store enable value
  cp->enable_s = array[1];

  // store squid in field num
  cp->field_idx_s = array[2];

  // store driver
  cp->driver_s = array[3];

  // store action
  cp->action_s = array[4];
    
  // store notes
  cp->notes = array[5];

  // store source name
  cp->source = array[6];

  // store source filter
  cp->source_filter = array[7];

  // done
  return 0;
}



//! find appropriate driver for checker
//! \param name driver name
//! \return pointer to found driver or NULL
static cdriver_t *checker_get_driver(char *name) {
  int i;
  for (i = 0; checker_drivers[i].name; i ++) {
    if (! strcmp(checker_drivers[i].name, name))
      return &checker_drivers[i];
  }
  return NULL;
}


//! comparison func for records (ip addr)
static int rec_cmp_ip(const void *r1, const void *r2) {
  in_addr_t ipnet1 = ((struct record *)r1)->rec.a.ipnet;
  in_addr_t ipnet2 = ((struct record *)r2)->rec.a.ipnet;

  if (! ipnet2)
    ipnet2 = ((struct record *)r2)->rec.a.ip & ((struct record *)r1)->rec.a.net;

  // result of 'r1 - r2' will be uint32, but we need int, so can't use 'r1 - r2'
  if (ipnet1 < ipnet2)
    return 1;
  else if (ipnet1 > ipnet2)
    return -1;

  return 0;
}


//! comparison func for records (generic string match)
static int rec_cmp_l(const void *r1, const void *r2) {
  return !!strcmp(((struct record *)r1)->data, ((struct record *)r2)->data);
}

//! comparison func for records (generic icase string match)
static int rec_cmp_li(const void *r1, const void *r2) {
  return !!strcasecmp(((struct record *)r1)->data, ((struct record *)r2)->data);
}

//! comparison func for records (special case, 'dresolve' driver)
//! here we need to resolve record data first and then compare ips
static int rec_cmp_dresolve(const void *r1, const void *r2) {

  // resolve fqdn
  in_addr_t *ips = calloc(MAX_RESOLVED_IPS + 1, sizeof(in_addr_t));
  assert(ips);
  int n_ips = resolve_host(((struct record *)r1)->data, ips, MAX_RESOLVED_IPS);

  // resolve failed, try next fqdn
  if (n_ips < 1) {
    free(ips);
    return 1;
  }

  // match over all resolved ips
  for (; n_ips > 0; n_ips --) {
    if (((struct record *)r2)->rec.a.ip == ips[n_ips - 1])
      break;
  }

  // done
  free(ips);
  return !n_ips;
}

//! comparison func for records (str)
static int rec_cmp_s(const void *r1, const void *r2) {
  return strcmp(((struct record *)r1)->data, ((struct record *)r2)->data);
}

//! comparison func for records (str case insensitive)
static int rec_cmp_si(const void *r1, const void *r2) {
  return strcasecmp(((struct record *)r1)->data, ((struct record *)r2)->data);
}

//! comparison func for records (shell pattern)
//! match search will require tree traversal which is much faster if
//! a tree is degraded into a list, so make a list!
static int rec_cmp_m(const void *r1, const void *r2) {
  return !!fnmatch(((struct record *)r1)->data, ((struct record *)r2)->data, 0);
}

//! comparison func for records (shell pattern icase)
//! match search will require tree traversal which is much faster if
//! a tree is degraded into a list, so make a list!
static int rec_cmp_mi(const void *r1, const void *r2) {
  return !!fnmatch(((struct record *)r1)->data, ((struct record *)r2)->data, FNM_CASEFOLD);
}

#ifdef USE_REGEX
//! comparison func for records (regex pattern match)
//! match search will require tree traversal which is much faster if
//! a tree is degraded into a list, so make a list!
static int rec_cmp_r(const void *r1, const void *r2) {
  return !!regexec(((struct record *)r1)->rec.r, ((struct record *)r2)->data, 0, NULL, 0);
}
#endif


#ifdef USE_PCRE
//! comparison func for records (pcre pattern match)
//! match search will require tree traversal which is much faster if
//! a tree is degraded into a list, so make a list!
static int rec_cmp_p(const void *r1, const void *r2) {
  return !!pcre_exec(((struct record *)r1)->rec.p, 
                     NULL, 
                     ((struct record *)r2)->data,
                     strlen(((struct record *)r1)->data),
                     0, 0, 0, 0);
}
#endif



//! parse loaded checker data into records and build a tree from them
//! \param cp checker pointer
//! \param data raw records data
//! \return num of loaded records or -1 on error
static int checker_store_records(struct checker *cp, char *data) {

  int recnum = 0;

  // data must be 'one record per line'
  char *d1 = data, *d2;
  while (*d1) {

    // search for '\n' or '\0'
    d2 = strchrnul(d1, '\n');

    // found!
    if (d2 != d1) {

      // extract line and strip it from excessive blanks
      char *line = strndup(d1, d2 - d1);
      assert(line);
      char *stripped_line = strip_blanks(line);
      // the line is empty - ignore it
      if (! *stripped_line) {
        free(line);
        d1 = ++ d2;
        continue;
      }

      // create new record
      struct record *rp = calloc(1, sizeof(struct record));
      assert(rp);
      rp->data = line;
 
      wlog(L_DEBUG9, "will add [%s]", rp->data);

      // add new record node to the tree
      int not_added = 0;
      switch (cp->driver->type) {

        // list (degraded tree) ops for exact matches
        case TYPE_LIST :
          // add to list but skip duplicates
          if (cp->driver->icase)
            tree_search(rp, &cp->records, rec_cmp_li);
          else
            tree_search(rp, &cp->records, rec_cmp_l);
          // attempt to insert already existing entry?
          if (errno != ENOENT)
            not_added ++;
          break;

        // full tree ops for exact matches
        case TYPE_STRING :
        case TYPE_DUMMY :
          // add to tree but skip duplicates
          if (cp->driver->icase)
            tree_search(rp, &cp->records, rec_cmp_si);
          else
            tree_search(rp, &cp->records, rec_cmp_s);
          // attempt to insert already existing entry?
          if (errno != ENOENT)
            not_added ++;
          break;

#ifdef USE_MATCH
        // patterns, regexes and pcre need no tree :(
        case TYPE_SHELL :
          // add to tree but skip duplicates
          if (cp->driver->icase)
            tree_search(rp, &cp->records, rec_cmp_mi);
          else
            tree_search(rp, &cp->records, rec_cmp_m);
          // attempt to insert already existing entry?
          if (errno != ENOENT) 
            not_added ++;
          break;
#endif

        case TYPE_IP :
          if (str2ipaddr(rp->data, &rp->rec.a.ip, &rp->rec.a.net)) {
            wlog(L_WARN, "skipping invalid IP [%s]", rp->data);
            not_added ++;
          } else {
            rp->rec.a.ipnet = rp->rec.a.ip & rp->rec.a.net;
            // add to tree but skip duplicates
            tree_search(rp, &cp->records, rec_cmp_ip);
            if (errno != ENOENT)
              not_added ++;
          }
          break;

#ifdef USE_REGEX
        // precompile regex
        case TYPE_REGEX :
          {
            rp->rec.r = malloc(sizeof(regex_t));
            assert(rp->rec.r);
            int rflags = REG_EXTENDED;
            if (cp->driver->icase)
              rflags |= REG_ICASE;
            int reg_err = regcomp(rp->rec.r, rp->data, rflags);
            if (reg_err) {
              char err_buf[128];
              regerror(reg_err, rp->rec.r, err_buf, sizeof(err_buf) - 1);
              wlog(L_WARN, "skipping invalid regex pattern [%s] => %s", rp->data, err_buf);
              //regfree(rp->rec.r);
              free(rp->rec.r);
              not_added ++;
            } else {
              tree_search(rp, &cp->records, rec_cmp_r);
              // attempt to insert already existing entry?
              if (errno != ENOENT) {
                regfree(rp->rec.r);
                free(rp->rec.r);
                not_added ++;
              }
            }
          }
          break;
#endif

#ifdef USE_PCRE
        // precompile pcre
        case TYPE_PCRE :
          {
            int pflags = PCRE_ANCHORED;
            if (cp->driver->icase)
              pflags |= PCRE_CASELESS;
            const char *err_str;
            int err_off;
            rp->rec.p = pcre_compile(rp->data, pflags, &err_str, &err_off, NULL);
            if (rp->rec.p == NULL)
              wlog(L_WARN, "skipping invalid pcre pattern [%s] => %s:%d", rp->data, err_str, err_off);
            else
              tree_search(rp, &cp->records, rec_cmp_p);
              // attempt to insert already existing entry?
              if (errno != ENOENT)
                not_added ++;
          }
          break;
#endif

        // others (unknown?)
        default:
          break;

      } //switch(type...)

      // failed to add new record
      if (not_added) {
        if (line)
          free(line);
        if (rp)
          free(rp);
      } else
        recnum ++;

      // last '\n' hit
      if (! *d2)
        break;

    } // if(found...)

    // advance in data string
    d1 = ++ d2;

  } // while(data...)

  // done
  wlog(L_DEBUG9, "added %d records", recnum);
  return recnum;
}


//! init all configured checkers
//! \return 0 if ok
int checkers_init(void) {

  // init each checker
  struct checker *cp = checkers;
  int err = 0;
  while (cp) {

    // check and init checker settings
    // also do possible substitution for %{[scope&]name[|val]} with runtime options value
    // not all settings may be substituted
    
    char *substed = NULL;

    // ENABLE (subst or leave as-is)
    substed = options_subst(cp->enable_s);
    if (substed) {
      wlog(L_DEBUG8, "checker: substing 'enable' [%s] -> [%s]", cp->enable_s, substed);
      free(cp->enable_s);
      cp->enable_s = substed;
    }

    // check 'enable'
    if (! strcasecmp("on", cp->enable_s))
      cp->enable = 1;
    else if (! strcasecmp("off", cp->enable_s))
      cp->enable = 0;
    else {
      cp->enable = str2int(cp->enable_s, 0, INT_MAX);
      if (errno) {
        wlog(L_ERR, "checker '%s': invalid value for 'enable': '%s'", cp->name, cp->enable_s);
        err ++;
        goto BAD_CHECKER;
      }
    }

    // INPUT FIELD IDX(subst or leave as-is)
    substed = options_subst(cp->field_idx_s);
    if (substed) {
      wlog(L_DEBUG8, "checker '%s': substing 'idx' [%s] -> [%s]", cp->name, cp->field_idx_s, substed);
      free(cp->field_idx_s);
      cp->field_idx_s = substed;
    }

    // check 'field_idx'
    cp->field_idx = str2int(cp->field_idx_s, 0, SQUID_MAX_TOKENS);
    if (errno || cp->field_idx < 0) {
      wlog(L_ERR, "checker '%s': invalid 'idx': '%s'", cp->name, cp->field_idx_s);
      err ++;
      goto BAD_CHECKER;
    }

    // DRIVER (subst or leave as-is)
    substed = options_subst(cp->driver_s);
    if (substed) {
      wlog(L_DEBUG8, "checker '%s': substing 'driver' [%s] -> [%s]", cp->name, cp->driver_s, substed);
      free(cp->driver_s);
      cp->driver_s = substed;
    }

    // check 'driver'
    cp->driver = checker_get_driver(cp->driver_s);
    if (! cp->driver) {
      wlog(L_ERR, "checker '%s': invalid driver '%s'", cp->name, cp->driver_s);
      err ++;
      goto BAD_CHECKER;
    }

    // ACTION (subst or leave as-is)
    substed = options_subst(cp->action_s);
    if (substed) {
      wlog(L_DEBUG8, "checker '%s': substing 'action' [%s] -> [%s]", cp->name, cp->action_s, substed);
      free(cp->action_s);
      cp->action_s = substed;
    }
 
    // check action  
    if (! strcasecmp("hit", cp->action_s))
      cp->action = ACTION_HIT;
    else if (! strcasecmp("miss", cp->action_s))
      cp->action = ACTION_MISS;
    else if (! strcasecmp("note", cp->action_s))
      cp->action = ACTION_NOTE;
    else {
      wlog(L_ERR, "checker '%s': invalid checker 'action' '%s'", cp->name, cp->action_s);
      err ++;
      goto BAD_CHECKER;
    }

    // NOTE (subst or leave as-is)
    substed = options_subst(cp->notes);
    if (substed) {
      wlog(L_DEBUG8, "checker '%s': substing 'notes' [%s] -> [%s]", cp->name, cp->notes, substed);
      free(cp->notes);
      cp->notes = substed;
    }

    // load data from source
    char *data = source_data(cp->source, cp->source_filter);
    if (! data) {
      wlog(L_WARN, "checker '%s': source '%s' failed", cp->name, cp->source);
      err ++;
    } else {
      // parse loaded data and store it
      int recnum = checker_store_records(cp, data);
      if (recnum < 0)
        wlog(L_ERR, "checker '%s': failed to load records from source '%s'", cp->name, cp->source);
      else
        wlog(L_INFO, "checker '%s': loaded %d records from source '%s'", cp->name, recnum, cp->source);
      free(data);
    }

BAD_CHECKER:
    // checker problems? disable it!
    if (err) {
      wlog(L_WARN, "checker '%s' failed to init, disabling it", cp->name);
      cp->enable = 0;
      err = 0;
    }

    // ok, move to next checker to init
    cp = cp->next;

  } //while(checker...)


  // all done
  return 0;
}


//! dummy matching func, always matches anything
static struct record dummy_record = { .data = "DUMMY", .rec = {0,}, .ret = NULL };
static void *rmatch_dummy(int dummy1, node_t **dummy2, int dummy3, char **dummy4) {
  return &dummy_record;
}


//! find data in records tree by ip addr match
//! \param idx token index
//! \param root pointer to records root
//! \param icase 0 to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_ip(int idx, node_t **root, int icase, char **tokens) {

  struct record *found = NULL;
  struct record *rec_to_find = calloc(1, sizeof(struct record));
  assert(rec_to_find);

  if (str2ipaddr(tokens[idx], &rec_to_find->rec.a.ip, &rec_to_find->rec.a.net))
    wlog(L_WARN, "invalid ip '%s'", tokens[idx]);
  else
    found = tree_find(rec_to_find, root, rec_cmp_ip);

 //wlog(L_DEBUG9, "found IP [%u] [%u] %p", rec_to_find->rec.a.ip, rec_to_find->rec.a.net, found);

  free(rec_to_find);

  return found;
};


//! find data in records tree by resolved ip addr match
//! \param idx token idx
//! \param root pointer to records root
//! \param icase 0 to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_resolve(int idx, node_t **root, int icase, char **tokens) {

  // first - resolve the domain
  in_addr_t *ips = calloc(MAX_RESOLVED_IPS + 1, sizeof(in_addr_t));
  assert(ips);
  int n_ips = resolve_host(tokens[idx], ips, MAX_RESOLVED_IPS);
  if (n_ips < 1) {
    wlog(L_WARN, "failed to resolve '%s'", (char *)tokens[idx]);
    free(ips);
    return NULL;
  }
 
  // alloc new matching record 
  struct record *rec_to_find = calloc(1, sizeof(struct record));
  assert(rec_to_find);
  struct record *found = NULL;
  
  // match over all resolved ips
  for (-- n_ips; n_ips >= 0; n_ips --) {
    rec_to_find->rec.a.ip = ips[n_ips];
    rec_to_find->rec.a.net = 0xFFFFFFFF;
    // rec_to_find->rec.a.ipnet = ips[n_ips] & 0xFFFFFFFF;
    rec_to_find->rec.a.ipnet = 0;
    found = tree_find(rec_to_find, root, rec_cmp_ip);
    if (found)
      break;
  }

  // done
  free(rec_to_find);
  free(ips);
  return found;
};


//! match given ip over resolved record domain
//! this is opposite to rmatch_resolve: it accepts single IP
//! then takes each record from source, resolves the record host
//! and then matches resolved IPs to provided one
//! the main purpose of all this is to match client src ip over
//! a configured list of dynamic (dyndns, etc) hosts
//! \param idx token idx
//! \param root pointer to records root
//! \param icase 0 to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_dresolve(int idx, node_t **root, int icase, char **tokens) {

  // convert string into ip
  in_addr_t ip;
  if (str2ipaddr(tokens[idx], &ip, NULL)) {
    wlog(L_WARN, "dresolve: invalid IP [%s]", tokens[idx]);
    return NULL;
  }

  // alloc and init new matching record 
  struct record *rec_to_find = calloc(1, sizeof(struct record));
  assert(rec_to_find);
  rec_to_find->rec.a.ip = ip;
 
  // search for matching fqdn
  struct record *found = tree_find(rec_to_find, root, rec_cmp_dresolve);

  // return matched record or NULL
  free(rec_to_find); 
  return found;
};


//! find data in records tree by string comparison
//! \param idx tokens idx
//! \param root pointer to records root
//! \param icase 0 to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_string(int idx, node_t **root, int icase, char **tokens) {
  struct record *rec_to_find = malloc(sizeof(struct record));
  assert(rec_to_find);

  rec_to_find->data = tokens[idx];

  struct record *found;
  if (icase)
    found = tree_find(rec_to_find, root, rec_cmp_si);
  else
    found = tree_find(rec_to_find, root, rec_cmp_s);

  free(rec_to_find);

  return found;
}


//! find data in records tree by (shell pattern)
//! \param idx tokens index
//! \param root pointer to records root
//! \param icase 0 if to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_shell(int idx, node_t **root, int icase, char **tokens) {
  struct record *rec_to_find = calloc(1, sizeof(struct record));
  assert(rec_to_find);

  rec_to_find->data = tokens[idx];

  struct record *found;
  if (icase)
    found = tree_find(rec_to_find, root, rec_cmp_mi);
  else
    found = tree_find(rec_to_find, root, rec_cmp_m);

  free(rec_to_find);

  return found;
}

#ifdef USE_REGEX
//! find data in records tree by regex pattern match
//! \param idx tokens index
//! \param root pointer to records root
//! \param icase 0 if to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_regex(int idx, node_t **root, int icase, char **tokens) {
  struct record *rec_to_find = calloc(1, sizeof(struct record));
  assert(rec_to_find);

  rec_to_find->data = tokens[idx];

  struct record *found = tree_find(rec_to_find, root, rec_cmp_r);

  free(rec_to_find);

  return found;
}
#endif

#ifdef USE_PCRE
//! find data in records tree by pcre pattern match
//! \param idx tokens idx
//! \param root pointer to records root
//! \param icase 0 if to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_pcre(int idx, node_t **root, int icase, char **tokens) {
  struct record *rec_to_find = calloc(1, sizeof(struct record));
  assert(rec_to_find);

  rec_to_find->data = tokens[idx];

  struct record *found = tree_find(rec_to_find, root, rec_cmp_p);

  free(rec_to_find);

  return found;
}
#endif



#ifdef USE_GEOIP2
//! mutex for geoip cache searches
static pthread_mutex_t geoip2_mutex = PTHREAD_MUTEX_INITIALIZER;

//! guess geo location of an ip/host
//! guessed country and city codes will be placed in record->ret field
//! \param idx tokens idx
//! \param root pointer to records root
//! \param icase 0 if to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_geoip2(int idx, node_t **root, int icase, char **tokens) {

  // prepare search data
  struct record *rec_to_find = calloc(1, sizeof(struct record));
  assert(rec_to_find);

  // will use provided token as tree key
  rec_to_find->data = strdup(tokens[idx]);
  assert(rec_to_find->data);
  
  // search the cache first (tree_search() always return valid pointer or... crashes :)
  pthread_mutex_lock(&geoip2_mutex);
  struct record *found = tree_search(rec_to_find, root, rec_cmp_si);
  pthread_mutex_unlock(&geoip2_mutex);

  // found cached entry (expiration > 0) - return it
  if (found->rec.t > 0) {
    free(rec_to_find->data);
    free(rec_to_find);
    wlog(L_DEBUG5, "found cached GeoIP2 entry for '%s'", found->data);
    return found;
  } else {
 }
 
  // now 'found' points to newly created record (rec_to_find)

  // do geoip2 lookup
  geoip2_data *gi2 = malloc(sizeof(geoip2_data));
  assert(gi2);
  geoip2_lookup(tokens[idx], gi2);

  // critical part again...
  pthread_mutex_lock(&geoip2_mutex);

  // alloc space for new geoip result
  found->ret = malloc(sizeof(GEOIP2_NOTES_TMPL) + sizeof(geoip2_data) + 1);
  assert(found->ret);

  // save new ssl result
  //! \todo replace sprintf() with something simpler: we actually have to place 3 short
  //! \todo strings into one fixed sized template
  sprintf(found->ret, GEOIP2_NOTES_TMPL, gi2->continent, gi2->country, gi2->city);
  free(gi2);

  // mark record as 'not expired'
  // (actually it will never expire, so we use this feature to
  // distinguish new records from already created)
  found->rec.t = 1;

  // critical part end
  pthread_mutex_unlock(&geoip2_mutex);


  // all done, return data
  return found;
}
#endif



#ifdef USE_SSL
//! mutex for ssl operations
static pthread_mutex_t ssl_mutex = PTHREAD_MUTEX_INITIALIZER;

//! get remote host SSL cert data
//! discovered data will be placed in returned record->ret field
//! \param idx tokens index
//! \param root pointer to records root (will be used as cache)
//! \param icase 0 if to ignore case
//! \param tokens broken squid input string array
//! \return pointer to found record or NULL
static void *rmatch_ssl(int idx, node_t **root, int icase, char **tokens) {

  // prepare search data
  struct record *rec_to_find = calloc(1, sizeof(struct record));
  assert(rec_to_find);

  // prepare and check port
  errno = 0;
  char *port_s = tokens[idx + 1] ? tokens[idx + 1] : "443";
  long port = strtol(port_s, NULL, 10);
  if (errno || port < 1 || port > 65535) {
    wlog(L_ERR, "invalid port '%s' for SSL type checker", port_s);
    return NULL;
  }

  // will use 'DomainPort' as tree key
  rec_to_find->data = malloc(strlen(tokens[idx]) + strlen(port_s) + 1);
  assert(rec_to_find->data);
  strcpy(rec_to_find->data, tokens[idx]);
  strcat(rec_to_find->data, port_s);
  
  // search the cache first (tree_search() always return valid pointer or... crashes :)
  pthread_mutex_lock(&ssl_mutex);
  struct record *found = tree_search(rec_to_find, root, rec_cmp_si);
  pthread_mutex_unlock(&ssl_mutex);

  // installed new entry? no need in 'rec_to_find' anymore
  // (new entry has expire time set to 0)
  if (found->rec.t > 0) {
    free(rec_to_find->data);
    free(rec_to_find);
  }
 
  // now 'found' points to newly created record (rec_to_find) or found one

  // check if record is new or not yet expired
  if (found->rec.t > time(NULL)) {
    wlog(L_DEBUG5, "found cached non-expired SSL entry for '%s'", found->data);
    return found;
  }

  // cache record is new or expired - (re)validate host cert

  // ok, have to get an SSL cert and examine it
  int ssl_error = ssl_verify_host(tokens[idx], (unsigned)port, config.ssl_timeout); 

  // critical part again...
  pthread_mutex_lock(&ssl_mutex);

  // free (possibly existing) old ssl check result
  if (found->ret)
    free(found->ret);

  // alloc space for new ssl result
  found->ret = malloc(sizeof(SSL_ERROR_NOTE_TMPL) + 8);
  assert(found->ret);

  // save new ssl result
  //! \todo replace sprintf() with something simple: we actually have to convert 2 digits into chars
  sprintf(found->ret, SSL_ERROR_NOTE_TMPL, ssl_error);
  found->rec.t = time(NULL) + config.ssl_verify_ttl;

  // critical part end
  pthread_mutex_unlock(&ssl_mutex);

  // all done, return data
  return found;
}
#endif


//! call all checkers against give squid request
//! \param tokens squid input parsed tokens array
//! \param max_idx max tokens idx
//! \return string to give out to squid
char *checkers_call(char **tokens, int max_idx) {

  char *notes = NULL;
  struct record *rp = NULL;
  struct checker *cp = checkers;

  // call all checkers in order
  while (cp) {

    // skip disabled checkers
    if (! cp->enable) {
      wlog(L_DEBUG9, "skipping disabled checker '%s'", cp->name);
      cp = cp->next;
      continue;
    }

    // make a match
    wlog(L_DEBUG5, "calling checker '%s'", cp->name);
    if (cp->field_idx > max_idx) {
      wlog(L_ERR, "invalid index for checker '%s': need #%d, but squid sent %d tokens, skipping checker",
           cp->name, cp->field_idx + 1, max_idx + 1);
      cp = cp->next;
      continue;
    } else
      rp = cp->driver->match_func(cp->field_idx, &cp->records, cp->driver->icase, tokens);

    // matched!
    if (rp) {

      wlog(L_DEBUG3, "found '%s', action '%s'", rp->data, cp->action_s);

      // always save checker note for squid 'note' acls: glue to notes list if needed
      if (cp->notes && *cp->notes) {
        if (! notes) {
          notes = calloc(1, strlen(cp->notes) + 2);
          assert(notes);
        } else {
          notes = realloc(notes, strlen(notes) + strlen(cp->notes) + 2);
          assert(notes);
          strcat(notes, " ");
        }
        strcat(notes, cp->notes);
      }

      // also add optional ret data from found records
      if (rp->ret && *(char *)rp->ret) {
        if (! notes) {
          notes = calloc(1, strlen((char *)rp->ret) + 2);
          assert(notes);
        } else {
          notes = realloc(notes, strlen(notes) + strlen((char *)rp->ret) + 2);
          assert(notes);
          strcat(notes, " ");
        }
        strcat(notes, (char *)rp->ret);
      }

    } //if(rp...)
 
    // should we abort checkers chain?
    if ((rp && cp->action == ACTION_HIT) || (!rp && cp->action == ACTION_MISS))
      break;

    // select next checker
    cp = cp->next;

  } // while(checkers...)


  // ok, got the result: compose a string for squid
  // ex: ERR notestring=YES message=\"notestring=YES\"
  size_t squid_out_len = (notes ? strlen(notes) : 0) * 2 + 32; // 32 is for overhead
  char *squid_out = malloc(squid_out_len);
  assert(squid_out);

  // compose an output string for squid
  snprintf(squid_out,
           squid_out_len - 1,
           "%s %s message=\"%s\"",
           (cp && cp->action == ACTION_MISS) ? "ERR" : "OK",
           notes ? notes : "",
           notes ? notes : "(none)"
  );

  // clenaup
  if (notes)
    free(notes);

  // return composed string
  return squid_out;
}



