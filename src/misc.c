/** \file */


#include "acl-helper.h"


//! split a string into tokens by delimiter
//! \param string a string to parse
//! \param array an array of pointers to store tokens, must be large enough to hold all tokens
//! \param sdelim a delimiter to use (one char, will be treated as 'multidelim' if has + at end)
//! \param max_tokens maximim number of tokens to extract (0 means all)
//! \return number of tokens stored
int parse_string(char *string, char **array, char *sdelim, int max_tokens) {

  char *head;            // head of found token
  char *tail;            // tail of found token
  unsigned char delim;   // real delimiter
  int multidelim = 0;    // treat a sequence of delims as one?
  int tokens_num = 0;    // number of extracted tokens

  // check args
  if (! string || ! *string ||
      ! array ||
      ! sdelim || ! *sdelim)
    return 0;

  // save delimiter char
  delim = sdelim[0];

  // see if we have a multidelim
  if (sdelim[1]) {
    if (sdelim[1] != '+')
      return 0;
    else
      multidelim = 1;
  }

  // adjust tokens limit
  max_tokens = max_tokens >= 0 ? max_tokens - 1 : 0;

  // ok, parse the string
  head = tail = string;
  while (1) {

    // is max tokens extracted? store the rest of string
    if (*tail && max_tokens >= 0 && tokens_num >= max_tokens)
      while (*(++tail));

    // hit a delimiter
    if (! *tail || *tail == delim) {

       // store new token (or NULL if it is empty)
      if (tail == head)
        array[tokens_num] = strdup("");
      else
        array[tokens_num] = strndup(head, tail - head);
      assert(array[tokens_num]);
      tokens_num ++;

      // multidelim is on? skip subsequent delims
      while (multidelim && *tail && *(tail+1) == delim)
        tail ++;

      // reposition head after delim
      head = tail + 1;

    }

    // is there anything to extract left?
    if (*tail)
      tail ++;
    else
      break;

  }  //while(*head)

  // all done
  return tokens_num;
}



//! strip leading and trailing blanks from a string
//! \param str a string to strip
//! \return pointer at original but stripped string
char *strip_blanks(char *str) {
  char *p = str;

  // skip leading and trailing blanks (move to first real non-space char)
  while (*p && isspace(*p))
    p ++;

  // there were blanks only...
  if (! *p)
    return p;

  // remove trailing blanks
  int i = strlen(p) - 1;
  for (; isspace(p[i]); p[i--] = '\0');

  return p;
}



//! check a string for unwanted chars and optinally replace them with anothe char
//! \param str source string
//! \param reject chars to reject
//! \param replace replacer for rejected chars (or <0 to disable)
//! \return 0 if no match, >0 - num of found/replaced chars
int str_reject(char *str, char *reject, int replace) {
  int found = 0;

  while (str && *str) {
    while (reject && *reject) {
      if (*str == *reject) {
        if (replace >= 0)
          *str = replace;
        found ++;
      }
    }
  }

  return found;
}



//! convert string into signed integer, converted value must be in
//! specified range (including), a string must contain decimals only
//! \param str string to convert
//! \param min min accepted converted value
//! \param max max accepted converted value
//! \return converted integer, set errno to EINVAL on error
int str2int(char *str, int min, int max) {

  char *pstr = str;

  // check args validness (mind possible leading minus for 'str')
  if (! str || !str[0] || (str[0] == '-' && !str[1]) || min >= max) {
    errno = EINVAL;
    return 0;
  } else if (str[0] == '-') {
    pstr ++;
  }

  while (*pstr) {
    if (! isdigit(*pstr++)) {
      errno = EINVAL;
      return 0;
    }
  }

  // convert string
  errno = 0;
  long res = strtol(str, NULL, 10);

  // check coversion result
  if (errno || res == LONG_MIN || res == LONG_MAX || res < min || res > max)
    errno = EINVAL;

  return (int)res;
}


