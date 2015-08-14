#!/bin/bash

GLFW_URL="https://github.com/glfw/glfw/releases/download/3.1.1/glfw-3.1.1.zip"
GLFW_ZIP_FILE="glfw-3.1.1.zip"

function install_glfw()
{
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

function install_bootstrap()
{
    mkdir -p external
    rm -rf external/*
    install_glfw
}


function clean_all()
{
    rm -rf glfw-3.1.1
    rm -rf $GLFW_ZIP_FILE
}

install_bootstrap