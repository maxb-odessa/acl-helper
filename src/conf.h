/** \file */


#ifndef __ACLH_CONF_H__
#define __ACLH_CONF_H__


//! common config data structure
struct config {

  // preserved args
  char *execpath;     //!< full path to our executable
  char **argv;        //!< argv[] from main()

  // runtime generated options
  char *file;         //!< config file path
  char *progname;     //!< our prog name
  pid_t pid;          //!< our pid
  char *pidfile;      //!< location of pid file

  // general settings obtained from conf file
  uid_t euid;              //!< run as user
  gid_t egid;              //!< rus a group
  int debug;               //!< debug level
  int concurrency;         //!< max num of threads to run at once
  char *ssl_ca_file;       //!< path to CA bundle for SSL checker
  int ssl_timeout;         //!< ssl connection timeout
  int ssl_verify_ttl;      //!< ttl for host SSL data cache entries
  int resolve_ttl;         //!< ttl for resolved host ips
  int resolve_neg_ttl;     //!< ttl for NEG resolved host ips
  char *geoip2_db;         //!< geoip2 db file location
};

//! max configurable threads concurrency
#define MAX_CONCURRENCY   255

//! default threads concurrency
#define DEFAULT_CONCURRENCY   10

//! max config line size
#define CONF_MAX_LINE_SIZE     1024

//! default TTL values
#define DEFAULT_SSL_VERIFY_TTL     86400
#define DEFAULT_SSL_TIMEOUT        10
#define DEFAULT_RESOLVE_TTL        3600
#define DEFAULT_NEG_RESOLVE_TTL    60
#define DEFAULT_CA_FILE            "/etc/ssl/certs/ca-bundle.crt"
#define DEFAULT_GEOIP2_DB_FILE     "/usr/share/GeoIP/GeoLite2-City.mmdb"

extern int config_read(void);


#endif //__ACLH_CONF_H__


