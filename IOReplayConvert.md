# Binary format #
Storing and also processing text output from strace command is inefficient. For that reason, one can convert it to a binary form that has following advantages:
  * it is approximately twice smaller
  * it is much faster to process when loading
  * it is also architecture (32/64bits) independent
  * the binary format is a default format for [ioprofiler](ioprofiler.md) (but it can also open pure strace outputs)

# How #
To convert file _strace.out_ to the binary file called _strace.bin_, run:
```
ioreplay -c -f strace.out -o strace.bin
```

Don't forget to use **-F binary** option for [ioreplay](ioreplay.md) when working with binary files.