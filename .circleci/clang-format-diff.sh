#!/bin/bash

RET=0
DIR=$(readlink -f $(dirname $0)/..)

echo "Running clang-format in:"
clang-format --version

for file in src/*.cpp src/*/*.cpp include/*.h src/*.h src/*/*.h; do
    clang-format $file > /tmp/file
    A=$(diff $file /tmp/file)
    if [ "$?" == 1 ] || [ "$RET" == 1 ]; then
        RET=1
        echo "file: $file"
        echo "$A"
    else
        RET=0
    fi
done

exit "${RET}"
