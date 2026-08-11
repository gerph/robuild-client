/*******************************************************************
 * File:        winstart
 * Purpose:     Windows process startup for the riscos-build-online tool.
 * Author:      Charles Ferguson (gerph@gerph.org)
 *
 * Two things need doing on Windows before main() runs, neither of which
 * the portable sources should have to know about:
 *
 *   * Winsock must be initialised, or every socket call fails.
 *   * The console must be told to interpret ANSI escape sequences, so that
 *     the formatting the build service sends (the '-a' option) appears as
 *     colour rather than as escape sequences. This is supported on
 *     Windows 10 and later; on earlier systems the call fails harmlessly
 *     and the user can turn the formatting off with '-a off'.
 *
 * Both are done from a constructor so that no change is needed in build.c.
 ******************************************************************/

/* winsock2.h must be included before windows.h */
#include <winsock2.h>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif


/*******************************************************************
 Function:      winstart_console
 Description:   Enable ANSI escape sequence interpretation on a console
                handle, if the system supports it.
 Parameters:    handle = the standard handle to configure
 Returns:       none
 ******************************************************************/
static void winstart_console(DWORD handle)
{
    HANDLE console = GetStdHandle(handle);
    DWORD mode;

    if (console == INVALID_HANDLE_VALUE || console == NULL)
        return;

    if (!GetConsoleMode(console, &mode))
        return; /* Redirected to a file or pipe; nothing to configure */

    SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}


/*******************************************************************
 Function:      winstart
 Description:   Initialise Winsock and the console before main() runs.
 Parameters:    none
 Returns:       none
 ******************************************************************/
static __attribute__((constructor)) void winstart(void)
{
    WSADATA wsadata;

    WSAStartup(MAKEWORD(2, 2), &wsadata);

    winstart_console(STD_OUTPUT_HANDLE);
    winstart_console(STD_ERROR_HANDLE);
}
