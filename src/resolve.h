/** \file */


#ifndef __ACLH_RESOLVE_H__
#define __ACLH_RESOLVE_H__


//! max resolved ips for one host to cache
#define MAX_RESOLVED_IPS 16

extern int resolve_host(char *, in_addr_t *, int);
extern int str2ipaddr(char *, in_addr_t *, in_addr_t *);

#endif //__ACLH_RESOLVE_H__

