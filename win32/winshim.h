/*******************************************************************
 * File:        winshim
 * Purpose:     Map the POSIX socket headers used by libwsclient on to
 *              the Winsock equivalents.
 * Author:      Charles Ferguson (gerph@gerph.org)
 *
 * libwsclient includes <netdb.h>, <sys/socket.h>, <netinet/in.h>,
 * <arpa/inet.h> and <sys/un.h>, none of which exist on Windows. The
 * shim headers alongside this one all defer to this file, so that the
 * library sources themselves need no changes. Place the 'win32'
 * directory on the include path ahead of the system headers.
 ******************************************************************/

#ifndef WINSHIM_H
#define WINSHIM_H

#include <winsock2.h>
#include <ws2tcpip.h>

/* Within libwsclient, close() is only ever applied to a socket, never to a
   file descriptor, so it is safe to redirect it wholesale. */
#define close(s) closesocket(s)

#endif
