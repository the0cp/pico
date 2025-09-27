/* C code produced by gperf version 3.1 */
/* Command-line: 'C:\\Users\\Admin\\AppData\\Local\\Microsoft\\WinGet\\Packages\\oss-winget.gperf_Microsoft.Winget.Source_8wekyb3d8bbwe\\gperf.exe' -L C -E -t -C -G -N findKeyword -H keywordHash keywords.gperf  */
/* Computed positions: -k'1-2' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 2 "keywords.gperf"

#include <string.h>
#include "scanner.h"
#line 9 "keywords.gperf"
struct Keyword {
    const char* name;
    TokenType type;
};
#include <string.h>
enum
  {
    TOTAL_KEYWORDS = 15,
    MIN_WORD_LENGTH = 2,
    MAX_WORD_LENGTH = 6,
    MIN_HASH_VALUE = 2,
    MAX_HASH_VALUE = 24
  };

/* maximum key range = 23, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
keywordHash (str, len)
     register const char *str;
     register size_t len;
{
  static const unsigned char asso_values[] =
    {
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25,  0, 25,  5,
      25,  0,  5, 25,  5,  0, 25, 25, 14, 25,
       0,  0,  0, 25,  0, 25, 10,  0, 10,  5,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
      25, 25, 25, 25, 25, 25
    };
  return len + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]];
}

static const struct Keyword wordlist[] =
  {
    {""}, {""},
#line 28 "keywords.gperf"
    {"or",         TOKEN_OR},
#line 27 "keywords.gperf"
    {"and",        TOKEN_AND},
#line 21 "keywords.gperf"
    {"null",       TOKEN_NULL},
#line 22 "keywords.gperf"
    {"print",      TOKEN_PRINT},
#line 23 "keywords.gperf"
    {"return",     TOKEN_RETURN},
#line 20 "keywords.gperf"
    {"if",         TOKEN_IF},
#line 17 "keywords.gperf"
    {"for",        TOKEN_FOR},
#line 18 "keywords.gperf"
    {"func",       TOKEN_FUNC},
#line 16 "keywords.gperf"
    {"false",      TOKEN_FALSE},
    {""}, {""},
#line 19 "keywords.gperf"
    {"var",        TOKEN_VAR},
#line 25 "keywords.gperf"
    {"true",       TOKEN_TRUE},
#line 26 "keywords.gperf"
    {"while",      TOKEN_WHILE},
    {""}, {""},
#line 15 "keywords.gperf"
    {"else",       TOKEN_ELSE},
#line 24 "keywords.gperf"
    {"this",       TOKEN_THIS},
    {""}, {""}, {""}, {""},
#line 14 "keywords.gperf"
    {"class",      TOKEN_CLASS}
  };

const struct Keyword *
findKeyword (str, len)
     register const char *str;
     register size_t len;
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = keywordHash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strncmp(str, wordlist[key].name, len))
            return &wordlist[key];
        }
    }
  return 0;
}
