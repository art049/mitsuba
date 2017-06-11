#!/bin/bash
OUTPUT="$(grep -E '[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}' /etc/hosts | awk 'END {print $NF}')"
echo "RES: ${OUTPUT}"

tab="--tab"
cmd="bash -c '<command-line_or_script>';bash"
foo=""

for i in 1 2 ... n; do
      foo+=($tab -e "$cmd")         
done

gnome-terminal "${foo[@]}"

exit 0

#./client ${OUTPUT} &

exit 0