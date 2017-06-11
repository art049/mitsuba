#!/bin/bash
grep -E '[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}' /etc/hosts | awk 'END {print $NF}'
exit 0