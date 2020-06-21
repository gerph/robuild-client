/*******************************************************************
 * File:     base64
 * Purpose:  Base64 manipulation routines - originally written for the
 *           DataURLFetcher module
 * Author:   Justin Fletcher
 * Date:     24 Dec 2000
 ******************************************************************/

#ifndef BASE64_H
#define BASE64_H

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
int base64_decode (const char **inp, int *inlen, char *out, int outlen);

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
int base64_encode (const char *in, int inlen, char *out, int outlen);

#endif
