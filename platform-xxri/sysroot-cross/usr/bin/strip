#! /bin/sh
# dummy-strip.sh for PTXdist - Copyright (C) 2009 by Wolfram Sang
#
# This scripts gets installed into the cross-environment as a 'dummy'-strip
# program. As programs like 'install -s' just call 'strip', this prevents
# leaking in of the host-version of strip. Also, it does intentionally
# nothing as we want to strip at a later stage.
#
echo "ptxdist: Discarding 'strip $*'"
exit 0

