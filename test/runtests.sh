#!/bin/sh

./update.sh
[ "$?" = 0 ] || exit 1

python test.py
[ "$?" = 0 ] || exit 2

gcc test.c libreglan.a -o test_static
[ "$?" = 0 ] || exit 3
./test_static
[ "$?" = 0 ] || exit 4

gcc test.c -L. -lreglan -Wl,-rpath,'.' -o test_shared
[ "$?" = 0 ] || exit 5
./test_shared
[ "$?" = 0 ] || exit 6

echo "All tests done"
