#!/usr/bin/env bash

if [ ! -e sigar ]
then
    git clone https://github.com/AlexYaruki/sigar
fi
cd sigar
git checkout master
git pull

# Build and test main library ##################################################
if [ ! -e build ]
then
    mkdir build
fi
cd build

cmake .. && make clean && make && make test
################################################################################

# Build and test "Java" bindings ###############################################
cd bindings/java
mvn clean package -P package-native
java -cp target/sigar.jar org.hyperic.sigar.cmd.Free
################################################################################
