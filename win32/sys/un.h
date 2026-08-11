/* Shim for the Unix domain socket header, which Windows has no equivalent of.
 *
 * Only libwsclient's helper socket support uses these, and the client never
 * calls libwsclient_helper_socket(), so a declaration sufficient to let the
 * library compile is all that is needed here.
 */

#ifndef SHIM_SYS_UN_H
#define SHIM_SYS_UN_H

#include "../winshim.h"

struct sockaddr_un {
    short sun_family;
    char sun_path[108];
};

#endif
