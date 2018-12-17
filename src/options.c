/** \file */


#include "acl-helper.h"
#include "log.h"
#include "tree.h"
#include "misc.h"
#include "source.h"

#include "options.h"

//! private runtime options list
static struct opt_scope *opt_scopes;


//! find options scope by name
//! \param scope_name options scope name
//! \return pointer to found options scope or NULL
static struct opt_scope *options_scope_find(char *scope_name) {
  struct opt_scope *os = opt_scopes;
  while (os) {
    if (! strcmp(os->name, scope_name))
      break;
    os = os->next;
  }
  return os;
}


//! parse config file options line and store them in tree
//! \param str config line to parse
//! \return 0 if ok, !0 otherwise
int option_config(char *str) {

  // split config line into tokens (we expect 3 tokens)
  char *array[3] = {0, };
  int n = parse_string(str, array, ":", 3);
  if (n < 3) {
    wlog(L_ERR, "not enough args for 'options'");
    return 1;
  }

  // allow no empty values (except last one)
  for (n --; n; n --) {
    if (! array[n-1]) {
      wlog(L_ERR, "empty option in 'option'");
      return 1;
    }
  }

  // no duplicate options allowed
  if (options_scope_find(array[0])) {
    wlog(L_ERR, "options '%s' already defined", array[0]);
    return 1;
  }

  // allocate space for new options
  struct opt_scope *new_os = calloc(1, sizeof(struct opt_scope));
  assert(new_os);

  // add new options to the list
  struct opt_scope *os = opt_scopes;
  if (! os) {
    opt_scopes = new_os;
    os = opt_scopes;
  } else {
    // go to last scope in list
    while (os->next)
      os = os->next;
    os->next = new_os;
    os = os->next;
  }

  // store options scope
  os->name = array[0];

  // store options source
  os->source = array[1];

  // store options source params
  os->source_filter = array[2];

  // done
  return 0;
}


//! cmp func for options searching
static int op_cmp_s(const void *o1, const void *o2) {
  return strcmp(((struct option *)o1)->name, ((struct option *)o2)->name);
}


//! parse options data and store them in a tree
//! \param os option scope
//! \param data data string to parse
//! \return 0 if OK, !0 otherwise
static int options_store_options(struct opt_scope *os, char *data) {

  // data must be 'one record per line'
  char *d1 = data, *d2;
  while (*d1) {

    // search for '\n' or '\0' NOT followed by '\'
    char *td1 = d1;
    while (1) {
      d2 = strchrnul(td1, '\n');
      if (*d2 && *(d2 - 1) == '\\')
        td1 ++;
      else
        break;
    }
        

    // found!
    if (d2 != d1) {

      // store the line
      char *opline = strndup(d1, d2 - d1);
      assert(opline);

      // remove leading and trailing blanks
      char *stripped_opline = strip_blanks(opline);

      // ignore comments and bad lines
      if (! stripped_opline || ! *stripped_opline || *stripped_opline == COMMENT_CHAR) {
        d1 = ++ d2;
        continue;
      }

      // parse the line (expect it to be 'key = val')
      char *eqchar = index(stripped_opline, '=');

      // invalid line
      if (! eqchar) {
        wlog(L_ERR, "invalid option format in source '%s', skipped", os->source);
      } else {

        // split the line by '='
        *eqchar  = '\0';

        // extract param and value
        char *param = strip_blanks(stripped_opline);
        char *value = strip_blanks(++ eqchar);

        wlog(L_DEBUG5, "loaded option for '%s': [%s] => [%s]", os->name, param, value);

        // create new option
        struct option *op = calloc(1, sizeof(struct option));
        assert(op);
        op->name = param;
        op->value = value;
        tree_search(op, &os->options, op_cmp_s);

        // last '\n' hit
        if (! *d2)
          break;
      }

    } // if(found...)

    // advance in data string
    d1 = ++ d2;

  } // while(data...)

  // done
  return 0;
}


//! read runtime options from various sources
//! \return 0 if ok, !0 otherwise
int options_init(void) {

  struct opt_scope *os = opt_scopes;

  // load options for all configured
  while (os) {

    // load from defined source
    char *data = source_data(os->source, os->source_filter);
    if (! data) {
      wlog(L_WARN, "source '%s' failed for options '%s', skipped", os->source, os->name);
    } else {
      // parse loaded options and store them in a tree
      options_store_options(os, data);
      free(data);
    }
 
    // load next options scope
    os = os->next;
  }

  // all done
  return 0;
}


//! find option value by name
//! \param scope options scope
//! \param opname option name
//! \return option value or NULL if not found
char *option_value(char *scope, char *opname) {

  wlog(L_DEBUG8, "searching for op '%s' in scope '%s'", opname, scope);

  struct opt_scope *os;
  struct option opt = {.name = opname, .value = NULL };
  struct option *found = NULL;

  // a scope defined, search for the option within it
  if (scope && *scope) {
    os = options_scope_find(scope);
    if (os)
      found = tree_find((void*)&opt, &os->options, op_cmp_s);
  } else {
  // no scope defined - search in all scopes and return first match
    os = opt_scopes;
    while (os) {
      found = tree_find((void*)&opt, &os->options, op_cmp_s);
      if (found)
        break;
      os = os->next;
    }
  }

  // all done
  return (found ? found->value : NULL);
}


#define SUBST_BEGIN         "%{"
#define SUBST_END            "}"
#define SUBST_SCOPE_DELIM    "&"
#define SUBST_DEFAULT_DELIM  "|"

//! find and replace all words beginning with % and enclosed 
//! in {} in a string with corresponding config value
//! option scope and its name is delimited by SUBST_SCOPE_DELIM, ex:
//! %{scope1&option2}; if scope is missed (i.e. %{&option2} or just
//! %{option2}) then first matching option name from any scope
//! will be used. If no match found then empty value will be substituted.
//! Also it is possible to specify default value to subst if option not found
//! use syntax, ex: %{scope1&option1|default_value}
//! \param in_str source string
//! \return pointer to NEW string with substituted values or NULL if error
//! \todo make this func to accept SUBST_BEGIN and SUBST_END as args
char *options_subst(char *in_str) {

  if (! in_str)
    return NULL;

  // a string to compose and return
  char *out_str = NULL;

  // examine input string and do the job
  while (in_str && *in_str) {

    // find SUBST_BEGIN marker
    char *begin = strstr(in_str, SUBST_BEGIN);

    // save unmodified part before SUBST_BEGIN (if any)
    int len_to_copy;
    if (! begin)
      len_to_copy = strlen(in_str);
    else
      len_to_copy = begin - in_str;

    if (! out_str) {
      out_str = strndup(in_str, len_to_copy);
      assert(out_str);
    } else {
      out_str = realloc(out_str, strlen(out_str) + len_to_copy + 1);
      assert(out_str);
      strncat(out_str, in_str, len_to_copy);
    }

    if (! begin)
      break;

    // skip BEGIN marker and search for ending
    // not using strstr() and such because SUBST_BEGIN may be 
    // equal to SUBST_END (i.e. %%WORD%% case)
    begin += sizeof(SUBST_BEGIN) - 1;

    // note: 'begin' may now become a NULL, so strstr() below will work correctly (return NULL too)
    in_str = strstr(begin, SUBST_END);

    // end-of-string reached or no SUBST_END found!
    // bare SUBST_BEGINs are not allowed...
    if (! in_str) {
      if (out_str) {
        free(out_str);
        out_str = NULL;
      }
      break;
    }

    // very good, found an ending
    // make a copy and search for its value
    // empty string will be used if no value found
    char *opt = strndup(begin, in_str - begin);
    assert(opt);

    char *opt_name = opt;

    // this probably contains scope name too, so extract it
    char *scope_name = NULL;
    char *scope_delim = strstr(opt, SUBST_SCOPE_DELIM);
    if (scope_delim) {
      *scope_delim = '\0';
      opt_name = ++ scope_delim;
      scope_name = opt;
    }

    // it is possible that default value was specified - extract it
    char *def_value = NULL;
    char *def_delim = strstr(opt_name, SUBST_DEFAULT_DELIM);
    if (def_delim) {
      *def_delim = '\0';
      def_value = ++ def_delim;
    }

    // get the value for option!
    char *value = option_value(scope_name, opt_name);

    // value not found, but the default is present - use it!
    if (! value && def_value)
      value = def_value;

    // found an option value - copy it to out_str
    if (value) {
      if (! out_str) {
        out_str = strdup(value);
        assert(out_str);
      } else {
        out_str = realloc(out_str, strlen(out_str) + strlen(value) + 1);
        assert(out_str);
        strcat(out_str, value);
      }
    }

    // cleanups and go on with next token
    free(opt);
    in_str += sizeof(SUBST_END) - 1;

  } //while (in_str)...

  // done
  return out_str;
}



