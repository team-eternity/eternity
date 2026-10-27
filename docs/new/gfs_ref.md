## Eternity Engine GFS Reference v1.01 \-- 7/05/04

[Return to the Eternity Engine Page](../etcengine.html)

- [**Table of Contents**]{#contents}
  - [Introduction to GFS](#intro)
  - [Using GFS](#using)
  - [How Files are Found](#files)
    - [Changing the Base Path](#base)
  - [Adding WAD Files](#wad)
  - [Adding DeHackEd/BEX Files](#deh)
  - [Adding Console Scripts](#csc)
  - [Specifying a Root EDF File](#edf)
  - [Specifying an IWAD](#iwad)

[]{#intro}

------------------------------------------------------------------------

**Introduction to GFS**

------------------------------------------------------------------------

\
GFS, or Game File Script, is a new method of adding multiple data files
to Eternity at once with the use of only one command-line parameter. GFS
syntax is extremely simple, and allows for more safety and semantic
checking than do batch files or response files. GFS also doesn\'t
exclude the use of any other command-line parameters in addition to
itself, so you can override files added in a GFS with your own if
needed. GFS is designed primarily for distribution along with
modifications such as PCs, TCs, megawads, etc., to make it easier for
users to play them without need for lots of command-line parameters.\
\
Now that Eternity supports drag-and-drop file loading in windowed
operating systems, users can also start your project by simply dragging
a GFS onto Eternity. This is the absolute best way to load a project,
perhaps aside from using a frontend. This functionality cannot be
duplicated with batch or response files.\
\
GFS uses the same libConfuse parser as EDF and ExtraData, written by
Martin Hedenfalk, and therefore shares most of the same syntactic
elements. GFS is free-form, meaning that whitespace is ignored and that
other tokens\' positions are irrelevant to their meanings. #, //, and
/\* \*/ comments are also supported, but are not very important for GFS
files, so they will not be discussed in this document. To see thorough
information on these elements of the syntax, see the [General
Syntax](edf_ref.html#gensyn) section of the Eternity Engine EDF
Reference.\
\
GFS supports long file names under operating systems other than DOS, but
they must be enclosed in quotation marks if they contain spaces.\
\
[Return to Table of Contents](#contents)

[]{#using}

------------------------------------------------------------------------

**Using GFS**

------------------------------------------------------------------------

\
To use a GFS file, use the -gfs command-line parameter, and give it an
argument of one absolute file path to the desired GFS file. If the GFS
file is in the executable\'s directory, it is not necessary for the path
to be absolute, but otherwise GFS may not function properly if the path
is only relative.\
\
Example:


    eternity -gfs C:\Games\AliensTC\atc.gfs

[Return to Table of Contents](#contents)

[]{#files}

------------------------------------------------------------------------

**How Files are Found**

------------------------------------------------------------------------

\
Normally, file names specified in a GFS are appended to the path of the
GFS file. So if a GFS file is in C:\\Games and specifies a file
\"superwar.wad\", then \"C:\\Games\\superwar.wad\" will be added. This
allows specification of subdirectories or the parent directory in a file
name. It is recommended that GFS files provided with modifications allow
the GFS to work in this manner.\
\
Note that an IWAD file name will not have any path appended to it, so it
should be an absolute path in all circumstances.\
\
[Return to Table of Contents](#contents)

[]{#base}

------------------------------------------------------------------------

**Changing the Base Path**

------------------------------------------------------------------------

\
It is possible to alter the path which will be appended to WAD, DEH/BEX,
CSC, and EDF file names by using the **basepath** variable. When
basepath is set to a path, that path will be appended to those file
names instead of the path of the GFS file. This allows GFS to be placed
by the user into a directory other than that in which the other files
exist. There can only be one basepath specification per GFS file. The
last one found during parsing will be the one which is used.\
\
This option is not generally useful to GFS provided with modifications,
since authors cannot know how the user\'s file system is set up. This
facility is present to allow users to create their own GFS files and
keep them wherever they wish.\
\
**basepath**, as mentioned above, does not affect the **iwad**
specification, if one is present.\
\
Example of Usage:


    #
    # Assume that Aliens TC 2.0 is in C:\Games\AliensTC, but this GFS is somewhere else
    # (like in the port's directory, for example
    #

    # The files I want to load:

    wadfile = alitcsf.wad
    wadfile = alitcsnd.wad
    wadfile = atclev.wad
    dehfile = atcud19.bex

    # The location where they're at, since the GFS file is not also there
    # (note you could also use "C:\\Games\\AliensTC"):

    basepath = "C:/Games/AliensTC"

[Return to Table of Contents](#contents)

[]{#wad}

------------------------------------------------------------------------

**Adding WAD Files**

------------------------------------------------------------------------

\
A GFS can specify any number of wad files to load using the **wadfile**
variable. WAD files will be loaded in the order in which they are
specified.\
\
A WAD file name should be a path which is relative either to the path of
the GFS file, or to the **basepath** if it is specified.\
\
GFS WAD files will be added before any other WAD files added via the
command-line or specified in the configuration file, so that any WADs
specified there can override those in the GFS.\
\
Example of Usage:


    wadfile = chextc.wad

[Return to Table of Contents](#contents)

[]{#deh}

------------------------------------------------------------------------

**Adding DeHackEd/BEX Files**

------------------------------------------------------------------------

\
A GFS can specify any number of DeHackEd/BEX files to load using the
**dehfile** variable. These files will be loaded in the order in which
they are specified.\
\
A DEH/BEX file name should be a path which is relative either to the
path of the GFS file, or to the **basepath** if it is specified.\
\
GFS DEH/BEX files will be added after any DeHackEd files added on the
command-line. This allows command-line DeHackEd files to take effect
before any changes made by GFS. However, DeHackEd lumps in WADs and any
specified in the configuration file are still added last.\
\
Example of Usage:


    dehfile = chexq.bex

[Return to Table of Contents](#contents)

[]{#csc}

------------------------------------------------------------------------

**Adding Console Scripts**

------------------------------------------------------------------------

\
A GFS can specify any number of console scripts to run using the
**cscfile** variable. These files will be executed in the order in which
they are specified.\
\
A CSC file name should be a path which is relative either to the path of
the GFS file, or to the **basepath** if it is specified.\
\
GFS CSC files will be executed immediately after any that are specified
via the configuration file.\
\
Example of Usage:


    cscfile = setvars.csc

[Return to Table of Contents](#contents)

[]{#edf}

------------------------------------------------------------------------

**Specifying a Root EDF File**

------------------------------------------------------------------------

\
A GFS can specify the root EDF file to process by using the **edffile**
variable. Only one root EDF file can be specified, so the last
definition encountered will become the one that is used.\
\
The EDF file name should be a path which is relative either to the path
of the GFS file, or to the **basepath** if it is specified.\
\
If the -edf command-line parameter is used, it will take precedence over
this setting.\
\
Example of Usage:


    edffile = myroot.edf

[Return to Table of Contents](#contents)

[]{#iwad}

------------------------------------------------------------------------

**Specifying an IWAD**

------------------------------------------------------------------------

\
A GFS can specify the IWAD file to use by using the **iwad** variable.
Only one IWAD file can be specified, so the last definition encountered
will become the one that is used.\
\
The IWAD file name should be the absolute path, including file name, of
the IWAD. The **basepath** variable does NOT affect this file name, and
the path of the GFS file will not be appended to it.\
\
If the -iwad command-line parameter is used, it will take precedence
over this setting.\
\
This option is not generally useful to GFS provided with modifications,
since they cannot know how the user\'s file system is set up. This
facility is to allow users to create their own GFS files, or to edit
existing ones, so that they can automatically be loaded without need to
additionally specify the IWAD.\
\
Example of Usage:


    iwad = "C:/Games/DOOM/DOOM.wad"

[Return to Table of Contents](#contents)
