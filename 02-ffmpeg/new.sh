#!/usr/bin/env bash

: [0-9][0-9]-ex
last=$_
: ${last/#0/}
n=${_/%-ex/}
output=$(printf "cp -r $last %02d-ex\n" "$((n + 1))")

echo '*** Create new dir ***' >&2
echo command: $output >&2
echo $output
