#!/bin/bash

function install_glfw()
{
    GLFW_URL="https://github.com/glfw/glfw/releases/download/3.1.1/glfw-3.1.1.zip"
    GLFW_ZIP_FILE="glfw-3.1.1.zip"
    
    if [ -f $GLFW_ZIP_FILE ]; then
        echo "Use previous downloaded $GLFW_ZIP_FILE"
    else
        wget $GLFW_URL
    fi
    
    unzip -q $GLFW_ZIP_FILE
    mv glfw-3.1.1 external
    
    cd external
    ln -s glfw-3.1.1 glfw
    cd glfw-3.1.1
    cmake .
    make
    cd ../..
}

function download_lodepng()
{
    rm -rf lodepng.*
    LODEPNG_CPP="https://raw.githubusercontent.com/lvandeve/lodepng/master/lodepng.cpp"
    LODEPNG_H="https://raw.githubusercontent.com/lvandeve/lodepng/master/lodepng.h"
    wget $LODEPNG_CPP
    wget $LODEPNG_H
    mv lodepng.cpp lodepng.c
}

function install_bootstrap()
{
    mkdir -p external
    rm -rf external/*
    install_glfw
    download_lodepng
}


function clean_all()
{
    rm -rf glfw-3.1.1
    rm -rf *.zip
}

install_bootstrap