#!/bin/bash

if ( test "`uname -s`" = "Darwin" )
then
    #cmake v2.8.12 is installed on the Mac workers now
    #brew update
    #brew install cmake
    brew update
    brew install boost
else
    #install a newer cmake since at this time Travis only has version 2.8.7
    #sudo add-apt-repository ppa:kalakris/cmake -y
    #sudo apt-get update -qq
    #sudo apt-get install -qq libboost-dev cmake
fi
