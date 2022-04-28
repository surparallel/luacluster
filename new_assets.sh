#!/bin/sh
echo off
parm_path=$(dirname $0)
currPath=$(cd $parm_path && pwd)
sourceDir=$currPath/res/n_assert/*
tarDir=./$1
Replacement=$currPath/core/bin/server/luacluster
mkdir $1
cp -r $sourceDir $tarDir
echo "$currPath/core/bin/server/luacluster">./$1/start_server.sh
chmod u+x ./$1/*.sh
echo "done"
