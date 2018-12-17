/** \file */


#ifndef __ACLH_CHECKER_H__
#define __ACLH_CHECKER_H__

#ifdef USE_MATCH
  #include <fnmatch.h>
  #ifndef FNM_CASEFOLD
    #define FNM_CASEFOLD 0
  #endif
#endif

#ifdef USE_REGEX
  #include <regex.h>
#endif

#ifdef USE_PCRE
  #include <pcre.h>
#endif


//! checker match types
enum checker_types {
  TYPE_DUMMY = -1,  //!< dummy
  TYPE_STRING,      //!< tree: direct strings comparison
  TYPE_SHELL,       //!< list: shell patterns match
  TYPE_REGEX,       //!< list: posix regex match
  TYPE_PCRE,        //!< list: pcre match
  TYPE_IP,          //!< tree: ip addr[/network] match
  TYPE_LIST,        //!< list: plain records list
  TYPE_SSL,         //!< not a match, but get SSL verify info
};


//! checker actions
enum checker_actions {
  ACTION_NONE = -1,
  ACTION_HIT,
  ACTION_MISS,
  ACTION_NOTE,
};

//! ip + net struct
typedef struct {
  in_addr_t ip;      //!< ip address
  in_addr_t net;     //!< network address
  in_addr_t ipnet;   //!< ip|net combined value
} ip_net_t;


//! checker data record
struct record {
  char *data;           //!< raw unmodified record data (a string)
  union {
    int i;              //!< an integer
    time_t t;           //!< timestamp
    ip_net_t a;         //!< ip addr[/network]
#ifdef USE_REGEX
    regex_t *r;         //!< compiled regex of 'data'
#endif
#ifdef USE_PCRE
    pcre *p;            //!< compiled pcre of 'data'
#endif
  } rec;
  void *ret;            //!< optional data to return to checker on record match
};


//! checker driver definition
typedef struct checker_driver {
  char *name;                  //!< checker name
  int type;                    //!< checker match type
  int icase;                   //!< is case sensitive?
  void *(*match_func)();       //!< matching func for this checker
} cdriver_t;


//! checker config
struct checker {
  // options from conf file
  char *name;                //!< checker name
  char *enable_s;            //!< is checker enabled
  char *field_idx_s;         //!< squid input line field index
  char *driver_s;            //!< checker driver
  char *action_s;            //!< action on match
  char *notes;               //!< 'notes' string to return to squid
  char *source;              //!< source to get data from
  char *source_filter;       //!< source filter
  // converted options
  int enable;                //!< is checker enabled
  int field_idx;             //!< squid input line field index
  int action;                //!< action on match
  // runtime options
  cdriver_t *driver;         //!< checker driver
  node_t *records;           //!< stored data (read from 'source') to match over
  struct checker *next;      //!< next checker in list
};


//! max line len of checker data in file (source 'file')
#define CHECKER_MAX_LINE_SIZE 32768

extern int checker_config(char *);
extern int checkers_init(void);
extern char *checkers_call(char **, int);

#endif //__ACLH_CHECKER_H__


