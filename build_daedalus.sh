#!/bin/bash

function prepare_build(){

    if [ -d $PWD/daedbuild ]; then
        echo "Removing previous build"
        rm -r "$PWD/daedbuild"
        rm -r $PWD/DaedalusX64/EBOOT.PBP > /dev/null 2>&1
        rm -r $PWD/DaedalusX64/daedalus > /dev/null 2>&1
    fi
    if [ ! -d "$PWD/DaedalusX64" ]; then
    mkdir DaedalusX64
    cp -R $PWD/Data/* $PWD/DaedalusX64
    fi
    mkdir "daedbuild" && cd "daedbuild"
}

function build(){
    cmake $1=1 $CMAKEFLAGS ../Source
    make -j8
}

if [ "$2" == "DEBUG" ]; then #Always assume release build, unless DEBUG is parsed.
    CMAKEFLAGS=-D"$1=1 DEBUG=1"
else
    CMAKEFLAGS=-D"$1=1"
fi

prepare_build

## Main loop
case $1 in 
    PSP)
    echo "PSP $2 Build"
     make -C "../Source/SysPSP/PRX/DveMgr" 
     make -C "../Source/SysPSP/PRX/ExceptionHandler" 
     make -C "../Source/SysPSP/PRX/KernelButtons"
     make -C "../Source/SysPSP/PRX/MediaEngine"
    build
    cp "$PWD/EBOOT.PBP" ../DaedalusX64/ 
    ;;
    MAC | LINUX)
    echo "Posix Build"
    build
    cp ../Source/SysGL/HLEGraphics/n64.psh ../DaedalusX64
    ;;
    VITA)
    echo "Vita Build"
    build
    ;;
    3DS)
    echo "3DS Build"
    build
    ;;
    *)
    echo "No build type specified, specify PSP, MAC, LINUX, VITA, 3DS"
    exit
    ;;
esac
