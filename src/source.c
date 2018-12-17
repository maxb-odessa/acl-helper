/** \file */


#include "acl-helper.h"
#include "tree.h"
#include "log.h"
#include "misc.h"

#include "source.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>


#ifdef USE_SQLITE3
  #include <sqlite3.h>
#endif

#ifdef USE_PGSQL
  #include <libpq-fe.h>
#endif

#ifdef USE_MEMCACHED
  #include <libmemcached/memcached.h>
#endif


// A STUB
int sources_init(void) {
  return 0;
}

// sources list
static struct source *sources;

//! find source by name
//! \param name source name
//! \return pointer at found source entry or NULL 
struct source *source_find(char *name) {
  struct source *sp = sources;
  while (sp) {
    if (! strcmp(sp->name, name))
      break;
    else
      sp = sp->next;
  }
  // wlog(L_DEBUG5, "%s source '%s'", sp ? "found" : "NOT FOUND", name);
  return sp;
}



//! return data from various sources;
//! returned data then must be freed by a caller
//! \param sname source name
//! \param filter source filter (from checker, option, etc)
//! \return char* to data string if success, NULL otherwise
char *source_data(char *sname, char *filter) {
  // find a source for given name
  struct source *sp = source_find(sname);
  if (sp && sp->driver)
    return sp->driver(sp->params, filter);
  return NULL;
}



//! raw source driver: provide embedded data from config file
//! raw data has line delimiter ',' so replace them with '\n'
//! \param params driver params
//! \param filter driver filter
//! \return char* source data or NULL
static char *source_from_raw(char *params, char *filter) {

  char *data = NULL;

  if (! params)
    return NULL;

  // alloc space for new data
  data = strdup(params);
  assert(data);

  // replace all ',' with '\n'
  char *p = data;
  while (*p) {
    if (*p == ',')
      *p = '\n';
    p ++;
  }

  // done
  return data;
}



//! file driver: file content will be returned to caller
//! all '\\r' chars will be stripped
//! \param params driver params
//! \param filter driver filter
//! \return char* source data or NULL
static char *source_from_file(char *params, char *filter) {

  char *data = NULL;

  if (! params)
    return NULL;

  // got filter - regcomp it
  regex_t *freg = NULL;
  if (filter) {
    freg = malloc(sizeof(regex_t));
    assert(freg);
    int reg_err = regcomp(freg, filter, REG_EXTENDED|REG_ICASE);
    if (reg_err) {
      char err_buf[128];
      regerror(reg_err, freg, err_buf, sizeof(err_buf) -1);
      wlog(L_WARN, "disabling invalid filter regex [%s] => %s", filter, err_buf);
      free(freg);
      freg = NULL;
    }
  }


  // get file size
  struct stat st;
  if (stat(params, &st) || st.st_size < 1) {
    wlog(L_ERR, "empty file or stat(%s) failed: %s", params, strerror(errno));
    return NULL;
  }

  // open the file
  FILE *fp = fopen(params, "r");
  if (! fp) {
    wlog(L_ERR, "failed to open source file '%s': %s", params, strerror(errno));
    return NULL;
  }

  // allocate space for data read from file
  data = calloc(1, st.st_size + 1);
  assert(data);

  // read file content and apply filter
  char *data_p = data;
  ssize_t num = 0;
  char *line = NULL;
  size_t n;

  // get next line from file
  while ((num = getline(&line, &n, fp)) >= 0) {

    // got something?
    if (line) {
      // apply filter
      if (freg && regexec(freg, line, 0, NULL, 0) == REG_NOMATCH) {
        // ignore the line
        wlog(L_DEBUG5, "filtering out '%s' matching '%s'", line, filter);
      } else {
        // save line in buf
        strcpy(data_p, line);
        data_p += num;
      }
    }

  }

  // done with the file
  fclose(fp);

  // free buffer allocated by getline()
  if (line)
    free(line);

  // free regex pattern
  if (freg) {
    regfree(freg);
    free(freg);
  }

  // done
  return data;
}


//! sqlite3 driver: read data from sqlite3 db;
//! records will be merged by '\n' char into a single string
//! only FIRST column will be fetched and stored
//! \param params driver params (db connection string)
//! \param filter driver filter (db query)
//! \return char* source data or NULL
static char *source_from_sqlite3(char *params, char *filter) {

  char *data = NULL;

#ifdef USE_SQLITE3

  if (! params || ! filter)
    return NULL;

  // sqlite3 bd handler
  sqlite3 *dbh;

  // enable URI support for file openings
  sqlite3_config(SQLITE_CONFIG_URI, 1);

  // open the db
  if (sqlite3_open(params, &dbh) != SQLITE_OK) {
    wlog(L_ERR, "failed to open db '%s': %s", params, sqlite3_errmsg(dbh));
    return NULL;
  }

  // prepare the query
  sqlite3_stmt *sth;
  if (sqlite3_prepare(dbh, filter, -1, &sth, NULL) != SQLITE_OK) {
    wlog(L_ERR, "failed to prepare sql query '%s': %s", filter, sqlite3_errmsg(dbh));
    sqlite3_close(dbh);
    return NULL;
  }

  // do the query and fetch records
  while (sqlite3_step(sth) == SQLITE_ROW) {

    // we take only FIRST column
    int colsize = sqlite3_column_bytes(sth, 0);
    if (colsize <= 0)
      continue;

    // fetch a row (first and the only one)
    // it must be in form "key=value", one row
    char *row = (char *)sqlite3_column_text(sth, 0);

    // count '\n' in the row (why? see comments below)
    int lf_n;
    char *lf_p;
    for (lf_n = 0, lf_p = row; (lf_p = strchr(lf_p, '\n')) != NULL; lf_p ++, lf_n ++);
 
    // alloc space for new row and merge it with returning data
    // don't forget about trailing '\n'
    if (! data)
      data = calloc(1, colsize + lf_n + 2);
    else
      data = realloc(data, strlen(data) + colsize + lf_n + 2);
    assert(data);

    // sometimes resulting row contains one or more '\n' (without leading '\')
    // so we have to append them ourselves
    char *rp = row, *dp = data + strlen(data);
    while (*rp) {
      // ignore '\r's
      if (*rp == '\r') {
        rp ++;
        continue;
      }
      // found '\n' and no '\' before it - store additional '\' in data
      if (*rp == '\n' && (rp == row || *(rp - 1) != '\\'))
        *dp ++ = '\\';
      // store row char
      *dp ++ = *rp ++;
    }

    // add trailing \n and finalize the string
    *dp ++ = '\n';
    *dp = '\0';

  }

  // done, destroy sqlite3 related structs
  sqlite3_finalize(sth);
  sqlite3_close(dbh);

#endif //USE_SQLITE3

  // done
  return data;
}


//! dummy sorce driver, returns no data
//! \param params ignored
//! \param filter ignored
//! \return empty string
static char *source_from_dummy(char *params, char *filter) {
  return strdup("");
}



//! postgresql driver
//! records will be merged by '\n' char into a single string
//! only FIRST column will be fetched and stored
//! \param params driver params (db connection string)
//! \param filter driver filter (db query)
//! \return char* source data or NULL
static char *source_from_pgsql(char *params, char *filter) {

  char *data = NULL;

#ifdef USE_PGSQL

  if (! params || ! filter)
    return NULL;

  // connect to psql
  PGconn *pg = PQconnectdb(params);
  if (PQstatus(pg) != CONNECTION_OK) {
    wlog(L_ERR, "connection to PgSQL failed: %s", PQerrorMessage(pg));
    PQfinish(pg);
    return NULL;
  }

  // make a query
  PGresult *res = PQexec(pg, filter);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    wlog(L_ERR, "PgSQL query failed: %s", PQerrorMessage(pg));
    PQclear(res);
    PQfinish(pg);
    return NULL;
  }

  // fetch records (start from last row, it doesn't matter actually)
  int i = PQntuples(res);
  for (; i >= 0; i --) {

    // we take only FIRST column
    int colsize = PQgetlength(res, i, 0);
    if (colsize <= 0)
      continue;

    // fetch a row (first and the only one)
    // it must be in form "key=value", one row
    char *row = PQgetvalue(res, i, 0);

    // count '\n' in the row (why? see comments below)
    int lf_n;
    char *lf_p;
    for (lf_n = 0, lf_p = row; (lf_p = strchr(lf_p, '\n')) != NULL; lf_p ++, lf_n ++);
 
    // alloc space for new row and merge it with returning data
    // don't forget about trailing '\n'
    if (! data)
      data = calloc(1, colsize + 2);
    else
      data = realloc(data, strlen(data) + colsize + 2);
    assert(data);

    // sometimes resulting row contains one or more '\n' (without leading '\')
    // so we have to append them ourselves
    char *rp = row, *dp = data + strlen(data);
    while (*rp) {
      // ignore '\r's
      if (*rp == '\r') {
        rp ++;
        continue;
      }
      // found '\n' and no '\' before it - store additional '\' in data
      if (*rp == '\n' && (rp == row || *(rp - 1) != '\\'))
        *dp ++ = '\\';
      // store row char
      *dp ++ = *rp ++;
    }

    // add trailing \n and finalize the string
    *dp ++ = '\n';
    *dp = '\0';
  }

  // we'r done with pgsql
  PQclear(res);
  PQfinish(pg);

#endif // USE_PGSQL

  // done
  return data;
}


//! memcached driver
//! A STUB
static char *source_from_memcached(char *param, char *filter) {
#ifdef USE_MEMCACHED
  // some memcache related code
#endif
  return 0;
}


//! parse source config line and create new source in list
//! \param str source definition line
//! \return 0 if ok, !0 otherwise
int source_config(char *str) {

  // split str into tokens (we expect exactly 3 tokens)
  char *array[3] = {0, };
  int n = parse_string(str, array, ":", 3);
  if (n < 3) {
    wlog(L_ERR, "not enough args for 'source'");
    return 1;
  }

  // allow no empty values (except last one)
  for (n --; n; n --) {
    if (! array[n - 1]) {
      wlog(L_ERR, "empty option in 'source'");
      return 1;
    }
  }

  // no duplicate sources allowed
  if (source_find(array[0])) {
    wlog(L_ERR, "source '%s' already defined", array[0]);
    return 1;
  }
 
  // go to last source in list
  struct source *sp = sources;
  while (sp && sp->next)
    sp = sp->next;

  // allocate space for new source
  struct source *new_sp = calloc(1, sizeof(struct source));
  assert(new_sp);

  // add new source to the list
  if (! sp)
    sp = sources = new_sp;
  else
    sp = sp->next = new_sp;

  // store source name
  sp->name = array[0];
      
  // store source driver type
  if (! strcmp("raw", array[1]))
    sp->driver = source_from_raw;
  else if (! strcmp("file", array[1]))
    sp->driver = source_from_file;
  else if (! strcmp("sqlite3", array[1]))
    sp->driver = source_from_sqlite3;
  else if (! strcmp("pgsql", array[1]))
    sp->driver = source_from_pgsql;
  else if (! strcmp("memcached", array[1]))
    sp->driver = source_from_memcached;
  else if (! strcmp("dummy", array[1]))
    sp->driver = source_from_dummy;
  else {
    wlog(L_ERR, "unsupported 'source' driver");
    return 1;
  }
 
  // store driver params
  sp->params = array[2];

  // done
  return 0;     
}


