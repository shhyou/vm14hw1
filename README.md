vm14hw1
=======

NTU Virtual Machine Homework 1 (Add shallow stack to QEMU 0.13)

## Build notes (on 217)

1. Add the missing `-lrt` flag to `LIBS += ...` in `Makefile.target`

1. Fix `optimisation.h`, `optimisation.c`

1. Build in a (separate) clean directory, say `build.qemu`. Then configure using

  ```
  ../vm14hw1/configure --target-list=x86_64-linux-user --disable-docs
  ```

1. Just `make`. The resulting binary is at `x86_64-linux-user/`

