/* Replacement for the autoconf-generated libwsclient config.h, for Windows.
 *
 * The autogen/configure process is not run for the Windows build, so this
 * file stands in for the generated header. HAVE_LIBSSL is the only macro
 * that libwsclient's sources actually test, and we build without SSL
 * support, exactly as the Linux build does (which comments the definition
 * out again after configure has run).
 */

#ifndef WSCLIENT_CONFIG_H
#define WSCLIENT_CONFIG_H

/* #undef HAVE_LIBSSL */

#define PACKAGE "libwsclient"
#define PACKAGE_NAME "libwsclient"
#define PACKAGE_VERSION "1.0.1"
#define VERSION "1.0.1"

#endif
