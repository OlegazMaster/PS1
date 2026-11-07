A minimal C library interface of Android (bionic, 32-bit ARM) for the port:
only what src/pc uses, declared as bionic does on LP32. The official NDK is
not needed (and not reachable from every build machine). tools/pc/build.py makes stubs of the system
libraries for the link.
