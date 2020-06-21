/*******************************************************************
 * File:     base64
 * Purpose:  Base64 manipulation routines
 * Author:   Justin Fletcher
 * Date:     24 Dec 2000
 ******************************************************************/

#include "base64.h"

/* Decode a section of base64 -
   =>
      in-> pointer to input buffer
      inlen-> length of input buffer
      out-> output buffer
      outlen = length of output buffer
   <=
      number of characters written into output buffer, or -1 if not enough
      space
*/
int base64_decode (const char **inp, int *inlen, char *out, int outlen)
{
  int j;
  unsigned int eq_count;
  unsigned long num;
  const char *in;
  int output=0;

  if (outlen<3)
    return -1;

  while (*inlen>0)
  {
    if (outlen<3)
      return output;

    eq_count=0;
    num=0;
    in=*inp;
    for (j = 0; j < 4; j++)
    {
      char inc;
      unsigned char c = 0;
      if (*inlen<j || (inc=in[j]) == '=') c = 0, eq_count++;
      else if (inc >= 'A' && inc <= 'Z')  c = inc - 'A';
      else if (inc >= 'a' && inc <= 'z')  c = inc - ('a' - 26);
      else if (inc >= '0' && inc <= '9')  c = inc - ('0' - 52);
      else if (inc == '+')                c = 62;
      else if (inc == '/')                c = 63;
      else
        return 0;
      num = (num << 6) | c;
    }

    *inlen-=4; *inp=in+4;
    if (*inlen<0) *inlen=0;

    if (eq_count>2)
      return output; /* Something up */

    *out++ = (char) (num >> 16);
    if (eq_count<2)
    {
      *out++ = (char) ((num >> 8) & 0xFF);
      if (eq_count==0)
        *out++ = (char) (num & 0xFF);
    }

    if (eq_count == 0)
      output+=3,outlen-=3; /* No "=" padding means 4 bytes mapped to 3. */
    else if (eq_count == 1)
      output+=2,outlen-=2; /* "xxx=" means 3 bytes mapped to 2. */
    else if (eq_count == 2)
      output+=1,outlen-=1; /* "xx==" means 2 bytes mapped to 1. */
    if (eq_count!=0)
      return output;
  }

  return output;
}
