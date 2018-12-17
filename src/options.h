/** \file */


#ifndef __ACLH_OPTIONS_H__
#define __ACLH_OPTIONS_H__


//! runtime options scopes list
struct opt_scope {
  char *name;                //!< scope name
  char *source;              //!< scope source
  char *source_filter;       //!< scope source filter
  node_t *options;           //!< scope options data (key=value pairs tree)
  struct opt_scope *next;    //!< next scope in list
};


//! runtime options names and values
struct option {
  char *name;               //!< option name
  char *value;              //!< option value
};

//! max options line size (for 'file' driver)
#define OPTIONS_MAX_LINE_SIZE     4096

extern int option_config(char *);
extern int options_init(void);
extern char *options_subst(char *);

#endif //__ACLH_OPTIONS_H__

