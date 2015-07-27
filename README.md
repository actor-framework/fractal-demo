fractal-demo
============

fractal-demo is a distributed fractal-calculator based on OpenCL, [Qt 4](http://qt.nokia.com/) and [CAF](https://github.com/actor-framework/actor-framework). Controll unit, calculation units and server/screen unit are distributed. OpenCL allows the usage of GPUs.

Screenshots
-----------

__Mandelbrot__
![ScreenShot](https://raw.github.com/josephnoir/fractal-cppa/master/screenshots/1-mandelbrot.png "Mandelbrot set")
![ScreenShot](https://raw.github.com/josephnoir/fractal-cppa/master/screenshots/2-mandelbrot.png "Mandelbrot set")

__Burning Ship__
![ScreenShot](https://raw.github.com/josephnoir/fractal-cppa/master/screenshots/3-burnship.png "Burning ship")
![ScreenShot](https://raw.github.com/josephnoir/fractal-cppa/master/screenshots/4-burnship.png "Burning ship")

Dependencies
------------

Since Qt is used as a GUI make sure cmake finds the Qt bin.

* __CAF__: [CAF](https://github.com/actor-framework/actor-framework)
* __Qt__: [qt.nokia.com](http://qt.nokia.com/)

Compiler
--------

* GCC >= 4.8

Get the source
--------------

    git clone https://github.com/actor-framework/fractal-demo.git
    cd fractal-demo

Build the apps
--------------

    ./configure
    make

Cmake needs to find qt. If it does not by default, add the qt bin directory to your PATH.
    
    export PATH=<PATH_TO_SDK>/QtSDK/Desktop/Qt/4.8.1/gcc/bin/:$PATH

It is possibile to use OpenCL for the calculation. To do this you need to compile libcppa and
this project with the OpenCL option.

    ./configure --with-opencl
    make

Usage:
------

fractal-demo allows you two ways to make a connection between worker and server. 
* Workers are published first and than the server gets with `-n domain:port` the location of published workers.
* Server starts first and workers connect with `-H server_domain:port` to the server.

In this example the second point is shown.
To calculate, controll and server on one computer without openCL:

    # Terminal 1
    # start server and publish at port 7000
    .build/bin/fractal-demo -s -p 7000

    # Terminal 2
    # publish two workers (CPU) on port 7000 of your client.
    .build/bin/fractal-demo -w 2 -H localhost:7000
    
    # Terminal 3
    # start controller with server on localhost
    .build/bin/fractal-demo -c -H localhost

Note: Of course you can start everything in one terminal. To do this, append `&` to end of each command. If you want to quit them all you can just type: `killall fractal-cppa`.

In case you want to enable OpenCL, add the -o flag to client start.

    .build/bin/fractal-demo -w 2 -H localhost:7000 -o

As above-mentioned, fractal-demo is distributed. That means, you can split up each terminal on diffrent computer and add even more workers trough diffrent computer. 

Operating Systems
-----------------

Tested on OSX, and Linux.
