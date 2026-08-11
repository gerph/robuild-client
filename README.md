# RISCOS Build client

![Linux](https://github.com/gerph/robuild-client/workflows/Linux/badge.svg)
![RISC OS](https://github.com/gerph/robuild-client/workflows/RISC%20OS/badge.svg)

This repository contains a WebSockets client for the RISC OS Build service at
https://build.riscos.online/.

## Usage

To use the tool:

    ./riscos-build-online -i <source-file-or-zip> -o <output-prefix>

Will run the build on the service, and report the results to the console. The final output binary will be written to the output-prefix. On non-RISC OS builds, this will be suffixed by `,xxx` for the filetype that was generated. On RISC OS builds, the file named will be given the type of the returned data.

The return code from the tool will be propagated from the build.

Other options that can be used:

* `-q`: Only write the output from the build process to the terminal.
* `-Q`: Write no output to the terminal except errors.
* `-b`: Write all the build process's output to a file.
* `-s`: Specifies the server URI to connect together.
* `-t`: Specifies the timeout in seconds for the remote execution.
* `-a`: Enables or disables the use of ANSI text formatting (default 'on').
* `-A`: Selects the architecture to run with ('`aarch32`' or '`aarch64`').

## Prerequisites

There are two submodules required for this tool to work; they will be static linked with
the resulting binary. To ensure that they are present, use:

    git submodule update --init

The `libwsclient` source needs `libtoolize` to be present to run the autogen process. On MacOS this
was installable with `brew install libtool` but not located where the scripts could find
it. This could be fixed with:

    ln -s /usr/local/bin/glibtoolize /usr/local/bin/libtoolize

## Building on Linux/MacOS

Once prerequisites are installed, you should just be able to run:

    make

to get a build tool called `riscos-build-online`.

Building a deb:

    make deb

to get a distributable deb containing the tool.


## Building for Windows

The Windows version is a cross-build using the mingw-w64 toolchain, so it can be built from
Linux or MacOS without a Windows system being present. On Debian or Ubuntu the toolchain is
installed with:

    apt-get install gcc-mingw-w64-x86-64-win32

Once the submodules are checked out, the tool is built with:

    make -f Makefile.win

to get `riscos-build-online.exe`, a 64-bit console application. It is statically linked, so
no mingw runtime DLLs need to be distributed with it; only the system `KERNEL32`, `msvcrt`
and `WS2_32` DLLs are required.

The `PREFIX` variable selects the toolchain, and defaults to `x86_64-w64-mingw32-`.

The build does not use libwsclient's `autogen`/`configure` process. Instead, the `win32`
directory supplies:

* Shim headers which redirect the POSIX socket headers used by libwsclient (`netdb.h`,
  `sys/socket.h`, `netinet/in.h`, `arpa/inet.h` and `sys/un.h`) on to Winsock.
* A replacement for the `config.h` that `configure` would have generated.
* `winstart.c`, which initialises Winsock and enables ANSI escape sequence handling on the
  console before `main` runs.

This means that no changes are needed in the libwsclient or cJSON sources themselves.

ANSI text formatting (the `-a` option) relies on the console supporting virtual terminal
processing, which is available on Windows 10 and later. On earlier systems, use `-a off`.

## Building on RISC OS

The template environment files are required, together with standard C and TCPIPLibs.
Before running the build, the submodules must be checked out as well.

The directory RISC OS contains the files necessary to build the source. Running `!BuildAll` will build both the libraries and the client itself. The build scripts will construct a RISC OS-style source directory based on the original names.

The RISC OS client has not been heavily tested; it runs the simple 'Hello World' script that is the testfile in the root of the repository.

Quite a few shortcuts were taken in making the client work:

* `libwsclient` isn't C89 compatible, so the `sed` tool fixes up the current issues, and truncates values that require long longs.
* `libwsclient` uses pthreads, which isn't available, so it's been completely stubbed out, and the necessary functions are called directly. This is possible because `libwsclient`, when used for a single operation has a linear flow.
* `libwsclient` calls `getaddrinfo`, which I haven't implemented yet, so I threw together a fake routine which does enough to get an address and therefore a connection.

## License

This code is licensed under the BSD license.
The distributed binary is under the GPL 3 clause license, due to the inclusion of the libwsclient library.
