#!/bin/bash
ps -eo pid,args | grep 'work/vm/mon[.]sock' | awk '{print $1}' | xargs -r kill -9 2>/dev/null
echo "vm killed"
