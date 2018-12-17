/** \file */


#ifndef __ACLH_LOG_H__
#define __ACLH_LOG_H__


// log modes
#define LOGMODE_STDERR   0
#define LOGMODE_FILE     1
#define LOGMODE_SYSLOG   2

// internal logging prios
#define L_ERR        0
#define L_INFO       1
#define L_WARN       2
#define L_NOTE       3
#define L_CRIT       4
#define L_DEBUG      9
#define L_DEBUG0     100
#define L_DEBUG1     101
#define L_DEBUG2     102
#define L_DEBUG3     103
#define L_DEBUG4     104
#define L_DEBUG5     105
#define L_DEBUG6     106
#define L_DEBUG7     107
#define L_DEBUG8     108
#define L_DEBUG9     109


//! logging config data
struct log {
  int mode;           //!< mode: file, syslog, etc
  char *ident;        //!< ident
  int facility;       //!< syslog facility
  char *file;         //!< file path
};

extern int log_init(void);
extern int log_config(char *);
extern void wlog(int, const char *, ...);

#endif //__ACLH_LOG_H__


