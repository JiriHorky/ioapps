# News #
  * 02.6.2014 - version 1.4-rev3 released - available at [GoogleDrive](https://drive.google.com/folderview?id=0BwCJhX70WYQsb3FuVElJSXlmWFU&usp=sharing)
    * No new features
    * Fix: declaration of simfs\_init masks that the function accepts parameter causing possible stack underflow (thanks Michael Tautschnig - Debian [Bug#748218](https://code.google.com/p/ioapps/issues/detail?id=48218))
    * Fix: [issue 7](https://code.google.com/p/ioapps/issues/detail?id=7): assertation fault with dup2 calls on already opened files
    * Fix: deleting from fdmap when the deleted item is the last one
    * Building on powerpc: http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=730505 Thanks to Roland Stigge for the patch
  * 19.4.2013 - version 1.4-rev2 released
    * No new features
    * Fix: Copryright a license information in all source files

  * 9.4.2013 - version 1.4-rev1 released
    * No new features, this is just a bug (build) fix release
    * Fix: Makefile and tarball cleanups

  * 5.4.2013 - version 1.4 released
    * New feature: ioprofiler can open traces from a file specified on the command line as an argument
    * New feature: Help menu for ioprofiler to aid people understand what it is useful for and to point to wiki
    * New feature: [ioprofiler-trace](ioprofiler#Getting_traces.md) wrapper around strace to help getting traces
    * Bug fix: Histograms now show 99% of the data - scale is automatically adjusted
    * Bug fix: "Show stats" button in ioprofiler works for histograms as well
    * Some minor bug fixes
    * New [screenshots](IOProfilerScreenshots.md) explaining [ioprofiler](ioprofiler.md) on profiling thunderbird

  * 21.10.2011 - version 1.3 released
    * IOprofiler can now read incomplete traces
    * new feature: Processes' info is now kept in hash tables to improve performance
    * various bugs fixed

  * 10.4.2011 - version 1.2 released
    * support for incomplete traces (obtained by strace -p <PID of already running program>)
    * support for sendfile syscall
  * 2.1.2011 - version 1.1 released - `pread` and `pwrite` system calls added

# Description #

IOapps consists of two applications: **ioreplay** and **ioprofiler**, both of them sharing common code base.

The tools are intended to run under Linux.

  * [ioreplay](ioreplay.md) is mainly intended for replaying of recorded (using `strace`) IO traces, which is useful for standalone benchmarking. It provides many features to ensure validity of such measurements.
  * [ioprofiler](ioprofiler.md) is a GUI application for profiling application's IO access pattern. It shows all the read/written files by the traced application, how many reads were performed, how much data were read and how much time was spent reading for each file. Most importantly, it produces nice diagrams showing read/write access pattern (i.e. whether the application was reading sequentially or randomly) in how big chunks etc. See [screenshots](IOProfilerScreenshots.md) with a real life example of profiling thunderbird.

Please refer to detailed description on dedicated pages for each tool.

# How it works #

The idea behind both applications is to collect all IO-related system calls that your traced application does. Both applications use currently `strace` as a source of information. Usage of `LD_PRELOAD` mechanism such as [IOprof](http://www.benchit.org/wiki/index.php/IOprof) as another source of traces is under investigation.
Currently, there are around 20 system calls we process. See [list](SystemCallList.md).

# Installation #

See [installation](IOAppsInstallation.md) instructions.

# TODO list #

Please see it on the dedicated [page](WishList.md). Please fire an issue if you have a specific wish that I should add.

# Acknowledgment #

The applications were written with partial support of [Institute of Physics of Academy of Science CR, v. v. i. (FZU)](http://www.fzu.cz) and The European Organization for Nuclear Research ([CERN](http://www.cern.ch)).

The applications are licensed under GNU GPL v2. They were written by Jiri Horky in 2010 as a part of his Master of Science Thesis.