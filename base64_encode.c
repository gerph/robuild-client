/*******************************************************************
 * File:     base64
 * Purpose:  Base64 manipulation routines
 * Author:   Justin Fletcher
 * Date:     28 Dec 2002
 ******************************************************************/

#include "base64.h"


/* Encode a data block in base64 -
   =>
      in-> pointer to input buffer
      inlen-> length of input buffer
      out-> output buffer
      outlen = length of output buffer
   <=
      number of characters written into output buffer, or -1 if not enough
      space
*/
int base64_encode (const char *inv, int inlen, char *out, int outlen)
{
  static char dictionary[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const unsigned char *in = (const unsigned char *)inv;
  int wrote=0;
  for (; inlen; in+=3)
  {
    char d1 = dictionary[ in[0] >> 2 ]; /* Top 6 bits */
    char d2 = dictionary[ ((in[0] << 4) |
                           (--inlen ? (in[1]>>4) : 0)) & 63
                        ]; /* 4 bits from byte 2 + 2 from byte 1 */
    char d3 = (inlen>0) ?
              dictionary[ ((in[1] << 2) |
                           (--inlen ? (in[2]>>6) : 0)) & 63
                        ]
                        : '='; /* 4 bits from byte 2 + 2 from byte 3 */
    char d4 = (inlen>0) ?
              dictionary[ in[2] & 63 ] : '='; /* 6 bits from byte 3 */
    if (inlen) inlen--;
    if (outlen < 4)
      return -1;
    *out++=d1;
    *out++=d2;
    *out++=d3;
    *out++=d4;
    outlen-=4; wrote+=4;
  }
  if (outlen<1)
    return -1;
  *out++='\0';
  return wrote;
}
