#! /bin/sh

for file in $(git ls-files | grep '\.[hc]$')
do
    clang-format-18 -i "$file"
done
