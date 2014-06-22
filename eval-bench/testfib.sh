echo qemu-no-opt
for (( i=0; i<5; i=i+1 )); do { time -p ./qemu-no-opt ./fib > /dev/null; } 2>&1 | head -n 1; done
echo qemu-def-hw
for (( i=0; i<5; i=i+1 )); do { time -p ./qemu-def-hw ./fib > /dev/null; } 2>&1 | head -n 1; done
echo qemu-new-shack
for (( i=0; i<5; i=i+1 )); do { time -p ./qemu-new-shack ./fib > /dev/null; } 2>&1 | head -n 1; done
echo qemu-no-push
for (( i=0; i<5; i=i+1 )); do { time -p ./qemu-new-shack ./fib > /dev/null; } 2>&1 | head -n 1; done
echo qemu-call-cache
for (( i=0; i<5; i=i+1 )); do { time -p ./qemu-call-cache ./fib > /dev/null; } 2>&1 | head -n 1; done
echo qemu-shack-cache
for (( i=0; i<5; i=i+1 )); do { time -p ./qemu-shack-cache ./fib > /dev/null; } 2>&1 | head -n 1; done

