## RISCOS Build client

This repository contains a WebSockets client for the RISC OS Build service at
[https://build.riscos.online/].

FIXME: Fill in some more details.

## Prerequisites

There are two submodules required for this tool to work; they will be static linked with
the resulting binary. To ensure that they are present, use:

    git submodule update --init

The libwsclient needs libtoolize to be present to run the autogen process. On MacOS this
was installable with `brew install libtool` but not located where the scripts could find
it. This could be fixed with:

    ln -s /usr/local/bin/glibtoolize /usr/local/bin/libtoolize

## Building

Once prerequisites are installed, you should just be able to run:

    make

to get a build tool called `riscos-build-online`.

## License

This code is licensed under the BSD license.
The distributed binary is under the GPL 3 clause license, due to the inclusion of the libwsclient library.
