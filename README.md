## RISCOS Build client

This repository contains a WebSockets client for the RISC OS Build service at
https://build.riscos.online/.

## Usage

To use the tool:

    ./riscos-build-online -i <source-file-or-zip> -o <output-prefix>

Will run the build on the service, and report the results to the console. The final output binary will be written to the output-prefix, suffixed by `,xxx` for the filetype that was generated.
The return code from the tool will be propagated from the build.

Other options that can be used:

* `-q`: Only write the output from the build process to the terminal.
* `-Q`: Write no output to the terminal except errors.
* `-b`: Write all the build process's output to a file.

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
