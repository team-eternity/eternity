### Building a Static Eternity

It is now possible to compile an Eternity executable (mostly) statically, below
are brief instructions on how to do so.

#### Dependencies

There may be pre-compiled dependencies for your platform in the EE dependencies
distribution at http://static.totaltrash.org/eedeps.tar.xz.  If not, the
distribution also contains source and a `build_deps.py` script in the `source`
folder that will build them.  You are strongly urged to this; some slight
modifications to various build systems, headers, and source files have been
made in order to make static compilation possible, and it will also ensure a
proper feature set.

That said, there's no technical reason you can't compile the dependencies
yourself.

#### Building

**WARNING**: If you have an `sbuild` folder inside the top-level Eternity
source distribution folder, its contents will be removed.

Edit the `build_static.sh` script, follow the directions therein to configure
it, and then run it.  This creates an `sbuild` folder, and if one already
exists it's cleaned out.  `cd` to that folder, and run `make`.

#### Installation/Distribution

**WARNING**: `make install` is untested and most likely has serious issues.

There is currently no installation or packaging functionality; should you
package a static binary, you must at least include the following:

  - `eternity(.exe)` (the executable program)
  - `base`         (EDF, configuration files, resource WAD, etc.)

Relative to the top-level Eternity source distribution folder:

  - `eternity(.exe)` can be found at `build/source/eternity(.exe)`.
  - `base` can be found at `base`

You may be legally required to enclose licenses as well.  While I am not
qualified to give legal advice, nor should you consider this legal advice, you
probably cover all your bases by just copying `docs/licenses` into your
distribution.

Finally, the generated executable will be quite large.  If you prefer, you can
run the `strip` utility on the executable which reduces its size tremendously.

#### Caveats

These binaries are not 100% statically linked, they still depend on some low-
level libraries.  Linux binaries will depend on:
  - libpthread
  - libm
  - librt
  - libstdc++

Windows binaries will depend on:
  - m
  - user32
  - gdi32
  - dxguid
  - ws2_32
  - wldap32
  - winmm
  - stdc++

Additionally building libmikmod statically so that SDL_mixer would link to it
proved to be too difficult, and thus MOD/etc. support is not included in these
binaries.

vim:tw=79 sw=4 ts=4 syntax=mkd et:

