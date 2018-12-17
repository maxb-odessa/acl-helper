
/** \file */

#include "acl-helper.h"
#include "conf.h"
#include "misc.h"

#include "log.h"

static FILE *log_fp;

static struct log log;

// this is to avoid logs mixing
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


//! internal logging function
//! \param prio logging priority
//! \param format logging string format
//! \param ... format option list
//! \return nothing
void wlog(int prio, const char *format, ...) {


  va_list ap;
  va_start(ap, format);

  // ignore debugs if bigger then level
  if (prio >= L_DEBUG && prio - L_DEBUG0 > config.debug) {
    return;
  }


  // log NOT into syslog...
  if (log.mode != LOGMODE_SYSLOG) {

    // log to file - open if it is not already or choose stderr otherwise
    if (! log_fp && log.file) {
      log_fp = fopen(log.file, "a");
      if (! log_fp) {
        fprintf(stderr, "ERROR: failed to open log file '%s': %s\nERROR: using STDERR for logging\n",
                log.file, strerror(errno));
        log_fp = stderr;
      }
    }

    // get current time
    time_t now = time(NULL);
    char tsbuf[26];
    ctime_r(&now, tsbuf);
    tsbuf[24] = '\0';

    // compose log prio into string
    char *sprio;
    switch (prio) {
      case L_ERR   : sprio = "ERROR"; break;
      case L_WARN  : sprio = "WARNING"; break;
      case L_NOTE  : sprio = "NOTICE"; break;
      case L_INFO  : sprio = "INFO"; break;
      case L_CRIT  : sprio = "CRITICAL"; break;
      default      : sprio = "DEBUG"; break;
    }

    // log where? use local var
    FILE *local_log_fp;
    if (! log_fp)
      local_log_fp = stderr;
    else
      local_log_fp = log_fp;

    // print first part of the message
    pthread_mutex_lock(&log_mutex);
#if (defined HAVE_SYS_SYSCALL_H && defined __NR_gettid)
    fprintf(local_log_fp, "%s %s[%lu:%lu] %s: ", 
            tsbuf, log.ident, (unsigned long)config.pid, (unsigned long)syscall(__NR_gettid), sprio);
#else
    fprintf(local_log_fp, "%s %s[%lu:%lu] %s: ",
            tsbuf, log.ident, (unsigned long)config.pid, (unsigned long)pthread_self(), sprio);
#endif
    // print the message itself
    vfprintf(local_log_fp, format, ap);
    fputc('\n', local_log_fp);
#ifdef DEBUG
    fflush(local_log_fp);
#endif
    pthread_mutex_unlock(&log_mutex);

  // log to syslog requested
  } else {

    // guess the prio
    int iprio;
    switch (prio) {
      case L_ERR   : iprio = LOG_ERR; break;
      case L_WARN  : iprio = LOG_WARNING; break;
      case L_NOTE  : iprio = LOG_NOTICE; break;
      case L_INFO  : iprio = LOG_INFO; break;
      case L_CRIT  : iprio = LOG_CRIT; break;
      default      : iprio = LOG_DEBUG; break;
    }

    // send the log 
    // (and yes, close it immediately for safety)
    //! \todo TODO: include thread_id support into message (as in above)
    openlog(log.ident, LOG_PID, log.facility);
    vsyslog(iprio, format, ap);
    closelog();

  }

  // all done
  va_end(ap);

}


//! convert syslog facility from string to int (only local0-local7 supported)
//! \param facility_str falility string
//! \return integer representation of param
int syslog_facility(char *facility_str) {
  int facility = -1;

  if (! strcasecmp("local0", facility_str))
    facility = LOG_LOCAL0;
  else if (! strcasecmp("local1", facility_str))
    facility = LOG_LOCAL1;
  else if (! strcasecmp("local2", facility_str))
    facility = LOG_LOCAL2;
  else if (! strcasecmp("local3", facility_str))
    facility = LOG_LOCAL3;
  else if (! strcasecmp("local4", facility_str))
    facility = LOG_LOCAL4;
  else if (! strcasecmp("local5", facility_str))
    facility = LOG_LOCAL5;
  else if (! strcasecmp("local6", facility_str))
    facility = LOG_LOCAL6;
  else if (! strcasecmp("local7", facility_str))
    facility = LOG_LOCAL7;

  return facility;
}


//! convert syslog prio from string to int (only info, notice, error, critical, alert supported)
//! \param prio_str falility string
//! \return integer representation of param
int syslog_prio(char *prio_str) {
  int prio = -1;

  if (! strcasecmp("info", prio_str))
    prio = LOG_INFO;
  else if (! strcasecmp("notice", prio_str))
    prio = LOG_NOTICE;
  else if (! strcasecmp("error", prio_str))
    prio = LOG_ERR;
  else if (! strcasecmp("alert", prio_str))
    prio = LOG_ALERT;
  else if (! strcasecmp("critical", prio_str))
    prio = LOG_CRIT;

  return prio;
}


//! read logging config string and init logger
//! \param str config string for logging
//! \return 0 if ok, !0 otherwise
int log_config(char *str) {

  // split into tokens (we need 3 tokens)
  char *array[3] = {0, };
  int n = parse_string(str, array, ":", 3);
  if (n < 3) {
    wlog(L_ERR, "not enough args for 'log'");
    return 1;
  }

  // store ident
  if (array[1])
    log.ident = array[1];
 
  // file mode
  if (! strcmp("file", array[0])) {
    // store log file name (required)
    if (array[2])
      log.file = array[2];
    else {
      wlog(L_ERR, "file path required for 'log'");
      return 1;
    }

    // adjust log mode
    log.mode = LOGMODE_FILE;

  // syslog mode
  } else if (! strcmp("syslog", array[0])) {

    // store facility
    if (array[2])
      log.facility = syslog_facility(array[2]);
    else
      log.facility = LOG_LOCAL0;

    // adjust log mode
    log.mode = LOGMODE_SYSLOG;

  // unknow log
  } else {
    wlog(L_ERR, "invalid 'log' mode");
    return 1;
  }

  // done
  return 0;
}



//! init logging
//! \return 0 if ok
int log_init(void) {
  if (! log.ident)
    log.ident = config.progname;
  return 0;
}


