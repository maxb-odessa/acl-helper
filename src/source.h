/** \file */


#ifndef __ACLH_SOURCE_H__
#define __ACLH_SOURCE_H__


//! sources list
struct source {
  char *name;          //!< name of source
  char *params;        //!< driver params
  char *(*driver)(char *, char *);  //! function to call to load data
  struct source *next; //!< next in list
};


extern int sources_init(void);
extern int source_config(char *);
extern char *source_data(char *, char *);

#endif // __ACLH_SOURCE_H__


