#!/bin/sh

cd ../src/
make all

cp reglan.h reglan libreglan.a libreglan.so ../test/

cd ../test/
