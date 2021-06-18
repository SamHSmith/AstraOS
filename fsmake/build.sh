#!/bin/sh

# Absolute path to this script, e.g. /home/user/bin/foo.sh
SCRIPT=$(readlink -f "$0")
# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH=$(dirname "$SCRIPT")
#echo $SCRIPTPATH
cd $SCRIPTPATH

if [ "$1" = "clean" ]
then
rm -drf bin
else

if [ -z ${CC+x} ]; then CC=gcc; fi

mkdir -p bin
$CC -g -O3 -lcrypto src/main.c -o bin/fsmake

fi
