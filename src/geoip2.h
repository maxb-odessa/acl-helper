/** \file */


#ifndef __ACLH_GEOIP2_H__
#define __ACLH_GEOIP2_H__

//! geoip data
typedef struct geoip2_data {
  char continent[4];  // enough for 2 letter abbrev (or "N/A" string)
  char country[4];    // ditto
  char city[128];     // should be enough :)
} geoip2_data;

// this is what will be returned to squid in 'notes' filled with lookup data
// or "N/A" if lookup failed
#define GEOIP2_NOTES_TMPL "geoip2_continent='%s' geoip2_country='%s' geoip2_city='%s'"

extern int geoip2_init(void);
extern void geoip2_lookup(char *, geoip2_data *);

#endif //__ACLH_GEOIP2_H__

