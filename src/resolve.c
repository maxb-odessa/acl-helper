/** \file */


#include "acl-helper.h"
#include "tree.h"
#include "log.h"
#include "conf.h"

#ifdef USE_RESOLVE
  #include <netdb.h>
#endif

#include "resolve.h"

//! cached host ips entry
struct ip_entry {
  char *hostname;                      //!< hostname to resolve
  time_t expire;                       //!< entry expiration time
  in_addr_t ips[MAX_RESOLVED_IPS + 1]; //!< resolved ips array
};

//! mutex for cache operations
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//! resolved hosts cache itself
node_t *ip_cache;

//! compare func to find hostname in cache tree
static int hostname_cmp(const void *h1, const void *h2) {
  return strcasecmp(((struct ip_entry *)h1)->hostname, ((struct ip_entry *)h2)->hostname);
}

//! search cache for resolved host ips
//! also add new entry to the tree if not found
//! \param host hostname
//! \return found cached entry or NULL
static struct ip_entry *ip_cache_find(char *host) {

  // inits and preparations
  struct ip_entry *ip_to_find, *ip_found;
  ip_to_find = calloc(1, sizeof(struct ip_entry));
  assert(ip_to_find);
  ip_to_find->hostname = strdup(host);
  assert(ip_to_find->hostname);

  // lock the cache
  pthread_mutex_lock(&mutex);

  // search the cache for non-expired host entry
  // OR
  // add new one
  ip_found = tree_search(ip_to_find, &ip_cache, hostname_cmp);

  // already existing entry?
  if (errno != ENOENT) {
    free(ip_to_find->hostname);
    free(ip_to_find);
  }

  // unlock the cache
  pthread_mutex_unlock(&mutex);

  // return address of found entry
  // (it should be valid all the time becase of no cache removal is doing)
  return ip_found;
}


//! resolve hostname
//! \param host hostname to resolve
//! \param aip address of array to place resolved ips
//! \param max_ips max number of ips to return
//! \return num of resolved ips or -1 on error
int resolve_host(char *host, in_addr_t *aip, int max_ips) {

  // search the cache first (or add new entry)
  struct ip_entry *ip = ip_cache_find(host);

  // check found entry expiration date (if not new)
  if (ip && ip->expire > time(NULL)) {
    wlog(L_DEBUG8, "using cached ip data for '%s'", host);
    max_ips = max_ips > 0 && max_ips < MAX_RESOLVED_IPS ? max_ips : MAX_RESOLVED_IPS;
    int i = 0;
    for (; ip->ips[i] && i < max_ips; i ++)
      aip[i] = ip->ips[i];
    return i;
  }


  // at this point "ip" is newly created one or expired entry
  // at least "ip" contains "host" stored and "expired" = 0
  // anyhow, we have to reinit it (re-resolve)

  // do the resolving
  // \todo set timeout here?
  wlog(L_DEBUG5, "resolving '%s'", host);
  struct addrinfo *res, hints = {0,};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  int err = getaddrinfo(host, NULL, &hints, &res);
  if (err) {
    wlog(L_DEBUG5, "failed to resolve '%s': %s", host, gai_strerror(err));
    ip->expire = time(NULL) + config.resolve_neg_ttl; // cache negative answers too
    return -1;
  }

  // walk over resolved ips and store them
  struct addrinfo *rp = res;
  int i = 0;
  for (; rp && i < max_ips; rp = rp->ai_next) {
    // skip non ipv4
    if (rp->ai_family != AF_INET)
      continue;
    // get an ipv4 addr
    struct in_addr *ina = &(((struct sockaddr_in*)(rp->ai_addr))->sin_addr);
    wlog(L_DEBUG8, "host '%s' has address '%d: %s'", host, i, inet_ntoa(*ina));
    aip[i ++] = ntohl(ina->s_addr);
  }   

  // done with resolving
  freeaddrinfo(res);

  // fill/update ip cache entry
  wlog(L_DEBUG8, "caching resolved ip(s) for '%s'", host);
  ip->expire = time(NULL) + config.resolve_ttl;
  // we may have no IPv4 addressed resolved, so nothing to return :(
  if (i)
    memcpy(ip->ips, aip, sizeof(in_addr_t) * i);

  // done
  return i;
}



//! Convert IP[/NETLEN|MASK] string into ip and net
//! in host byte order. The func expects network part in
//! full format only, i.e. 1.2.3.4/24, 1.2.3.4/255.255.224.0
//! \param ipstr ip addres string with possible netmask
//! \param ipa where to place converted host addr portion
//! \param neta where to place converted network portion (may be NULL)
//! \return 0 if ok
//! note: ip and net validness check is limited!
int str2ipaddr(char *ipstr, in_addr_t *ipa, in_addr_t *neta) {

  // find ip/net separator (if exists)
  char *net = strchrnul(ipstr, '/');

  // convert ip part
  char *ip = strndup(ipstr, net - ipstr);
  assert(ip);
  struct in_addr ina;
  if (inet_aton(ip, &ina))
    *ipa = ntohl(ina.s_addr);
  else
    *ipa = -1;
  free(ip);

  // invalid ip (0 is ok in case of ex: 0.0.0.0/0)
  if (*ipa < 0)
    return 1;

  // no network extraction required, just return
  if (neta == NULL)
    return 0;

  // no net specified, assume /32
  // no net after slash (i.e. 1.2.3.4/), assume /32
  // OR advance beyond found '/' char and go on
  if (! *net || ! *(++ net)) {
    *neta = 0xFFFFFFFF;
    return 0;
  }

  // calc net part string length
  size_t netlen = strlen(net);

  // is it a netmask len? must be 1 or 2 bytes long (i.e. /8 or /24 etc)
  // OR
  // is it a netmask? must be >= 7 and <= 15 bytes long (i.e. 255.255.255.0)
  if (netlen <= 2) {
    errno = 0;
    int len = strtol(net, NULL, 10);
    if (! errno && len > 0 && len <= 32)
      *neta = 0xFFFFFFFF << (32 - len);
    else
      *neta = 0;
  } else if (netlen >= 7 && netlen <= 15) {
    struct in_addr ina;
    if (inet_aton(net, &ina))
      *neta = ntohl(ina.s_addr);
    else
      *neta = 0;
  }

  // all OK or invalid network
  return ! *neta;
}

