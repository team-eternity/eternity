### Cross-Compiling for MinGW

It is now possible to compile an Eternity executable for Windows on Linux using
MinGW32; below are brief instructions on how to do so.

#### Dependencies

In order to save yourself a lot of trouble, you can download the Eternity
dependencies at _URL coming soon_.  You are strongly urged to do this; some
slight modifications to various build systems and headers have been made in
order to make cross-compilation possible, and you can ensure a proper feature
set.  Both source and binaries are available.

That said, there's no technical reason you can't compile the dependencies
yourself.

#### Building

**WARNING**: If you have a `build` folder inside the top-level Eternity source
distribution folder, its contents will be removed.

You will, of course, need to install MinGW32 on your system.  Installation
procedures vary and are best left to documentation for your own distribution.
Once finished, edit the `compile_mingw.sh` script in the top-level Eternity
source distribution folder, and replace the variables `EEDEPS`, `MINGW_PATH`,
and `MINGW_MACHINE` with proper values for your platform and installation.
Once you've configured the script, run it.  This will create a `build` folder,
and if one already exists it will be cleaned out.  `cd` to that folder, and run
`make`.

#### Installation/Distribution

**WARNING**: `make install` is untested and most likely has serious issues.

There is currently no installation or packaging functionality; should you
package a cross-compiled binary, you must at least include the following:

  - `eternity.exe` (the executable program)
  - `snes_spc.dll` (a required shared library dependency)
  - `base`         (EDF, configuration files, resource WAD, etc.)

Relative to the top-level Eternity source distribution folder:

  - `eternity.exe` can be found at `build/source/eternity.exe`.
  - `snes_spc.dll` can be found at `build/snes_spc/libsnes_spc.dll`
  - `base` can be found at `base`

You may be legally required to enclose licenses as well.  While I am not
qualified to give legal advice, nor should you consider this legal advice, you
probably cover all your bases by just copying `docs/licenses` into your
distribution

Finally, the generated executable will be quite large.  If you prefer, you can
run the `strip` utility on the executable which reduces its size tremendously.
Of course, you'll have to run MinGW's `strip`, otherwise you'll likely corrupt
the executable, and this will remove all debugging symbols so users will be
unable to use a debugger.

vim:tw=79 sw=4 ts=4 syntax=mkd et:

