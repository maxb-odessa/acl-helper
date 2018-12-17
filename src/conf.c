/** \file */


#include "acl-helper.h"
#include "log.h"
#include "tree.h"
#include "options.h"
#include "misc.h"
#include "checker.h"
#include "source.h"

#include "conf.h"



//! read config file.
//! \return 0 on success or >0 on error
int config_read(void) {

  // open config file
  FILE *fp = fopen(config.file, "r");
  if (!fp) {
    wlog(L_ERR, "failed to read config file '%s': %s", config.file, strerror(errno));
    return 1;
  }

  // read and parse config file
  char buf[CONF_MAX_LINE_SIZE + 1];
  int lines_num = 0;
  char *line = NULL;
  int concating = 0;   // a flag to indicate concatenated line
  while (fgets(buf, sizeof(buf) - 1, fp)) {

    // reinit line for parsing (if not concatting)
    if (! concating)
      line = NULL;

    // lines counter
    lines_num ++;

    // strip leading and trailing blanks
    char *bufp = strip_blanks(buf);

    // throw away comments and empty lines
    if (! *bufp || *bufp == COMMENT_CHAR)
      continue;

    // too long line?
    size_t buf_len = strlen(bufp);
    if (buf_len >= CONF_MAX_LINE_SIZE) {
      wlog(L_ERR, "too long line (max is %d) in config file '%s:%d'", CONF_MAX_LINE_SIZE, config.file, lines_num);
      return 4;
    }

    // splitted line ('\' at the end) must be concatted
    if (bufp[buf_len - 1] == '\\') {
      // remove trailing '\'
      bufp[buf_len - 1] = '\0';
      // rise 'we are concatting long lines!' flag
      concating ++;
    } else {
      // not concating the line (anymore)
      if (! concating) {
        if (line)
          line = NULL;
      } else
        concating = 0;
    }

    // make a copy of buf (concating or not)
    if (! line)
      line = calloc(buf_len + 1, 1);
    else
      line = realloc(line, strlen(line) + buf_len + 1);
    assert(line);

    // make a working copy of conf string or concat to prev one
    strcat(line, bufp);

    // still concating lines - read next one
    if (concating)
      continue;

    // break like into param and value
    // delimiter is first '=' with optional spaces around it

    // find a '=' char or next line
    char *eqchar = index(line, '=');

    // invalid line
    if (! eqchar) {
      wlog(L_ERR, "invalid line in config file '%s:%d'", config.file, lines_num);
      return 3;
    }

    // split the line by '='
    *eqchar  = '\0';

    // extract param and value
    char *param = strip_blanks(line);
    char *value = strip_blanks(++ eqchar);

    // empty values are not allowed
    if (! value || ! *value) {
      wlog(L_ERR, "empty value for '%s' in config file '%s:%d'", param, config.file, lines_num);
      return 4;
    }

    wlog(L_DEBUG5, "CONFIG OPTION: [%s] => [%s]", param, value);

    // time to fill config structure
   
    // get checker (we expect 6 tokens) 
    if (! strcmp("checker", param)) {
      if (checker_config(value)) {
        wlog(L_ERR, "invalid 'checker' in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }

    // get debug level 0-9
    if (! strcmp("debug", param)) {
      config.debug = str2int(value, 0, 10);
      if (errno) {
        wlog(L_ERR, "invalid 'debug' in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }

    // get pidfile
    if (! strcmp("pidfile", param)) {
      config.pidfile = strdup(value);
      assert(config.pidfile);
      continue;
    }

    // get concurrency value 0-255
    if (! strcmp("concurrency", param)) {
      errno = 0;
      config.concurrency = (int)strtol(value, NULL, 10);
      if (errno || config.concurrency < 0 || config.concurrency > MAX_CONCURRENCY) {
        wlog(L_ERR, "invalid 'concurrency' in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }
   
    // get user to seteuid
    if (! strcmp("user", param)) {
      struct passwd *pw = getpwnam(value);
      if (! pw) {
        wlog(L_ERR, "invalid 'user' in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      config.euid = pw->pw_uid;
      continue; 
    }

    // get group to setegid
    if (! strcmp("user", param)) {
      struct group *gr = getgrnam(value);
      if (! gr) {
        wlog(L_ERR, "invalid 'group' in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      config.egid = gr->gr_gid;
      continue;
    }

    // get logging settings
    if (! strcmp("log", param)) {
      if (log_config(value)) {
        wlog(L_ERR, "invalid 'log' in config file '%s:%d'", config.file, lines_num);
        return 5;
      } 
      continue;
    }

    // get sources
    if (! strcmp("source", param)) {
      if (source_config(value)) {
        wlog(L_ERR, "invalid 'source' in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }

    // get options
    if (! strcmp("options", param)) {
      if (option_config(value)) {
        wlog(L_ERR, "invalid 'options' in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }

    // get ssl CA file path
    if (! strcmp("ssl_ca_file", param)) {
      config.ssl_ca_file = strdup(param);
      assert(config.ssl_ca_file);
      continue;
    }

    // get ssl ttl value
    if (! strcmp("ssl_verify_ttl", param)) {
      config.ssl_verify_ttl = (int)strtol(value, NULL, 10);
      if (errno || config.ssl_verify_ttl < 0 || config.ssl_verify_ttl > 86400 * 7) {
        wlog(L_WARN, "invalid 'ssl_verify_ttl' value in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }

    // get ssl ttl connection timeout
    if (! strcmp("ssl_timeout", param)) {
      config.ssl_timeout = (int)strtol(value, NULL, 10);
      if (errno || config.ssl_timeout < 0 || config.ssl_timeout > 3600) {
        wlog(L_WARN, "invalid 'ssl_timeout' value in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }

    // get resolve ttl value
    if (! strcmp("resolve_ttl", param)) {
      config.resolve_ttl = (int)strtol(value, NULL, 10);
      if (errno || config.resolve_ttl < 0 || config.resolve_ttl > 86400 * 7) {
        wlog(L_WARN, "invalid 'resolve_ttl' value in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }

    // get resolve ttl value
    if (! strcmp("resolve_neg_ttl", param)) {
      config.resolve_neg_ttl = (int)strtol(value, NULL, 10);
      if (errno || config.resolve_neg_ttl < 0 || config.resolve_neg_ttl > 86400 * 7) {
        wlog(L_WARN, "invalid 'resolve_neg_ttl' value in config file '%s:%d'", config.file, lines_num);
        return 5;
      }
      continue;
    }

    // get GeoIP db file location
    if (! strcmp("geoip2_db", param)) {
      config.geoip2_db = strdup(value);
      assert(config.geoip2_db);
      continue;
    }


 
  } // while(fgets...)

  // cleanups
  fclose(fp);

  // done
  return 0;
}



