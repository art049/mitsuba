#!/bin/bash
OUTPUT="$(grep -E '[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}' /etc/hosts | awk 'END {print $NF}')"
echo "${OUTPUT}"

#./client ${OUTPUT} &

exit 0