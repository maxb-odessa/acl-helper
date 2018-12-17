
#include "acl-helper.h"

#include "log.h"
#include "conf.h"
#include "resolve.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_GEOIP2
  #include <maxminddb.h>
#endif

#include "geoip2.h"


#ifdef USE_GEOIP2
//! static geoip2 db handler
static struct MMDB_s mmdb;
#endif

//! init geoip2 (maxmind) lookup engine
//! \return 0 if ok, !0 on error
int geoip2_init(void) {

#ifdef USE_GEOIP2
  // open geoip2 db file
  int status = MMDB_open(config.geoip2_db, MMDB_MODE_MMAP, &mmdb);
  if (status != MMDB_SUCCESS) {
    wlog(L_ERR, "failed to open GeoIP2 DB '%s': %s", config.geoip2_db, MMDB_strerror(status));
    return 1;
  }
#endif

  // done
  return 0;
}



//! perform geoip lookup by provided ip
//! the function never fails: in case of error it will fill '*gi2'
//! structure with 'N/A' values
//! \param ip_in ip address (or host name) to lookup
//! \param gi2 address of geolication data to store in
//! \return nothing
void geoip2_lookup(char *ip_in, geoip2_data *gi2) {

#ifdef USE_GEOIP2

#define MIN_OF(a, b) ((a) < (b) ? (a) : (b));

  // init lookup results storage
  strcpy(gi2->continent, "N/A");  // it is 4 bytes long
  strcpy(gi2->country, "N/A");    // it is 4 bytes long
  strcpy(gi2->city, "N/A");       // it is 64 bytes long

  // resolve ip (it MAY be not an ip, but a hostname, so try
  // to resolve it anyway)
  in_addr_t *ips = calloc(MAX_RESOLVED_IPS + 1, sizeof(in_addr_t));
  assert(ips);
  int n_ips = resolve_host(ip_in, ips, MAX_RESOLVED_IPS);

  // try to lookup in geoip2 db
  int gai_error = 0, mmdb_error = 0;
  MMDB_lookup_result_s res;
  if (n_ips < 1) 
    // resolve failed, will try to go on as-is
    res = MMDB_lookup_string(&mmdb, ip_in, &gai_error, &mmdb_error);
  else
    // take first resolved IP (dunno what to do if host resolves to multiple addressess)
    res = MMDB_lookup_string(&mmdb, inet_ntoa(*(struct in_addr *)&ips[0]), &gai_error, &mmdb_error);

  // no need in this anymore
  free(ips);

  // lookup success!
  if (! gai_error && mmdb_error == MMDB_SUCCESS && res.found_entry) {

    wlog(L_DEBUG5, "geoip2: found entry for '%s'", ip_in);

    MMDB_entry_data_s entry_data;

    // fetch continent (2 letters)
    memset(&entry_data, 0, sizeof(entry_data));
    MMDB_get_value(&res.entry, &entry_data, "continent", "code", NULL);
    if (entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
      size_t len = MIN_OF(entry_data.data_size, sizeof(gi2->continent) - 1);
      strncpy(gi2->continent, entry_data.utf8_string, len);
      *(gi2->continent + len) = '\0';
      
    }

    // fetch country (2 letters)
    memset(&entry_data, 0, sizeof(entry_data));
    MMDB_get_value(&res.entry, &entry_data, "country", "iso_code", NULL);
    if (entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
      size_t len = MIN_OF(entry_data.data_size, sizeof(gi2->country) - 1);
      strncpy(gi2->country, entry_data.utf8_string, len);
      *(gi2->country + len) = '\0';
    }

    // fetch city (many letters, English name only)
    memset(&entry_data, 0, sizeof(entry_data));
    MMDB_get_value(&res.entry, &entry_data, "city", "names", "en", NULL);
    if (entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
      size_t len = MIN_OF(entry_data.data_size, sizeof(gi2->city) - 1);
      strncpy(gi2->city, entry_data.utf8_string, len);
      *(gi2->city + len) = '\0';
    }

  // lookup failure...
  } else {
    wlog(L_DEBUG0, "geoip2: no entry found for '%s'", ip_in);
  }

#endif // USE_GEOIP2

  // all done
  return;
}


