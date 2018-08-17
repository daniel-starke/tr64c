tr64c
=====

A client for the [TR-064 protocol](https://www.broadband-forum.org/technical/download/TR-064_Corrigendum-1.pdf).  

Features
========

A simple client for TR-064 conform servers (CPE) to queue get and set actions.  
Only Windows XP and newer is supported (Unicode enabled).  
Also Linux is supported.  

Usage
=====

    tr64c [options] [[<device>/]<service/action> [<variable=value> ...]]
    
    -c, --cache <file>
          Cache action descriptions of the device in this file.
    -f, --format <string>
          Defines the output format for queries. Possible values are:
          TEXT - plain text (default)
          CSV  - comma-separated values
          JSON - JavaScript Object Notation
          XML  - Extensible Markup Language
    -h, --help
          Print short usage instruction.
    -i, --interactive
          Run in interactive mode.
    -l, --list
          List services and actions available on the device.
    -o, --host <URL>
          Device address to connect to in the format http://<host>:<port>/<file>.
          The protocol defaults to http if omitted.
          The port defaults to 49000 if omitted.
          For scan mode set this parameter to the local interface IP address on
          which the local discovery shall be performed on.
    -p, --password <string>
          Use this password to authenticate to the device.
    -s, --scan
          Perform a local device discovery scan.
    -u, --user <string>
          Use this user name to authenticate to the device.
        --utf8
          Sets the encoding for console inputs/outputs to UTF-8.
          The default is UTF-16.
    -t, --timeout <number>
          Timeout for network operations in milliseconds.
    -v
          Increases verbosity.
        --version
          Outputs the program version.

Example
=======

Scanning for available devices on local interface with IP address 192.168.178.10:  

    tr64c -o 192.168.178.10 -l

Listing possible action on FRITZ!Box 7490:  

    tr64c -o http://192.168.178.1:49000/tr64desc.xml -l

Obtaining some user interface properties from a FRITZ!Box 7490:  

    tr64c -o http://192.168.178.1:49000/tr64desc.xml -q UserInterface/GetInfo

Check also the binding example for Python [here](etc/tr64c.py).

Building
========

The following dependencies are given:  
- C99

Edit Makefile to match your target system configuration.  
Building the program:  

    make

[![Linux GCC Build Status](https://img.shields.io/travis/daniel-starke/tr64c/master.svg?label=Linux)](https://travis-ci.org/daniel-starke/tr64c)
[![Windows LLVM/Clang Build Status](https://img.shields.io/appveyor/ci/danielstarke/tr64c/master.svg?label=Windows)](https://ci.appveyor.com/project/danielstarke/tr64c)    

Files
=====

|Name           |Meaning
|---------------|--------------------------------------------
|*.mk           |Target specific Makefile setup.
|argp*, getopt* |Command-line parser.
|bsearch.*      |Binary search algorithm.
|cvutf8.*       |UTF-8 conversion functions.
|hmd5.*         |MD5 hashing function.
|http.*         |HTTP/1.x parser.
|mingw-unicode.h|Unicode enabled main() for MinGW targets.
|parser.*       |Text parsers and parser helpers.
|sax.*          |SAX based XML parser.
|target.h       |Target specific functions and macros.
|tchar.*        |Functions to simplify ASCII/Unicode support.
|tr64c.*        |Main application files.
|tr64c-*        |Platform abstraction layer (backend).
|url.*          |URL parser.
|utf8.*         |UTF-8 support functions.
|version.*      |Program version information.

License
=======

See [unlicense file](doc/UNLICENSE).  
