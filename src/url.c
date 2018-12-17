/*******************************************************************
 *
 * Original code taken from http://geekhideout.com/urlcode.shtml
 *
 *
 ******************************************************************/

/** \file */


#include "acl-helper.h"


/* Converts a hex character to its integer value */
#define FROM_HEX(ch) (isdigit(ch) ? (ch) - '0' : tolower(ch) - 'a' + 10)

/* Converts an integer value to its hex character*/
static char hex[] = "0123456789abcdef";
#define TO_HEX(code) (hex[(code) & 15])


/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(char *str) {

  char *buf = malloc(strlen(str) * 3 + 1);
  assert(buf);
  char *pstr = str, *pbuf = buf;

  while (*pstr) {

    if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') 
      *pbuf++ = *pstr;
    else if (*pstr == ' ') 
      *pbuf++ = '+';
    else 
      *pbuf++ = '%', *pbuf++ = TO_HEX(*pstr >> 4), *pbuf++ = TO_HEX(*pstr & 15);

    pstr++;

  }

  *pbuf = '\0';

  return buf;
}



/* Returns a url-decoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_decode(char *str) {

  char *buf = malloc(strlen(str) + 1);
  assert(buf);
  char *pstr = str, *pbuf = buf;

  while (*pstr) {

    if (*pstr == '%') {
      if (pstr[1] && pstr[2]) {
        *pbuf++ = FROM_HEX(pstr[1]) << 4 | FROM_HEX(pstr[2]);
        pstr += 2;
      }
    } else if (*pstr == '+') {
      *pbuf++ = ' ';
    } else {
      *pbuf++ = *pstr;
    }

    pstr++;

  }

  *pbuf = '\0';

  return buf;
}

//! decode url-encoded string "in place"
//! \param str url encoded string
//! \return pointer to original but converted string or NULL
char *url_indecode(char *str) {

  if (! str)
    return NULL;

  // this optimises speed by 1000% because 99.99% of its input strings
  // are not encoded
  if (! strchr(str, '%'))
    return str;

  char *pstr = str, *pbuf = str;

  while (*pstr) {

    if (*pstr == '%') {
      if (pstr[1] && pstr[2]) {
        *pbuf ++ = FROM_HEX(pstr[1]) << 4 | FROM_HEX(pstr[2]);
        pstr += 2;
      }
    } else if (*pstr == '+') {
      *pbuf ++ = ' ';
    } else {
      *pbuf ++ = *pstr;
    }

    pstr ++;

  }

  *pbuf = '\0';

  return str;
}


