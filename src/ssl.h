/** \file */


#ifndef __ACLH_SSL_H__
#define __ACLH_SSL_H__

#define SSL_ERROR_NOTE_TMPL  "ssl_error=%d"

extern int ssl_init(void);
extern int ssl_verify_host(char *, unsigned, int);

#endif //__ACLH_SSL_H__

