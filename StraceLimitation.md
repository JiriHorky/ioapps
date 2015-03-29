Using `strace` as a source of traces for IO profiling and trace replaying has its drawbacks that one should be aware of:

# Advantages #
  * Installed on every Linux installation (besides Gentoo :-) from default
  * Very easy to use
  * Reasonable for performance hit for majority of applications

# Limitations #
  * Performance hit
    * Depending on the number of system calls and made by the traced application and your CPU, the overhead can range from 0-2% (up to 100 calls per second) to as much as 100% (20 000 syscalls per second).
    * This should't be a problem for profiling and determining the access pattern (random, sequential) traced application uses, but can be a significant problem for [ioreplay](ioreplay.md).
  * Strace has bugs when the traced job structure is very complicated (i.e. it means spawning of hundred children (with another children...))
    * The output can get corrupted, see the bug submitted: http://www.mail-archive.com/strace-devel@lists.sourceforge.net/msg01595.html
  * Not possible to trace accesses to memory-mapped files