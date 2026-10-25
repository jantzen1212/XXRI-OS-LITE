#!/bin/sh

set -x

CHRPATH=../chrpath

retval=0

rm prog
make prog

if $CHRPATH -h ; then
    echo "success: chrpath -h worked."
else
    echo "error: chrpath -h failed."
    retval=1
fi

if $CHRPATH -v ; then
    echo "success: chrpath -v worked."
else
    echo "error: chrpath -v failed."
    retval=1
fi

if $CHRPATH $0 ; then
    echo "error: chrpath on /bin/sh script worked."
    retval=1
else
    echo "success: chrpath on /bin/sh script failed."
fi

if $CHRPATH non-existant-file ; then
    echo "error: chrpath on non-existant file worked."
    retval=1
else
    echo "success: chrpath on non-existant file failed."
fi

if $CHRPATH -l prog | grep -q 'R.*PATH=.*/usr/local/lib' ; then
    echo "success: chrpath listed current rpath."
else
    echo "error: chrpath unable to list current rpath."
    retval=1
fi

$CHRPATH -r '/usr/lib' prog > /dev/null

if $CHRPATH -l prog | grep -q 'R.*PATH=/usr/lib' ; then
    echo "success: chrpath changed rpath."
else
    echo "error: chrpath unable to change rpath."
    retval=1
fi

# I wish inserting a larger path would work, but it doesn't yet
if $CHRPATH -r '/usr/lib:/usr/local/lib' prog > /dev/null ; then
    echo "success: chrpath changed rpath to larger path."
else
    echo "error: chrpath unable to change rpath to larger path."
fi

$CHRPATH -c prog > /dev/null

if $CHRPATH -l prog | grep -q 'RUNPATH=/usr/lib' ; then
    echo "success: chrpath converted rpath to runpath."
else
    # Not all archs support runpath, ie not a fatal error
    echo "warning: chrpath unable to convert rpath to runpath."
fi

$CHRPATH -d prog > /dev/null

if $CHRPATH -l prog | grep -q 'no rpath or runpath tag found' ; then
    echo "success: chrpath removed rpath and runpath successfully."
else
    echo "error: chrpath unable to remove rpath or runpath."
    retval=1
fi

exit $retval

