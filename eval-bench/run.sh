for (( i=1; i<=5; i=i+1 )); do { time -p $1 $2; } 2>&1 | head -n 1; done
