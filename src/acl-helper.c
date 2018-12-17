/** \file */

/***************************************************************************
* acl helper for squid
*
****************************************************************************/

#include "acl-helper.h"

#ifdef HAVE_SYS_AUXV_H
  #include <sys/auxv.h>
#endif

#include "tree.h"
#include "log.h"
#include "conf.h"
#include "loop.h"
#include "source.h"
#include "checker.h"
#include "ssl.h"
#include "geoip2.h"
#include "options.h"


//! supported drivers/features list
static char *aclh_features = ""
  " +file"
#ifdef USE_MATCH
  " +match"
#else
  " -match"
#endif
#ifdef USE_PCRE
  " +pcre"
#else
  " -pcre"
#endif
#ifdef USE_REGEX
  " +regex"
#else
  " -regex"
#endif
#ifdef USE_SQLITE3
  " +sqlite3"
#else
  " -sqlite3"
#endif
#ifdef USE_PGSQL
  " +pgsql"
#else
  " -pgsql"
#endif
#ifdef USE_MEMCACHED
  " +memcached"
#else
  " -memcached"
#endif
#ifdef USE_RESOLVE
  " +resolve"
#else
  " -resolve"
#endif
#ifdef USE_SSL
  " +ssl"
#else
  " -ssl"
#endif
#ifdef USE_GEOIP2
  " +geoip2"
#else
  " -geoip2"
#endif
;

static char *aclh_version = ACLH_VERSION;

//! cleanup function called at exit
void clean_exit(void) {
  // remove pid file
  if (config.pidfile)
    unlink(config.pidfile);
  // try to log our exit
  wlog(L_INFO, "Exiting.");
}


//! exit/abort/quit/etc signal handler
//! \param sig signal number
sighandler_t sighandler(int sig) {
  wlog(L_INFO, "Signalled: %d", sig);
  exit(sig);
  return 0;
} 


//! reconfig/restart signal handler
//! \param sig signal number
void restart(int sig) {
  wlog(L_INFO, "got SIGHUP, executing self from '%s'", config.execpath);
  execve(config.execpath, config.argv, NULL);
  wlog(L_ERR, "execve() failed: %s", strerror(errno));
} 



//! show the help and nothing more
static void show_help(char *pname) {
  printf("Usage: %s [options]\n"
         "where 'options' are:\n"
         "  -h          show this help and exit\n"
         "  -t          test config and exit\n"
         "  -v          show version and exit\n"
         "  -c <file>   use 'file' as config (default is '%s')\n",
         pname, DEFAULT_CONFIG_FILE);
}


//! common config data
struct config config;

//! the main function.
//! \param argc number of args.
//! \param argv args list.
int main(int argc, char *argv[]) {

  // this flag is for 'test config and exit'
  int test_config = 0;

  // extract and store our visible prog name (for logging, etc)
  config.progname = rindex(argv[0], '/');
  if (config.progname)
    config.progname ++;
  else
    config.progname = argv[0];

  // guess our exec path (needed for execve())
  // or if this fails we disable SIGHUP handling
  // 1. try argv[0] method
  config.execpath = realpath(argv[0], NULL);

  // 2. try linux specific /proc method
  if (! config.execpath) {
     config.execpath = calloc(1, PATH_MAX + 1);
     if (readlink("/proc/self/exe", config.execpath, PATH_MAX) < 0) {
       free(config.execpath);
       config.execpath = NULL;
     }
  }

  // 3. try ELF loader
  #ifdef HAVE_GETAUXVAL
  if (! config.execpath)
    config.execpath = (char *)getauxval(AT_EXECFN);
  #endif

  // preserve args for further execve() call
  config.argv = calloc(argc + 1, sizeof(char *));
  int i;
  for (i = 0; argv[i]; i ++)
    config.argv[i] = strdup(argv[i]);

  // get cmdline options
  int opt;
  while ((opt = getopt(argc, argv, "tvhc:")) != -1) {

    switch (opt) {

      case 'h':
        show_help(config.progname);
        exit(0);
        break;

      case 'v':
        printf("%s\nSupported features:%s\n", aclh_version, aclh_features);
        exit(0);
        break;

      case 'c':
        config.file = optarg;
        break;

      case 't':
        test_config ++;
        break;

      default:
        show_help(config.progname);
        exit(1);
        break;
    }

  } //while(opts...)

  // set exit handler
  atexit(&clean_exit);

  // catch some signals
  signal(SIGINT, (void*)&sighandler);
  signal(SIGQUIT, (void*)&sighandler);
  signal(SIGABRT, (void*)&sighandler);
  //signal(SIGSEGV, (void*)&sighandler); // uhm... may be risky

  // check required arg(s)
  if (! config.file)
    config.file = DEFAULT_CONFIG_FILE;

  // pre-init config struct (set defaults)
  config.debug = 0;
  config.pid = getpid();
  config.euid = geteuid();
  config.egid = getegid();
  config.concurrency = DEFAULT_CONCURRENCY;
  config.ssl_ca_file = DEFAULT_CA_FILE;
  config.ssl_timeout = DEFAULT_SSL_TIMEOUT;
  config.ssl_verify_ttl = DEFAULT_SSL_VERIFY_TTL;
  config.resolve_ttl = DEFAULT_RESOLVE_TTL;
  config.resolve_neg_ttl = DEFAULT_NEG_RESOLVE_TTL;
  config.geoip2_db = DEFAULT_GEOIP2_DB_FILE;

  // adjust stdout buffering
  setlinebuf(stdout);
 
  // read config file
  if (config_read()) {
    wlog(L_CRIT, "Configuration failed, exiting!");
    fprintf(stderr, "Configuration failed, exiting!\n");
    exit(2);
  }

  // init logging
  if (log_init())
    wlog(L_WARN, "failed to init logging, using STDERR");

  // check/create pid file
  if (!test_config && config.pidfile) {
    // read pid from pidfile
    FILE *pidfp = fopen(config.pidfile, "a+");
    if (! pidfp) {
      wlog(L_CRIT, "failed to open/create pid file '%s': %s", config.pidfile, strerror(errno));
      exit(3);
    }
    // get pid and check its validness
    pid_t pid = 0;
    if (! fscanf(pidfp, "%u", &pid) || ! pid)
      wlog(L_WARN, "invalid pid in '%s', overwriting...", config.pidfile);
    else {
      // see if pid is alive
      // saved pid must not be our current pid (got SIGHUP and execve() was called)
      if (pid != config.pid) {
        if (! kill(pid, 0)) {
          wlog(L_CRIT, "another copy is running (pid: %u)", (unsigned)pid);
          exit(3);
        } else
          wlog(L_NOTE, "stale pid (%u) detected in pid file", (unsigned)pid);
      }
    }
    // ok, write our pid into file 
    ftruncate(fileno(pidfp), 0);
    fprintf(pidfp, "%u", (unsigned)config.pid);
    fclose(pidfp);
  }

  // drop privs if have to
  if (! test_config && setegid(config.egid)) {
    wlog(L_CRIT, "setegid(%d) failed: %s", config.egid, strerror(errno));
    exit(3);
  }

  if (! test_config && seteuid(config.euid)) {
    wlog(L_CRIT, "seteuid(%d) failed: %s", config.euid, strerror(errno));
    exit(4);
  }

  wlog(L_INFO, "Started as user %d:%d", config.euid, config.egid);

  // init data sources
  if (sources_init()) {
    wlog(L_CRIT, "failed to init source(s), exiting");
    exit(10);
  }

  // init runtime options
  if (options_init()) {
    wlog(L_CRIT, "failed to init runtime options, exiting");
    exit(11);
  } 

  // init ssl engine
  if (ssl_init()) {
    wlog(L_CRIT, "failed to init SSL engine, exiting");
    exit(12);
  }

  // init geoip2
  if (geoip2_init()) {
    wlog(L_CRIT, "failed to init GeoIP2 engine, exiting");
    exit(13);
  }

  // init checkers
  if (checkers_init()) {
    wlog(L_CRIT, "failed to init checker(s), exiting");
    exit(14);
  }

  // we do have our exec paths, so we can setup SIGHUP handler
  if (config.execpath) {
    struct sigaction sig_action;
    sig_action.sa_handler = restart;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = SA_NODEFER;
    sigaction(SIGHUP, &sig_action, NULL);
  } else
    wlog(L_WARN, "Failed to guess our exec path, reconfig via SIGHUP is DISABLED");

  // only config test was requested, exiting
  if (test_config) {
    wlog(L_INFO, "Config test finished: OK");
    exit(0);
  } else
    wlog(L_INFO, "Ready to process requests");

  // run main loop
  if (loop_run()) {
    wlog(L_CRIT, "main loop failure, exiting");
    exit(99);
  }

  // finita la comedia! :)
  exit(0);
}


