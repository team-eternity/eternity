## Eternity Engine EDF Reference v1.7 \-- 10/22/06

[Return to the Eternity Engine Page](../etcengine.html)

- [**Table of Contents**]{#contents}
  - [Introduction to EDF](#intro)
  - [Changes in EDF 1.6](#changes)
  - [General Syntax](#gensyn)
  - [Reserved Identifiers](#reserved)
  - [Files](#files)
    - [Specifying the Root EDF File](#root)
    - [Loading EDF From a WAD Lump](#lump)
    - [ESTRINGS Lump](#estrings)
    - [ETERRAIN Lump](#eterrain)
    - [EMENUS Lump](#emenus)
    - [ESNDINFO and ESNDSEQ Lumps](#esndinfo)
    - [Including Files](#inc)
    - [Including Lumps](#inclump)
    - [Including BEX Files](#bex)
    - [Default EDF Files](#defs)
    - [Default Fallbacks](#fallbacks)
    - [Verbose EDF Logging](#verbose)
  - [Enable Functions](#enables)
  - [Gametype Functions](#gametypes)
  - [Strings](#strings)
  - [Sprite Names](#sprnames)
  - [Sprite-Related Variables](#spritevars)
  - [Sprite-Based Pickup Items](#pickups)
  - [Frames](#frames)
  - [Compressed Frame Definitions](#cmpframes)
  - [Thing Types](#things)
  - [Thing Type Inheritance](#inherit)
  - [DOOM II Cast Call](#cast)
  - [DOOM II Boss Brain Types](#boss)
  - [Sounds](#sound)
  - [Ambience](#ambience)
  - [Sound Sequences](#soundseqs)
  - [Enviroment Sequence Manager](#enviromgr)
  - [TerrainTypes](#terrain)
    - [Splash Objects](#splashes)
    - [Terrain Objects](#terrain)
    - [Floor Objects](#floors)
    - [TERTYPES Lump Processing](#tertypes)
  - [Dynamic Menus](#menus)
  - [Delta Structures](#delta)
  - [Miscellaneous Settings](#misc)

[]{#intro}

------------------------------------------------------------------------

**Introduction to EDF**

------------------------------------------------------------------------

\
EDF, which stands for **Eternity Definition Files**, is a new data
specification language for the Eternity Engine that allows dynamic
definition of sprites, thing types, frames, and other previously
internal data. The EDF parser is based on the libConfuse configuration
file parser library by Martin Hedenfalk, which is also used by GFS and
ExtraData.\
\
EDF supercedes DeHackEd, and should become the preferred method for
\"exe\" editing in the future. However, EDF retains features that allow
DeHackEd compatibility, and DeHackEd patches may be loaded over EDF, and
even have access to \"new\" things and frames that are defined by it.\
\
Each section in this document deals with one of the EDF constructs, as
well as showing their locations in the default EDF files. However,
user-made EDF files can, with a few caveats, contain these structures in
any arrangement of files and in any order. User EDF files do not need to
use the same names or arrangements as the defaults, and they should
NEVER be intended to overwrite the Eternity Engine\'s default files,
unless they are being provided with a customized distribution of the
engine itself.\
\
Plans are in place to steadily introduce more features and functionality
to EDF in future versions of the Eternity Engine, including most
notably, weapon definitions. This documentation will be regularly
updated to reflect all changes and additions.\
\
[Return to Table of Contents](#contents)

[]{#changes}

------------------------------------------------------------------------

**Changes in EDF 1.7**

------------------------------------------------------------------------

\

- Many errors have been downgraded to warnings, allowing fewer
  edit-and-run cycles for debugging EDF files. As usual, all warnings
  are issued to the verbose log activated with **-edfout**.
- basictype, topdamage, topdamagemask fields added to thingtype section.
- pitchvariance and alias fields added to sound section.
- Ambience definitions.
- Sound sequences.
- Environment sequence manager.
- ESNDINFO and ESNDSEQ lumps.
- Additive sounds and sounddeltas.
- Semantics of several sound fields have been adjusted and improved.

[Return to Table of Contents](#contents)

[]{#gensyn}

------------------------------------------------------------------------

**General Syntax**

------------------------------------------------------------------------

\

- EDF is Free-form\
  \
  Whitespace and token positioning are totally free-form in EDF. In
  addition, semicolons are discarded as whitespace and can therefore be
  used to terminate field assignments, lists, etc. as in C or C++. As an
  example, all of the following are strictly equivalent:

           thingtype w00t { mass = 1000 }
           
           thingtype w00t
           {
              mass = 1000;
           }
           
           thingtype
                            w00t
                       {
              mass     =
                   1000     ;;;;;;
                           }

  Obviously, you are encouraged to use a clean and consistent format,
  even though it is not required.\
  \

- Comments\
  \
  EDF files can use three forms of comments:

  - \# Comments\
    Like in DeHackEd, this comment type extends to the end of the line.
  - // Comments\
    An alternate form of single-line comment, equivalent to #.
  - /\* \*/ Comments\
    This type of comment is multiline, and extends from the opening /\*
    to the first \*/ found. This type of comment CANNOT be nested.
    Nested multiline comments will cause a syntax error.

  Examples:


           # Single line comment
           
           // Another single line comment
           
           /*
           
              This is a multiline comment
              
           */
           

- Strings\
  \
  Many fields in EDF take strings. Strings, if they do not contain
  whitespace or any character that needs to be escaped, may be unquoted.
  Unquoted strings additionally cannot contain any of the following
  characters: \" \' = { } ( ) + , \# / ;\
  If any of those characters are found, the unquoted string will be
  terminated at the last valid character.\
  \
  Example:


           spritenames += { SPR1, SPR2, SPR3 }
           

  The items SPR1, SPR2, and SPR3 are unquoted strings. Note how the
  commas serve to separate the items in a list, and therefore do not
  become part of the strings.\
  \
  Many fields more or less require quoted strings. Quoted strings must
  start and end with either single or double quotes (the beginning and
  ending quote types must match). Quoted strings may contain any
  character except a line break, including those not allowed in unquoted
  strings. In addition, quoted strings also support the following escape
  codes for non-typable characters:\
  \

  - \\n : Hard line break
  - \\t : Tab
  - \\b : Backspace
  - \\a : Beep (causes a sound when printed to the console)
  - \\\\ : Literal \\ character.
  - \\\" : Literal \", necessary to use in double-quoted strings only
  - \\\' : Literal \', necessary to use in single-quoted strings only
  - \\0 : Null character
  - \\K : Changes text color to \"brick\" range where supported.
  - \\1 : Changes text color to \"tan\" range where supported.
  - \\2 : Changes text color to \"gray\" range where supported.
  - \\3 : Changes text color to \"green\" range where supported.
  - \\4 : Changes text color to \"brown\" range where supported.
  - \\5 : Changes text color to \"gold\" range where supported.
  - \\6 : Changes text color to \"red\" range where supported.
  - \\7 : Changes text color to \"blue\" range where supported.
  - \\8 : Changes text color to \"orange\" range where supported.
  - \\9 : Changes text color to \"yellow\" range where supported.
  - \\N : Changes text color to gamemode \"normal\" range where
    supported.
  - \\H : Changes text color to gamemode \"hilite\" range where
    supported.
  - \\E : Changes text color to gamemode \"error\" range where
    supported.
  - \\S : Toggles text shadowing where supported.
  - \\C : Toggles absolute centering of text where supported.

  \
  \
  If \\ is followed by any other character, the slash is discarded and
  the next character is treated normally (ie, \\d becomes d). Avoid
  doing this, however, to maintain forward compatibility.\
  \
  Examples:


           thingtype w00t
           {
              obituary_normal = "w00ts"
              obituary_melee  = 'says \'w00t\''
           }
           

  Line Continuation:\
  \
  Starting with Eternity Engine v3.31 Delta, line continuation can be
  used to split quoted strings across multiple lines. The syntax for
  doing this is demonstrated in the following example:


           frame S_MYFRAME
           {
              cmp = "TROO|A|*|6 \
                     TroopAttack|@next"
           }
           

  The \"\\\" character, when followed immediately by a line break,
  signifies that line continuation should be triggered. Whitespace
  before the line continuation will be included in the string, but any
  spaces or tabs at the beginning of the next line will NOT be included
  (this is the same as line continuation in BEX files). This allows the
  following continued parts of the string to be arbitrarily indented for
  purposes of beautification. A string can be split across any number of
  lines in this fashion.\
  \

- Numbers\
  \
  Most non-string fields in EDF are numbers, either integer or
  floating-point. Starting with EDF 1.1, decimal, octal, and hexadecimal
  integers will be accepted in all number fields. Octal numbers start
  with a zero, and hexadecimal numbers start with the sequence 0x \--
  Examples:


           # this is a normal, decimal number (base 10)
           spriteframe = 16
           ...
           # this is an octal number (base 8)
           spriteframe = 020
           ...
           # this is a hexadecimal number (base 16)
           spriteframe = 0x10

  Floating-point numbers must have a decimal point in them, as in
  \"20.0\". Floating-point numbers are always base 10.

[Return to Table of Contents](#contents)

[]{#reserved}

------------------------------------------------------------------------

**Reserved Identifiers**

------------------------------------------------------------------------

\
This section is new to EDF 1.6 and describes classes of identifiers
(such as thing, frame, and sound mnemonics) which are considered
reserved by the Eternity Engine. You should avoid using any identifier
in the following classes for objects which you define yourself. Failure
to do so could result in incompatibility of your project with future
versions of the Eternity Engine due to conflicts with newly added game
engine features.

- All identifiers beginning with \_ (underscore)
- All identifiers beginning with EE or ee (this prefix means \"Eternity
  Engine\" and is therefore reserved for use by the source port\'s
  authors only).
- All identifiers containing ANY non-alphanumeric character excepting
  non-leading underscores (ex: \$ is reserved).

For maximum safety, it is recommended that you develop a consistent and
unique naming scheme for objects you create yourself. A simplistic
example would be prefixing the word My to mnemonics, such as
\"MyNewThingType\". Such names are more or less guaranteed to never
conflict with anything defined by the game engine itself. The Chex Quest
III project uses the prefix \"CQ\" for this purpose. As a side effect,
this also improves the ability of your EDF patches to combine with those
of others.\
\
[Return to Table of Contents](#contents)

[]{#files}

------------------------------------------------------------------------

**Files**

------------------------------------------------------------------------

\
[]{#root}

------------------------------------------------------------------------

**Specifying the Root EDF File**

------------------------------------------------------------------------

\
Eternity supports several methods for locating the root EDF file, which
is the only EDF file loaded directly for parsing. Excepting special lump
chains documented below, all other EDF files must be included through
the root EDF.

- Default \-- root.edf\
  \
  If no EDF file is specifically requested through any other method
  listed here, the engine will load the file \"root.edf\" from its own
  directory. The default root.edf contains only include statements,
  which are covered in the next section.\
  \
- Command-line Parameter -edf\
  \
  The -edf command-line parameter allows the root EDF file to be
  specified explicitly, just like adding a WAD or DeHackEd file. The
  argument to the parameter should be exactly one complete relative or
  absolute file path, as in this example:\
  \
  eternity -edf c:\\blah\\root.edf\
  \
- Game File Script (GFS) Specification\
  \
  The new GFS file can specify the root EDF file with the \"edffile\"
  keyword. Only one EDF file can be specified in a GFS. If -edf is used
  when a GFS is loaded, the command-line parameter will take precedence.
  See the [GFS documentation](gfs_ref.html) for further information.

[Return to Table of Contents](#contents)

[]{#lump}

------------------------------------------------------------------------

**Loading EDF From a WAD Lump**

------------------------------------------------------------------------

\
Starting with EDF 1.4, it is possible to load a WAD lump as the root EDF
file. The name of the root EDF lump must be \"EDFROOT\", and the newest
lump of that name is the one that will be loaded and parsed. See the
section below on including files for information on how this affects EDF
processing.\
\
As of EDF 1.5, selective sets of EDF definitions may also be loaded
additively from separate chains of optional lumps. These lumps are
documented under their own sections below.\
\
[Return to Table of Contents](#contents)

[]{#estrings}

------------------------------------------------------------------------

**ESTRINGS Lump**

------------------------------------------------------------------------

\
The ESTRINGS lump is an optional EDF lump which can contain any number
of [**string**](#strings) definitions. String definitions found in the
ESTRINGS lump which already exist will overwrite string definitions with
the same mnemonic. ESTRINGS lumps do not cascade by default, but can be
made to do so by using the **include_prev** funtion documented below.\
\
Note: it is inappropriate to include an ESTRINGS lump from EDFROOT or
from any file included by it, because ESTRINGS will be processed
separately after the root EDF has been processed. Doing this will cause
the definitions to be processed twice.\
\
[Return to Table of Contents](#contents)

[]{#eterrain}

------------------------------------------------------------------------

**ETERRAIN Lump**

------------------------------------------------------------------------

\
The ETERRAIN lump is an optional EDF lump which can contain any number
of **splash**, **terrain**, and **floor** definitions. All definitions
are additive, and if definitions of the same name already exist, they
will overwrite. ETERRAIN lumps do not cascade by default, but can be
made to do so by using the **include_prev** function documented below.\
\
Note: it is inappropriate to include an ETERRAIN lump from EDFROOT or
from any file included by it, because ETERRAIN will be processed
separately after the root EDF has been processed. Doing this will cause
the definitions to be processed twice.\
\
Special note for ETERRAIN: If zero splash, terrain, AND floor objects
are defined by both the root EDF and any ETERRAIN lump processed, the
default terrain.edf module will be loaded in order to maintain
compatibility with older projects. In order to disable this behavior,
you will need to define at least one splash, terrain, or floor object.
This object can be a dummy which is never used, however.\
\
[Return to Table of Contents](#contents)

[]{#emenus}

------------------------------------------------------------------------

**EMENUS Lump**

------------------------------------------------------------------------

\
The EMENUS lump is an optional EDF lump which can contain any number of
**menu** definitions and menu-system global variables. All definitions
are additive, and if definitions of the same name already exist, they
will overwrite. EMENUS lumps do not cascade by default, but can be made
to do so by using the **include_prev** function documented below.\
\
Note: it is inappropriate to include an EMENUS lump from EDFROOT or from
any file included by it, because EMENUS will be processed separately
after the root EDF has been processed. Doing this will cause the
definitions to be processed twice.\
\
[Return to Table of Contents](#contents)

[]{#esndinfo}

------------------------------------------------------------------------

**ESNDINFO and ESNDSEQ Lumps**

------------------------------------------------------------------------

\
The ESNDINFO and ESNDSEQ lumps are optional EDF lumps which can contain
any number of **sound**, **sounddelta**, **ambience**,
**soundsequence**, and **enviromanager** definitions. All definitions
are additive, and if definitions of the same name already exist, they
will overwrite. These lumps do not cascade by default, but can be made
to do so by using the **include_prev** function documented below.\
\
Note: it is inappropriate to include an ESNDINFO or ESNDSEQ lump from
EDFROOT or from any file included by it, because both of these lumps
will be processed separately after the root EDF has been processed.
Doing this will cause the definitions to be needlessly processed twice.\
\
[Return to Table of Contents](#contents)

[]{#inc}

------------------------------------------------------------------------

**Including Files**

------------------------------------------------------------------------

\
Use of include files is critical for several purposes. First, it\'s
unorganized and difficult to maintain EDF files when everything is put
haphazardly into one file. Second, through use of the **stdinclude**
function, user-provided EDF files can include the standard defaults,
without need to duplicate the data in them. This helps keep EDF files
compatible with future versions of the Eternity Engine, besides making
your files much smaller.\
\
To include a file or wad lump normally, with a path relative to the
current file doing the inclusion, use the **include** function, like in
these examples:


    # include examples

    include("myfile.edf")
    include("../prevdir.edf");  # remember, semicolons are purely optional

This example would include \"myfile.edf\" from the same directory that
the current file is in, and \"prevdir.edf\" from the parent directory.
It is important to note that this function can only include files when
called from within a file, and it can only include WAD lumps when called
from within a WAD lump. This restriction is necessary due to how the
complete path of the included file must be formed.\
\
In order to remain compatible with future versions of Eternity, and to
greatly reduce EDF file sizes, you can include the standard, default EDF
files into your own. To include files relative to the Eternity
executable\'s directory, use the **stdinclude** function, like in this
example:


    stdinclude("things.edf")

This would include \"things.edf\" in the Eternity Engine\'s folder. This
function may be called from both files and WAD lumps.\
\
Include statements may only be located at the topmost level of an EDF
file, and not inside any section. The following example would not be
valid:


    spritenames =
    {
       include("sprnames.txt")
    }

Note that for maximum compatibility, you should limit EDF file names to
MS-DOS 8.3 format and use only alphanumeric characters (ie, no spaces).
This is not required for Windows or Linux, but the DOS port of Eternity
cannot use long file names.\
\
New to EDF 1.6 (Eternity Engine v3.33.33), the **userinclude** function
allows the optional inclusion of a file from Eternity\'s folder. This
function is identical to **stdinclude**, except that it will test to see
if the file exists beforehand, and if it does not, no error will occur.
This function is **NOT** for use by EDF editors in projects. This
function is intended for end-user modification of EDF settings,
including but not limited to the custom user menu. Example:


    userinclude("user.edf")

This example can be found in Eternity\'s root.edf, where it exists to
include the optional user-defined \"user.edf\" file. You should never
attempt to overwrite the user.edf file with one from your own project,
as this is outside the scope of its purpose and will be offensive to the
end user.\
\
[Return to Table of Contents](#contents)

[]{#inclump}

------------------------------------------------------------------------

**Including Lumps**

------------------------------------------------------------------------

\
Two special functions are provided for specifically including EDF lumps.
The first function is **lumpinclude**. This function includes a wad lump
when called from either a file or a lump. The lump named must exist, or
an error will occur. The name must also be no longer than eight
characters. Case of the lump name will be ignored as usual.\
Example:


    lumpinclude("MYEDFLMP")

The second function, **include_prev**, may be called only from within a
wad lump. This function is special in that you do not tell it what lump
to include. Rather, it includes the previously loaded lump of the same
name as the current lump being processed, if and only if such a lump
exists. This allows cascading behavior to be enabled for lumps such as
ESTRINGS and ETERRAIN, so that several wads adding such lumps will all
load their definitions. For full flexibility, this does not happen by
default. No error will occur if there is no previous lump \-- the
function call is simply ignored in that case.\
Example:


    include_prev()

The order of lumps is determined first by the order of lumps in each
wad, and then by the order in which wads were loaded. The first lump to
be loaded by the game engine will be the last lump of a given name in
the last wad that was loaded. include_prev will then continue back
through lumps of the same name in the same wad, then in the previously
loaded wad, etc.\
\
[Return to Table of Contents](#contents)

[]{#inc}

------------------------------------------------------------------------

**Including BEX Files**

------------------------------------------------------------------------

\
As of Eternity Engine v3.31 public beta 5, EDF now supports including
DeHackEd/BEX files. Any DeHackEd/BEX files included will be queued in
the order they are included, after any other DeHackEd/BEX files to be
processed. DeHackEd/BEX processing occurs immediately after EDF
processing is complete. This allows better cohesion between the EDF and
BEX languages, and also allows the user to specify fewer command line
parameters or to avoid the need for a GFS file in some cases.\
\
To include a DeHackEd/BEX file for queueing from EDF, use the
**bexinclude** function, like in this example:


    bexinclude("strings.bex")

The DeHackEd/BEX file will be included relative to the path of the
including EDF file. There is no **stdinclude** equivalent for
DeHackEd/BEX files, since the engine does not provide any default BEX
files. You **cannot** call this function from within an EDF WAD lump,
and an error message will be given if this is attempted.\
\
[Return to Table of Contents](#contents)

[]{#defs}

------------------------------------------------------------------------

**Default EDF Files**

------------------------------------------------------------------------

\
This section explains the contents of each of the standard default EDF
modules as of Eternity Engine v3.33.33.

- root.edf \-- includes all other modules
- sprites.edf \-- contains the sprite names list and sprite-based pickup
  item definitions
- sounds.edf \-- contains sound and soundsequence definitions.
- frames.edf \-- contains frame data for use by thing types and player
  guns
- things.edf \-- contains thing type definitions
- cast.edf \-- contains DOOM II cast call definitions
- misc.edf \-- contains the default list of DOOM II Boss Spawner thing
  types
- terrain.edf \-- contains default TerrainTypes definitions
- user.edf \-- optional user-defined EDF file which may contain any
  definitions

[Return to Table of Contents](#contents)

[]{#fallbacks}

------------------------------------------------------------------------

**Default Fallbacks**

------------------------------------------------------------------------

\
As a failsafe to allow old EDF modifications to continue working, EDF is
now capable of loading default modules individually when it determines
there are zero definitions of certain sections. The following modules
will be loaded as fallbacks when the given conditions are met:

- sprites.edf \-- Loaded when zero sprite names are defined. Note that
  even if pickup items are defined by the user EDF, they will be
  overridden by the pickup items defined in sprites.edf.
- sounds.edf \-- Loaded when zero sounds are defined.
- things.edf \-- Loaded when zero thingtypes are defined.
- frames.edf \-- Loaded when zero frames are defined.
- cast.edf \-- Loaded when zero cast members are defined.
- terrain.edf \-- Loaded when zero splash, terrain, and floor objects
  are defined.

Note that misc.edf cannot currently be used as a fallback. Also, this
functionality is NOT meant to allow users to neglect including the
minimum required number of sections. This functionality is only intended
to preserve maximum backward compatibility with old EDF modifications.
Relying on fallback behavior in new EDFs may have unexpected results,
including possibly having your new definitions ignored.\
\
As mentioned earlier, to prevent default terrain loading, it will be
necessary to define at least one splash, terrain, or floor object in the
root EDF or ETERRAIN lump.\
\
If the engine-default EDF files cannot be found or parsed for any
reason, EDF processing will cease immediately.\
\
[Return to Table of Contents](#contents)

[]{#verbose}

------------------------------------------------------------------------

**Verbose EDF Logging**

------------------------------------------------------------------------

\
Eternity includes a verbose logging feature in the EDF processor that
allows a view of more detailed information on what is occuring while EDF
is being processed. This can help nail down the location of any errors
or omissions. To enable verbose EDF logging, use the command-line
parameter **-edfout**. This will cause Eternity to write an
\"edfout##.txt\" log file in its current working directory, where \## is
a unique number from 0 to 99.\
\
[Return to Table of Contents](#contents)

[]{#enables}

------------------------------------------------------------------------

**Enable Functions**

------------------------------------------------------------------------

\
Starting with EDF 1.2, special functions are provided which allow the
user to enable or disable options within the EDF parser. The primary use
for these functions is to instruct the parser to skip definitions for
game modes other than the one currently being played. Although all the
thing, frame, sound, and sprite definitions will not conflict and can be
loaded with each other, most of the time this is unnecessary and
requires a significant amount of memory which will never be used.
Allowing such definitions to be discarded during parsing speeds up
processing and reduces memory usage. Note that all of these functions
are only valid at the topmost level of an EDF file, and not within
definitions.\
\
Enable Functions

- **enable(\<option\>)**\
  \
  This function enables the provided option from the point of its call
  onward. If the option is already enabled, no change takes place. If an
  invalid option name is provided, an error will occur.\
  \
- **disable(\<option\>)**\
  \
  This function disables the provided option from the point of its call
  onward. If the option is already disabled, no change takes place. If
  an invalid option name is provided, an error will occur.\
  \
- **ifenabled(\<option\>, \...)**\
  \
  This function tests if the the provided option is enabled. If so,
  nothing will change. Otherwise, the EDF parser will be instructed to
  skip forward until it finds an **endif** function (see below). The
  **endif** function will then be invoked. If the option provided is
  invalid, an error will occur. Note that ifenabled/endif pairs cannot
  be nested.\
  \
  As of EDF 1.5, it is possible to provide any number of valid enable
  options to this function. In the case that more than one enable option
  is provided, \*all\* specified options must be enabled or the section
  will be skipped.\
  \
- **ifenabledany(\<option\>, \...)**\
  \
  New to EDF 1.5, this function tests if \*any\* of the provided options
  are enabled. If any are, nothing will change. Otherwise, parsing will
  skip to the next **endif** function.\
  \
- **ifdisabled(\<option\>, \...)**\
  \
  New to EDF 1.5, this function tests if \*all\* of the provided options
  are disabled. If all options are disabled, nothing will change.
  Otherwise, parsing will skip to the next **endif** function.\
  \
- **ifdisabledany(\<option\>, \...)**\
  \
  New to EDF 1.5, this function tests if \*any\* of the provided options
  are disabled. If any of the options are disabled, nothing will change.
  Otherwise, parsing will skip to the next **endif** function.\
  \
- **endif()**\
  \
  This function marks the end of a section of the file started by an
  **ifenabled**, **ifenabledany**, **ifdisabled**, **ifdisabledany**,
  **ifgametype**, or **ifngametype** call.\
  \
  When any of the aforementioned functions tests an option or value that
  is not enabled or is set to a different value, the parser will look
  for this function and then call it. This function will cause the
  parser to start recognizing definitions again from its line onward. If
  this function does not appear after one of the mentioned functions and
  before the end of the current file, a \"missing conditional function\"
  error will occur.

Available Options

- **DOOM**\
  \
  When this option is enabled, DOOM thing, frame, state, and sound
  definitions will be available. This variable is enabled by default for
  all user EDFs, but is disabled if the game is loaded in Heretic mode
  and the default root.edf file in Eternity\'s directory is loaded.\
  \
- **HERETIC**\
  \
  When this option is enabled, Heretic thing, frame, state, and sound
  definitions will be available. This variable is enabled by default for
  all user EDFs, but is disabled if the game is loaded in DOOM or DOOM
  II mode and the default root.edf in Eternity\'s directory is loaded.

Notes on Game Mode Options:\
\
As mentioned in their descriptions, the DOOM and HERETIC options will be
enabled by default when user EDF files are loaded. However, user EDF
files can explicitly call the **disable** function to turn off one or
both options before including any of Eternity\'s defaults. It is best to
do this when you know you will not be using any of one of the game
modes\' definitions.\
\
Example:


    // this thing type is only available if HERETIC is enabled

    ifenabled(HERETIC)

    thingtype foo { spawnstate = S_FOO1 }

    endif()

Note on command-line option \"-edfenables\": The -edfenables
command-line option allows the user to override the default behavior
explicitly and enable all gamemodes\' definitions without adding an EDF
file. This does not interfere with explicit usage of enable functions in
user EDFs, but it does allow older DeHackEd patches and WADs which might
assume Heretic definitions are available in DOOM or vice versa to work.\
\
[Return to Table of Contents](#contents)

[]{#gametypes}

------------------------------------------------------------------------

**Gametype Functions**

------------------------------------------------------------------------

\
Starting with EDF 1.5, special functions are provided which allow the
EDF parser to evaluate definitions conditionally depending on the actual
type of game being played. This enables definitions which should only
exist in one game type or another to be placed within a single EDF
module without conflict or unnecessary waste of memory and processing
time. Note that all of these functions are only valid at the topmost
level of an EDF file, and not within definitions.\
\
Gametype Functions

- **ifgametype(\<gametype\>, \...)**\
  \
  This function tests the active gametype against every gametype listed
  as a parameter. If the gametype matches any of the listed types,
  nothing will change. Otherwise, processing will skip to the next
  **endif** function.\
  \
- **ifngametype(\<option\>, \...)**\
  \
  This function tests the active gametype against every gametype listed
  as a parameter. If the gametype matches none of the listed types,
  nothing will change. Otherwise, processing will skip to the next
  **endif** function.

Gametype Values

- **DOOM**\
  \
  This gametype is used for DOOM, Ultimate DOOM, DOOM II, and Final
  DOOM.\
  \
- **HERETIC**\
  \
  This gametype is used for Heretic and Heretic: Shadow of the Serpent
  Riders.

Notes on Gametype Values:\
\
Gametype values, like the rest of EDF, are case-insensitive. Only values
listed above should be used with the gametype detection functions.\
\
Example:


    // this TerrainTypes floor is only available when Heretic is being played

    ifgametype(HERETIC)

    floor { flat = FLATHUH1; terrain = Lava }

    endif()

[Return to Table of Contents](#contents)

[]{#strings}

------------------------------------------------------------------------

**Strings**

------------------------------------------------------------------------

\
New to EDF 1.4, strings allow custom messages to be defined which may be
used directly by various game engine subsystems and can be used by other
facilities such as the ShowMessage codepointer.\
\
Each string must be given a unique mnemonic in its definition\'s header,
and this is the name used to identify the string elsewhere. Each field
in the string definition is optional. If a field is not provided, it
takes on the default value indicated below the syntax information.
Fields may also be provided in any order.\
\
As of EDF 1.5, string definitions may also be provided within special
lumps named ESTRINGS. The ESTRINGS lump is parsed separately after the
root EDF has been processed, and thus all string definitions found
within the ESTRINGS lump are additive over those defined through the
root EDF. If string definitions within the ESTRINGS lump have the same
mnemonic as one already defined, they will overwrite the original
definitions (note this also includes the numeric id field).\
\
Syntax:


    string <mnemonic>
    {
       num = <unique number>
       val = <string>
    }

Explanation of Fields:

- **num**\
  Default: -1\
  Some subsystems which use EDF strings, such as the ShowMessage
  codepointer, require the strings they use to have a numeric ID. This
  field sets the optional numeric ID of an EDF string to the value
  given. The default of -1 means that the string has no numeric ID and
  is thus not accessible to such subsystems. This ID number must be
  unique unless it is equal to -1.
- **val**\
  Default = \"\"\
  This is the text which will be stored inside the string object.

Restrictions and Caveats:

- All strings must have a unique mnemonic no longer than 32 characters.
  They should only contain alphanumeric characters and underscores.
  Length will be verified, but format will not (non-unique mnemonics
  will overwrite previous definitions, see below). All string mnemonics
  beginning with an underscore character are reserved for use by strings
  for which the use is engine-defined. See a list below for
  engine-defined string mnemonics.
- All strings with a numeric ID not equal to -1 must have a unique
  numeric ID. Any strings with a non-unique numeric ID will cause an
  error.

Replacing Existing Strings:\
\
To replace the values of an existing EDF string definition, simply
define a new string with the exact same mnemonic value (as stated above,
all strings need a unique mnemonic, so duplicate mnemonics serve to
indicate that an existing string should be replaced).\
\
Full Example:


    # Define a string for use by the ShowMessage codepointer

    string CyberMsg
    {
       num = 0;
       val = "Moooo!";
    }

    # Define a string for use by the intermission
    # See below for notes on reserved mnemonics and how the game engine uses them.

    string _IN_NAME_MAP01 { val = "The Slough of Despond" }

Engine-Defined String Mnemonics\
\
The following string mnemonics, if defined and properly enabled, will be
used by game engine subsystems directly for the indicated purposes. All
reserved mnemonics start with underscores. Using mnemonics that begin
with underscores for your own strings is done at your own risk, since
this may break compatibility of your patch with future versions of the
Eternity Engine.\
\


    Mnemonic Name or Class           Purpose                         Use Enabled By
    -------------------------------------------------------------------------------
    _IN_NAME_*                       Intermission map names          MapInfo
    -------------------------------------------------------------------------------

[Return to Table of Contents](#contents)

[]{#sprnames}

------------------------------------------------------------------------

**Sprite Names**

------------------------------------------------------------------------

\
Sprite names are defined as a list of string values which must be
exactly four characters long, and should contain only capital letters
and numbers.\
\
Syntax:


    spritenames = { <string>, ... }

If this syntax is used more than once, the definition which occurs last
during parsing will take precedence as the original definition of the
sprite name list. Values may either be added by copying the entire list
in a new EDF file and adding the new values anywhere in the list, or by
using the following syntax:


    spritenames += { <string>, ... }

This syntax allows the addition of new sprite names to the list without
requiring the original list to be changed or copied.\
\
The names defined in this list are the names by which a sprite must be
referred to in all other EDF structures. Sprites are the first item to
be parsed, regardless of their location, so like all EDF structures, the
list and any additions to it can be placed anywhere.\
\
Note: Sprite names may be duplicated in the list. When this occurs,
objects will use the corresponding index of the last definition of that
sprite name. Avoid this whenever possible, as it is wasteful and may
lead to inconsistencies if DeHackEd patches are subsequently applied.
Previous versions of the EDF documentation inappropriately stated that
all sprite names must be unique. This has never been true, and cannot be
made true for purposes of forward compatibility (otherwise addition of
new sprites to the default sprites.edf could break older EDFs).\
\
Restrictions and Caveats:

- There must be at least one sprite name defined.
- If invalid sprite names are used by other structures, a warning will
  be issued to the verbose log and the value of the blanksprite variable
  will be substituted as a safe fallback.
- Sprite names will be checked for length, but not for invalid
  characters.
- If compatibility with DeHackEd patches is desired, new sprites should
  use addition lists.

Full example:


    # defining an original spritenames array (this would replace the default list)
    # notice that sprite names never NEED to be quoted, but can be if so desired.
    spritenames =
    {
       FOO1, FOO2, FOO3, TNT1, PLAY
    }

    # add a few values in later (maybe near a thing or frames that use them)...
    spritenames += { "BLAH", "WOOT" }

[Return to Table of Contents](#contents)

[]{#spritevars}

------------------------------------------------------------------------

**Sprite-Related Variables**

------------------------------------------------------------------------

\
There are two sprite-related variables which may be specified in user
EDF files:

- **playersprite**
- **blanksprite**

**playersprite** sets the sprite to be used by the default \"Marine\"
player skin. This must be one of the four-character sprite mnemonics
defined in the **spritenames** array. If not provided in any EDF file,
this variable defaults to the value \"PLAY\" (and if PLAY is not defined
in that case, an error will occur).\
\
**blanksprite** sets the sprite to be used when objects or guns attempt
to use a sprite which has no graphics loaded. This must be one of the
four-character sprite mnemonics defined in the **spritenames** array. If
not provided in any EDF file, this variable defaults to the value
\"TNT1\" (and if TNT1 is not defined in that case, an error will
occur).\
\
These values are parsed immediately after the sprite name list is
loaded, and can be placed anywhere. If defined more than once, the last
definition encountered takes precedence. These values must be defined at
the topmost level of an EDF file.\
\
Syntax:


    playersprite = <sprite mnemonic>

    blanksprite = <sprite mnemonic>

Full example:


    # set the player skin sprite to BLAH
    playersprite = BLAH

    # set the blank sprite to FOO1
    blanksprite = FOO1

[Return to Table of Contents](#contents)

[]{#pickups}

------------------------------------------------------------------------

**Sprite-Based Pickup Items**

------------------------------------------------------------------------

\
Sprite-based pickup item definitions allow one of a set of predefined
pickup effects to be associated with a sprite, so that collectable
objects (with the SPECIAL flag set) using it will have that effect on
the collecting player.\
\
It is currently only possible to associate one effect with a given
sprite, but effects can be assigned to as many sprites as is desired. If
a sprite is assigned more than one pickup effect, the one occuring last
takes precedence. There are plans to extend this feature in the future
to include customizable pickup items, but for now only the predefined
effects are available.\
\
Syntax:


    pickupitem <sprite mnemonic> { effect = <effect name> }

**sprite mnemonic** must be a valid sprite mnemonic defined in the
**spritenames** list.\
**effect name** may be any one of the following:


       ** Note: all ammo amounts double in "I'm Too Young To Die" and "Nightmare!"
       ** Note: weapons give 5 clips of ammo when "Weapons Stay" DM flag is on
       ** Note: dropped weapons give half normal ammo
       
       Effect Name         Item is...               Special effects
      ----------------------------------------------------------------------------
       PFX_NONE            No-op collectable        None
       PFX_GREENARMOR      Green armor jacket       +100% armor
       PFX_BLUEARMOR       Blue armor jacket        +200% armor
       PFX_POTION          DOOM health potion       +1% health
       PFX_ARMORBONUS      Armor bonus              +1% armor
       PFX_SOULSPHERE      Soulsphere               +100% health
       PFX_MEGASPHERE      Megasphere               200% health/armor
       PFX_BLUEKEY         DOOM Blue key card
       PFX_YELLOWKEY       DOOM Yellow key card
       PFX_REDKEY          DOOM Red key card
       PFX_BLUESKULL       DOOM Blue skull key
       PFX_YELLOWSKULL     DOOM Yellow skull key
       PFX_REDSKULL        DOOM Red skull key
       PFX_STIMPACK        Stimpack                 +10% health
       PFX_MEDIKIT         Medikit                  +25% health
       PFX_INVULNSPHERE    Invulnerability Sphere   Temporary god mode
       PFX_BERZERKBOX      Berzerk Box              Super strength
       PFX_INVISISPHERE    Invisibility Sphere      Temporary partial invis.
       PFX_RADSUIT         Radiation Suit           No nukage damage
       PFX_ALLMAP          Computer Map             All of automap revealed
       PFX_LIGHTAMP        Light Amp Visor          All lights at full level
       PFX_CLIP            Clip                     +10 Bullets
       PFX_CLIPBOX         Clip box                 +50 Bullets
       PFX_ROCKET          Rocket                   +1 Rocket
       PFX_ROCKETBOX       Box of Rockets           +10 Rockets
       PFX_CELL            Cells                    +20 Cells
       PFX_CELLPACK        Cell pack                +100 Cells
       PFX_SHELL           Shells                   +4 Shells
       PFX_SHELLBOX        Box of Shells            +20 Shells
       PFX_BACKPACK        Backpack                 Max ammo *= 2, +1 clip all ammo
       PFX_BFG             BFG                      BFG weapon, +2 clips cells
       PFX_CHAINGUN        Chaingun                 Chaingun weapon, +2 clips bull.
       PFX_CHAINSAW        Chainsaw                 Chainsaw weapon
       PFX_LAUNCHER        Rocket launcher          Rocket launcher weapon, +2 rock.
       PFX_PLASMA          Plasma Gun               Plasma gun weapon, +2 clips cells
       PFX_SHOTGUN         Shotgun                  Shotgun weapon, +2 clips shells
       PFX_SSG             Super Shotgun            SSG weapon, +2 clips shells
       PFX_HGREENKEY       Heretic Green key        ** Gives both DOOM red keys
       PFX_HBLUEKEY        Heretic Blue key         ** Gives both DOOM blue keys
       PFX_HYELLOWKEY      Heretic Yellow key       ** Gives both DOOM yellow keys
       PFX_HPOTION         Heretic Potion           +10% health
       PFX_SILVERSHIELD    Silver Shield            +100% Heretic armor (stronger)
       PFX_ENCHANTEDSHIELD Enchanted Shield         +200% Heretic armor (stronger)
       PFX_BAGOFHOLDING    Bag of Holding           No effect yet
       PFX_HMAP            Map Scroll               All of automap revealed
       PFX_TOTALINVIS      Total InvisiSphere       Temporary total invisibility
      -----------------------------------------------------------------------------

Full example:


    # define a couple of sprite-based pickups
    pickupitem FOO2 { effect = PFX_TOTALINVIS }
    pickupitem FOO3 { effect = PFX_LAUNCHER }

    # redefine an existing pickup effect (don't forget free-form syntax...)
    pickupitem FOO3 
    { 
       effect = PFX_ENCHANTEDSHIELD;
    }

[Return to Table of Contents](#contents)

[]{#frames}

------------------------------------------------------------------------

**Frames**

------------------------------------------------------------------------

\
Frames, also known as states, define the animation and logic sequences
for thing types and player weapons.\
\
Each frame must be given a unique mnemonic in its definition\'s header,
and this is the name used to identify the frame elsewhere. Each field in
the frame definition is optional. If a field is not provided, it takes
on the default value indicated below the syntax information. Fields may
also be provided in any order.\
\
If a frame\'s mnemonic is not unique, the latest definition of a frame
with that mnemonic replaces any earlier ones. As of EDF 1.1 (Eternity
Engine v3.31 public beta 4), frame mnemonics are completely
case-insensitive. This means that a frame defined with a mnemonic that
differs from an existing one only by case of letters will overwrite the
original frame.\
\
Note that the order of frame definitions in EDF is not important. For
purposes of DeHackEd and parameterized codepointers, the number used to
access a frame is now defined in the frame itself. For the original
frames, these happen to be the same as their order.\
\
User frames that need to be accessible from DeHackEd or from
parameterized codepointers can define a **unique** DeHackEd number,
which for purposes of forward compatibility, should be greater than or
equal to 10000, and less than 32768. Beginning with EDF 1.6, DeHackEd
numbers will be automatically allocated to frames which are used in misc
or args fields using the \"frame:\" prefix if they have a DeHackEd
number of -1. The previous behavior in this case was to post a warning
message and replace the frame with frame 0. This change makes it
unnecessary in many cases to assign and keep track of DeHackEd numbers
yourself.\
\
Syntax:


    # Remember that all fields are optional and can be in any order.

    frame <unique mnemonic>
    {
       sprite         = <sprite mnemonic>
       spriteframe    = <number> OR <character>
       fullbright     = <boolean>
       tics           = <number>
       action         = <bex codeptr name>
       nextframe      = <frame mnemonic> OR <nextframe specifier>
       misc1          = <special field>
       misc2          = <special field>
       particle_event = <event name>
       args           = { <special field>, ... }
       dehackednum    = <unique number>
       cmp            = <compressed frame definition>
    }

Explanation of fields:

- **sprite**\
  Default = \"BLANK\" (this special value can be used to access the
  blanksprite value)\
  Sets the sprite displayed when this frame is used. Must be a sprite
  mnemonic defined in the **spritenames** list, or the special value
  \"BLANK\".

- **spriteframe**\
  Default = 0\
  Sets the frame of the sprite displayed. A frame value of 0 displays
  the graphic that is named with an A in wads, 1 displays B, and so
  forth. Starting with EDF 1.1, you may also specify a character from A
  to Z or one of \[, \\, or \], matching the lump name.\
  Examples:


          # These are equivalent
          spriteframe = 0
          ....
          spriteframe = A

- **fullbright**\
  Default = false\
  Determines if this frame renders a sprite at fullbright light level.
  Can be set to any of the values yes/no, on/off, or true/false, with
  the obvious meanings.

- **tics**\
  Default = 1\
  Determines the length of time an object remains in this frame. A tic
  is equivalent to 1/35th of a second, so 35 tics is 1 second. A frame
  with 0 tics is passed through instantly. A frame with -1 tics is
  stayed in forever (see nextframe below). This field is not used for
  some player weapon frames.

- **action**\
  Default = \"NULL\"\
  Sets the action to be taken when this frame is entered. This is also
  known as the codepointer, and uses the same mnemonics that are used in
  the [BEX Codepointer Block](dehref.html#codeptr). Also see the
  [Eternity Engine Definitive Codepointer Reference](codeptrs.html) for
  detailed information on what every available codepointer does.

- **nextframe**\
  Default = S_NULL\
  Sets the frame to which the object will transfer when this one has
  been passed. Note that if the frame\'s tics value is -1, this field
  will never be used. This field must be a valid frame mnemonic. It can
  be the name of the same or any other frame.\
  Starting with EDF 1.1, you may also specify one of several special
  reserved words for this field:
  - **\@next** \-- sets nextframe to the next frame defined in EDF
  - **\@prev** \-- sets nextframe to the previous frame defined in EDF
  - **\@this** \-- sets nextframe to the same frame (the one being
    processed)
  - **\@null** \-- sets nextframe to the S_NULL frame

  \
  Note that if \@next or \@prev are used for the last or first frames
  respectively, an error will occur.

- **misc1**\
  Default = \"0\"\
  This is a special field which can accept a wide range of special value
  types. See the information below on special fields.

- **misc2**\
  Default = \"0\"\
  This is a special field which can accept a wide range of special value
  types. See the information below on special fields.

- **particle_event**\
  Default = pevt_none\
  This field lists the name of a particle event that will be triggered
  by an object when it enters this frame. Valid particle event names are
  as follows:


          Effect name           Effect
          --------------------------------------
          pevt_none             No effect
          pevt_rexpl            Rocket Burst
          pevt_bfgexpl          BFG Burst
          

- **args**\
  Default = All values will be set to 0\
  This list will accept up to five special values. If more than five are
  provided, only the first five will be used. If less than 5 are
  provided, any not provided are set to zero. This list corresponds to
  the DeHackEd fields Args1 through Args5, in order. The purpose of this
  list is to serve as a set of arguments to parameterized codepointers.
  See the information on special fields below.

- **dehackednum**\
  Default = -1\
  As explained above, this is the number which must be used to refer to
  this frame in DeHackEd patches and arguments to parameterized
  codepointers. User-defined frames should currently only use DeHackEd
  numbers between 10000 and 32768, or simply use -1 if no DeHackEd
  functionality is required. This number must be unique unless it is -1.
  As of EDF 1.6, if a frame is referred to by name in a misc or args
  field and has a DeHackEd number of -1, it will be automatically
  allocated a unique number by the game engine.

- **cmp**\
  Default = none\
  The cmp field is used to provide a compressed definition for the
  normal fields of this frame. When the cmp field is provided, all other
  fields except **misc1**, **misc2**, **args**, and **dehackednum** will
  be ignored. See the [Compressed Frame Definitions](#cmpframes) section
  below for full information. Note that this field is NOT accepted
  inside framedelta blocks. This field is new to EDF 1.1. The ability to
  specify misc and args fields along with a cmp field is new to EDF
  1.5.\
  Note: When a cmp field is defined, misc and args fields will be
  treated as though they are defined in a **framedelta** block, meaning
  that if the cmp field specifies any misc or args values, the values
  specified in misc and args will overwrite them only if the fields are
  present.

Special Fields:\
\
The special fields misc1 and misc2, as well as all five usable values of
the args list, can take special values of the following type by using
prefix:value syntax as shown:

- **frame:\<frame mnemonic\>**\
  Translated to the DeHackEd number of the indicated frame. If the
  indicated frame has no unique DeHackEd number, one will be assigned to
  it if possible.\
  Example:


           misc1 = frame:S_FOOBAR1
           

- **thing:\<thing mnemonic\>**\
  Translated to the DeHackEd number of the indicated thing type. If the
  indicated thing has no unique DeHackEd number, one will be assigned to
  it if possible.\
  Example:


           misc1 = thing:BaronOfHell
           

- **sound:\<sound mnemonic\>**\
  Translated to the DeHackEd number of the indicated sound. See the
  [Internal Sound List](dehref.html#sndtable) in the Eternity
  DeHackEd/BEX Reference for a list of documented sound mnemonics, or
  view the default sounds.edf file. If the indicated sound has no unique
  DeHackEd number, one will be assigned to it if possible.\
  Example:


           misc2 = sound:pistol
           

- **flags:\<flag list\>**

- **flags2:\<flag list\>**

- **flags3:\<flag list\>**\
  Translated into the corresponding integer value for the appropriate
  thing flags field (needed for some parameterized codepointers). See
  the [Thing Flag List](dehref.html#tngflags) documentation in the
  Eternity DeHackEd/BEX Reference for full information. The format here
  is the same with the exception that if white space or restricted
  characters are put into a flag list, the entire special field value
  MUST be enclosed in quotation marks.\
  Examples:


          args = { 1, flags:SOLID|SHOOTABLE|COUNTKILL }
          
          # this needs quotations here, whereas it doesn't in a BEX file, because both
          # whitespace and + characters aren't allowed in unquoted strings in EDF
          
          misc2 = "flags2:LOGRAV + BOSS"
          

- **bexptr:\<codepointer mnemonic\>**\
  Translated into an internal codepointer table index. Used by
  parameterized codepointers that request another codepointer which they
  will call directly, such as PlayerThunk. The values to which
  codepointer mnemonics are translated are \*not\* published, and could
  change from version to version. Therefore, you \*must\* use EDF for
  these types of codepointers.\
  Example:


          args = { bexptr:Chase }
          

- **string:\<string mnemonic\>**\
  Translated into the corresponding numeric id of the indicated string
  object. If no such string exists, a warning will be issued to the
  verbose EDF logfile and the numeric id of zero will be substituted
  (all facilities that use EDF strings by number are fail-safe, so if
  string zero does not exist, nothing will happen). String mnemonic
  support was new to EDF 1.4, but was accidentally omitted from this
  documentation until EDF 1.5.\
  Example:


          args = { string:CyberMsg }
          

- **No Prefix**\
  If no prefix is given for a value, it is interpreted as an integer
  number via the C function \"strtol\" (meaning it may be decimal,
  octal, or hexadecimal).\
  Examples:


          misc1 = 32768
          misc2 = -45
          args = { 1, 1, 2, 3, 5 }
          

- **Invalid prefixes**\
  If any prefix is used other than the ones listed here, an error will
  occur.

Restrictions and Caveats:

- At least one frame must be defined.
- The S_NULL frame must be defined (it may be the only frame, but it
  must exist).
- All frames must have a unique mnemonic no longer than 40 characters.
  Frame mnemonics are case-insensitive. They should only contain
  alphanumeric characters and underscores. All other punctuation marks
  are explicity reserved for extended syntactical use. Length will be
  verified, but format will not (non-unique mnemonics will overwrite
  previous definitions, see below).
- All frames with a DeHackEd number not equal to -1 must have a unique
  DeHackEd number. This will be verified.

Replacing Existing Frames:\
\
To replace the values of an existing frame, simply define a new frame
with the exact same mnemonic value (as stated above, all frames need a
unique mnemonic, so duplicate mnemonics serve to indicate that an
existing frame should be replaced). EDF 1.1 also adds delta structures,
which allow fields of existing frames to be edited without replacing all
values in those frames. Delta structures also cascade, and are thus
useful for layering of different modifications. See the section [Delta
Structures](#delta) for full information.\
\
Full Example:


    # define some new frames
    frame BLAH1
    {
       sprite = FOO1
       fullbright = true
       tics = 12
       nextframe = BLAH2
    }

    frame BLAH2
    {
       sprite = FOO1
       tics = 12
       nextframe = BLAH1
    }

    # Overwrite an existing frame (since BLAH1 is already defined above...)
    # Also remember that mnemonics are case-insensitive!
    frame blah1
    {
       sprite = FOO1
       fullbright = true
       tics = 12
       action = HticExplode
       args = { 1 }
       nextframe = S_NULL
    }

[Return to Table of Contents](#contents)

[]{#cmpframes}

------------------------------------------------------------------------

**Compressed Frame Definitions**

------------------------------------------------------------------------

\
Compressed frame definitions are a new feature in EDF 1.1 that allows a
shorthand syntax for specifying the fields of a frame block.\
\
A compressed frame definition resides inside a normal EDF frame
definition, using the **cmp** keyword outlined in the syntax of the
**[frame](#frames)** block. It accepts a single string value with an
extended syntax, and the value of all other fields in the frame block
except for the **dehackednum** can be set via this string. When the cmp
field is present in a frame block, all other fields except **misc1**,
**misc2**, **args**, and **dehackednum** are ignored.\
\
Note that the **cmp** field is NOT accepted inside framedelta blocks.\
\
As of Eternity Engine v3.31 Delta, the format of cmp fields has been
relaxed to allow arbitrary whitespace, quoted values, and line
continuation. Line continuation was discussed in the [General
Syntax](#gensyn) section, but the new changes to the cmp field are
discussed below. Note that all existing cmp strings are still accepted
as valid; the new features simply allow for greater flexibility.\
\
Syntax:


    frame
    {
       cmp = <compressed frame definition>
    }

The compressed frame definition string has the following syntax:


    "sprite|spriteframe|fullbright|tics|action|nextframe|particle_event|misc1|misc2|args1-5"

Fields MUST be provided in the order given, but any number of fields can
be left off at the end of the definition, and those fields will receive
their normal default values. To let an internal field default, you must
use the special reserved value \"\*\" for that field.\
\
Each field is delimited by pipe or, as of EDF v1.4, comma characters,
and field values may NOT contain pipe or comma characters, unless,
starting with EDF v1.3, the entire field value is enclosed in single or
escaped double quotation marks. Beginning with EDF v1.3, extraneous
whitespace IS allowed between field delimiters and their values. Only
whitespace which is enclosed within quotation marks will be interpreted
as part of a field\'s value, so if BEX flags strings are to use pipes,
commas, or spaces to separate flags, they must be enclosed with
quotations. When using quoted values, the beginning and end quotation
mark type must match (ie, if it starts with \', it must end with \', and
if it starts with \\\", it must end with \\\").\
\
The fullbright field has a special caveat within compressed frame
definitions. Any value beginning with the letter T (small or capital)
will be interpreted to mean \"true\", and any other value, including the
default, will be interpreted to mean \"false\".\
\
Otherwise, all fields can accept exactly the same types and ranges of
values that they normally accept when specified separately within a
frame block. This includes letters and numbers for the spriteframe
field, special keywords for the nextframe field, and prefix:value syntax
for misc1, misc2, and the five args fields.\
\
**Examples:**


    # this is a compressed frame definition which specifies ALL the values

    frame FOOBAR1 { cmp = "AAAA|A|F|6|Look|@next|pevt_none|0|0|0|0|0|0|0" }

    # this is an equivalent frame definition, using default specifiers and
    # leaving off defaulting fields at the end of the definition. This frame
    # and the above mean the EXACT same thing.

    frame FOOBAR1 { cmp = "AAAA|*|*|6|Look|@next" }

    # here is a complicated example using a parameterized codepointer

    #                                                          Args start here
    #                                                          |
    #                                                          V
    frame FOOBAR2 { cmp = "TROO|6|*|6|MissileAttack|@next|*|*|*|thing:DoomImpShot|*|*|20" }

**Bad Frame Examples:**\
\
Some illegal compressed frame definitions would include the following.
Don\'t make these mistakes:


    # This is wrong, field values may NOT be empty -- use "*" to indicate a default.

    frame BAD1 { cmp = "AAAA|||6|Look|@next" }

    # This is wrong because BEX flag fields CANNOT use '|' or ',' inside compressed frames, unless
    # they are surrounded by escaped quotation marks.

    frame BAD3 { cmp = "AAAA|B|T|23|SomePointer|*|*|*|flags:SOLID|SHOOTABLE|COUNTKILL" }

    # This is wrong because you cannot leave off fields at the beginning.

    frame BAD4 { cmp = "Look|@next" }

    # This is wrong because fields must be in the correct order.

    frame BAD5 { cmp = "6|TROO|*|6|thing:DoomImp|@next|MissileAttack|*|20" }

**Examples Using EDF 1.3 and Later Extended Syntax:**\
\
The following frames would have been invalid prior to EDF v1.3, but are
now accepted due to the new relaxation of the cmp field format:


    # Arbitrary whitespace is now allowed, and is ignored outside of quoted values:

    frame NOWGOOD1 { cmp = "AAAA | * | * | 6 | Look | @next" }

    # Values may now be quoted using single or escaped double quotation marks so 
    # that they can contain normally ignored or reserved characters.
    # This allows BEX flags strings to contain pipes, commas, and/or spaces:

    frame NOWGOOD2 { cmp = "AAAA | * | * | 6 | Look | @next | * | 'flags:SHOOTABLE | SOLID | COUNTKILL'" }
    frame NOWGOOD3 { cmp = "AAAA | * | * | 6 | Look | @prev | * | \"flags:SHOOTABLE | SOLID | COUNTKILL\"" }

    # EDF line continuation may be used inside any quoted string, including cmp field values.
    # This frame is completely equivalent to the first example above:

    frame NOWGOOD4 
    { 
       cmp = "AAAA | *    | *    | \
              6    | Look | @next" 
    }

    # As of EDF v1.4, you can use commas instead of pipes as delimiters. This was added
    # by user request because it makes some definitions more readable. When using commas, the
    # entire cmp field must be quoted without exception, since commas cannot be used in 
    # unquoted strings.

    frame NOWGOOD5 { cmp = "AAAA, *, *, 6, Look, @next" }

[Return to Table of Contents](#contents)

[]{#things}

------------------------------------------------------------------------

**Thing Types**

------------------------------------------------------------------------

\
Thing types define monsters, lamps, control points, items, etc \--
anything that moves, occupies space, can display a sprite, or is useful
for singling out locations.\
\
Each thing type must be given a unique mnemonic in its definition\'s
header, and this is the name used to identify the thing type elsewhere.
Each field in the thing type definition is optional. If a field is not
provided, it takes on the default value indicated below the syntax
information. Fields may also be provided in any order.\
\
If a thing type\'s mnemonic is not unique, the latest definition of a
thing type with that mnemonic replaces any earlier ones. See the
information below for more on this. As of EDF 1.1 (Eternity Engine v3.31
public beta 4), thing type mnemonics are completely case-insensitive.
This means that a thing type defined with a mnemonic that differs from
an existing one only by case of letters will overwrite the original
type.\
\
Note that the order of thing type definitions in EDF is not important.
For purposes of DeHackEd, the number used to access a thing type is now
defined in the type itself. For the original thing types, these happen
to be the same as their order.\
\
User thing types that need to be accessible from DeHackEd or by
parameterized codepointers can define a **unique** DeHackEd number,
which for purposes of forward compatibility, should be greater than or
equal to 10000 and less than 32768. User types not needing access via
DeHackEd or by parameterized codepointers can be given a DeHackEd number
of -1. Beginning with EDF 1.6, if a thingtype with a DeHackEd number of
-1 is used by name in a misc or args field using the \"thing:\" prefix,
it will be automatically assigned a unique DeHackEd number by the game
engine. The previous behavior was to post a warning message and replace
the thing with the \"Unknown\" type. This largely removes the need for
users to assign and keep track of DeHackEd numbers.\
\
Syntax:


    # Remember that all fields are optional and can be in any order.

    thingtype <unique mnemonic>
    {
       inherits        = <thing type mnemonic>
       basictype       = <basic type name>
       doomednum       = <number>
       spawnstate      = <frame mnemonic>
       spawnhealth     = <number>
       seestate        = <frame mnemonic>
       seesound        = <sound mnemonic>
       reactiontime    = <number>
       attacksound     = <sound mnemonic>
       painstate       = <frame mnemonic>
       painchance      = <number>
       painsound       = <sound mnemonic>
       meleestate      = <frame mnemonic>
       missilestate    = <frame mnemonic>
       crashstate      = <frame mnemonic>
       deathstate      = <frame mnemonic>
       xdeathstate     = <frame mnemonic>
       deathsound      = <sound mnemonic>
       speed           = <number> OR <floating-point number>
       radius          = <floating-point number>
       height          = <floating-point number>
       correct_height  = <floating-point number>
       mass            = <number>
       damage          = <number>
       topdamage       = <number>
       topdamagemask   = <number>
       activesound     = <sound mnemonic>
       flags           = <flag list>
       flags2          = <flag list>
       flags3          = <flag list>
       cflags          = <flag list>
       addflags        = <flag list>
       remflags        = <flag list>
       raisestate      = <frame mnemonic>
       translucency    = <number> OR <percentage>
       bloodcolor      = <number>
       fastspeed       = <number> OR <floating-point number>
       nukespecial     = <BEX codepointer mnemonic>
       particlefx      = <particle effect flag list>
       droptype        = <thing type mnemonic>
       mod             = <MOD name> OR <number>
       obituary_normal = <string>
       obituary_melee  = <string>
       translation     = <number> OR <translation table lump name>
       dmgspecial      = <dmgspecial name>
       skinsprite      = <sprite mnemonic>
       dehackednum     = <unique number>
    }

Explanation of fields:

- **inherits**\
  No default value\
  Sets the thing type from which this type will inherit values. See the
  [Thing Type Inheritance](#inherit) section below for full information
  on how thing type inheritance works.

- **basictype**\
  No default value\
  Sets the basic type of this thingtype definition. Basic types are not
  thingtypes, but are engine-defined sets of flags and states which an
  object can adopt as its default values for the corresponding flag and
  state fields. Use of basictypes is encouraged because thingtypes using
  them can automatically adapt to occasionally necessary changes to
  monsters and other thing categories that involve the addition of new
  thingtype flag values. This is a table of all currently existing
  basictypes and the values that they impart upon thingtypes:
  - Monster\
    Purpose: An ordinary walking monster with no fancy features.\
    cflags =
    SOLID\|SHOOTABLE\|COUNTKILL\|FOOTCLIP\|SPACMONSTER\|PASSMOBJ\
    \
  - FlyingMonster\
    Purpose: An ordinary flying monster.\
    cflags =
    SOLID\|SHOOTABLE\|COUNTKILL\|NOGRAVITY\|FLOAT\|SPACMONSTER\|PASSMOBJ\
    \
  - FriendlyHelper\
    Purpose: A player helper with maximum friendliness options.\
    cflags =
    SOLID\|SHOOTABLE\|COUNTKILL\|FRIEND\|JUMPDOWN\|FOOTCLIP\|WINDTHRUST\|SUPERFRIEND\|SPACMONSTER\|PASSMOBJ\
    \
  - Projectile\
    Purpose: A standard projectile.\
    cflags = NOBLOCKMAP\|NOGRAVITY\|DROPOFF\|MISSILE\|NOCROSS\
    \
  - PlayerProjectile\
    Purpose: A projectile for use by players.\
    cflags =
    NOBLOCKMAP\|NOGRAVITY\|DROPOFF\|MISSILE\|NOCROSS\|SPACMISSILE\
    \
  - Seeker\
    Purpose: A missile prepared to be fired by homing missile
    codepointers.\
    cflags =
    NOBLOCKMAP\|NOGRAVITY\|DROPOFF\|MISSILE\|NOCROSS\|SEEKERMISSILE\
    \
  - SolidDecor\
    Purpose: A solid decorative item.\
    cflags = SOLID\
    \
  - HangingDecor\
    Purpose: A hanging decorative item that is passable even without 3D
    object clipping.\
    cflags = SPAWNCEILING\|NOGRAVITY\
    \
  - SolidHangingDecor\
    Purpose: A solid hanging decorative item.\
    cflags = SOLID\|SPAWNCEILING\|NOGRAVITY\
    \
  - ShootableDecor\
    Purpose: A shootable decorative item.\
    cflags = SOLID\|SHOOTABLE\|NOBLOOD\
    \
  - Fog\
    Purpose: A fog item such as telefog or item fog.\
    cflags = NOBLOCKMAP\|NOGRAVITY\|TRANSLUCENT\|NOSPLASH\
    \
  - Item\
    Purpose: A collectable item. Doesn\'t count.\
    cflags = SPECIAL\
    \
  - ItemCount\
    Purpose: A collectable item. Counts for item %.\
    cflags = SPECIAL\|COUNTITEM\
    \
  - TerrainBase\
    Purpose: A TerrainTypes base item.\
    cflags = NOBLOCKMAP\|NOGRAVITY\|NOSPLASH\
    \
  - TerrainChunk\
    Purpose: A TerrainTypes chunk item.\
    cflags =
    NOBLOCKMAP\|DROPOFF\|MISSILE\|LOGRAV\|NOSPLASH\|NOCROSS\|CANNOTPUSH\
    \
  - ControlPoint\
    Purpose: An inert control point.\
    cflags = NOBLOCKMAP\|NOSECTOR\|NOGRAVITY\
    spawnstate = S_TNT1\
    \
  - ControlPointGrav\
    Purpose: An inert control point that is subject to gravity.\
    cflags = DONTDRAW\|NOSPLASH\
    spawnstate = S_TNT1

  New to EDF 1.7.

- **doomednum**\
  Default = -1\
  Sets the ID used to identify this thing type in maps. For example,
  when a map calls for a thing with ID 2003, the game engine will find
  the thing type which has that number in this field. Every thing that
  will be spawned on maps must have a unique doomednum. Things which are
  not spawned on maps, such as fireballs, can be given the default
  doomednum of -1.\
  For purposes of forward compatibility, user-defined thing types should
  use doomednums greater than or equal to 20000 and less than 32768. All
  other doomednum ranges are considered to be reserved as outlined here.
  The purposes of various blocks may be relaxed in the future for
  purposes of internal use, so these are only rough guidelines provided
  for completeness.


           Doomednum Range     Usage
          ---------------------------------------------------------------
              0                No-op, no thing will be spawned at all
              1 -  4999        Original DOOM things and some extensions
           5000 -  5999        BOOM-style control points
           6000 -  6999        Reserved for port use
           7000 -  7999        Things translated from other games
           8000 -  8999        Reserved for port use
           9000 -  9999        zdoom-style control points
          10000 - 19999        Reserved for port use
          20000 - 32767        Designated for custom EDF thingtypes

- **spawnstate**\
  Default = S_NULL\
  Sets the frame which a thing starts in when it is spawned. Monsters
  generally use this frame to stand still and look for targets.

- **spawnhealth**\
  Default = 1000\
  Sets a thing\'s maximum amount of life. It is only used for things
  which can be shot.

- **seestate**\
  Default = S_NULL\
  Sets the frame which a monster will jump to when it sees or hears a
  target. Also called walking frames, or first moving frame.

- **seesound**\
  Default = none\
  Sets the sound that a monster will play when it awakens. This sound is
  also played by missiles when they are shot. (Note: \"none\" is a
  special mnemonic for no sound)

- **reactiontime**\
  Default = 8\
  Sets an amount of time that a thing must wait under certain
  circumstances. Monsters use this value as a means to control their
  rate of attack. This value is also used as an internal field for
  varying purposes, but those uses are not editable.

- **attacksound**\
  Default = none\
  Sets a sound used by some monster attacks, including the Lost Soul\'s
  charge.

- **painstate**\
  Default = S_NULL\
  Sets the frame a thing may randomly enter when it has been injured.

- **painchance**\
  Default = 0\
  Sets the chance out of 255 that this thing will enter its painstate
  when it is injured. A thing with 0 painchance will never feel pain,
  whereas a thing with 255 painchance will always feel pain. If a thing
  has a painchance greater than zero, it should have a valid painstate,
  otherwise it will disappear forever.

- **painsound**\
  Default = none\
  Sets the sound played by the Pain codepointer when this thing uses it.
  Monsters and the player always call Pain from their painstate.

- **meleestate**\
  Default = S_NULL\
  If this frame is not S_NULL, monsters chasing a target will attempt to
  close to melee range quickly. When they determine they are within 64
  units of their target, they may randomly enter this frame to perform a
  melee attack.

- **missilestate**\
  Default = S_NULL\
  If this frame is not S_NULL, monsters chasing a target will randomly
  enter it to perform a missile attack.

- **crashstate**\
  Default = S_NULL\
  When a thing with this state dies and then subsequently hits the
  ground, it will enter this state. If this state S_NULL, it is not
  used. New to EDF 1.3.

- **deathstate**\
  Default = S_NULL\
  When a thing dies normally, it will enter this state. If the state is
  S_NULL, the thing will disappear forever.

- **xdeathstate**\
  Default = S_NULL\
  If a thing dies by taking enough damage that its health is equal to
  its negated spawnhealth value, it will enter this frame instead of its
  deathstate. This is also known as gibbing, or extreme death. If this
  frame is S_NULL, the thing will always die normally.

- **deathsound**\
  Default = none\
  Sound played by the Scream codepointer. Used to play monster death
  sounds.

- **speed**\
  Default = 0\
  Value used by monsters as a movement delta per walking frame. The
  monster will move by an amount relative to this value each time it
  uses the Chase codepointer. For missiles, this value is a fixed-point
  number (multiplied by 65536), and represents a constant speed by which
  the missile advances every game tic. Missile speed is not affected by
  the missile frame durations, whereas monster walking speed is.\
  Starting with EDF 1.1, this field also supports floating-point values.
  When a floating-point value is provided, it will be translated into
  the corresponding fixed-point value. This means that a value of 10.0
  is equivalent to the integer value 655360 (10\*65536). It is NOT
  equivalent to 10. This distinction is very important. Small integer
  speed values are appropriate for monsters, whereas floating-point
  values will be suitable only for missiles.

- **radius**\
  Default = 20.0\
  Floating-point value that represents the radius of this thing type. A
  thing can fit into corridors which are at least as wide as this value
  times two, plus one unit (ie a thing with radius 32 can fit into a
  65-wide hallway). The maximum value this field \*should\* have is 64.0
  units. However, the game breaks its own rule here, giving several
  monsters radii up to 128 units. These monsters, which include the
  Mancubus, Arachnotron, and Spider Demon, exhibit clipping errors which
  enable other things to walk into them, and which can cause some moving
  sectors to malfunction. Avoid giving things radii larger than 64.0 to
  remain absolutely safe.

- **height**\
  Default = 16.0\
  Floating-point value that represents the height of this thing type. A
  thing can fit into areas which are of this exact height or taller.
  Note that in DOOM, this value is only used to see where a monster can
  fit relative to walls and floor/ceiling heights. For purposes of
  clipping against other things, monsters were considered to be
  infinitely tall. However, Eternity\'s extended 3D object clipping
  enables this field to also be used to allow or disallow things to pass
  over, under, or stand on this thing.

- **correct_height**\
  Default = 0.0\
  Floating-point value that represents a height for this thing type
  which is corrected for 3D object clipping. When 3D object clipping is
  enabled and the comp_theights variable is set to off, objects which
  have a non-zero correct_height field will begin to use this value for
  their height instead of the normal height field. If an object has the
  3DDECORATION flags3 bit set, it will still clip missiles at its
  original height instead of at this value, so that playability of old
  maps is not altered. This field will generally only be necessary for
  old DOOM objects, which have now been assigned correct_height values
  in the default things.edf module. Use for new objects is discouraged;
  instead, set the **height** field above to the proper value. New to
  EDF 1.5.

- **mass**\
  Default = 100\
  Normal integer value that serves as a mass factor for this object.
  Mass affects the amount of thrust an object is given when it is
  damaged, the amount of effect an archvile\'s attack can have on it,
  and for bouncing objects, the rate at which they fall. Objects with a
  mass less than 10 will cause small terrain hits when landing on
  terrain that supports small splashes.

- **damage**\
  Default = 0\
  This number is used as a damage multiplier when a missile hits a
  thing. The damage formula used is:


              damage = ((rnd % 8) + 1) * missiledamage
          

  This field is also used as a parameter by some new, parameterized
  codepointers, including BetaSkullAttack and Detonate.

- **topdamage**\
  Default = 0\
  This number is a constant amount of damage inflicted on any shootable
  object that stands on top of this object when 3D object clipping is
  active. This is useful for objects such as torches and flaming
  barrels. The frequency of damage is determined by the topdamagemask
  field. New to EDF 1.7.

- **topdamagemask**\
  Default = 0\
  This number determines how often a thing with a non-zero topdamage
  field burns objects that are standing on top of it. This number should
  be a power of two minus 1 to work properly, such as 1, 3, 7, 15, or 31
  (31 would be approximately once per second and is used by DOOM\'s
  nukage sector types). New to EDF 1.7.

- **activesound**\
  Default = none\
  Sets the sound used by a thing when it is wandering around. A thing is
  given a 3 out of 256 chance (1.17%) of making this sound every time it
  calls the Chase codepointer. There are some new flags3 flags which can
  alter the behavior of this sound.

- **flags**\
  Default = \"\"\
  This field controls a number of thing type properties using bit flags,
  where each bit in the field stands for some effect, and has the value
  of being either on or off. See the [Thing Flag
  List](dehref.html#tngflags) documentation in the Eternity DeHackEd/BEX
  Reference, and also see the [Bits Mnemonics](dehref.html#tngbits)
  section for the values which can be used in this field. Remember that
  if whitespace or disallowed characters are used, this field\'s value
  must be enclosed in quotation marks.

- **flags2**\
  Default = \"\"\
  Similar to flags but takes a different set of values with different
  meanings. See the [Thing Flag List](dehref.html#tngflags)
  documentation in the Eternity DeHackEd/BEX Reference, and also see the
  [Bits2 Mnemonics](dehref.html#tngbits2) section for the values which
  can be used in this field. Remember that if whitespace or disallowed
  characters are used, this field\'s value must be enclosed in quotation
  marks.

- **flags3**\
  Default = \"\"\
  Similar to flags, but takes a different set of values with different
  meanings. See the [Thing Flag List](dehref.html#tngflags)
  documentation in the Eternity DeHackEd/BEX Reference, and also see the
  [Bits3 Mnemonics](dehref.html#tngbits3) section for the values which
  can be used in this field. Remember that if whitespace or disallowed
  characters are used, this field\'s value must be enclosed in quotation
  marks.

- **cflags**\
  Default = \"\"\
  New to EDF 1.2, this field allows specification of all the thing\'s
  flag values in one single flag list. When this field is defined, the 3
  normal flags fields above are ignored. You can use one or the other in
  any definition, but not both in the same definition. The only caveat
  to this field is that the **flags** bit \"SLIDE\", which has no
  effect, cannot be specified in this field. If SLIDE is used, it will
  activate the flags3 bit with the same name, which has the expected
  effect. All other flags will be assigned to their appropriate internal
  fields as normal. See the DeHackEd documentation listed in the fields
  above for full information on flag list format and flag mnemonics.

- **addflags**\
  Default = \"\"\
  New to EDF 1.2, this field specifies a combined list of flags to be
  added to the thing\'s existing flags values (this works the same as
  the **cflags** field above). Flags in this list are \"turned on\" in
  the thing. If flags are listed here which are already \"on,\" there is
  no effect on those flags. This field is most useful within
  **thingdelta** sections and in concert with thing type inheritance.

- **remflags**\
  Default = \"\"\
  New to EDF 1.2, this field specifies a combined list of flags to be
  subtracted from the thing\'s existing flags values (this works the
  same as the **cflags** field above). Flags in this list are \"turned
  off\" in the thing, as if they had never been listed. If flags are
  listed here which are not already \"on,\" there is no effect on those
  flags. This field is most useful within **thingdelta** sections and in
  concert with thing type inheritance. This field will be applied
  \*AFTER\* the addflags field is applied, so it could potentially turn
  off flags turned on by that field.

- **raisestate**\
  Default = S_NULL\
  If this frame is not S_NULL, an Archvile can perform a resurrection on
  this creature. When this occurs, the creature will enter this frame.

- **translucency**\
  Default = 65536 (100%)\
  This field allows fine, customizable control over a thing\'s
  translucency. This field accepts two types of values. First, it may be
  given an integer value in the range of 0 to 65536, with 0 being
  completely invisible, and 65536 being normal. Alternatively, beginning
  with EDF 1.3, you can provide a percentage value to this field
  indicating the amount of the foreground sprite that is blended with
  the background. A percentage value must be a base 10 integer between 0
  and 100, with the final digit followed immediately by a \'%\'
  character (percent sign).\
  \
  Note this effect is mutually exclusive of BOOM-style translucency. If
  this value is not 65536 (or 100%), and the BOOM translucency flag is
  turned on for the same thing type, the flag will be turned off at
  run-time, and this field will take precedence.

- **bloodcolor**\
  Default = 0\
  Allows this thing type to have differently colored blood when particle
  blood effects are enabled (this does not currently have any effect on
  blood sprites). Currently available blood color values are as follows:


           Blood color       Number
          --------------------------
           Red (normal)        0
           Grey                1
           Green               2
           Blue                3
           Yellow              4
           Black               5
           Purple              6
           White               7
           Orange              8
          --------------------------
          

- **fastspeed**\
  Default = 0\
  When the game is switched into Nightmare difficulty or -fast mode is
  active, all thing types with a non-zero fastspeed will have their
  speed fields changed to their fastspeed value. This enables editing of
  the -fast speed of projectiles and monsters. For detailed information
  on speed, see the speed field itself above. Note: as of EDF 1.2, this
  field also supports floating-point numbers, with the same meaning as
  that for the speed field.

- **nukespecial**\
  Default = NULL\
  This field is the name of a special codepointer to call when the
  player uses the KILLEM cheat to kill all monsters. Currently only two
  special pointers are provided for this purpose, PainNukeSpec and
  SorcNukeSpec. These enable monsters with spawn-on-death actions to
  either spawn enemies early, or not at all. See the [Eternity Engine
  Definitive Codepointer Reference](codeptrs.html) for detailed
  information.

- **particlefx**\
  Default = 0\
  This field is a flag field with syntax identical to the flags, flags2,
  and flags3 fields, but which accepts the following values which
  control what persistent particle effects are associated with objects
  of this type:


           Flag name       Hex Value     Effect
          --------------------------------------------------------
           ROCKET          0x00000001    Rocket trail
           GRENADE         0x00000002    Grenade trail
           FLIES           0x00000004    Swarm of flies w/ sound
           BFG             0x00000008    BFG particle swarm
           FLIESONDEATH    0x00000010    Flies when object dies
           DRIP            0x00000020    Parameterized drip effect
           REDFOUNTAIN     0x00010000    Red fountain
           GREENFOUNTAIN   0x00020000    Green fountain
           BLUEFOUNTAIN    0x00030000    Blue fountain
           YELLOWFOUNTAIN  0x00040000    Yellow fountain
           PURPLEFOUNTAIN  0x00050000    Purple fountain
           BLACKFOUNTAIN   0x00060000    Black fountain
           WHITEFOUNTAIN   0x00070000    White fountain
          --------------------------------------------------------
          

  Note that the fountain flag values cannot be combined with each other.
  Doing so will simply result in another one of the existing fountain
  colors.\
  \
  The DRIP effect requires parameters to be specified in a thing\'s
  ExtraData **mapthing** block. The parameters to this effect are as
  follows:

  - args 0: color (palette index from 0 to 255)
  - args 1: particle width/height in pixels
  - args 2: frequency in gametics
  - args 3: if 1, particles cause terrain hits
  - args 4: if 1, particles are fullbright

  Examples:


          # a single effect
          particlefx = ROCKET
          
          # multiple effects
          particlefx = BFG|GREENFOUNTAIN
          
          # if there are spaces or disallowed characters, you must use quotations
          particlefx = "BFG + GREENFOUNTAIN"
          

- **droptype**\
  Default = NONE\
  This field allows the type of thing that is dropped by this thing when
  it dies to be edited. This field should either be a valid thing type
  mnemonic, or the special value NONE.

- **mod**\
  Default = UNKNOWN (0)\
  Special Means of Death flag for this thing type. Currently supported
  values follow below. The use of numbers in this field is now
  considered deprecated. Please use the descriptive textual MOD names in
  all new EDF projects.


          Mod Name    Number
          ------------------
          UNKNOWN       0
          FIST          1
          PISTOL        2
          SHOTGUN       3
          CHAINGUN      4
          ROCKET        5
          R_SPLASH      6
          PLASMA        7
          BFG           8
          BFG_SPLASH    9
          CHAINSAW      10
          SSHOTGUN      11
          SLIME         12
          LAVA          13
          CRUSH         14
          TELEFRAG      15
          FALLING       16
          SUICIDE       17
          BARREL        18
          SPLASH        19
          HIT           20
          BFG11K_SPLASH 21
          BETABFG       22
          BFGBURST      23
          PLAYERMISC    24
          GRENADE       25
          

  Specifying this field is not necessary for most thing types. It IS
  highly useful for controlling the obituaries triggered by player
  projectiles, however. Some of the values above are not fully supported
  yet. Eventually the LAVA type will cause fire death animations for
  things which define them.

- **obituary_normal**\
  Default = empty\
  Defines the obituary seen by players when killed by a projectile or
  other miscellaneous attack by a monster. If the obituary is not
  defined for this type, it will simply read that the player died. Note
  that you are only allowed to specify an obituary starting immediately
  after the player\'s name. If you want an obituary to say \"Player was
  flamed\", then simply make the obituary read \"was flamed\".

- **obituary_melee**\
  Default = empty\
  Defines the obituary seen by players when killed by a monster\'s melee
  attack. If the obituary is not defined for this type, it will simply
  read that the player died. Note that you are only allowed to specify
  an obituary starting immediately after the player\'s name. If you want
  an obituary to say \"Player was stabbed mercilessly\", then simply
  make the obituary read \"was stabbed mercilessly\".

- **translation**\
  Default = 0\
  Sets a translation for the thing\'s sprite. Translation tables are
  256-byte lumps which can be used to remap any range of colors in a
  sprite. When this field defaults, no translation will be used.
  Otherwise, you may either provide a number to access one of the
  internal player translation tables, or you may provide the name of a
  translation lump, which must be found between T_START and T_END
  markers amongst one of the currently loaded wad files. Numeric values
  for player translations are as follows. All of the player translations
  remap the pure green range.


          Number     Color
          ------------------
           1        Indigo
           2        Brown
           3        Red
           4        Tomato
           5        Dirt
           6        Blue
           7        Gold
           8        Felt
           9        Black
           10       Purple
           11       "Vomit"
           12       Pink
           13       Cream
           14       Yellow
          ------------------
          

- **dmgspecial**\
  Default = \"none\"\
  The dmgspecial field allows specification of a special action that
  will be taken when this object is involved in damaging another object
  as the inflictor (an inflictor is the object actually doing the
  damage, and not necessarily the object that is blamed for the damage).
  Supported values and their meanings are as follows:\
  \
  - none\
    This is the default value. No special action is taken; the object is
    normal when damaging other things.\
    \
  - MinotaurCharge\
    If the thing is flying like a skull (has flags bit SKULLFLY set), it
    will inflict a large amount of thrust on the target along the vector
    of the impact, will hit it directly for ((rnd % 8) + 1) \* 6 damage,
    and if the target is a player, the player\'s controls will be frozen
    for 14 + (rnd % 8) gametics. When the thing is not in SKULLFLY mode,
    normal damage is done by all attacks.\
    \
  - Whirlwind\
    The target will have its angle and x/y momenta modified by random
    amounts. If the current level time has the 5th bit (value 16) set
    and the target is not a boss (possesses the BOSS flag), the target
    will be given randomized z momentum up to but no greater than 12
    units per tic. When the level time is divisible by 8, the target
    will be hit directly for 3 damage.

  \

- **skinsprite**\
  Default = \"noskin\"\
  The skinsprite field defines a sprite that will be used to override
  the sprite value in any frame a thing of this type enters. This allows
  the creation of thingtypes which behave identically but look different
  without the creation of any new frames. Note that when things are
  crushed into gibs, the skinsprite is cleared at that time. If an
  Archvile resurrects such a thing, the proper skinsprite is then
  restored.

- **dehackednum**\
  Default = -1\
  As explained above, this is the number which must be used to refer to
  this thing type in DeHackEd patches. -1 means that the thing type is
  not accessible from DeHackEd. User-defined thing types should
  currently only use DeHackEd numbers between 10000 and 32768, or simply
  use -1 if no DeHackEd functionality is required. This number must be
  unique unless it is -1. As of EDF 1.6, if a thing with a DeHackEd
  number of -1 is used in a misc or args field, it will be automatically
  allocated a unique number by the game engine.

Restrictions and Caveats:

- At least two thing types must be defined (Unknown and DoomPlayer).
- The Unknown thing type must be defined.
- The DoomPlayer thing type must currently be defined (subject to
  change).
- All thing types must have a unique mnemonic no longer than 40
  characters. They should only contain alphanumeric characters and
  underscores. All other punctuation marks are explicity reserved for
  extended syntactical use. Length will be verified, but format will not
  (non-unique mnemonics will overwrite previous definitions, see below).
- All thing types with a DeHackEd number not equal to -1 must have a
  unique DeHackEd number. This will be verified.

Replacing Existing Thing Types:\
\
To replace the values of an existing thing type, simply define a new
thing type with the exact same mnemonic value (as stated above, all
thing types need a unique mnemonic, so duplicate mnemonics serve to
indicate that an existing thing type should be replaced).\
\
EDF 1.1 also adds delta structures, which allow fields of existing
things to be edited without replacing all values in those things. Delta
structures also cascade, and are thus useful for layering of different
modifications. See the section [Delta Structures](#delta) for full
information.\
\
Full Example:


    # define a new thing type
    thingtype ScarletPimpernel
    {
      doomednum = 15000
      spawnstate = S_SCARPIMP1
      flags = SPECIAL|NOTDMATCH
      dehackednum = 10000
    }

    # overwrite an existing thing type (assume BaronOfHell is already defined normally...)
    # everything is the same as the original except the fields I changed...
    thingtype BaronOfHell
    {
      doomednum = 3003
      spawnstate = S_BOSS_STND
      seestate = S_BOSS_RUN1
      seesound = brssit
      painstate = S_BOSS_PAIN
      painchance = 10                  # changed from 50 to 10
      painsound = dmpain
      meleestate = S_BOSS_ATK1
      missilestate = S_BOSS_ATK1
      deathstate = S_BOSS_DIE1
      deathsound = brsdth
      speed = 12                       # changed to from 8 to 12
      radius = 24.0000
      height = 64.0000
      mass = 1000
      activesound = dmact
      flags = SOLID|SHOOTABLE|COUNTKILL
      flags2 = E1M8BOSS|FOOTCLIP
      raisestate = S_BOSS_RAISE1
      obituary_normal = "was burned by a baron"
      obituary_melee  = "was ripped open by a baron"
      dehackednum = 16
    }

[Return to Table of Contents](#contents)

[]{#inherit}

------------------------------------------------------------------------

**Thing Type Inheritance**

------------------------------------------------------------------------

\
New to EDF 1.2, thing type inheritance allows you to derive new thing
types from existing ones, so that there is no need to duplicate all the
fields in the original type. To activate inheritance for a thing type,
use the following syntax:


    thingtype <unique mnemonic>
    {
       inherits = <thing type mnemonic>
       <any other thingtype fields>
    }

When the **inherits** field is set in a thing type, its parent type will
be processed first if it has not already been processed. Then, all the
fields from the parent type, except for **dehackednum** and
**doomednum**, will be copied from the parent type to this type. Once
the parent is copied, any other fields in this type will be treated as a
**thingdelta** section, such that only fields explicitly provided will
overwrite values from the parent thing type. Defaults will not be
applied for unlisted fields, again with the exceptions of
**dehackednum** and **doomednum**.\
\
Thing delta sections are applied to thing types after initial processing
is finished, and thus do not affect the inheritance process. If a thing
delta section is applied to a parent type, it will not affect the child
type. Thing deltas applied to child types apply after both inheritance
and any values overridden in the child type.\
\
Restrictions and Caveats:

- A thing type may not inherit from itself, or from other thing types in
  a cycle. Circular inheritance errors will be caught by the processor
  and an error will occur, although possibly not during processing of
  the first inheriting type (the error may be found while processing a
  parent type).
- Only one thing type may be inherited from by each thing type, so there
  is no concept of multiple inheritance. However, there is no limit to
  the number of levels of inheritance other than the number of existing
  thing types, so each parent thing type may have its own parent,
  excepting the ultimate parent in an inheritance tree.

Inheritance Example:


    /*
       As you can see here, thing type inheritance allows inheriting type 
       definitions to be minimized to only those fields which must differ. 
       In this example, the dehackednum of the new thing type will be -1, 
       since it is unspecified. The doomednum will be 20001, since it is 
       not inherited, but is specified in the child type. Remember that the 
       dehackednum and doomednum fields are NOT copied between thing types.
    */

    thingtype MyNewBaron
    {
       inherits     = BaronOfHell   # Inherit from the BaronOfHell
       doomednum    = 20001         # Set the doomednum to something meaningful
       translucency = 50%           # Make it 50% translucent
    }

[Return to Table of Contents](#contents)

[]{#cast}

------------------------------------------------------------------------

**DOOM II Cast Call**

------------------------------------------------------------------------

\
The cast call structure allows you to edit and extend the DOOM II cast
call used after beating the game in DOOM II. Editing existing thing
types via EDF or DeHackEd can otherwise cause this part of the game to
malfunction, and it has never before been possible to add your own
monsters into the fray. Now this can be done.\
\
Unlike most other EDF sections, **castinfo** sections will be kept in
the order they are specified, unless the **castorder** array is defined.
The **castorder** array feature allows you to explicitly specify a
complete ordering for the **castinfo** sections which overrides the
order of their definitions. See below for more information on
**castorder**.\
\
Beginning with EDF 1.1, all **castinfo** sections are required to have a
unique mnemonic. Sections with duplicate mnemonics will overwrite, with
the last one occuring during parsing taking precedence as the
definition.\
\
Syntax:


    castinfo <mnemonic>
    {
       type       = <thing type mnemonic>
       name       = <string>
       stopattack = <boolean>
       
       # see notes about this; there can be from zero to four sound blocks
       sound
       { 
          frame = <frame mnemonic>
          sfx   = <sound mnemonic>
       }
    }

Explanation of Fields:

- **type**\
  Default = No default; this field is required.\
  This field must be set to a valid thing type mnemonic. This denotes
  which thing type is displayed at this entry\'s position in the cast
  call. As of EDF 1.2, if this type is not valid, the thing type
  \"Unknown\" will be substituted. This may cause the cast call to
  malfunction, however. The previous behavior was to cause an error.
- **name**\
  Default = See below.\
  This is an optional field that allows providing a name to display on
  the screen for the thing type. Note that if this field is not present
  in any of the first 17 definitions, those cast members will use the
  normal default names from the executable, which are editable via BEX
  string replacement. If cast members beyond the first 17 have no name
  provided, they will say \"unknown\" (so you should always provide it
  in those cases).
- **stopattack**\
  Default = false\
  This is a special boolean value (can be any of yes/no, on/off, or
  true/false) which makes the thing break out of its attack frame cycle
  after its initial frame. This is only used by the player type by
  default, but could be used by any other type as well.
- **sound**\
  There can be up to four of these special blocks defined within each
  cast member. They define a sound event, so that whenever the thing
  type enters the designated frame, it will play the designated sound.
  If there are less than four definitions, the unused ones will be
  zeroed out. If there are more than four, any beyond four are simply
  ignored. Also, if the values provided to the fields in this block are
  invalid, they are simply ignored.\
  Explanation of Inner Fields:
  - **frame**\
    Default = S_NULL\
    Name of the frame this event should occur on
  - **sfx**\
    Default = none\
    Name of the sound to play when this event occurs.

Restrictions and Caveats:

- The castorder list is NOT required.
- At least one cast member must be defined.
- All cast members must have a unique mnemonic. Mnemonics should only
  contain alphanumeric characters and underscores. All other punctuation
  marks are explicity reserved for extended syntactical use. Format will
  not be verified. Non-unique mnemonics will overwrite previous
  definitions, see below.

Full Example:


    # My New Cast Call -- Only The Ultimate FooBar and the Player are worthy!
    castinfo foobar
    {
       type = FooBar
       name = "The Ultimate FooBar"
       
       sound { frame = S_FOOBAR_ATK1; sfx = firsht }
       sound { frame = S_FOOBAR_ATK3; sfx = firsht }
       sound { frame = S_FOOBAR_ATK5; sfx = firsht }
    }

    # Notice since DoomPlayer is now #2 and not #17, I still need to set his
    # name, otherwise it would call him a former sergeant o_O
    castinfo player
    {
       type = DoomPlayer
       name = "Our Hero"
       stopattack = true
    }

Using the **castorder** Array\
\
When editing the cast call, it will probably be advantageous to specify
the **castorder** array. Doing so will allow you to avoid the EDF 1.0
problem of being forced to redefine the entire cast call simply to add a
new cast member in the middle. It also allows you to move around
existing **castinfo** definitions without editing them, and to omit or
repeat definitions.\
\
Syntax:


    castorder = { <mnemonic>, ... }

All mnemonics specified in the **castorder** list must be valid. If any
do not exist, an error will occur.\
\
As with other EDF lists, if the list is defined more than once, the
latest definition encountered is the one used. Also, you may use
addition lists to add entries to the end of the **castorder** list.\
\
Full Example:


    # I decided to switch the cast call order around
    castorder = { player, foobar }

    ...

    # Later on, perhaps in another file, I add a new castinfo. I can do this:
    castinfo weirdo
    {
       type = WeirdoGuy
       name = "The Weirdo Guy"
    }

    # This will add weirdo to the end of the existing castorder list.
    # I could also just redefine the entire list, but there's no point in this case.
    castorder += { weirdo }

[Return to Table of Contents](#contents)

[]{#boss}

------------------------------------------------------------------------

**DOOM II Boss Brain Types**

------------------------------------------------------------------------

\
The Boss Brain thing types list allows editing of the types of monsters
which can be spawned by the DOOM II Boss Brain cube spitter object. This
structure is a simple list of thing type mnemonics, which are kept in
the order they are provided. You can redefine the list at will, just as
with the **spritenames** list.\
\
Starting with EDF 1.1, there is no longer an 11-type limitation on the
boss spawner list. However, if you provide more than 11 thing types in
this list, you must also define the new **boss_spawner_probs** list,
which must be a list of numbers which when added together equal 256.
These numbers serve as the probability values out of 256 for the
corresponding thing types. If exactly 11 types are defined and the
**boss_spawner_probs** list is not defined, the normal defaults will be
used. If boss_spawner_probs is defined, it MUST be the exact same length
as the boss_spawner_types list.\
\
Starting with EDF 1.2, if a thing type in the **boss_spawner_types**
list is invalid, the \"Unknown\" thing type will be substituted for it.
The previous behavior was to cause an error.\
\
Syntax:


    # A list of thing types

    boss_spawner_types =
    {
       <thing type mnemonic>, ...
    }

    # A list of probabilities for the above thing types. If provided, this list
    # must be the same length as the above, and if not provided, the above list
    # must contain exactly 11 types. The numbers in this list must add up to 256.

    boss_spawner_probs =
    {
       <number>, ...  
    }

Full Example:


    # Changed DoomImp to FooBar, and Archvile to the ScarletPimpernel
    # (so it can give you a powerup, but not very often, since Archvile is the
    #  rarest type ;)
    # In a TC you might want to change ALL the monster types...
    # These will use the default probabilities listed below, unless a
    # boss_spawner_probs list is also defined somewhere.

    boss_spawner_types =
    {
       FooBar, Demon, Spectre, PainElemental, Cacodemon, ScarletPimpernel,
       Revenant, Arachnotron, Mancubus, HellKnight, BaronOfHell
    }

Example with More than 11 Thing Types:


    boss_spawner_types =
    {
       DoomImp, Demon, Spectre, PainElemental, Cacodemon, Archvile, Revenant,
       Arachnotron, Mancubus, HellKnight, BaronOfHell, FooBar, ScarletPimpernel
    }

    boss_spawner_probs =
    {
       40, 40, 30, 10, 30, 2, 10, 20, 30, 22, 10, 10, 2
    }

Normal Default Probabilities Used When 11 Types are Defined:


    boss_spawner_probs =
    {
    # prob. / thing type normally in this position in the list
       50,  # DoomImp
       40,  # Demon
       30,  # Spectre
       10,  # PainElemental
       30,  # Cacodemon
        2,  # Archvile
       10,  # Revenant
       20,  # Arachnotron
       30,  # Mancubus
       24,  # HellKnight
       10   # BaronOfHell
    }

[Return to Table of Contents](#contents)

[]{#sound}

------------------------------------------------------------------------

**Sounds**

------------------------------------------------------------------------

\
New to EDF 1.1, sound definitions allow editing of existing sounds, as
well as the addition of new sounds which can be referred to by things
and frames. Although Eternity already supports the implicit addition of
new sounds whose lumps start with the DS prefix, those sounds are only
available to MapInfo and scripting. Since EDF allows the assignment of
DeHackEd numbers, it allows much greater flexibility for new sounds.\
\
EDF also provides access to some sound structure fields which are not
currently supported in DeHackEd.\
\
Each sound must be given a unique mnemonic in its definition\'s header,
and this is the name used to identify the sound elsewhere. Each field in
the sound definition is optional. If a field is not provided, it takes
on the default value indicated below the syntax information. Fields may
also be provided in any order.\
\
Syntax:


    sound <mnemonic>
    {
       lump          = <string>
       prefix        = <boolean>
       singularity   = <singularity class>
       priority      = <number>
       alias         = <sound mnemonic>
       link          = <sound mnemonic>
       linkvol       = <number>
       linkpitch     = <number>
       skinindex     = <skin index type>
       clipping_dist = <number>
       close_dist    = <number>
       pitchvariance = <pitch variance type>
       dehackednum   = <number>
    }

Explanation of Fields:

- **lump**\
  Default: If a value is not provided to this field, then this sound\'s
  mnemonic will be copied directly to this field. In this case, the
  default value for the prefix field becomes \"true\". This means that
  the mnemonic will be prefixed with the \"DS\" prefix to construct the
  final sound lump name. If an explicit value is provided to this field,
  then the default value of the prefix field becomes \"false\". This
  means the lump name, as provided, becomes the final sound lump name
  and will not be prefixed in any way.\
  \
  The lump field indicates the lump name that this sound resource will
  have in WAD files. If the prefix field below is true, the prefix
  \"DS\" will be appended to this name when looking for the sound.
  Otherwise, this name will be used exactly as it is provided. In the
  event a sound is not found, a default sound for the current game mode
  (DSPISTOL in DOOM, GLDHIT in Heretic) will be played instead.

- **prefix**\
  Default = Depends on presence of **lump** field\
  This boolean field indicates whether to prefix the lump name with DS
  when searching for this sound\'s lump. Values of false/no/off mean
  that the lump will not be prefixed. Values of true/yes/on mean that it
  should be prefixed. If the lump field is provided, the default value
  of this field changes from true to false (this behavior is new as of
  EDF v1.7).

- **singularity**\
  Default = sg_none\
  This field indicates a singularity class for this sound. If another
  sound of the same singularity class is playing when this sound is
  played, the former will be stopped. This is used to implement
  appropriate player sound behavior in an extensible and modifiable
  manner. The possible values for the singularity class are as follows:


          sg_none
          sg_itemup
          sg_wpnup
          sg_oof
          sg_getpow

- **priority**\
  Default = 64\
  This field establishes a relative priority for this sound. When sounds
  must be stopped in order to start new ones, those with the lowest
  priority will be replaced first. Priority scales down with distance,
  so sounds playing at higher volume or closer to the player have
  priority over quieter and more distant sounds. Please note carefully
  that a higher value in this field means a lower priority. So 0 is the
  highest priority possible, and 255 is the lowest priority (priorities
  lower than 255 will be capped into range).

- **alias**\
  Default = \"none\"\
  This field causes this sound definition to become an alias for another
  existing sound. Its value must either be \"none,\" or another valid
  sound mnemonic. When this sound is played, the aliased sound will be
  played instead, using all the data defined in the aliased sound (when
  an alias references another alias, the resolution is done recursively
  until a non- aliased sound is found). New to EDF 1.7.

- **link**\
  Default = \"none\"\
  This field links this sound definition to another existing sound
  definition. Its value must either be \"none,\" or another valid sound
  mnemonic. When a linked sound is played, the sound that is linked to
  will be played instead, using the linkvol and linkpitch parameters
  provided in this sound entry. All parameters for playing the sound,
  including priority, singularity, pitch variance, and clipping
  distances come from **this** sound definition, and not from the sound
  to which this one is linked.

- **linkvol**\
  Default = 127 (full volume)\
  This field provides a value to be added to the sound\'s volume when a
  linked sound is played through this sound definition. Volume range is
  from 0 to 127, but this field may be any value. The resulting value
  when added to the base volume will be capped into range. For example,
  if a sound normally plays at full volume, specifying a negative value
  in this field will reduce the volume. Warning: The semantics of this
  field have changed as of EE v3.33.50.

- **linkpitch**\
  Default = 127 (normal)\
  This field provides a pitch value to be used when a linked sound is
  played through this sound definition. Pitch values should be between 0
  and 255 when provided. Warning: The semantics of this field have
  changed as of EE v3.33.50.

- **clipping_dist**\
  Default = 1200\
  This value specifies the distance from the origin at which this sound
  will be completely clipped out; that is, reduced to zero volume and
  therefore killed completely. The sound engine currently only considers
  an approximate distance on the X-Y plane, so this behavior may be
  slightly more rough than expected. The value of this field should be
  greater than the value of the **close_dist** field below. If not, the
  sound will always be played at full volume (this can be intentionally
  exploited). New to EDF 1.5.

- **close_dist**\
  Default = 200\
  This value specifies the distance from the origin at which this sound
  will be at full volume. Attenuation between the **clipping_dist** and
  **close_dist** values is currently always linear. The sound engine
  currently only considers an approximate distance on the X-Y plane, so
  this behavior may be slightly more rough than expected. The value of
  this field should be less than the value of the **clipping_dist**
  field above. If not, the sound will always be played at full volume
  (this can be intentionally exploited). New to EDF 1.5.

- **skinindex**\
  Default = sk_none\
  This field indicates a player skin sound index for this sound. If the
  player object plays this sound, it will become subject to remapping
  through the player\'s skin. The following values are possible for skin
  index, and each corresponds to one of the fields in the extended
  Legacy skin format:


          sk_none
          sk_plpain
          sk_pdiehi
          sk_oof
          sk_slop
          sk_punch
          sk_radio
          sk_pldeth
          sk_plfall
          sk_plfeet
          sk_fallht

- **pitchvariance**\
  Default = none\
  This field allows specification of the type of pitch variation this
  sound plays with when pitched sounds are enabled. The following values
  are allowed:


          none            -- No pitch variation
          Doom            -- Pitch variation similar to that used in DOOM v1.1 for most sounds
          DoomSaw         -- Pitch variation similar to that used for saw sounds in DOOM v1.1.
          Heretic         -- Pitch variation similar to that used in Heretic
          HereticAmbience -- Pitch variation similar to that used in Heretic for global ambience.
          

  In general, DOOM types provide less variation than Heretic types. Note
  that Eternity has now repaired some bugs in the BOOM sound pitch
  variation code, and so the effect works much better than it did
  previously. New to EDF 1.7.

- **dehackednum**\
  Default = -1\
  In order to be accessible to thing and frame definitions, a sound
  definition must be given a unique DeHackEd number. The value zero is
  reserved and cannot be used. In order to avoid conflict, and to remain
  compatible with future versions of the Eternity Engine, user sounds
  should either use DeHackEd numbers between 10000 and 32768, or simply
  use the value -1 if no thing/frame or DeHackEd access is necessary. As
  of EDF 1.6, if a sound with no DeHackEd number is used by name (such
  as with the \"sound:\" prefix in a misc or args field), the game
  engine will automatically allocate a unique DeHackEd number to the
  sound. The previous behavior was to print a warning to the verbose EDF
  log and set the sound field in question to \"none\". This eliminates
  most need to assign and keep track of DeHackEd numbers yourself.

Restrictions and Caveats:

- All sounds must have a unique mnemonic no longer than 32 characters.
  They should only contain alphanumeric characters and underscores.
  Length will be verified, but format will not (non-unique mnemonics
  will overwrite previous definitions, see below). The mnemonic \"none\"
  is reserved and cannot be overridden or edited. Sounds using the
  mnemonic \"none\" will not be playable. Note: The limit on sound
  mnemonic length limit was increased from 16 to 32 as of EE v3.33.50
  (EDF v1.7).
- If the lump name is not provided, so that the mnemonic will be copied
  to it, then the mnemonic must also either be a full, valid lump name,
  or a string that becomes one if the prefix field is set to true. If a
  mnemonic is used which is not a valid sound lump name, but no lump
  name is provided, then the sound entry will not be able to play, and
  the default replacement sound for the game mode will play instead.
- All sounds with a DeHackEd number not equal to -1 must have a unique
  DeHackEd number. As of EDF 1.6, if a sound with no unique DeHackEd
  number is used by a thing, frame, or other section, it will be
  automatically allocated a unique number by the game engine. The
  DeHackEd number 0 (zero) is reserved for sounds and cannot be used in
  an EDF sound definition.

Replacing Existing Sounds:\
\
To replace the values of an existing EDF sound definition, simply define
a new sound with the exact same mnemonic value (as stated above, all
sounds need a unique mnemonic, so duplicate mnemonics serve to indicate
that an existing sound should be replaced).\
\
EDF 1.1 adds delta structures, which allow fields of existing sounds to
be edited without replacing all values in those sounds. Delta structures
also cascade, and are thus useful for layering of different
modifications. See the section [Delta Structures](#delta) for full
information.\
\
Full Example:


    # Define some new sounds
    # This one is only accessible via scripting because it doesn't have a DeHackEd number.
    # (Although a DeHackEd number may be auto-assigned to it if something in EDF attempts
    #  to use its DeHackEd number...)

    sound MySound
    {
       lump = foo    # this will play FOO, since prefix defaults to false when
                     # a lump name is explicitly provided. Note that this case is
                     # affected by the change in EDF v1.7. Previously, this would have
                     # played the sound DSFOO, even though the lump name is specified
                     # as "foo". Automatic prefixing now ONLY applies to sounds which
                     # copy their mnemonic to the lumpname and allow prefix to default.
    }

    # This entry uses its mnemonic to double as the sound lump name, but already provides
    # the standard DS prefix in the mnemonic.

    sound dsblah
    {
       prefix = false  # Because we did not explicitly specify a lump name, the
                       # lump name has become "dsblah", and the default value of
                       # prefix is true. That means we have to change it to false,
                       # or else the game would try to play DSDSBLAH, which is
                       # wrong. This has not changed.
                       
       dehackednum = 10000
    }

    # This entry also uses the mnemonic as the sound name, but with prefixing.
    # Since prefix defaults to true when the lump name is not explicitly provided, 
    # the sound lump played will be "DSEXPLOD". This has also not changed.

    sound explod
    {
       priority = 128
       dehackednum = 10001
    }

    # This entry links to another sound.
    sound BlahLink
    {
       link = dsblah   # we've linked this entry to the dsblah sound
       linkvol = 64    # use some suitable values for these (must experiment)
       linkpitch = 72
       dehackednum = 10002
    }

    # This entry overrides a previously defined sound.
    sound explod
    {
       priority = 96        # Maybe we like 96 better than 128...
       dehackednum = 10001  # We can reuse the same DeHackEd number, since it overwrites
    }

    # This sound is just an alias for explod. It will play using all the parameters
    # as they are defined in explod. Anything else we defined in this sound would be
    # totally ignored.

    sound Explosion { alias = explod }

[Return to Table of Contents](#contents)

[]{#ambience}

------------------------------------------------------------------------

**Ambience**

------------------------------------------------------------------------

\
New to EDF v1.7, **ambience** sections allow the user to define the
behavior of ambient sound spots, which can be placed on maps using
DoomEd numbers 14001 through 14065. The first 64 of these objects
automatically use the ambience sections with numeric indexes 1 through
64. Object 14065 accepts the index number in its first argument field,
which may be specified either through the use of ExtraData or via the
Hexen map format.\
\
Syntax:


    ambience
    {
       sound       = <sound mnemonic>
       index       = <number>
       volume      = <number>
       attenuation = <attenuation type>
       type        = <ambience behavior type>
       period      = <number>
       minperiod   = <number>
       maxperiod   = <number>
    }

Explanation of Fields:

- **sound**\
  Default = \"none\"\
  This field specifies the sound to be played by an ambience object
  using this definition. If the value provided to this field is invalid,
  no sound will be played.

- **index**\
  Default = 0\
  Defines the index of this ambience definition. This may be an integer
  of any value zero or greater, although only the first 256 ambience
  definitions are usable via the Hexen map format. When an ambience
  definition has the same index as a previously defined ambience
  definition, it will overwrite that previous definition. Note that if a
  thing placed on a map references an invalid ambience sequence, that
  object will do nothing.

- **volume**\
  Default = 127\
  The relative volume of this ambient sound, ranging from 0 to 127. This
  is multiplied by the user\'s volume setting so that the true range of
  volume is from 0 to the user\'s setting. For all attenuation types
  except \"none\", the volume is additionally decreased by the user\'s
  distance from the ambience object.

- **attenuation**\
  Default = \"normal\"\
  This field defines the attenuation behavior of the ambience object.
  Attenuation is how the sound changes with distance between the
  listener and the ambience object. The following values are available
  for this field:


          normal -- Use the close_dist and clipping_dist fields defined in the sound definition.
          idle   -- Use DOOM's normal default sound attenuation behavior.
          static -- Fades quickly (inaudible from 512 units).
          none   -- Plays at this ambience definition's volume level regardless of distance.
          

- **type**\
  Default = \"continuous\"\
  This field defines the repetition behavior of this ambience
  definition. The following values are available for this field:


          continuous -- loops the sound seamlessly
          periodic   -- restarts the sound every "period" number of gametics
          random     -- restarts the sound after a randomized wait period determined by the
                        minperiod and maxperiod fields
          

- **period**\
  Default = 35\
  For periodic ambience definitions, this field defines how frequently
  the sound is restarted in gametics (1/35 of a second).

- **minperiod**\
  Default = 35\
  For random ambience definitions, this field defines the shortest wait
  period before the sound will be restarted.

- **maxperiod**\
  Default = 35\
  For random ambience definitions, this field defines the longest wait
  period before the sound will be restarted. The ambience object will
  wait a period of time somewhere between minperiod and maxperiod
  gametics, inclusive. For proper behavior, maxperiod should be a number
  greater than minperiod.

\
Example:


    ambience
    {
       index       = 0
       sound       = ht_waterfl
       type        = continuous
       volume      = 127
       attenuation = static
    }

    ambience
    {
       index       = 1
       sound       = ht_bstsit
       type        = periodic
       volume      = 64
       attenuation = none
       period      = 70
    }

    ambience
    {
       index       = 2
       sound       = ht_bstatk
       type        = random
       volume      = 127
       attenuation = idle
       minperiod   = 35
       maxperiod   = 105
    }

[Return to Table of Contents](#contents)

[]{#soundseqs}

------------------------------------------------------------------------

**Sound Sequences**

------------------------------------------------------------------------

\
New to EDF v1.7, **soundsequence** sections allow the user to define
scripted sequences of sounds that can be used to control the auditory
behavior of sectors, PolyObjects, and global environmental ambience
effects. Every soundsequence section should have a unique mnemonic of 32
characters\' length or less. If a sound sequence redefines an existing
sequence\'s mnemonic, it will overwrite the original sequence.\
\
Sound sequences are assigned to sectors via use of objects with DoomEd
numbers 1400 through 1411. The first ten of these objects assign
sequences 0 through 10 to the sector in which they are placed. Object
1411 accepts the sequence id number in its first argument value, which
can be specified via ExtraData or through use of the Hexen map format.\
\
Sound sequences are assigned to PolyObjects by specifying the sequence
id number in the last argument to the Polyobj_StartLine or
Polyobj_ExplicitLine specials.\
\
Sound sequences are assigned for playing by the environmental sequence
engine by being given a type value of \"environment\". Environment
sequences exist in their own numeric namespace and may thus have ids the
same as sequences used for other purposes. Environmental sound sequences
are assigned to a map by placing objects with DoomEd numbers 1200
through 1300 anywhere on the map. The first 100 of these objects specify
environmental sequences 0 through 99. Object 1300 accepts an
environmental sequence number in its first argument, which may be
specified via ExtraData or through use of the Hexen map format.\
\
When played from scripts, sound sequences are always started by their
mnemonic.\
\
Syntax:


    soundsequence <mnemonic>
    {
       id              = <number>
       cmds            = { <string>, ... }
       type            = <sequence type>
       stopsound       = <sound mnemonic>
       attenuation     = <attenuation type>
       volume          = <number>
       minvolume       = <number>
       nostopcutoff    = <boolean>
       doorsequence    = <sound sequence mnemonic>
       platsequence    = <sound sequence mnemonic>
       floorsequence   = <sound sequence mnemonic>
       ceilingsequence = <sound sequence mnemonic>
    }

Explanation of Fields:

- **id**\
  Default: -1\
  For a sequence to be playable by sectors, PolyObjects, or the
  environmental ambience engine, the sequence must define an id number.
  The id number is affected by the sequence type, however. For sequences
  marked as being of type \"door\" or \"plat\", the first 64 sequences
  from 0 to 63 are kept in a special, separate lookup. This means that
  if a door sector or a PolyObject requests sequence #0, it will first
  look for a sequence of type \"door\" which has the id number 0. If
  none is found, it will then look amongst all sequences. If one is not
  found at that point, no sequence is played. The same process applies
  for plat/floor/ceiling sectors. For environmental sequences, the id
  numbers are kept in an entirely separate namespace. This means that
  environmental sequence #0 is not the same as sector sequence #0 and
  will not hide it.\
  For sector, door, or plat sequences greater than 63, all sequences
  exist in the same numeric namespace. If a sequence has the default id
  of -1, it cannot be played by number and can only be accessed via
  scripting.

- **cmds**\
  Default = { \"end\" } This is a list of strings that make up the
  command script for this sound sequence. The syntax and function of all
  commands is documented below.

- **type**\
  Default = \"sector\"\
  This defines the particular type of this sound sequence. The following
  values apply to this field:


          sector -- A general sector sequence. The ID number is always in the main namespace.
          door   -- A sector sequence meant particularly for doors or PolyObjects. The first 64
                    sequences with ID numbers 0 through 63 of this type are kept in a separate 
                    lookup table, and will be found first before checking the main namespace.
          plat   -- A sector sequence meant particularly for plat, ceiling, or floor actions.
                    This works like doors above, with the first 64 such sequences (ids 0 through
                    63) having a separate lookup that has priority over the main namespace.
                    
          NOTE: If both a sector and door or plat sequence exist with the same id numbers
          between 0 and 63, the door or plat sequence will be found first when a sector 
          starts that type of action. Otherwise, the last such sequence parsed will prevail.
                    
          environment -- Sequences of this type are for use by the environmental sound engine.
                         They are kept in an entirely separate namespace, and cannot be found
                         by sector or PolyObject actions. This means an environment and sector
                         type sequence can have the same ID numbers without conflicting.
          

- **stopsound**\
  Default = \"none\"\
  Defines an optional sound to play when this sequence is ended.
  Sequences with a stopsound defined will wait for the game engine to
  cut them off, and this sound will be played at that point. If no
  stopsound is defined, then the sequence is cut off immediately, with
  sound behavior depending on the \"nostopcutoff\" field. Note that this
  value can also be specified via a command in the **cmds** list.

- **attenuation**\
  Default = \"normal\"\
  Defines the starting attenuation of the sound sequence. May be one of
  the following values:


          normal -- use the values defined in the sound block for the current sound
                    being played by the sound sequence.
          idle   -- use DOOM's default attenuation behavior.
          static -- Fade quickly.
          none   -- Plays at the sound sequence's current volume regardless of distance from
                    the origin.
          

  Note that the attenuation type can be changed at runtime by the
  attenuation command in the **cmds** block as well.

- **volume**\
  Default = 127\
  Defines the starting relative volume of the sound sequence, from 0
  to 127. Volume can also be set and modified by a number of sound
  sequence commands.

- **minvolume**\
  Default = -1\
  If given a value between 0 and 127, but less than the volume field,
  the starting volume of the sound sequence will be randomized between
  minvolume and volume inclusive.

- **nostopcutoff**\
  Default = false\
  This boolean value determines what happens to any sound playing when a
  sequence that doesn\'t define a stopsound is stopped. If false, the
  sound is cut off immediately. If true, the sound is allowed to play to
  completion. It is redundant to specify this value as true for a
  sequence that defines a stopsound. Note that this attribute may also
  be toggled to true via use of the nostopcutoff command in the **cmds**
  list.

- **doorsequence**\
  Default = No default\
  If a valid sound sequence mnemonic is specified in this field, then
  when this sound sequence is started by a door action, it will redirect
  to the named sequence. This allows sectors with multiple possible
  actions affecting them to use different sound sequences for each
  action.

- **platsequence**\
  Default = No default\
  If a valid sound sequence mnemonic is specified in this field, then
  when this sound sequence is started by a platform action, it will
  redirect to the named sequence. This allows sectors with multiple
  possible actions affecting them to use different sound sequences for
  each action.

- **floorsequence**\
  Default = No default\
  If a valid sound sequence mnemonic is specified in this field, then
  when this sound sequence is started by a floor action, it will
  redirect to the named sequence. This allows sectors with multiple
  possible actions affecting them to use different sound sequences for
  each action.

- **ceilingsequence**\
  Default = No default\
  If a valid sound sequence mnemonic is specified in this field, then
  when this sound sequence is started by a ceiling action, it will
  redirect to the named sequence. This allows sectors with multiple
  possible actions affecting them to use different sound sequences for
  each action.

\
Sound Sequence Commands\
\
The following is a list of the commands and their functions that can be
specified inside a sound sequence\'s **cmds** list. Note that whitespace
within each command is ignored, and that invalid or missing argument
values are always corrected to a safe and suitable default.

- **play \<sound\>**\
  Plays the given sound and advances to the next command immediately.\
  \
- **playuntildone \<sound\>**\
  Plays the given sound and delays the sequence until the sound has
  stopped playing. The sequence then advances to the next command.\
  \
- **playtime \<sound\> \<delay\>**\
  Plays the given sound and delays for the given number of gametics,
  then advances to the next command.\
  \
- **playrepeat \<sound\>**\
  Plays the given sound in a continuous, seamless loop. The sequence
  will not advance and must be stopped explicitly by the game engine.
  Environmental sequences should not use this command or they will lock
  out all other sequences for the duration of the level.\
  \
- **playloop \<sound\> \<delay\>**\
  Plays the given sound in a time-delayed loop. The sound is restarted
  every \"delay\" gametics, even if it has not finished, or possibly
  long after it has ended. The sequence will not advance and must be
  stopped explicitly by the game engine. Environmental sequences should
  not use this command or they will lock out all other sequences for the
  duration of the level.\
  \
- **playabsvol \<sound\> \<volume\>**\
  Plays the given sound at the given absolute volume, which becomes the
  new current volume level for the sound sequence (subsequent sounds
  will continue to play at this volume). The sequence will advance to
  the next command immediately. Volume should be between 0 and 127
  inclusive.\
  \
- **playrelvol \<sound\> \<volume\>**\
  Plays the given sound at the current sequence volume level plus the
  \"volume\" argument, which becomes the new current volume level for
  the sound sequence. The volume argument to this command may be any
  number, including negative values which will decrease the sequence
  volume. The volume will be wrapped into the range of 0 to 127. The
  sequence will advance to the next command immediately.\
  \
- **delay \<tics\>**\
  Delays execution of the sound sequence by the specified number of
  gametics.\
  \
- **delayrand \<min\> \<max\>**\
  Delays execution of the sound sequence by an amount between min and
  max inclusive. Min and max should not be equal and should be between 0
  and 255 for best results.\
  \
- **volume \<vol\>**\
  Sets the current volume level of the sound sequence to \"vol\", which
  should be a number between 0 and 127.\
  \
- **relvolume \<vol\>**\
  Adds \"vol\" to the current volume level of the sound sequence.
  \"vol\" may be any value, with negative numbers decreasing the current
  volume level. The resulting volume level will be clipped into the
  range of 0 to 127.\
  \
- **attenuation \<attn\>**\
  Sets the current attenuation behavior type for the sound sequence.
  Allowed values are normal, idle, static, and none.\
  \
- **stopsound \<sound\>**\
  Sets the stopsound for the sequence to \"sound\", if such a sound
  exists. The last such command in the command stream is the only one
  that will be used.\
  \
- **nostopcutoff**\
  Sets the **nostopcutoff** property of this sequence to true.\
  \
- **restart**\
  Restarts the sound sequence from the beginning.\
  \
- **end**\
  Marks the end of this sound sequence. It is never necessary to specify
  this command explicitly, as an end command is automatically added to
  every sound sequence as a convenience to the user.

\
Example:


    // This is a sequence playable by name only. It is one of the default sector sequences
    // defined in sounds.edf, and is used by Heretic doors when they close.
    soundsequence EEDoorCloseNormal
    {
       cmds =
       {
          "play EE_DoorOpen",
          "stopsound EE_DoorClose"
       }
    }

    // This is an environmental sequence for Heretic, which is used by placing a thing of
    // type 1200 anywhere on the map. It is started by the environmental sequence manager
    // at random, with the probability of it being played being determined by the proportion
    // of 1200 objects to other environmental sequence spots on the map.
    soundsequence EEHticAmbScream
    {
       type = environment
       id = 0
       cmds = { "playuntildone ht_amb1" }
       
       volume = 96
       minvolume = 32
       attenuation = none
    }

    // This is a sound sequence meant to be played by a PolyObject. It could also use
    // type sector and would still work, but using type door is more explicit. PolyObjects
    // always use door-type actions when starting sequences, so they check the door lookup
    // first for id numbers between 0 and 63.
    soundsequence PolyDoor
    {
       type = door
       id   = 0
       cmds =
       {
          "play EE_BDoorOpen",
          "stopsound EE_PlatStop"
       }
    }

    // This is one of the more complicated environmental sequences from sounds.edf.
    // It plays two alternating footstep sounds every 15 gametics at a constantly
    // decreasing volume level.
    soundsequence EEHticSlowFootSteps
    {
       type = environment
       id = 3
       cmds =
       {
          "playtime ht_amb4 15",
          "playrelvol ht_amb11 -3",
          "delay 15",
          "playrelvol ht_amb4  -3",
          "delay 15",
          "playrelvol ht_amb11 -3",
          "delay 15",
          "playrelvol ht_amb4  -3",
          "delay 15",
          "playrelvol ht_amb11 -3",
          "delay 15",
          "playrelvol ht_amb4  -3",
          "delay 15",
          "playrelvol ht_amb11 -3",
          "nostopcutoff"
       }
       
       volume = 96
       minvolume = 32
       attenuation = none
    }

    // Yes, sound sequences can be completely silent :) All fields are defaulting, and
    // the only command in the command list is "end".
    soundsequence EEPlatSilent
    {
    }

[Return to Table of Contents](#contents)

[]{#enviromgr}

------------------------------------------------------------------------

**Environmental Sequence Manager**

------------------------------------------------------------------------

\
EDF can alter basic properties of the environmental ambience sound
sequence manager. This system runs in every level and randomly chooses
environmental sequences to play based on what objects with DoomEd
numbers 1200 through 1300 are present on the level (these objects are
collected in much the same way as Boss Brain spots or D\'Sparil\'s
teleport landings). The probability of a given sequence playing on any
particular event is determined solely by how many objects on the level
specify that sequence as compared to the total number of objects.\
\
There can be any number of enviromanager blocks specified, but only the
values in the last one parsed will be used.\
\
Syntax:


    enviromanager
    {
       minstartwait = <number> // minimum wait time at level start in tics
       maxstartwait = <number> // maximum wait time at level start in tics
       minwait      = <number> // minimum wait time between sequences in tics
       maxwait      = <number> // maximum wait time between sequences in tics
    }

In all cases, the minimum wait times should be less than the maximum.
The engine will adjust the values to make this true if it is not.\
\
[Return to Table of Contents](#contents)

[]{#terrain}

------------------------------------------------------------------------

**TerrainTypes**

------------------------------------------------------------------------

\
As of EDF 1.5, the TerrainTypes system has been completely absorbed into
EDF. This means it is now possible to completely customize not only what
flats are associated with what terrain effects, but the effects
themselves. There are three principle object types associated with the
TerrainTypes system \-- splashes, terrains, and floors. Each will be
discussed in its own subsection below.\
\
[Return to Table of Contents](#contents)

[]{#splashes}

------------------------------------------------------------------------

**Splash Objects**

------------------------------------------------------------------------

\
Splash objects define the thingtypes, sounds, and other properties of a
splash caused by an thing hitting a terrain. Not all terrains must have
splashes, and each terrain can only refer to one splash object. A splash
object itself defines properties for both a low mass and high mass
splash, however. All fields in the splash block are optional, and as
usual, order is irrelevant.\
\
All splash blocks must have a unique mnemonic. Non-unique mnemonics will
overwrite, meaning that the properties of the last splash block
encountered with a given name will become the final properties of that
splash object.\
\
Syntax:


    splash <unique mnemonic>
    {
       smallclass = <thingtype mnemonic>
       smallclip  = <number>
       smallsound = <sound mnemonic>
       baseclass  = <thingtype mnemonic>
       chunkclass = <thingtype mnemonic>
       sound      = <sound mnemonic>
       
       chunkxvelshift = <number>
       chunkyvelshift = <number>
       chunkzvelshift = <number>
       chunkbasezvel  = <number>
    }

Explanation of Fields:

- **smallclass**\
  Default = No object will be spawned\
  This field specifies the thingtype spawned by a low mass splash
  (objects of mass less than 10 cause low mass splashes if possible). If
  this field is unspecified or is an invalid thing type, low mass splash
  objects will not spawn for any terrain that uses this splash object.
- **smallclip**\
  Default = 0\
  This field specifies an amount of floorclip in units to add to the low
  mass splash object. This allows use of objects that would otherwise
  look incorrect for low mass splashes by making them appear smaller. A
  typical value for this field is 12 units.
- **smallsound**\
  Default = \"none\"\
  The sound specified in this field will be played by mnemonic whenever
  a low mass splash occurs, independent of whether a valid
  **smallclass** is defined. This means you should always define the
  smallsound field if sounds are desired, even if its value is the same
  as the **sound** field. Otherwise, no sound will be played for low
  mass splashes.
- **baseclass**\
  Default = No object will be spawned\
  This field specifies the thingtype spawned as the base of a normal
  splash. If this field is unspecified or is an invalid thing type, no
  splash base will be spawned for any terrain that uses this splash
  object.
- **chunkclass**\
  Default = No object will be spawned\
  This field specifies the thingtype spawned as a chunk thrown upward
  from the point of a normal splash. The velocities given to this object
  when spawned are randomized and are customized via the chunk?velshift
  and chunkbasezvel fields below. If this field is unspecified or is an
  invalid thing type, no splash chunk will be spawned for any terrain
  that uses this splash object.
- **sound**\
  Default = \"none\"\
  The sound specified in this field will be played by mnemonic whenever
  a normal splash occurs, independent of whether a valid **baseclass**
  or **chunkclass** is defined.
- **chunkxvelshift**\
  Default = -1\
  When set to a positive value, this field indicates a value 2\^N by
  which to multiply a randomized number used as the x axis velocity of
  the splash chunk object. When set to -1, the splash chunk will be
  given no x velocity.
- **chunkyvelshift**\
  Default = -1\
  When set to a positive value, this field indicates a value 2\^N by
  which to multiply a randomized number used as the y axis velocity of
  the splash chunk object. When set to -1, the splash chunk will be
  given no y velocity.
- **chunkzvelshift**\
  Default = -1\
  When set to a positive value, this field indicates a value 2\^N by
  which to multiply a randomized number which is added to the
  **chunkbasezvel**, the sum of which becomes the z velocity of the
  splash chunk object. When set to -1, no randomized addend will be
  used.
- **chunkbasezvel**\
  Default = 0\
  When set to a positive value, this field indicates a base amount of z
  velocity to apply to the splash chunk object. The randomized value
  determined by chunkzvelshift is then added to this base amount of
  velocity. If set to zero while the chunkzvelshift value is set to -1,
  the splash chunk object will be given no z velocity.

Restrictions and Caveats:

- All splashes must have a unique mnemonic no longer than 32 characters.
  They should only contain alphanumeric characters and underscores.
  Length will be verified, but format will not (non-unique mnemonics
  will overwrite previous definitions).

Replacing Existing Splashes:\
\
To replace the values of an existing EDF splash definition, simply
define a new splash with the exact same mnemonic value (as stated above,
all splashes need a unique mnemonic, so duplicate mnemonics serve to
indicate that an existing splash should be replaced).\
\
Full Example:


    // The Water splash object as defined in terrain.edf demonstrates a 
    // fully-defined splash object:

    splash Water
    {
       smallclass = EETerrainWaterBase
       smallclip  = 12
       smallsound = eedrip
       
       baseclass  = EETerrainWaterBase
       chunkclass = EETerrainWaterSplash
       sound      = gloop   
       
       chunkxvelshift = 8
       chunkyvelshift = 8
       chunkzvelshift = 8
       chunkbasezvel  = 2
    }

[Return to Table of Contents](#contents)

[]{#terrain}

------------------------------------------------------------------------

**Terrain Objects**

------------------------------------------------------------------------

\
Terrain objects define the full properties of a TerrainType which can be
assigned directly to multiple flats. Terrains may optionally define what
splash object they use, as well as values related to inflicting damage,
whether or not the terrain is liquid, whether or not hitting the terrain
causes enemies to awaken, and other important information. All fields in
the splash block are optional, and as usual, order is irrelevant.\
\
All terrain blocks must have a unique mnemonic. Non-unique mnemonics
will overwrite, meaning that the properties of the last terrain block
encountered with a given name will become the final properties of that
terrain object.\
\
Terrain deltas are also supported for editing the properties of existing
terrain objects without duplicating all fields within them. See the
[Delta Structures](#delta) section for full information.\
\
Syntax:


    terrain <unique mnemonic>
    {
       splash         = <splash mnemonic>
       damageamount   = <number>
       damagetype     = <MOD name>
       damagetimemask = <number>
       footclip       = <number>
       liquid         = <boolean>
       splashalert    = <boolean>
       useptclcolors  = <boolean>
       ptclcolor1     = <palette index> OR <RGB triplet>
       ptclcolor2     = <palette index> OR <RGB triplet>
       minversion     = <number>
    }

Explanation of Fields:

- **splash**\
  Default = No splash is used by this terrain\
  This field specifies what splash object, if any, to use when objects
  strike a floor using a flat that is associated with this terrain
  object.
- **damageamount**\
  Default = 0\
  This field specifies the amount of damage this terrain inflicts on
  players at each damage opportunity (determined by the damagetimemask
  field below). If this field is zero, no damage will be inflicted.
- **damagetype**\
  Default = \"UNKNOWN\"\
  Defines the Means of Damage flag used when this terrain damages a
  player. Values of interest to terrain are typically SLIME and LAVA.
  Eventually, LAVA will cause thing types which define a special flaming
  death frame to enter that frame. Please keep this in mind for forward
  compatbility. Other values for the Means of Death flag can be found in
  the [thingtype](#things) section.
- **damagetimemask**\
  Default = 0\
  Sets the interval at which this terrain will inflict damage on
  players. This value is applied as a mask to the current level time, so
  to be most meaningful, this value must be a power of two minus one. A
  few valid values are 1, 3, 7, 15, and 31. Most DOOM nukage sectors use
  a mask of 31 as an example, and thus damage the player every 32
  gametics, or approximately once per second.
- **footclip**\
  Default = 0\
  This value defines the amount of footclip in units that this terrain
  applies to objects standing on it that have the flags2 bit FOOTCLIP
  set in their options. A footclip of zero means no footclipping is
  applied. Some special objects may set their own footclip values and
  thus may not be affected by terrain footclipping in any case.
  Footclipping is generally only proper for liquid terrain.
- **liquid**\
  Default = false\
  Specifies whether or not this terrain is liquid. Currently the only
  effect controlled by this field is suppressing player \"oof\" sounds
  when landing on liquid terrains, under the assumption that a splash
  sound will playing instead. In the future, certain special Heretic
  objects will react specially to liquid terrain.
- **splashalert**\
  Default = false\
  Specifies whether or not this terrain causes a noise alert to
  propagate when a player hits a floor using this terrain. Setting this
  field to true means that a player hitting the given floors will cause
  enemies to awaken and begin chasing him. By default, DOOM terrain
  types do not do this, and Heretic terrain types do.
- **useptclcolors**\
  Default = false\
  When set to true, this terrain object wants to use customized particle
  colors when gunshots and certain other effects strike the floor or
  ceiling of a sector using a flat with which this terrain is
  associated. Terrains which set this value to false will have their
  **ptclcolor1** and **ptclcolor2** fields ignored and will use the
  default gray bullet puff effects.
- **ptclcolor1**\
  Default = 0\
  This field may specify either a raw palette index from 0 to 255, or an
  RGB triplet string in the format \"r g b\", with each of r, g, and b
  being a number from 0 to 255. When an RGB triplet is specified, EDF
  will attempt to match the specified color to the closest color in the
  game\'s main palette. If no suitable match can be found, color zero
  will end up being chosen. For terrain effects that must work across
  multiple games, you should always utilize RGB triplets. Otherwise,
  palette indices are more accurate. This value is ignored if
  **useptclcolors** is false for this terrain object. This color is
  typically the darker of the two particle colors used for terrain
  effects.
- **ptclcolor2**\
  Default = 0\
  This field may specify either a raw palette index from 0 to 255, or an
  RGB triplet string in the format \"r g b\", with each of r, g, and b
  being a number from 0 to 255. When an RGB triplet is specified, EDF
  will attempt to match the specified color to the closest color in the
  game\'s main palette. If no suitable match can be found, color zero
  will end up being chosen. For terrain effects that must work across
  multiple games, you should always utilize RGB triplets. Otherwise,
  palette indices are more accurate. This value is ignored if
  **useptclcolors** is false for this terrain object. This color is
  typically the lighter of the two particle colors used for terrain
  effects.
- **minversion**\
  Default = 0\
  This field controls the minimum demo version in which this terrain
  object is usable. This field is unnecessary for the majority of
  user-created terrain objects, and this value should not be edited in
  the default terrain objects, neither via overriding nor terraindeltas,
  if your project is meant to function in the presence of old demos.
  minversion for the default objects is the first Eternity Engine
  version in which that terrain appeared.

Restrictions and Caveats:

- All terrains must have a unique mnemonic no longer than 32 characters.
  They should only contain alphanumeric characters and underscores.
  Length will be verified, but format will not (non-unique mnemonics
  will overwrite previous definitions).
- The default terrain called \"Solid\" is defined internally by the game
  engine, but can be edited by creating a terrain named \"Solid\" in
  EDF. This should be done with caution, however, since the default
  terrain is used in all circumstances when no terrain effects are
  usually expected. The Solid terrain has default values for all fields.

Replacing Existing Terrains:\
\
To replace the values of an existing EDF terrain definition, simply
define a new terrain with the exact same mnemonic value (as stated
above, all terrains need a unique mnemonic, so duplicate mnemonics serve
to indicate that an existing terrain should be replaced).\
\
Full Example:


    // The Water terrain object as defined in terrain.edf demonstrates a 
    // typical terrain object:

    terrain Water
    {
       splash   = Water
       footclip = 10
       liquid   = true
       
       useptclcolors = true
       ptclcolor1    = "127 127 255"
       ptclcolor2    = "0 0 255"
       
       minversion = 329
    }

    // This terrain defines damage properties only to create an 
    // instant-death floor type:

    terrain SuperDeadly
    {
       damageamount   = 10000  // inflict telefrag damage
       damagetype     = LAVA   // use LAVA MOD
       damagetimemask = 1      // inflict damage more or less constantly
    }

[Return to Table of Contents](#contents)

[]{#floors}

------------------------------------------------------------------------

**Floor Objects**

------------------------------------------------------------------------

\
Floor objects associate a terrain with a flat graphic. All fields in the
floor block are optional, and as usual, order is irrelevant. Floor
objects are precisely equivalent to entries in the old, obsolete binary
TERTYPES lump, and in fact, TERTYPES is still supported for legacy
projects, and will be used to generate additional floor objects if it is
found. See the [TERTYPES Lump Processing](#tertypes) section below for
more information.\
\
Multiple floor objects may associate the same flat with different
terrains. The last such definition encountered will determine what
terrain the flat uses.\
\
Syntax:


    floor
    {
       flat    = <string>
       terrain = <terrain mnemonic>
    }

Explanation of Fields:

- **flat**\
  Default = \"none\"\
  This field determines the flat to which this floor object\'s terrain
  is applied. If a terrain has already been applied to this flat, the
  terrain specified in this object will override the previous
  definition. This field need not refer to a valid flat name. No error
  will occur if the flat specified does not exist; this object will
  simply have no effect. However, the string specified in this field
  must be no longer than eight characters or an error will occur.
- **terrain**\
  Default = Solid\
  This field specifies the mnemonic of the terrain to associate with the
  flat named in this floor object. As mentioned in the terrain section
  above, the default Solid terrain type is defined by the game engine.
  By default, all floors are associated with the Solid terrain. The
  terrain mnemonic need not be a valid terrain. If it is invalid, a
  warning will be issued to the verbose EDF log file and the floor
  object will be modified to use the Solid terrain object.

Full Example:


    // A couple of the floor objects defined for DOOM in terrain.edf:
    floor { flat = FWATER1; terrain = Water }
    floor { flat = LAVA1;   terrain = Lava  }

[Return to Table of Contents](#contents)

[]{#tertypes}

------------------------------------------------------------------------

**TERTYPES Lump Processing**

------------------------------------------------------------------------

\
The old binary TERTYPES lump is now considered obsolete, and thus its
format is not described here, nor is any way to generate one any longer
available from Team Eternity. However, to support legacy projects,
Eternity will translate the last TERTYPES lump loaded into EDF floor
definitions dynamically, using the following assumptions:

- TERTYPES floor definitions associated with the value zero are mapped
  to the default Solid terrain.
- TERTYPES floor definitions associated with the value 1 are mapped to
  the terrain named \"Water\". If no Water terrain exists, the floor
  will be mapped to the Solid type.
- TERTYPES floor definitions associated with the value 2 are mapped to
  the terrain named \"Lava\". If no Lava terrain exists, the floor will
  be mapped to the Solid type.
- TERTYPES floor definitions associated with the value 3 are mapped to
  the terrain named \"Sludge\". If no Sludge terrain exists, the floor
  will be mapped to the Solid type.

This results in behavior identical to that under the old static
TerrainTypes system, if and only if the default terrain objects or
objects with equivalent names and behaviors are available for use.
TERTYPES lump processing is done after EDF floor objects are processed,
and thus any TERTYPES-derived definitions may override EDF floor objects
that affect the same flats.\
\
[Return to Table of Contents](#contents)

[]{#menus}

------------------------------------------------------------------------

**Dynamic Menus**

------------------------------------------------------------------------

\
New to EDF 1.6, dynamic menus allow the Eternity Engine\'s menu system
to be customized in various ways, including through the definition of
the custom user menu and via overriding of specific menus within the
native menu heirarchy.\
\
Remember that all fields are optional and may be provided in any order.
Note that the order of **item** sections within a menu is important,
however.\
\
Syntax:


    menu <unique mnemonic>
    {
       # one or more item sections can exist; the items appear on the menu in the
       # order they are defined within their parent menu section
       item
       {
          type  = <menu item type name>
          text  = <string>
          cmd   = <string>
          patch = <string>
          flags = <menu item flags string>
       }
       
       prevpage = <menu mnemonic>
       nextpage = <menu mnemonic>
       x        = <number>
       y        = <number>
       first    = <number>
       flags    = <menu flags string>
    }

Explanation of Fields:

- **item**\
  Default = No default\
  One or more items must be defined in every menu. There is no upper
  limit on the number of item sections. The items will appear in the
  menu in the same order in which they were defined in EDF.\
  Explanation of menu item fields:
  - **type**\
    Default = info\
    One of the following values with the given meanings:
    - **info** - The item is an informational string. It cannot be
      selected or execute a command, even if one is provided. This type
      of item is used as a section title. It is usual for the text to be
      given gold coloration, but this is up to the user.
    - **gap** - The item is a gap of blank space. It cannot be selected
      or execute a command. The amount of space is determined by the
      font used by the menu.
    - **runcmd** - This item type executes a command when the key bound
      to menu_confirm is pressed while it is selected. Only the text
      description is displayed; there is no value or other graphic to
      the right. If the command provided to this item is invalid, this
      item will be disabled.
    - **variable** - This item type allows editing of a console
      variable\'s value directly. The value editing must be done by
      typing in the new value. The value will appear to the right of the
      text description of this item at a distance determined by the
      layout of the menu. If the variable provided to this item is
      invalid, it will be disabled.
    - **toggle** - This item allows editing of a console variable\'s
      value using the menu_left or menu_right actions to scroll through
      its possible set of values. This type of item is only appropriate
      for console variables which are of \"yes/no\", \"on/off\", or
      \"named value\" types. The value of the variable will appear to
      the right of the text description of this item at a distance
      determined by the layout of the menu. If the variable provided to
      this item is invalid, it will be disabled.
    - **title** - This item type is appropriate only for use as the main
      title of the menu. Titles are drawn using the gamemode\'s \"Big\"
      font if no graphic patch is available for them. In Heretic, menu
      title text is also shadowed by default. This type of item has no
      value, is not selectable, and cannot execute a command.
    - **slider** - This item type allows editing of a console
      variable\'s value using a slider. The slider will appear to the
      right of the text description at a distance determined by the
      menu\'s layout. If the variable assigned to this item is invalid,
      the slider will be disabled. Sliders are only appropriate for use
      with ranged integer console variables.
    - **automap** - This item is an automap color selector.
    - **binding** - This item is a keybinding. The value provided to the
      **cmd** field must be a key action name instead of a console
      command. The keys currently bound to the action will appear to the
      right of text description at a distance determined by the menu\'s
      layout.
    - **bigslider** - This item is a giant slider for emulated menus.
      Otherwise, it works similar to the normal slider.

  - **text**\
    Default = \"\"\
    This is the textual description of the menu item, which is drawn for
    all item types unless a valid **patch** graphic is specified, in
    which case it will be drawn instead. The height of the menu item is
    determined by the text or graphic used by the item, and item spacing
    is performed automatically by the menu engine.

  - **cmd**\
    Default = No default\
    This is the console command, console variable, or other special
    value which this item will execute or set a value to when it is
    activated. This field only has meaning for item types which are
    selectable. Non-selectable types will ignore it entirely. If a
    runcmd, variable, toggle, slider, automap, or binding item type is
    given an invalid console command or variable, the item will be
    disabled by the menu engine.

  - **patch**\
    Default = No default\
    If specified, this field will override the text description with a
    patch graphic of the given name. If the patch named doesn\'t exist
    amongst all loaded WAD files, the text description will appear
    instead. If the graphic named exists but is not a valid patch
    graphic lump, behavior is undefined.

  - **flags**\
    A BEX-format flags field which can accept any combination of the
    following values. All values are off by default.


                 Flag name       Effect
                --------------------------------------------------------------------
                 BIGFONT         Item uses the gamemode's "big" font
                 CENTERED        Menu item is centered. Useful only for info items.
                 LALIGNED        Menu item is aligned to the left.
                --------------------------------------------------------------------
                

    Example:


                menu foo
                {
                   item { type = info; text = "Hello EDF!"; flags = BIGFONT|CENTERED }
                }
                

    As with all other BEX-format flag fields, if the field\'s value
    contains whitespace, commas, or other characters disallowed in
    unquoted strings, you must surround the entire field value in single
    or double quotation marks.

- **prevpage**\
  Default = No default\
  This field specifies the previous page of this menu. Menus may be
  attached to each other in a circular chain of pages, which can be
  perused either by pressing Ctrl plus the keys bound to menu_left or
  menu_right, or by pressing the keys bound to menu_pageup or
  menu_pagedown. The default value means that there is no previous page.
  If the menu named by this field does not exist, there will also be no
  previous page. Table of contents functionality is not available for
  dynamic menus.

- **nextpage**\
  Default = No default\
  This field specifies the next page of this menu. It is otherwise
  identical to the **prevpage** field above.

- **x**\
  Default = 200\
  Specifies the x offset of the menu starting from the upper lefthand
  corner of the screen. Menu coordinates are relative to a 320x200
  screen and are scaled at runtime by the game engine. All patches are
  clipped to the screen, so it is not harmful if a menu or any items on
  it extend off the framebuffer. Note that this value specifies the
  right end of description strings for normal menu items. Item values
  will be drawn further to the right of this.

- **y**\
  Default = 15\
  Specifies the y offset of the menu starting from the upper lefthand
  corner of the screen. Menu coordinates are relative to a 320x200
  screen and are scaled at runtime by the game engine.

- **first**\
  Default = 0\
  Zero-based index of the first item that should be selected on the
  menu. If this value is out of range or is set to a normally
  unselectable item, the selection pointer will not appear until the
  user moves to a new item, at which point it will appear to jump to the
  proper item. On most native menus, the first selectable item is at
  index 3. This is due to a title, gap, and info item usually being the
  first three items on the menu.

- **flags**\
  A BEX-format flags field which can accept any combination of the
  following values. All values are off by default.


            Flag name       Effect
           ------------------------------------------------------------------------
            skullmenu       This menu uses the large skull pointer.
            background      This menu has a background and uses the small pointer.
            leftaligned     This menu is aligned to the left. Does not work with
                            all item types.
            centeraligned   All items on this menu are centered. Not useful for
                            menus that contain anything other than title, gap,
                            info, or command items.
            emulated        This menu is an emulated original DOOM menu. All items
                            are spaced at 16-pixel intervals.
           ------------------------------------------------------------------------
           

  Example:


           menu foo
           {
              item { type = info; text = "Hello EDF!"; flags = BIGFONT }
              
              flags = skullmenu|leftaligned
           }
           

  As with all other BEX-format flag fields, if the field\'s value
  contains whitespace, commas, or other characters disallowed in
  unquoted strings, you must surround the entire field value in single
  or double quotation marks.

Restrictions and Caveats:

- All menus must have a unique mnemonic no longer than 32 characters.
  They should only contain alphanumeric characters and underscores.
  Length will be verified, but format will not (non-unique mnemonics
  will overwrite previous definitions).

Replacing Existing Menus:\
\
To replace the values of an existing EDF menu definition, simply define
a new menu with the exact same mnemonic value (as stated above, all
menus need a unique mnemonic, so duplicate mnemonics serve to indicate
that an existing menu should be replaced).\
\
Example:


    /*

      Define the reserved _MN_Custom menu, which is automatically made available on
      the second page of Eternity's main options menu when it is defined.
      This is a great use for the new optional "user.edf" file. Note that by using
      the new "mn_dynamenu" command, it is possible to create your own inter-menu
      links and form a complete heirarchy of custom menus. See the console reference
      for full information.
      
    */ 
    menu _MN_Custom
    {
       item { type = title;  text = "my custom menu" }
       item { type = gap }
       item { type = info;   text = "\Hitems here" }
       item { type = runcmd; text = "get outta here!"; cmd = "mn_prevmenu" }
       
       first = 3
       flags = background
    }

Overriding Native Menus:\
\
Eternity allows specific native menus to be overridden by menus you
define in EDF. Currently the following menus can be overridden:

- Episode selection menu

To override a specific menu, you set a global variable in either the
root EDF or in an EMENUS lump to the mnemonic of the menu you have
provided. A table of variables and the menus they replace follows:


       EDF Variable                   Menu Overridden
      -------------------------------------------------------
       mn_episode                     Episode selection menu
      -------------------------------------------------------

Example:


    /*
       This could be placed at global scope anywhere within the root chain of EDF
       files/lumps or within an EMENUS lump along with the menu it names.
    */
    mn_episode = MyEpisodeMenu

[Return to Table of Contents](#contents)

[]{#delta}

------------------------------------------------------------------------

**Delta Structures**

------------------------------------------------------------------------

\
New to EDF 1.1, delta structures are an easier and more flexible way to
edit existing thing type, frame, sound, and terrain definitions.\
\
Each delta structure for a thing type, frame, sound, or terrain
specifies by mnemonic the section that it edits inside it. This allows
for any number of delta structures to be defined. The structures are
always applied in the order in which they are encountered during
parsing, so that, like style sheets in HTML, delta structures cascade.
This means you can load your own set of delta structures on top of an
existing set in a modification.\
\
Delta Structure Syntax:


    # A delta structure for a frame has this syntax:
    framedelta
    {
       name = <frame mnemonic>
       <list of frame fields>
    }

    # A delta structure for a thing type has this syntax:
    thingdelta
    {
       name = <thingtype mnemonic>
       <list of thingtype fields>
    }

    # A delta structure for a sound has this syntax:
    sounddelta
    {
       name = <sound mnemonic>
       <list of sound fields>
    }

    # A delta structure for a terrain has this syntax:
    terraindelta
    {
       name = <terrain mnemonic>
       <list of terrain fields>
    }

The name field is required, and must be set to a valid terrain, sound,
frame, or thingtype mnemonic in order to specify the definition that the
delta structure edits. Along with the name, the delta structure can
specify any field that is valid in that type of definition (see
exceptions below).\
\
See the [Frames](#frames), [Thing Types](#things), [Sounds](#sound), and
[Terrain](#terrain) sections for full information on their fields and
syntax.\
\
Any field not specified in a delta structure will not be affected, and
retains the value that it had before the delta structure was applied. A
delta structure must specify values explicitly, even if the new values
are the usual defaults for those fields. Note that the frame args list
must be completely specified. It is not possible to override only some
of the values in that list.\
\
Fields not supported in delta structures:

- The **dehackednum** field of any section edited by a delta structure
  cannot be changed. Although the field will be syntactically accepted
  by the parser, any value provided to it in a delta structure will have
  no effect. Only definition sections can provide a DeHackEd number.
- The **cmp** field of the **frame** block is not supported in
  **framedelta** sections.
- The **inherits** field of the **thingtype** block is not supported in
  **thingdelta** sections.

Frame Delta Example:


    # Change the Zombieman attack frame to use the shotgun zombie attack
    framedelta
    {
       name = S_POSS_ATK2
       action = SPosAttack
    }

Thing Delta Example:


    # Change the Zombieman's drop type to MegaSphere, and his spawnhealth to 400
    thingdelta
    {
       name = Zombieman
       droptype = MegaSphere
       spawnhealth = 400
    }


    # Later on, maybe in a different file, we change the spawnhealth again
    thingdelta
    {
       name = Zombieman
       spawnhealth = 20
    }

    # At this point, the Zombieman drops MegaSpheres, but his spawnhealth is 20, not 400.

Sound Delta Example:


    # This changes the sound explod we defined earlier
    sounddelta
    {
       name = explod
       priority = 200
       singularity = sg_getpow
    }

Terrain Delta Example:


    # Terrain deltas used by Heretic in terrain.edf to enable splash alerts:
    terraindelta { name = Water;  splashalert = true }
    terraindelta { name = Lava;   splashalert = true }
    terraindelta { name = Sludge; splashalert = true }

[Return to Table of Contents](#contents)

[]{#misc}

------------------------------------------------------------------------

**Miscellaneous Settings**

------------------------------------------------------------------------

\
These optional settings allow customization of various game engine
behaviors. When not provided, they take on the indicated default values.
All of these options can only be specified in the topmost level of an
EDF file. The last definition encountered is the one which will be
used.\
\

- **doom2_title_tics**\
  Default: 385 (11 seconds)\
  This variable allows you to change the length of time for which the
  DOOM II title screen is displayed. This is useful if your title song
  needs to be longer or shorter than the original. The unit for this
  variable is tics (35 per second).\
  Example:


          doom2_title_tics = 525    # make the DOOM II title screen last 15 seconds
          

- **intermission_pause**\
  Default: 0\
  This variable allows you to specify a period of time in gametics that
  the DOOM/DOOM II statistics intermission will pause while displaying
  the background picture. After the pause period has expired, the
  intermission will resume as normal by displaying the map name and
  statistics.\
  Example:


          intermission_pause = 70  # wait for 2 seconds
          

- **intermission_fade**\
  Default: None\
  This variable allows you to specify a color which will be overlaid on
  the background picture in the DOOM/DOOM II statistics intermission
  once the optional pause period has expired. If no pause period is
  specified, the color overlay will appear immediately at the start of
  the intermission. The value accepted by this variable is a palette
  index, which will be clamped into the range of 0 to 255.\
  Example:


          intermission_fade = 0  # Fade to Black (dun dun dun dun...)
          

- **intermission_tl**\
  Default: 0%\
  This variable specifies the translucency value with which the fade
  color will be overlaid on the background picture when an intermission
  fade color is specified. This field can accept an integer between 0
  and 65536, or a percentage value. A percentage value must be a base 10
  integer between 0 and 100 followed immediately by a % character.

[Return to Table of Contents](#contents)
