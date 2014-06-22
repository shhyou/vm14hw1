#!/usr/bin/python

repeat_num = 5
log_name = "~/eval-bench/log.txt"

bench_set = {
  "automotive": {
    "compile": ["for fil in `ls`; do pushd $fil; make; popd; done"],
    "test": ["basicmath", "bitcount", "qsort", "susan"]
  },
  "consumer": {
    "compile": [
      "make -C typeset/lout-3.24",
      "make -C jpeg/jpeg-6a",
      "cd tiff-v3.5.4 && ./configure && make",
      "cd mad/mad-0.14.2b && ./configure && make",
      "make -C lame/lame3.70",
    ],
    "test": ["typeset", "jpeg", "tiff2bw", "mad", "tiff2rgba",
             "tiffmedian", "lame", "tiffdither"]
  },
  "network": {
    "compile": ["for fil in `ls`; do pushd $fil; make; popd; done"],
    "test": ["dijkstra", "patricia"]
  },
  "office": {
    "compile": [
#      "make -C ghostscript/src",
#      "make ispell -C ispell",
#      "pushd rsynth && ./configure && make",
#      "pushd sphinx && ./configure && make",
      "make -C stringsearch"
    ],
#    "test": ["ghostscript", "ispell", "rsynth", "sphinx", "stringsearch"]
    "test": ["stringsearch"]
  },
  "security": {
    "compile": [
      "make -C blowfish",
      "make -C pgp/src",
      "make -C rijndael",
      "make -C sha"
    ],
#    "test": ["blowfish", "pgp", "rijndael", "sha"]
    "test": ["pgp", "rijndael", "sha"] # blowfish seg faults itself
  },
  "telecomm": {
    "compile": [
      "make -C adpcm/src",
      "make -C CRC32",
      "make -C FFT",
      "make -C gsm"
    ],
    "test": ["adpcm", "CRC32", "FFT", "gsm"]
  }
}

all_test_num = len([test for bench in bench_set for test in bench_set[bench]["test"]])

import os
import sys
import subprocess
import time # for time.sleep

def compile_all():
  cwd = os.getcwd()
  for bench in bench_set:
    print "# compiling", bench,
    os.chdir(bench)
    print "in", os.getcwd()
    for cmd in bench_set[bench]["compile"]:
      subprocess.check_call(cmd, shell=True)
    os.chdir(cwd)

def test_bench(bench, prefix, qemu="", repeat=False, small=True, large=False):
  cwd = os.getcwd()
  test_num = len(bench["test"])
  i = 0
  repeat_run = 1
  if repeat:
    repeat_run = repeat_num
  for test in bench["test"]:
    i = i + 1
    os.chdir(test)
    shs_all = [s for s in os.listdir(".") if s[-3:] == ".sh"]
    shs_small = [s for s in shs_all if "small" in s]
    shs_large = [s for s in shs_all if "large" in s]
    shs = []
    if small:
      shs += shs_small
    if large:
      shs += shs_large
    for sh in shs:
      print "|", os.getcwd(), sh
      with open(sh, 'r') as f:
        cmds = [s[:-1] for s in f if s[:-1]!="" and s[0]!='#']
        for cmd in cmds:
          print "+----", prefix, str(i) + "/" + str(test_num), cmd,
          subprocess.check_call("echo " + cmd + " >>" + log_name, shell=True)
          timed_cmd = "{ time -p " + qemu + " ./" + cmd + " 2>/dev/null; } 2>&1" \
                      + " | head -n 1 >>" + log_name
          for k in xrange(repeat_run):
            subprocess.check_call(timed_cmd, shell=True)
            print ".",
            sys.stdout.flush()
          print ""
#          time.sleep(2)
    os.chdir(cwd)

def test_all(qemu="", repeat=False, small=True, large=False):
  subprocess.check_call("echo \# QEMU = '" + qemu + "' >" + log_name, shell=True)
  cwd = os.getcwd()
  i = 0
  for bench in bench_set:
    prefix = str(i) + "/" + str(all_test_num)
    print "# testing", prefix, bench
    os.chdir(bench)
    test_bench(bench_set[bench], prefix, qemu=qemu, repeat=repeat, small=small, large=large)
    os.chdir(cwd)
    i = i + len(bench_set[bench]["test"])

if __name__ == "__main__":
  #compile_all()
  qemu = ""
  if len(sys.argv) == 2:
    qemu = sys.argv[1]
  test_all(qemu=qemu,repeat=True, small=False, large=True)
