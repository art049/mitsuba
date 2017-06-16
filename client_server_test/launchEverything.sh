#!/bin/bash

xterm -e './router' $1 $2 &
sleep 2

maxIdx=$(($2-2))
lastIdx=$(($2-1))

for var in `seq 0 $lastIdx`; do
    xterm -e './client' $1 chunk$var &
	sleep 1
done
xterm -e './client' $1 $1 chunk$lastIdx &

echo We done here 

exit 0
