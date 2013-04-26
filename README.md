fractal-cppa
============

Calculating fractals on multiple workers using libcppa. Still in development ...

Dependencies
------------

Since Qt is used as a GUI make sure cmake finds the Qt bin.

* __libcppa__: https://github.com/Neverlord/libcppa
* __Qt__: http://qt.nokia.com/

Compiler
--------

* GCC >= 4.7

Get the source
--------------

    git clone git://github.com/josephnoir/fractal-cppa.git
    cd fractal-cppa

Build the apps
--------------

    ./configure --with-gcc=<your-gcc-compiler>
    make

Cmake needs to find qt. If it does not by default, add the qt bin directory to your PATH.
    
    export PATH=<PATH_TO_SDK>/QtSDK/Desktop/Qt/4.8.1/gcc/bin/:$PATH

It is possibile to use OpenCL for the calculation. To do this you need to compile libcppa and
this project with the OpenCL option. (currently only supported in libcppa's unstable branch)

    ./configure --with-gcc=<your-gcc-compiler> --with-opencl
    make

Usage:
------

    ./build/bin/fractal_server -p 1234
    ./build/bin/fractal_client -h localhost -p 1234

To enable OpenCL usage you should run the client with the -o flag.

    ./build/bin/fractal_client -h localhost -p 1234 -o

Operating Systems
-----------------

Tested on OSX, and Linux too.
