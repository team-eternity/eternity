### Eternity Engine MapInfo Reference v1.3 \-- 10/22/06

[Return to the Eternity Engine Page](../etcengine.html)

- [**Table of Contents**]{#contents}
  - [Basic Syntax](#syn)
  - [MapInfo Variables](#vars)
  - [The Global MapInfo System](#global)
    - [Specific Syntax Information](#gsyn)
    - [Global Lump Cascading](#cascade)
    - [Priority of Level Header MapInfo](#priority)
  - [The Level Header MapInfo System](#maph)
    - [Specific Syntax Information](#mhsyn)
    - [Embedding Level-Specific MapInfo](#embed)
  - [Defaults](#defaults)

[]{#syn}

------------------------------------------------------------------------

**Basic Syntax**

------------------------------------------------------------------------

\
The basic syntax of MapInfo is very simple. Here are the rules:\
\

- All whitespace not between two non-whitespace characters is ignored.
- Any unknown variable name is ignored.
- All values are treated as text strings, and quotation marks are not
  required.
- Comments may start with \'#\', \';\', or \'//\'. All three comments
  are single line only, and extend to the end of the line.
- An optional equal sign can be placed between a variable and the value
  it is given. It is not an error to omit it.
- Block headers are required, and are enclosed in square brackets (\[
  \]). Block headers differ between global and level header MapInfo. See
  the respective sections in this documentation for what block headers
  you can use in a particular MapInfo lump.

[Return to Table of Contents](#contents)

[]{#vars}

------------------------------------------------------------------------

**MapInfo Variables**

------------------------------------------------------------------------

\
This section explains each variable which is available in MapInfo. All
variables are available in both global and level header MapInfo, and
accept the same values in both types.\
\

- **altskyname**\
  Name of a texture to display instead of the normal sky texture during
  a lightning strike. This value is only used if lightning is enabled
  for this level. If this value is not provided, no sky animation will
  occur during lightning strikes.\
  Example:

      altskyname = LBOLTSKY

- **boss-specials**\
  A flags field that specifies what formerly level-specific boss \"magic
  codepointer\" actions are enabled for this level. This field takes a
  value in the same format as that used by EDF, DeHackEd/BEX, and
  ExtraData flag fields, but the format will be reviewed here for
  completeness.\
  \
  A BEX flag list is a string of flag names separated by whitespace,
  pipe characters, commas, or plus characters. Flags are combined using
  bitwise-OR logic, so a flag name can be specified more than once.
  Specifying a flag name causes that flag to be turned on. The default
  state of all flags is off.\
  \
  These are the flag values which are available for this field:


          Flag name  Decimal  Hex     Meaning
          -----------------------------------------------------------------
          MAP07_1    1        0x0001  MAP07 Special 1 (BossDeath only)
          MAP07_2    2        0x0002  MAP07 Special 2 (BossDeath only)
          E1M8       4        0x0004  E1M8 Special
          E2M8       8        0x0008  E2M8 Special
          E3M8       16       0x0010  E3M8 Special
          E4M6       32       0x0020  E4M6 Special (BossDeath only)
          E4M8       64       0x0040  E4M8 Special
          E5M8       128      0x0080  E5M8 Special (HticBossDeath Only)
          -----------------------------------------------------------------

  Notes: Actions differ depending on whether monsters use the BossDeath
  or HticBossDeath codepointers. See the documentation for those
  specific codepointers to see what actions they perform. The monsters
  which must be dead for a given special to activate are controlled by
  various EDF/BEX thingtype flags. Together, these features allow
  complete customization.\
  \
  Example flags fields \-- All of these are equivalent:


          boss-specials = MAP07_1|MAP07_2
          boss-specials = MAP07_1 | MAP07_2
          
          boss-specials = MAP07_1+MAP07_2
          boss-specials = MAP07_2 + MAP07_2
          
          boss-specials = MAP07_1,MAP07_2
          boss-specials = MAP07_1, MAP07_2

          boss-specials = MAP07_1 MAP07_2

- **colormap**\
  Name of a colormap lump to use as the global colormap for this level.
  This colormap is applied to any sectors which do not have their own
  colormaps specified either via use of 242 height transfer linedefs or
  scripting. This lump must be between C_START and C_END tags.\
  Example:

      colormap = FOGMAP

- **creator**\
  The level author\'s name, which can be viewed in the console using the
  \"creator\" console command. If this value is not provided, the
  default value is \"unknown\".\
  Example:

      creator = Edwin Chadwick 'chadders'

- **doublesky**\
  Boolean value that determines whether or not this map uses double
  skies. When double skies are enabled, any F_SKY1 or F_SKY2 sectors
  \*not\* affected by MBF sky transfer lines will use both the primary
  and secondary sky textures together. The secondary texture will be
  drawn first, normally, then the primary texture will be drawn over it,
  omitting any pixels which use palette index zero (this is usually pure
  black). Any MapInfo sky delta values provided will be applied to their
  respective textures.\
  Example:

      doublesky = true

- **edf-intername**\
  Boolean value which, if true, enables the use of EDF-defined
  intermission map name strings which will be drawn using the FONTB
  graphics (provided in eternity.wad for DOOM/DOOM II, and natively
  supported by Heretic). This feature is meant to obsolete the levelpic
  variable and to remove the need for authors to create title graphics
  for their maps in DOOM/DOOM II. It also allows intermission map name
  replacement to work correctly for the next level and for Heretic
  without any hacks.\
  The EDF string which will be used as the intermission name for the
  current map should have the mnemonic \_IN_NAME\_\<mapname\>, where
  \<mapname\> is the exact name of the map\'s header lump (Example:
  \_IN_NAME_MAP01).\
  In order for the name of the next map to display correctly, the
  current level must either define its nextlevel/nextsecret values
  explicitly, or it must be an ExMy or MAPxy level.\
  Example:

      edf-intername = true

- **endofgame**\
  Boolean value which, if true, causes a DOOM II cast call to occur when
  this map is exited. This value is currently only usable in DOOM II. A
  level must also have intermission text defined for a cast call to take
  place.\
  Example:

      endofgame = true

- **extradata**\
  Name of the ExtraData script lump for this map. See the [ExtraData
  Reference](extradata.html) for full information on ExtraData. There
  are no naming restrictions on ExtraData script lumps.\
  Example:

      extradata = EXTRDT01

- **finale-secret**\
  Boolean value which, if true, indicates that a text intermission
  should only occur after this map if a secret exit was taken. If no
  intermission text is defined for this map, this variable has no
  effect.\
  Example:

      finale-secret = true

- **finaletype**\
  This string-valued field determines what type of finale will take
  place after a level\'s intermission, if one is set to occur (the
  intertext must be provided to trigger a finale).


          Value         Meaning
          ---------------------------------------------------------------------------
          text          Only the intertext will be displayed. Play will then continue
                        to the next level normally.
          doom_credits  The DOOM CREDITS screen will be displayed after the intertext.
                        Play ends after this map.
          doom_deimos   The DOOM picture of Deimos above Hell will be displayed after
                        the intertext. Play ends after this map.
          doom_bunny    The scrolling picture of Daisy's head on a pike will be 
                        displayed, accompanied by the words "THE END" which are shot 
                        up with several bullets. Play ends after this map.
          doom_marine   The E4 ending picture of the Marine holding Daisy's head is
                        displayed after the intertext. Play ends after this map.
          htic_credtis  The Heretic CREDITS screen will be displayed after the 
                        intertext. Play ends after this map.
          htic_water    The Heretic underwater screen with special palette is
                        displayed after the intertext. Play ends after this map.
          htic_demon    The Heretic demon scroller is displayed after the intertext.
                        Play ends after this map.
          ----------------------------------------------------------------------------

  Example:

      finaletype = text

- **fullbright**\
  Boolean value that determines whether fullbright can be used on this
  level. When fullbright is disabled, player gun flashes will not occur,
  and fullbright sprites and particles will be drawn normally. This
  should be used with most custom colormaps, since flashes and
  fullbright sprites usually look incorrect with them.\
  Example:

      fullbright = false

- **gravity**\
  Global gravity factor for the current level. The default gravity value
  is 65536.\
  Example:


          # Enable 50% gravity
          gravity = 32768

- **inter-backdrop**\
  Name of a 64x64 flat OR a 320x200 graphic lump to be displayed as the
  background of a text intermission which occurs after this map. The
  flat namespace will be searched first, so if both a flat and a 320x200
  picture of the same name exist, the flat will be chosen.\
  Example:

      inter-backdrop = FLATHUH1

- **intermusic**\
  Name of a MUS or MIDI lump to use as music during a text intermission
  which occurs after this map. If no intertext is defined for the map,
  this value will not be used. In DOOM gamemodes, the string provided
  should be the lump name minus the \"D\_\" prefix. In Heretic, the
  string provided should be the lump name minus the \"MUS\_\" prefix.
  You are \*not\* limited to using only the built-in music lump names;
  Eternity automatically adds any new lump which matches the naming
  convention for the current gamemode to the internal music list.\
  Example:

      intermusic = ROMERO

- **interpic**\
  Name of a 320x200 graphic lump to be displayed as the background of
  the statistics intermission. This value is currently only used for
  DOOM II and Ultimate DOOM episode 4 maps.\
  Example:

      interpic = BOSSBACK

- **intertext**\
  Lump name of a text lump to be loaded as intermission text for this
  level. If the level has an intermission by default, this will replace
  the default intermission text, even if it has been edited via
  DeHackEd. If there is not normally a text intermission after this map,
  the presence of this variable serves to enable it. If no intertext is
  defined, a text intermission will not occur after a map. The
  formatting of the text inside the indicated lump will be preserved
  exactly, with the exception of tabs, which are converted to spaces.
  There is no length limitation, but only one screen of text can be
  displayed. Any extra text will be drawn off the bottom of the screen.\
  Example:

      intertext = MAP04TXT

- **killfinale**\
  Boolean value that allows built-in text intermission points to be
  disabled. For example, if you do not want text to appear after DOOM II
  MAP11, set this variable to true.\
  Example:

      killfinale = true

- **killstats**\
  Boolean value that allows the statistics intermission between two maps
  to be disabled. If the map has a finale sequence to be displayed, it
  will start immediately, otherwise the next map will be loaded.\
  Example:

      killstats = true

- **levelname**\
  This is the name of the level which will be displayed in the automap.
  This value overrides any string provided via DeHackEd.\
  Example:

      levelname = chadders' lair

- **levelpic**\
  This is the name of a graphic lump which will be shown as the level
  name during the statistics intermission when this map is the previous
  map which was just beaten. Note: you cannot currently specify the
  graphic to be shown for the next map. This is a known bug in MapInfo
  and may not be repaired due to the new edf-intername feature. This
  value is only used for DOOM and DOOM II maps.\
  Example:

      levelpic = E1M1PIC

- **levelscript**\
  Name of a compiled Small script lump which will serve as the
  Levelscript for this map. See the [Small Usage
  Documentation](small_usagedoc.html) for full information on how the
  Levelscript works. There are no naming restrictions on script lumps.\
  Example:

      levelscript = SCRIPT01

- **leveltype**\
  String value that declares what type of level the current map is
  classified as. Possible values are \"doom\" and \"heretic\". This
  variable affects the automatic translation of thing types and line
  specials.\
  Example:

      leveltype = heretic

- **lightning**\
  Boolean value that toggles the global lightning effect. When set to
  true, all sky sectors on the map will flash at randomly determined
  intervals along with a thunder sound and optional animation of the
  normal sky texture. Warning: sectors must currently have a sky ceiling
  at the beginning of the level in order to be affected by lightning.
  The only current method to make non-sky sectors flash during lightning
  strikes is via use of BOOM lighting transfer linedefs. This may be
  improved in the near future.\
  Example:

      lightning = true

- **music**\
  Provides the name of a MUS or MIDI lump to use as music. In DOOM
  gamemodes, the string provided should be the lump name minus the
  \"D\_\" prefix. In Heretic, the string provided should be the lump
  name minus the \"MUS\_\" prefix. You are \*not\* limited to using only
  the built-in music lump names; Eternity automatically adds any new
  lump which matches the naming convention for the current gamemode to
  the internal music list.\
  Example:

      music = cheese

- **nextlevel**\
  The exact header name of the next level to go to when a normal exit
  occurs on this map. This can specify \*any\* lump name, but the lump
  should be a valid level header or an error will occur.\
  Example:

      nextlevel = CHAD2

- **nextsecret**\
  The exact header name of the next level to go to when a secret exit
  occurs on this map. This can specify \*any\* lump name, but the lump
  should be a valid level header or an error will occur. Note that
  except for maps which normally have secret exits, the default behavior
  is to repeat the same level. This MapInfo value allows you to avoid
  that and use a secret exit on any map.\
  Example:

      nextsecret = MAP34

- **noautosequences**\
  Determines the default assignment of sound sequences. If set to true,
  no sound sequences will be automatically assigned to sectors. The
  sequence of all sectors by default will be sequence number 0. If set
  to false, the default sound sequences defined in **sounds.edf** will
  be used. These sequences implement the normal, default behaviors for
  DOOM and Heretic, and can be easily overridden by the user if
  desired.\
  Example:

      noautosequences = true

- **partime**\
  Partime for the map in seconds. This value overrides any value
  provided via DeHackEd. Note that partimes are only supported in
  episodes 1-3 of DOOM and any map of DOOM II. Ultimate DOOM episode 4,
  Heretic, and non-standard maps currently never display par times.\
  Example:

      partime = 200

- **skydelta**\
  Pixels per gametic that the primary sky texture should scroll. This
  effect will only be applied to double skies and to F_SKY1 sectors
  which are NOT affected by any MBF sky transfer linedefs.\
  Example:

      skydelta = 5

- **sky2delta**\
  Pixels per gametic that the secondary sky texture should scroll. This
  effect will only be applied to double skies and to F_SKY2 sectors
  (which cannot be affected by MBF sky transfer linedefs).\
  Example:

      sky2delta = 7

- **skyname**\
  Name of a texture to use as the primary sky for this map. This sky
  will appear in all sectors which use the F_SKY1 flat, and it will also
  appear as the top layer of Hexen-style double skies. The name provided
  for this lump must be a valid texture or an error will occur.\
  Example:

      skyname = SKY2

- **sky2name**\
  Name of a texture to use as the secondary sky for this map. This sky
  will appear in all sectors which use the F_SKY2 flat, and it will also
  appear as the bottom layer of Hexen- style double skies. The name
  provided for this lump must be a valid texture or an error will occur.
  If no name is provided for the secondary sky, the primary sky name
  will be copied to it, so it is not an error to use F_SKY2 or double
  skies in a level where the sky2 texture is not explicitly defined.\
  Example:

      sky2name = SKY4

- **unevenlight**\
  Boolean value that determines whether walls parallel to the x and y
  axes of the game world are given lighting values 1 level higher or
  lower than other walls. This can dramatically affect the perceived
  light level of an area, so only use it for maps that are designed
  specifically for it. Maps which use custom global colormaps should
  usually set this value to false, since it only generally looks
  appropriate with the normal colormap.\
  Example:

      unevenlight = false

- **sound-swtchn**\
  EDF mnemonic of a sound to replace the default \"swtchn\" sound for
  this map.\
  \

- **sound-swtchx**\
  EDF mnemonic of a sound to replace the default \"swtchx\" sound for
  this map.\
  \

- **sound-stnmov**\
  EDF mnemonic of a sound to replace the default \"stnmov\" sound when
  used for plat actions on this map.\
  \

- **sound-pstop**\
  EDF mnemonic of a sound to replace the default \"pstop\" sound for
  this map.\
  \

- **sound-pstart**\
  EDF mnemonic of a sound to replace the default \"pstart\" sound for
  this map.\
  \

- **sound-bdcls**\
  EDF mnemonic of a sound to replace the default \"bdcls\" sound for
  this map.\
  \

- **sound-bdopn**\
  EDF mnemonic of a sound to replace the default \"bdopn\" sound for
  this map.\
  \

- **sound-dorcls**\
  EDF mnemonic of a sound to replace the default \"dorcls\" sound for
  this map.\
  \

- **sound-doropn**\
  EDF mnemonic of a sound to replace the default \"doropn\" sound for
  this map.\
  \

- **sound-fcmove**\
  EDF mnemonic of a sound to replace the default \"stnmov\" sound when
  used for floor and ceiling movement on this map.

[Return to Table of Contents](#contents)

[]{#global}

------------------------------------------------------------------------

**The Global MapInfo System**

------------------------------------------------------------------------

\
Starting with Eternity Engine v3.31 \'Delta\', Eternity supports global
MapInfo. Global MapInfo has mostly the same format as level header
MapInfo, except the information for multiple maps can be placed into a
single lump named EMAPINFO. Any number of EMAPINFO lumps can exist and
will cascade as documented below.\
\
[Return to Table of Contents](#contents)

[]{#gsyn}

------------------------------------------------------------------------

**Specific Syntax Information**

------------------------------------------------------------------------

\
In the global MapInfo lump, variables for each level must be placed
under a section header that has the same name as the map\'s header
lump.\
Example:

         [START]
         levelname = Eternity Start Map
         music = romero
         creator = Derek 'Afterglow' Mac Donald
         skyname = SKY3
         levelscript = STARTSCR

This would assign all the values beneath the \[START\] section header
and above any subsequent header to the map named START (in this case,
the Eternity startmap).

Notes:

- Entries can exist for maps that do not exist; such entries will never
  be used.
- Any number of maps can have entries in the EMAPINFO lump.
- Only the first section defined for a map in an EMAPINFO lump will be
  used. If another section for the same map exists in the same lump, it
  will be ignored.

[Return to Table of Contents](#contents)

[]{#cascade}

------------------------------------------------------------------------

**Global Lump Cascading**

------------------------------------------------------------------------

\
As mentioned in the introduction to this section, global MapInfo lumps
can cascade. This means that an entry for a map will be searched for
starting with the newest EMAPINFO lump in the wad directory (this will
be the last such lump in the last wad to be loaded). If an entry is
found for the map in that lump, it will be used and any others are
ignored. If an entry is not found, processing continues through the
lumps in the newest wad, and then through lumps in all other wads in the
reverse order they were loaded. If no MapInfo entry is found for a map,
the default values for all variables will apply.\
\
[Return to Table of Contents](#contents)

[]{#priority}

------------------------------------------------------------------------

**Priority of Level Header MapInfo**

------------------------------------------------------------------------

\
If a map has both global and level header MapInfo entries, the global
entry will be processed first, and then the level header MapInfo will be
processed. This means that level header MapInfo has a higher priority
than global MapInfo, and values specified in the level header will
overwrite values provided by the global lump. This allows maps to keep
their specified individual behaviors even when used with any combination
of wads which have global MapInfo lumps.\
\
[Return to Table of Contents](#contents)

[]{#maph}

------------------------------------------------------------------------

**The Level Header MapInfo System**

------------------------------------------------------------------------

\
SMMU added support for embedding MapInfo into level headers. Although
level header lumps are usually zero length, this is not required by the
engine itself, making the level header a good place to store information
associated with a particular map.

Since Eternity now supports global MapInfo lumps, the use of level
header MapInfo is not required. It does allow for greater flexibility
when combining wads, however, and ensures that a map will keep critical
customized behaviors even in the presence of global MapInfo.

Note that Eternity currently only supports specification of MapInfo
blocks within the level header. SMMU allowed several other blocks,
including one for intertext and one for FraggleScript. Eternity does not
support these blocks and will ignore them if they exist.\
\
[Return to Table of Contents](#contents)

[]{#mhsyn}

------------------------------------------------------------------------

**Specific Syntax Information**

------------------------------------------------------------------------

\
In level header MapInfo, variables for the current level must be placed
under a section header that has the name \"level info\".\
Example:

         [level info]
         levelname = Eternity Start Map
         music = romero
         creator = Derek 'Afterglow' Mac Donald
         skyname = SKY3
         levelscript = STARTSCR

This would assign all the values beneath the \[level info\] section
header and above any subsequent header to the map that possesses this
information in its map header lump, overriding any of the same values
which were specified via global MapInfo lumps.

Notes:

- If multiple \[level info\] sections exist in level header MapInfo, all
  of them will be parsed from top to bottom.

[Return to Table of Contents](#contents)

[]{#embed}

------------------------------------------------------------------------

**Embedding Level-Specific MapInfo**

------------------------------------------------------------------------

\
Some editors now include support for embedding MapInfo data into the
level header. We recommend the use of Doom Builder for this
functionality. Doom Builder will always respect any existing MapInfo,
whereas many older editors will strip it out when saving the file (this
includes DETH). Doom Builder also has good support for most other
features of the Eternity Engine, and it is open source freeware.

Please be aware that if your editor strips out data in the level header
when saving maps, you will need to re-embed the level-specific MapInfo
after each edit. This and other problems that external utilities have
with data in the level header are one reason why Eternity now supports
global MapInfo. Consider using it instead, or using it only during level
development and then converting it to level header MapInfo if you wish.

We do not recommend use of the old add_fs utility and will not provide
support for its use, since it is a DOS command-line utility and only
supports wads which contain one map.

[Return to Table of Contents](#contents)

[]{#defaults}

------------------------------------------------------------------------

**Defaults**

------------------------------------------------------------------------

\
Here you can find specification of the default values that will be used
for maps. Some of the defaults can be edited by DeHackEd as well.

- **altskyname**\
  When this variable is not defined, no animation takes place during
  lightning strikes, so there is effectively no default value.\
  \
- **boss-specials**\
  This variable has special defaults for the following maps:
  - DOOM II MAP07: \"MAP07_1 \| MAP07_2\"
  - DOOM/Heretic E1M8: \"E1M8\"
  - DOOM/Heretic E2M8: \"E2M8\"
  - DOOM/Heretic E3M8: \"E3M8\"
  - DOOM/Heretic E4M6: \"E4M6\" \*\* Note: HticBossDeath does not
    recognize E4M6
  - DOOM/Heretic E4M8: \"E4M8\"
  - DOOM/Heretic E5M8: \"E5M8\" \*\* Note: BossDeath does not recognize
    E5M8
  - All other maps: No specials are enabled.

  \
- **colormap**\
  The default value of this variable for all maps is the normal global
  COLORMAP lump. Note that COLORMAP is a special case colormap which is
  not required to exist between C_START and C_END lumps. All other
  colormaps must follow that rule.\
  \
- **creator**\
  The default value of this variable for all maps is \"unknown\".\
  \
- **doublesky**\
  The default value of this variable for all maps is \"false\".\
  \
- **edf-intername**\
  In Heretic, this variable defaults to \"true\" for all maps and cannot
  be disabled. Appropriate default values are defined in Eternity\'s
  misc.edf file. In all other gamemodes, this value defaults to
  \"false\" for all maps. When this value is \"true\" and a proper
  string object is defined in EDF, the \"levelpic\" variable will be
  completely ignored.\
  \
- **endofgame**\
  The default value of this variable for all maps except DOOM II MAP30
  is \"false\".\
  The default value for DOOM II MAP30 is \"true\".\
  \
- **extradata**\
  There is no default value for this variable on any map. If no lump
  name is specified, the level does not have ExtraData. Use of ExtraData
  control things or specials in a map which does not have ExtraData may
  result in \"Unknown\" things and/or error messages displayed to the
  player.\
  \
- **finale-secret**\
  The default value for all maps except DOOM II maps MAP15 and MAP31 is
  \"false\".\
  The default values for DOOM II maps MAP15 and MAP31 is \"true\".\
  \
- **finaletype**\
  This variable has special defaults for the following maps:
  - DOOM E1M8: doom_credits
  - DOOM E2M8: doom_deimos
  - DOOM E3M8: doom_bunny
  - DOOM E4M8: doom_marine
  - All other maps in DOOM: text
  - All maps in DOOM II: text
  - Heretic E1M8: htic_credits
  - Heretic E2M8: htic_water
  - Heretic E3M8: htic_demon
  - Heretic E4M8: htic_credits
  - Heretic E5M8: htic_credits
  - All other maps in Heretic: text

  \
- **fullbright**\
  The default value of this variable for all maps is \"true\".\
  \
- **gravity**\
  The default value of this variable for all maps is 65536.\
  \
- **inter-backdrop**\
  This variable has special defaults for the following maps:
  - DOOM E1M8: BEX String BGFLATE1 (FLOOR4_8)
  - DOOM E2M8: BEX String BGFLATE2 (SFLR6_1)
  - DOOM E3M8: BEX String BGFLATE3 (MFLR8_4)
  - DOOM E4M8: BEX String BGFLATE4 (MFLR8_3)
  - All other maps in DOOM: BEX String BGFLATE1 (FLOOR4_8)
  - DOOM II MAP06: BEX String BGFLAT06 (SLIME16)
  - DOOM II MAP11: BEX String BGFLAT11 (RROCK14)
  - DOOM II MAP20: BEX String BGFLAT20 (RROCK07)
  - DOOM II MAP30: BEX String BGFLAT30 (RROCK17)
  - DOOM II MAP15: BEX String BGFLAT15 (RROCK13)
  - DOOM II MAP31: BEX String BGFLAT31 (RROCK19)
  - All other maps in DOOM II: BEX String BGFLAT06 (SLIME16)
  - Heretic E1M8: BEX String BGFLATHE1 (FLOOR25)
  - Heretic E2M8: BEX String BGFLATHE2 (FLATHUH1)
  - Heretic E3M8: BEX String BGFLATHE3 (FLTWAWA1)
  - Heretic E4M8: BEX String BGFLATHE4 (FLOOR28)
  - Heretic E5M8: BEX String BGFLATHE5 (FLOOR08)
  - All other maps in Heretic: BEX String BGFLATHE1 (FLOOR25)
  - Undefined gamemode: F_SKY2 (this flat is present in eternity.wad)

  \
- **intermusic**\
  This variable has the following defaults for all maps by gamemode:
  - DOOM: \"victor\"
  - DOOM II: \"read_m\"
  - Heretic: \"cptd\"
  - Undefined gamemode: no music will be played.

  \
- **interpic**\
  This variable has the default value \"INTERPIC\" for all maps. Note
  that this variable is not currently usable in the first 3 episodes of
  DOOM or in Heretic.\
  \
- **intertext**\
  This variable has special defaults for the following maps:
  - DOOM E1M8: BEX String E1TEXT (\"Once you beat the big badasses
    and\\nclean out \"\...)
  - DOOM E2M8: BEX String E2TEXT (\"You\'ve done it! The hideous
    cyber-\\ndemon lord\"\...)
  - DOOM E3M8: BEX String E3TEXT (\"The loathsome spiderdemon
    that\\nmasterminded t\"\...)
  - DOOM E4M8: BEX String E4TEXT (\"the spider mastermind must have sent
    forth\\nit\"\...)
  - All other maps in DOOM: No default. No text intermission will occur.
  - DOOM II MAP06
    - \"commercial\" game mission: BEX String C1TEXT (\"YOU HAVE ENTERED
      DEEPLY INTO THE INFESTED\\nSTA\"\...)
    - Evilution game mission: BEX String T1TEXT (\"You\'ve fought your
      way out of the infested\\nex\"\...)
    - Plutonia game mission: BEX String P1TEXT (\"You gloat over the
      steaming carcass of the\\nGu\"\...)
  - DOOM II MAP11
    - \"commercial\" game mission: BEX String C2TEXT (\"YOU HAVE WON!
      YOUR VICTORY HAS ENABLED\\nHUMANK\"\...)
    - Evilution game mission: BEX String T2TEXT (\"You hear the grinding
      of heavy machinery\\nahea\"\...)
    - Plutonia game mission: BEX String P2TEXT (\"Even the deadly
      Arch-Vile labyrinth could\\nnot\"\...)
  - DOOM II MAP20
    - \"commercial\" game mission: BEX String C3TEXT (\"YOU ARE AT THE
      CORRUPT HEART OF THE CITY,\\nSUR\"\...)
    - Evilution game mission: BEX String T3TEXT (\"The vista opening
      ahead looks real damn\\nfamil\"\...)
    - Plutonia game mission: BEX String P3TEXT (\"You\'ve bashed and
      battered your way into\\nthe \"\...)
  - DOOM II MAP30
    - \"commercial\" game mission: BEX String C4TEXT (\"THE HORRENDOUS
      VISAGE OF THE BIGGEST\\nDEMON YO\"\...)
    - Evilution game mission: BEX String T4TEXT (\"Suddenly, all is
      silent, from one horizon\\nto \"\...)
    - Plutonia game mission: BEX String P4TEXT (\"The Gatekeeper\'s evil
      face is splattered\\nall \"\...)
  - DOOM II MAP15
    - \"commercial\" game mission: BEX String C5TEXT (\"CONGRATULATIONS,
      YOU\'VE FOUND THE SECRET\\nLEVE\"\...)
    - Evilution game mission: BEX String T5TEXT (\"What now? Looks
      totally different. Kind\\nof li\"\...)
    - Plutonia game mission: BEX String P5TEXT (\"You\'ve found the
      second-hardest level we\\ngot.\"\...)
  - DOOM II MAP31
    - \"commercial\" game mission: BEX String C6TEXT (\"CONGRATULATIONS,
      YOU\'VE FOUND THE\\nSUPER SECRE\"\...)
    - Evilution game mission: BEX String T6TEXT (\"Time for a vacation.
      You\'ve burst the\\nbowels \"\...)
    - Plutonia game mission: BEX String P6TEXT (\"Betcha wondered just
      what WAS the hardest\\nlev\"\...)
  - All other maps in DOOM II: No default. No text intermission will
    occur.
  - Heretic E1M8: BEX String H1TEXT (\"with the destruction of the
    iron\\nliches and t\"\...)
  - Heretic E2M8: BEX String H2TEXT (\"the mighty maulotaurs have
    proved\\nto be no ma\"\...)
  - Heretic E3M8: BEX String H3TEXT (\"the death of d\'sparil has
    loosed\\nthe magical \"\...)
  - Heretic E4M8: BEX String H4TEXT (\"you thought you would return to
    your\\nown worl\"\...)
  - Heretic E5M8: BEX String H5TEXT (\"as the final maulotaur bellows
    his\\ndeath-agon\"\...)
  - All other maps in Heretic: No default. No text intermission will
    occur.
  - Undefined gamemode: No default. No text intermission will occur.

  \
- **killfinale**\
  This variable has the default value of \"false\" for all maps.\
  \
- **killstats**\
  This variable has the default value of \"false\" for all maps except
  for maps in the \"hidden\" episode of Heretic (Episode 4 in registered
  Heretic, Episode 6 in Heretic: Shadow of the Serpent Riders) and in
  any higher episode. Those maps default to \"true\".\
  \
- **levelname**\
  This variable has a special default for the following maps. Default
  values are available in the [Eternity Engine DeHackEd / BEX
  Reference](dehref.html) and will not be duplicated here because of
  space constraints.
  - DOOM maps E1M1-E1M9, E2M1-E2M9, E3M1-E3M9, E4M1-E4M9
  - DOOM II maps MAP01 - MAP32
  - Heretic maps E1M1-E1M9, E2M1-E2M9, E3M1-E3M9, E4M1-E4M9, E5M1-E5M9
  - Other maps in DOOM or DOOM II: \"new level\"
  - Maps in Heretic with episode 4 or higher: \"hidden level\"
  - Maps in Heretic: SoSR with episode 6 or higher: \"hidden level\"

  Note that when user wads are added, maps from new wads which do not
  have a DeHackEd or MapInfo level name will use \"new level\".\
  \
- **levelpic**\
  This variable has the following special default values:
  - DOOM maps ExM1 - ExM9: WILVxy where x is the episode number minus 1
    and y is the level number minus 1.
  - DOOM II maps MAP01 - MAP32: CWILVxy where xy is the 2-digit level
    number minus 1.

  This value cannot be used for Heretic maps. If the edf-intername
  variable is set to true, no levelpic will be drawn by the
  intermission. Instead, text strings defined by EDF with the proper
  names will be written to the screen using the large font (FONTB).\
  \
- **levelscript**\
  This variable has no default value. If a Levelscript lump is not
  defined for a map, that map has no Levelscript. Attempted use of the
  Levelscript under that circumstance may result in error messages
  displayed to the player.\
  \
- **leveltype**\
  The default value of this variable depends on the game being played:
  - DOOM (Shareware, Registered, Ultimate, DOOM II, Final): doom
  - Heretic (Shareware, Registered, SoSR) : heretic

  \
- **lightning**\
  This variable has the default value of \"false\" for all maps.\
  \
- **music**\
  This variable has the following special default values:
  - DOOM maps E1M1-E1M9: E1M1, E1M2, E1M3\... E1M9
  - DOOM maps E2M1-E2M9: E2M1, E2M2, E2M3\... E2M9
  - DOOM maps E3M1-E3M9: E3M1, E3M2, E3M3\... E3M9
  - DOOM maps E4M1-E4M9: E3M4, E3M2, E3M3, E1M5, E2M7, E2M4, E2M6, E2M5,
    E1M9
  - DOOM II maps MAP01-MAP32:\
    runnin, stalks, countd, betwee, doom, the_da, shawn, ddtblu, in_cit,
    dead,\
    stlks2, theda2, doom2, ddtbl2, runni2, dead2, stlks3, romero,
    shawn2, messag,\
    count2, ddtbl3, ampie, theda3, adrian, messg2, romer2, tense,
    shawn3, openin,\
    evil, ultima
  - Heretic maps E1M1-E1M9: E1M1, E1M2, E1M3, E1M4, E1M5, E1M6, E1M7,
    E1M8, E1M9
  - Heretic maps E2M1-E2M9: E2M1, E2M2, E2M3, E2M4, E1M4, E2M6, E2M7,
    E2M8, E2M9
  - Heretic maps E3M1-E3M9: E1M1, E3M2, E3M3, E1M6, E1M3, E1M2, E1M5,
    E1M9, E2M6
  - Heretic maps E4M1-E4M9: E1M6, E1M2, E1M3, E1M4, E1M5, E1M1, E1M7,
    E1M8, E1M9
  - Heretic maps E5M1-E5M9: E2M1, E2M2, E2M3, E2M4, E1M4, E2M6, E2M7,
    E2M8, E2M9
  - Heretic maps E6M1-E6M9: E3M2, E3M3, E1M6, TITL, INTR, CPTD, E1M1,
    E1M1, E1M1
  - Other maps: No music will be defined. An error message may be shown
    to the player.

  Remember that in DOOM gamemodes, a \"D\_\" prefix is added to the
  music name automatically. In Heretic gamemodes, a \"MUS\_\" prefix is
  added. You should NOT add the prefix yourself.\
  \
- **nextlevel**\
  This variable has the following special default values:
  - DOOM maps ExMy: The next normal level in the episode.
  - DOOM II maps MAPxx: The next map.
  - DOOM II maps ExMy: The next numeric ExMy map.
  - Heretic maps ExMy: The next normal level in the episode.
  - Other maps: Maps outside the normal episode range will be wrapped
    into range if no MapInfo value is provided. This means that an E5M1
    map in Ultimate DOOM would exit to E4M2 if no MapInfo value is
    provided. In DOOM II, the next level MUST be defined or an error
    will occur. Eternity allows playing of ExMy maps under DOOM II, but
    the levels always exit to the next level in numeric order, including
    secret maps and crossing episode boundaries.\
    \
- **nextsecret**\
  This variable has the following special default values:
  - All DOOM maps: ExM9 where x is the current episode number.
  - DOOM II map 15: MAP31
  - DOOM II map 31: MAP32
  - All other DOOM II maps: repeat the same level.
  - All Heretic maps: ExM9 where x is the current episode number.

  Note: if nextsecret is defined for a map that does not normally have a
  secret exit, the secret map must also define its nextlevel explicitly.
  Otherwise, the player may become stuck in the secret level.\
  \
- **noautosequences**\
  This variable has the default value false for all maps.\
  \
- **partime**\
  This variable has the following special default values:
  - DOOM maps E1M1-E1M9: 30, 75, 120, 90, 165, 180, 180, 30, 165
  - DOOM maps E2M1-E2M9: 90, 90, 90, 120, 90, 360, 240, 30, 170
  - DOOM maps E3M1-E3M9: 90, 45, 90, 150, 90, 90, 165, 30, 135
  - DOOM II maps MAP01-MAP34:\
    30, 90, 120, 120, 90, 150, 120, 120, 270, 90\
    210, 150, 150, 150, 210, 150, 420, 150, 210, 150\
    240, 150, 180, 150, 150, 300, 330, 420, 300, 180\
    120, 30, 30, 30

  Note that par times are not supported for Ultimate DOOM episode 4 or
  Heretic. Even if par values are set for those maps, they will not be
  displayed by the intermission. Also, if DeHackEd/BEX par times are not
  loaded and a new map does not provide a MapInfo par time, the default
  par time for that map will not be displayed, since it does not apply
  to user-made levels.\
  \
- **skydelta**\
  This variable has the default value \"0\" (zero) for all maps.\
  \
- **sky2delta**\
  This variable has the default value \"0\" (zero) for all maps.\
  \
- **skyname**\
  This variable has the following special default values:
  - DOOM Episode 1: SKY1
  - DOOM Episode 2: SKY2
  - DOOM Episode 3: SKY3
  - DOOM Episode 4: SKY4
  - All other DOOM maps: SKY4
  - DOOM II MAP01-MAP11: SKY1
  - DOOM II MAP12-MAP20: SKY2
  - All other DOOM II maps: SKY3
  - Heretic Episode 1: SKY1
  - Heretic Episode 2: SKY2
  - Heretic Episode 3: SKY3
  - Heretic Episode 4: SKY1
  - Heretic Episode 5: SKY3
  - All other Heretic maps: SKY1

  In the original engine, levels with undefined sky textures would
  display the AASTINKY or AASHITTY textures in the sky via a bug in the
  texture lookup code. This is not supported by Eternity, which always
  defines a suitable default sky texture for all maps.\
  \
- **sky2name**\
  When this variable is not specified by MapInfo, the value of the
  normal, primary sky texture is copied to it.\
  \
- **unevenlight**\
  This variable defaults to the value \"true\" for all DOOM and Heretic
  maps.
