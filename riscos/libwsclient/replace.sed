s/#include <pthread.h>/#include "fakepthread.h"/
s/#include "wsclient.h"/#include "wsclient_ro.h"/
s/#include <stdint.h>//
/#include <netinet.in.h>/ i\
#include "getaddrinfo.h"

s/long long/long/g
s/inline //g
s/0xffffffffffffffff/0xffffffff/g
s/0xffffffffLL/0xffffffff/g
s/(void \*)&payload_len/(char *)\&payload_len/g
s/socklen_t/int/

# wsclient fix for type mismatch
s/shactx, pre_encode, /shactx, (unsigned char *)pre_encode, /

# base64 fix for type mismatch
s/tmplen = _base64_decode_triple(quadruple, tmpresult);/tmplen = _base64_decode_triple(quadruple, (unsigned char *)tmpresult);/
