import os

bench = [
  "qemu-no-opt",
  "qemu-def-hw",
  "qemu-new-shack",
  "qemu-shack-cache",
  "qemu-no-push",
  "qemu-call-cache"
]
test = [
  "dijkstra",
  "patricia",
  "stringsearch",
  "rawcaudio large.pcm",
  "rawdaudio large.adpcm",
  "crc",
  "fft",
  "fft -i",
  "toast",
  "untoast",
  "basicmath_large",
  "bitcnts",
  "qsort_large",
  "susan -s",
  "susan -e",
  "susan -c",
  "rijndael e",
  "rijndael d",
  "sha",
  "lout",
  "cjpeg",
  "djpeg",
  "tiff2bw",
  "madplay",
  "tiff2rgba",
  "tiffmedian",
  "lame",
  "tiffdither"
]

def read_file(name):
  with open(name, "r") as f:
    qemu = f.readline()[:-1]
    data = f.readlines()
    test = []
    res = []
    for idx_ in xrange(len(data)/6):
      idx = idx_*6
      test.append(data[idx][:-1])
      tot = 0.0
      max_val = -1.0
      min_val = 1000000.0
      for i in xrange(5):
        tm = float(data[idx+1+i][4:])
        tot = tot + tm
        if tm > max_val:
          max_val = tm
        if tm < min_val:
          min_val = tm
      res.append((tot - max_val - min_val)/3)
  return {"qemu": qemu, "name": name, "test": test, "res": res}

if __name__ == "__main__":
  logs = os.listdir(".")
  bench_res = {}
  for name in logs:
    if ".txt" not in name or "log" not in name:
      continue
    bench_res["qemu-" + name[4:-4]] = read_file(name)

  print "\\documentclass{article}\n\\usepackage[margin=0.2cm]{geometry}\n\\begin{document}"
  print "\\begin{tabular}{|c|" + "c|"*len(bench) + "}\n\\hline"

  print "  ",
  for ben in bench:
    print "&", "{\\tiny", ben, "}",
  print "\\\\\n  \\hline"

  i = 0
  for te in test:
    print "  " + te.replace("_", "\\_"),
    for ben in bench:
      print "&", "%.3f" % bench_res[ben]["res"][i],
    print "\\\\\n  \\hline"
    i = i + 1

  print "\\end{tabular}"
  print "\\end{document}"
