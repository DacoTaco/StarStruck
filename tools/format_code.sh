#! /bin/sh

for file in $(git ls-files | grep '\.[hc]$')
do
    clang-format -i "$file"
done
