### Cross-Compiling for MinGW

It is now possible to compile an Eternity executable for Windows on Linux using
MinGW32; below are brief instructions on how to do so.

#### Building/Installing/Distributing

The procedures are the same as those found in `README-STATIC.txt`.  You will,
of course, need to install MinGW32 on your system.  Installation procedures
vary and are best left to documentation for your own distribution.  Of course,
you'll have to run MinGW's `strip`, otherwise you'll likely corrupt the
executable, and this will remove all debugging symbols so users will be unable
to use a debugger.

vim:tw=79 sw=4 ts=4 syntax=mkd et:

