strace_file=strace2.file
strace -q -a1 -s0 -f -tttT -o${strace_file} -e trace=file,desc,process,socket "$@"
