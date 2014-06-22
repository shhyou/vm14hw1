#!/bin/bash
./runbench.py `pwd`/qemu-no-opt
mv log.txt log-no-opt.txt
./runbench.py `pwd`/qemu-def-hw
mv log.txt log-def-hw.txt
./runbench.py `pwd`/qemu-new-shack
mv log.txt log-new-shack.txt
./runbench.py `pwd`/qemu-no-push
mv log.txt log-no-push.txt
./runbench.py `pwd`/qemu-call-cache
mv log.txt log-call-cache.txt
./runbench.py `pwd`/qemu-shack-cache
mv log.txt log-shack-cache.txt
ls -l log*.txt
