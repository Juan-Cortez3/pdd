#!/bin/sh
### autogen.sh - tool to help build pdd from a repository checkout

aclocal     # set up an m4 environment
autoconf    # generate configure from configure.ac
automake --add-missing # Generate Makefile.in from Makefile.am
