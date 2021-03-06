MCell
=====

MCell: Monte Carlo Simulator of Cellular Microphysiology

[![Build Status](https://travis-ci.org/mcellteam/mcell.svg?branch=master)](https://travis-ci.org/mcellteam/mcell)
[![Build status](https://ci.appveyor.com/api/projects/status/github/mcellteam/mcell?branch=master&svg=true)](https://ci.appveyor.com/project/jczech/mcell/branch/master)

MCell3 Build Requirements:
--------------------------

flex newer than 2.5.6, due to the 'reentrant' option. Extensive testing has
been done using Flex 2.5.33.


How To Build:
-------------

To build MCell for Macs or Linux, run the following commands from the main
mcell directory:

        mkdir build
        cd build
        cmake ..
        make

The old build system is still available and can be used by issuing the 
following commands:

        cd ./src
        ./bootstrap
        cd ..
        mkdir build
        cd build
        ../src/configure CC=gcc CFLAGS='-O2 -Wall' 
        make

You only need to bootstrap (first three steps) when starting from a fresh
branch or checkout. Depending on your needs, you may have to change the
build options slightly.

See the [Windows
Development](https://github.com/mcellteam/mcell/wiki/Windows-Development) page
on the github wiki for information about building MCell on Windows.

How To Test:
------------

[nutmeg](https://github.com/mcellteam/nutmeg) is a regression test
framework for MCell. Installation and usage instructions are listed on the
nutmeg project page.
