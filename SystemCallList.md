Both [ioreplay](ioreplay.md) and [ioprofiler](ioprofiler.md) process the list of recorded system calls in order to reconstruct the IO behaviour of the traced applications. Here is the list of the syscalls that we take into account:

  * `open`
  * `close`
  * `read`
  * `write`
  * `lseek`
  * `_llseek`
  * `creat`
  * `mkdir`
  * `access`
  * `stat` (and `stat64`)
  * `unlink`
  * `rmdir`
  * `sendfile`
  * `pread` (and `pread64`)
  * `pwrite` (and `pwrite64`)
  * `socket` (only for file descriptors mapping handling, we don't replay this one)
  * `pipe` (only for file descriptors mapping handling, we don't replay this one)
  * `dup`, `dup2` and `dup3` (only for file descriptors mapping handling, we don't replay this one)
  * `clone` (only for file descriptors mapping handling, we don't replay this one)

# TODO system calls #

To ensure even better functionality, these additional system calls should be included in the near future:
  * `fstat`
  * `rename`
  * `pipe2`
  * `tee`
  * `splice`
  * `vmsplice`