## Eternity Engine DeHackEd / BEX Reference v2.06 \-- 10/22/06

[Return to the Eternity Engine Page](../etcengine.html)

- [**Table of Contents**]{#contents}
  - [Revisions](#history)
  - [General DeHackEd Information](#general)
    - [What is DeHackEd?](#what)
    - [General Syntax](#syntax)
    - [BEX Extension: INCLUDE Directive](#include)
  - [Thing Block](#things)
    - [Description](#tngdesc)
    - [Syntax](#tngsyntax)
    - [Fields](#tngfields)
    - [BEX Extension: Thing Flag Lists](#tngflags)
    - [Bits Mnemonics](#tngbits)
    - [Bits2 Mnemonics (Eternity extension)](#tngbits2)
    - [Bits3 Mnemonics (Eternity extension)](#tngbits3)
  - [Frame Block](#frames)
    - [Description](#framedesc)
    - [Syntax](#framesyntax)
    - [Fields](#framefields)
  - [Pointer Block](#pointer) **(Deprecated)**
    - [Description](#ptrdesc)
    - [Syntax](#ptrsyntax)
  - [Sound Block](#sound)
    - [Description](#sounddesc)
    - [Syntax](#soundsyntax)
    - [Fields](#soundfields)
  - [Ammo Block](#ammo)
    - [Description](#ammodesc)
    - [Syntax](#ammosyntax)
    - [Fields](#ammofields)
  - [Weapon Block](#weapons)
    - [Description](#weapondesc)
    - [Syntax](#weapsyntax)
    - [Fields](#weapfields)
  - [Cheat Block](#cheats)
    - [Description](#cheatdesc)
    - [Syntax](#cheatsyntax)
  - [Misc Block](#misc)
    - [Description](#miscdesc)
    - [Syntax](#miscsyntax)
    - [Fields](#miscfields)
  - [Text Block](#text) **(Deprecated)**
    - [Description](#textdesc)
    - [Syntax](#textsyntax)
  - [Sprite Block](#sprite) **(Obsolete)**
    - [Obsoletion Info](#spriteobs)
  - [BEX Extension: \[STRINGS\] Block](#strings)
    - [Description](#stringdesc)
    - [Syntax](#stringsyntax)
  - [BEX Extension: \[PARS\] Block](#pars)
    - [Description](#pardesc)
    - [Syntax](#parsyntax)
  - [BEX Extension: \[CODEPTR\] Block](#codeptr)
    - [Description](#cptrdesc)
    - [Syntax](#cptrsyntax)
  - [Eternity Extension: \[HELPER\] Block](#helper)
    - [Description](#helperdesc)
    - [Syntax](#helpersyntax)
  - [Eternity Extension: \[SPRITES\] Block](#sprites)
    - [Description](#spritesdesc)
    - [Syntax](#spritessyntax)
  - [Eternity Extension: \[SOUNDS\] Block](#sounds)
    - [Description](#soundsdesc)
    - [Syntax](#soundssyntax)
  - [Eternity Extension: \[MUSIC\] Block](#music)
    - [Description](#musicdesc)
    - [Syntax](#musicsyntax)
  - [BEX String Mnemonics Table](#textmn)
    - [Part 1 - GENERAL INITIALIZATION AND PROMPTS](#tpart1)
    - [Part 2 - MESSAGES WHEN THE PLAYER GETS THINGS](#tpart2)
    - [Part 3 - MESSAGES WHEN KEYS ARE NEEDED](#tpart3)
    - [Part 4 - MULTIPLAYER MESSAGING](#tpart4)
    - [Part 5 - LEVEL NAMES IN THE AUTOMAP](#tpart5)
    - [Part 6 - MESSAGES AS A RESULT OF TOGGLING STATES](#tpart6)
    - [Part 7 - EPISODE INTERMISSION TEXTS](#tpart7)
    - [Part 8 - CREATURE NAMES FOR THE FINALE](#tpart8)
    - [Part 9 - INTERMISSION TILED BACKGROUNDS](#tpart9)
    - [Part 10 - NEW: STARTUP BANNERS](#tpart10)
    - [Part 11 - NEW: OBITUARIES](#tpart11)
    - [Part 12 - NEW: DEMO SEQUENCE/HELP/CREDITS](#tpart12)
  - [Sound Table](#sndtable)

[]{#history}

------------------------------------------------------------------------

**Revisions**

------------------------------------------------------------------------

- 10/22/06: Added Bits3 flags ALWAYSFAST, PASSMOBJ, DONTOVERLAP. Updated
  BEX strings.
- 9/12/05: Added Bits3 flag 3DDECORATION
- 6/17/05: Added Bits3 flag NOFRIENDDMG.
- 5/26/05: Added Bits3 flags SPACMONSTER and SPACMISSILE.
- 2/10/05: Added Bits3 flag TLSTYLEADD.

[Return to Table of Contents](#contents)

[]{#general}

------------------------------------------------------------------------

**General DeHackEd Information**

------------------------------------------------------------------------

\
[]{#what}

------------------------------------------------------------------------

**What is DeHackEd?**

------------------------------------------------------------------------

\
DeHackEd with BOOM Extensions (BEX) is a form of text script for
modifying various aspects of the DOOM engine. Originally, DeHackEd
patches were used by the DeHackEd program to modify the DOOM.EXE file
itself. Now, modern source ports provide alternative methods of use.\
\
Eternity allows use of DeHackEd files via the -deh command-line
parameter, via GFS files, and as a text lump called \"DEHACKED\" in wad
files. Eternity can also specify up to two DeHackEd patches which are
always added at start-up in its configuration file. Starting with
Eternity Engine v3.31 public beta 5, DeHackEd/BEX files can also be
included by EDF modules.\
\
DeHackEd allows editing of things, frames, codepointers, ammo and weapon
data, cheats, miscellaneous settings, and text strings. Through BEX and
Eternity extensions, support has also been added for a cleaner form of
text substitution, default level par times, a more powerful method of
codepointer editing, helper thing type editing, and better support for
substitution of sprite, sound, and music names.\
\
DeHackEd does NOT support the addition or deletion of any of these
elements, however. It is limited to the number and order of entries as
they exist in the source port. For information on how Eternity addresses
this problem via the EDF system, see the [EDF
Documentation](edf_ref.html).\
\
[Return to Table of Contents](#contents)

[]{#syntax}

------------------------------------------------------------------------

**General Syntax**

------------------------------------------------------------------------

\
White-space, empty lines, and unrecognized tokens are ignored outside of
any block, and the DeHackEd parser is in general very forgiving.\
\
Comments can be provided for your own benefit, or for that of other
editors. Comments start with a pound sign (#) and extend to the end of
the line. Example:


       # This is a comment

Note that comments are not allowed within some blocks, including several
of the BEX extension blocks. It is best to keep all comments outside of
blocks for this reason.\
\
It is common for this header to appear in DeHackEd files which pre-date
the DOOM source release:


       Patch File for DeHackEd v3.0

       # Note: Use the pound sign ('#') to start comment lines.

       Doom version = 21
       Patch format = 6

However, none of this information is needed or used by Eternity in any
way, and it is discarded as unrecognized text. You do not need to
include any header information in your own files. Note that the patch
version information is important for old patches in that it will tell
you whether or not Eternity can use the file. Patches of format other
than 6 are not guaranteed to work, and Eternity does NOT support the
older, obsolete binary patch format.\
\
[Return to Table of Contents](#contents)

[]{#include}

------------------------------------------------------------------------

**BEX Extension: INCLUDE Directive**

------------------------------------------------------------------------

\
To include a DEH or BEX file in another one, put one or more of the
following lines in your BEX file:


       INCLUDE filename
       INCLUDE NOTEXT filename

Filename must be specified in full, including a path if it\'s not going
to be in the current directory, and including its .deh or .bex
extension. It is better by far to ship a file completely self-contained,
but you can use this during testing and development, or to adjust
existing patches for your own use.\
\
The NOTEXT directive will cause Eternity to skip any DeHackEd-style text
blocks in the included file, with the assumption that you\'re doing
something else in your main BEX file instead.\
\
You may not nest include files (one include file can\'t include
another). In addition, DEHACKED lumps in wads cannot use INCLUDE
directives in any form.\
\
[Return to Table of Contents](#contents)

[]{#things}

------------------------------------------------------------------------

**Thing Block**

------------------------------------------------------------------------

\
[]{#tngdesc}

------------------------------------------------------------------------

**Things: Description**

------------------------------------------------------------------------

\
The DeHackEd Thing block allows you to edit the properties of DOOM\'s
map object types. Map objects are what comprise anything you see with a
sprite in DOOM, including enemies, powerups, missiles, decorations, and
invisible control objects.\
\
Eternity provides several extensions to the DeHackEd thing block, some
of which are considered part of the BEX standard specification. Extended
fields will be noted as such in their descriptions.\
\
[Return to Table of Contents](#contents)

[]{#tngsyntax}

------------------------------------------------------------------------

**Things: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
All fields in the block are optional. If a field is not provided, that
value of the thing being edited will not change.\
A \|\| symbol means that the field can have one of the two listed types
of values.\
\


       Thing <dehacked number> [(thing type name)]
       ID # = <number>
       Initial frame = <frame number>
       Hit points = <number>
       First moving frame = <frame number>
       Alert sound = <sound number>
       Reaction time = <number>
       Attack sound = <sound number>
       Injury frame = <frame number>
       Pain chance = <number>
       Pain sound = <sound number>
       Close attack frame = <frame number>
       Far attack frame = <frame number>
       Death frame = <frame number>
       Exploding frame = <frame number>
       Death sound = <sound number>
       Speed = <number>
       Width = <number>
       Height = <number>
       Mass = <number>
       Missile damage = <number>
       Action sound = <sound number>
       # The ability to provide flag lists is a BEX extension
       Bits = <number || flag list>
       Respawn frame = <frame number>      
       # The below fields are Eternity Engine extensions
       Bits2 = <number || flag list>
       Bits3 = <number || flag list>
       Translucency = <number>
       Blood color = <number>   

Explanation of special information:\
\
**dehacked number** is the DeHackEd number of the thing type that this
block edits. The DeHackEd number for a thing type is given in its EDF
**thingtype** definition.\
\
**(thing type name)** is an optional string which gives a descriptive
name to this block. This field is NOT used by Eternity, and can be left
off. Its only purpose is to serve as a comment letting you better know
which thing type you are editing.\
\
[Return to Table of Contents](#contents)

[]{#tngfields}

------------------------------------------------------------------------

**Things: Fields**

------------------------------------------------------------------------

\

- **ID \# = \<number\>**\
  Sets the id, or \"doomednum\", used to identify this thing type in
  maps. For example, when a map calls for a thing with id 2003, the game
  engine will find the thing type which has that number in this field.
  Every thing that will be spawned on maps must have a unique ID number.
  Things which are not spawned on maps, such as fireballs, can be given
  an ID of -1.\
  \

- **Initial frame = \<frame number\>**\
  Sets the frame which a thing starts in when it is spawned. Monsters
  generally use this frame to stand still and look for targets. \<frame
  number\> must be a valid frame DeHackEd number.\
  \

- **Hit points = \<number\>**\
  Sets a thing\'s maximum amount of life. It is only used for things
  which can be shot.\
  \

- **First moving frame = \<frame number\>**\
  Sets the frame which a monster will jump to when it sees or hears a
  target. Also called walking frames.\
  \

- **Alert sound = \<sound number\>**\
  Sets the sound that a monster will play when it awakens. This sound is
  also played by missiles when they are shot. \<sound number\> must be a
  valid sound number. See the [sound number table](#sndtable) for
  details.\
  \

- **Reaction time = \<number\>**\
  Sets an amount of time that a thing must wait under certain
  circumstances. Monsters use this value as a means to control their
  rate of attack. This value is also used as an internal field for
  varying purposes, but those uses are not editable.\
  \

- **Attack sound = \<sound number\>**\
  Sets a sound used by some monster attacks, including the Lost Soul\'s
  charge.\
  \

- **Injury frame = \<frame number\>**\
  Sets the frame a thing may randomly enter when it has been injured.\
  \

- **Pain chance = \<number\>**\
  Sets the chance out of 255 that this thing will enter its pain state
  when it is injured. A thing with 0 pain chance will never feel pain,
  whereas a thing with 255 pain chance will always feel pain. If a thing
  has a pain chance greater than zero, it should have a valid Injury
  frame, otherwise it will disappear forever.\
  \

- **Pain sound = \<sound number\>**\
  Sets the sound played by the Pain codepointer when this thing uses it.
  Monsters and the player always call Pain from their Injury frames.\
  \

- **Close attack frame = \<frame number\>**\
  If this frame is non-zero, monsters chasing a target will attempt to
  close to melee range quickly. When they determine they are within 64
  units of their target, they may randomly enter this frame to perform a
  melee attack.\
  \

- **Far attack frame = \<frame number\>**\
  If this frame is non-zero, monsters chasing a target will randomly
  enter it to perform a missile attack.\
  \

- **Death frame = \<frame number\>**\
  When a thing dies normally, it will enter this state. If the state is
  zero, the thing will disappear forever.\
  \

- **Exploding frame = \<frame number\>**\
  If a thing dies by taking enough damage that its health is equal to
  its negated Hit points value, it will enter this frame instead of its
  Death frame. This is also known as gibbing, or extreme death. If this
  frame is zero, the thing will always die normally.\
  \

- **Death sound = \<sound number\>**\
  Sound played by the Scream codepointer. Used to play monster death
  sounds.\
  \

- **Speed = \<number\>**\
  Value used by monsters as a movement delta per walking frame. The
  monster will move by an amount relative to this value each time it
  uses the Chase codepointer. For missiles, this value is a fixed-point
  number (multiplied by 65536), and represents a constant speed by which
  the missile advances every game tic. Missile speed is not affected by
  the missile frame durations, whereas monster walking speed is.\
  \

- **Width = \<number\>**\
  Fixed-point value (multiplied by 65536) that represents the radius of
  this thing type. A thing can fit into corridors which are at least as
  wide as this value times two, plus one unit (ie a thing with radius 32
  can fit into a 65-wide hallway). The maximum value this field
  \*should\* have is 2097152, which is equal to 32 units. However, the
  game breaks its own rule here, giving several monsters radii up to 128
  units. These monsters, which include the mancubus, arachnotron, and
  spider demon, exhibit clipping errors which enable other things to
  walk into them, and which can cause some moving sectors to
  malfunction. Avoid giving things radii larger than this to remain
  absolutely safe.\
  \

- **Height = \<number\>**\
  Fixed-point value that represents the height of this thing type. A
  thing can fit into areas which are of this exact height or taller.
  Note that in DOOM, this value is only used to see where a monster can
  fit. For purposes of clipping, monsters are considered to be
  infinitely tall. However, Eternity\'s extended 3D object clipping
  enables this field to also be used to allow or disallow things to pass
  over, under, or stand on this thing.\
  \

- **Mass = \<number\>**\
  Normal integer value that serves as a mass factor for this object.
  Mass affects the amount of thrust an object is given when it is
  damaged, the amount of effect an archvile\'s attack can have on it,
  and for bouncing objects, the rate at which they fall.\
  \

- **Missile damage = \<number\>**\
  This number is used as a damage multiplier when a missile hits a
  thing. The damage formula used is:


              damage = ((rnd % 8) + 1) * missiledamage
           

  This field is also used as a parameter by some new, parameterized
  codepointers, including BetaSkullAttack and Detonate.\
  \

- **Action sound = \<sound number\>**\
  Sets the sound used by a thing when it is wandering around. A thing is
  given a 3 out of 256 chance (1.17%) of making this sound every time it
  calls the Chase codepointer. There are some new Bits3 flags which can
  alter the behavior of this sound.\
  \

- **Bits = \<number \|\| flag list\>**\
  This field controls a number of thing type properties using bit flags,
  where each bit in the field stands for some effect, and has the value
  of being either on or off. In old DeHackEd patches, this field is an
  integer number. BEX Extensions allow a human-readable string of flag
  values to be provided instead. See the flag field lists below for more
  information.\
  \

- **Bits2 = \<number \|\| flag list\>**\
  Similar to Bits but takes a different set of values with different
  meanings. This field is an Eternity extension. See below for more
  information.\
  \

- **Bits3 = \<number \|\| flag list\>**\
  Similar to Bits but takes a different set of values with different
  meanings. This field is an Eternity extension. See below for more
  information.\
  \

- **Respawn frame = \<frame number\>**\
  If this frame is non-zero, an Archvile can perform a resurrection on
  this creature. When this occurs, the creature will enter this frame.\
  \

- **Translucency = \<number\>**\
  This field is an Eternity extension which allows fine, customizable
  control over a thing\'s translucency. The value of this field may
  range from 0 to 65536, with 0 being completely invisible, and 65536
  being normal. Consider the following example:


              Thing 2 (Trooper)
              Translucency = 32768
           

  Trooper objects will be 50% translucent in this example, since 32768
  is half of 65536. Note this effect is mutually exclusive of BOOM-style
  translucency. If this value is not 65536, and the BOOM translucency
  flag is turned on, the flag will be turned off at run-time, and this
  field will take precedence.\
  \

- **Blood color = \<number\>**\
  This field is an Eternity extension. Allows this thing type to have
  differently colored blood when particle blood effects are enabled
  (this does not currently have any effect on blood sprites). Currently
  available blood color values are as follows:


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
           

[Return to Table of Contents](#contents)

[]{#tngflags}

------------------------------------------------------------------------

**BEX Extension: Thing Flag Lists**

------------------------------------------------------------------------

\
This is actually something you can use to hack your DeHackEd file to be
more understandable and modifiable. Note that once you do this you
won\'t be able to bring it back into DeHackEd.\
\
If you\'ve altered a thing\'s bits at all, you\'ll see a line like:


      Bits = 205636102

That horrible number is why we are providing some mnemonics instead. All
you need to do is to put the appropriate ones of the following mnemonics
on the line where the big number is, and it\'ll be read in and converted
to the number for you. Some valid example lines:


      Bits = SOLID+SHOOTABLE+NOBLOOD
      ...
      Bits = NOBLOCKMAP | NOGRAVITY
      ...
      Bits = SPECIAL, COUNTITEM

which converts to Bits = 524294 but you didn\'t have to figure that part
out. Just use a plus, \'\|\', comma, or whitespace between the
mnemonics. Below are mnemonic lists, along with a bit of explanation of
each, and the phrase that\'s used in Dehacked to describe them. If there
are any disagreements between the 2 definitions, ours wins.\
\
[Return to Table of Contents](#contents)

[]{#tngbits}

------------------------------------------------------------------------

**Things: Bits Mnemonics**

------------------------------------------------------------------------

\


      Mnemonic      Bit mask     Meaning / Dehacked phrase
      ------------------------------------------------------------------
      SPECIAL       0x00000001 - Object is a collectable item.
                                      "Can be picked up"
      SOLID         0x00000002 - Blocks other objects.
                                      "Obstacle"
      SHOOTABLE     0x00000004 - Can be targetted and hit with bullets, missiles.
                                      "Shootable"
      NOSECTOR      0x00000008 - Invisible but still clipped -- see notes.
                                      "Total invisibility"
      NOBLOCKMAP    0x00000010 - Inert but displayable; can hit but cannot be hit.
                                      "Can't be hit"
      AMBUSH        0x00000020 - Deaf monster.
                                      "Semi-deaf"
      JUSTHIT       0x00000040 - Internal, monster will try to attack immediately; see notes.
                                      "In pain"
      JUSTATTACKED  0x00000080 - Internal, monster takes at least 1 step before attacking; see notes.
                                      "Steps before attack"
      SPAWNCEILING  0x00000100 - Thing spawns hanging from the ceiling of its sector.
                                      "Hangs from ceiling"
      NOGRAVITY     0x00000200 - Thing is not subject to gravity.
                                      "No Gravity"
      DROPOFF       0x00000400 - Thing can walk over tall dropoffs (> 24 units).
                                      "Travels over cliffs"
      PICKUP        0x00000800 - Thing can pick up items -- see notes.
                                      "Picks up items"
      NOCLIP        0x00001000 - Thing is not clipped, can walk through walls.
                                      "No clipping"
      SLIDE         0x00002000 - No effect -- see notes.
                                      "Slides along walls"
      FLOAT         0x00004000 - Monster may move to any height; is flying.
                                      "Floating"
      TELEPORT      0x00008000 - Don't cross lines or look at teleport heights.
                                      "Semi-no clipping"
      MISSILE       0x00010000 - Thing is a missile, explodes and inflicts damage when blocked.
                                      "Projectiles"
      DROPPED       0x00020000 - Internal, item was dropped by a creature and won't respawn.
                                      "Disappearing weapon"
      SHADOW        0x00040000 - Use fuzz draw effect like spectres, also causes aiming inaccuracy.
                                      "Partial invisibility"
      NOBLOOD       0x00080000 - Object puffs instead of bleeds when shot.
                                      "Puffs (vs. bleeds)"
      CORPSE        0x00100000 - Internal, object is dead and will slide down steps.
                                      "Sliding helpless"
      INFLOAT       0x00200000 - Internal, monster is floating but not to target height -- see notes.
                                      "No auto-leveling"
      COUNTKILL     0x00400000 - Can be targetted, adds 1 to kill percent when it dies -- see notes.
                                      "Affects Kill %"
      COUNTITEM     0x00800000 - Item adds 1 to item percent when collected.
                                      "Affects Item %"
      SKULLFLY      0x01000000 - Internal, special handling for flying skulls -- see notes.
                                      "Running"
      NOTDMATCH     0x02000000 - Thing will not be spawned at level start when in Deathmatch.
                                      "Not in deathmatch"
      TRANSLATION1  0x04000000 - Use translation table for color (players)'.
      (aka TRANSLATION)               "Color 1 (gray/red)"
      
      TRANSLATION2  0x08000000 - Second bit for color translation;
      (aka UNUSED1)                   "Color 2 (brown/red)"
                                      
      TOUCHY        0x10000000 - Thing dies on contact with any solid object of another species;
      (aka UNUSED2)                    MBF Extension, see notes.

      BOUNCES       0x20000000 - Thing may bounce off walls and ceilings;
      (aka UNUSED3)                    MBF Extension, see notes.

      FRIEND        0x40000000 - All things of this type are allied with player;
      (aka UNUSED4)                    MBF Extension.
      
      TRANSLUCENT   0x80000000 - Apply BOOM-style translucency to sprite;
                                       BOOM Extension, see notes.
      ------------------------------------------------------------------                                   

Notes on Bits values:\
\

- NOSECTOR:\
  This flag has some odd side effects. For a pure total invisibility
  effect, use the new DONTDRAW flag in the Bits2 field.
- JUSTHIT:\
  This flag is for internal AI use, applying it to a thing will have
  more or less no effect.
- JUSTATTACKED:\
  As above.
- PICKUP:\
  Only the player should possess this flag. If monsters try to pick up
  items, results will be undefined. This may change in the future.
- SLIDE:\
  This flag has absolutely no effect and is never used anywhere. For
  compatibility, it must remain this way. A new flag, SLIDE, in the
  Bits3 field, DOES have this effect, however, and is used by the player
  object and Heretic pods, by default.
- INFLOAT:\
  This flag is for internal AI use only.
- COUNTKILL:\
  This flag actually has two effects. If a monster does not have the
  COUNTKILL flag, other monsters may ignore it, and some other engine
  features will not work properly for it. This is generally undesirable.
  A new Bits3 flag KILLABLE, used by the Lost Soul by default, allows
  monsters to behave normally even without having this flag.
- SKULLFLY:\
  This flag is for internal AI use only.
- TOUCHY:\
  This new MBF extension allows things to instantly die if any solid
  thing of a differing species touches it. Note that for purposes of
  this effect, Lost Souls and Pain Elementals are considered to be the
  same species.
- BOUNCES:\
  This flag declares that the thing is bouncy. The exact behavior of the
  thing can be modified by combining this with other flags in the
  following manners:
  - BOUNCES + MISSILE:\
    Object bounces off of floors and ceilings only.
  - BOUNCES + NOGRAVITY:\
    Object bounces off all surfaces without losing momentum, unless its
    also a missile.
  - BOUNCES + FLOAT:\
    Without NOGRAVITY, allows monsters to jump to reach their target.
  - BOUNCES + FLOAT + DROPOFF:\
    Allows even higher bouncing / jumping.
  - BOUNCES:\
    When the flag is alone, without MISSILE, it allows the object to
    bounce off all surfaces, losing momentum at each contact. It can
    also roll up and down steps. This is highly realistic.
- TRANSLUCENT:\
  This thing will use the global BOOM translucency level for its sprite.
  If general translucency is off, it will draw normally. This effect is
  mutually exclusive with the new Eternity extension for translucency.
  If the latter is enabled, this flag will be turned off by the game at
  run-time.

[Return to Table of Contents](#contents)

[]{#tngbits2}

------------------------------------------------------------------------

**Things: Bits2 Mnemonics (Eternity extension)**

------------------------------------------------------------------------

\


      Mnemonic      Bit mask     Meaning
      ------------------------------------------------------------------
      LOGRAV        0x00000001 - Thing experiences low gravity; see notes.
      
      NOSPLASH      0x00000002 - Thing cannot trigger TerrainTypes.
      
      NOSTRAFE      0x00000004 - Thing never uses advanced strafing logic.
      
      NORESPAWN     0x00000008 - Thing will not respawn in nightmare mode.
      
      ALWAYSRESPAWN 0x00000010 - Thing will respawn at any difficulty level.
      
      REMOVEDEAD    0x00000020 - Thing removes itself shortly after death; see notes.
      
      NOTHRUST      0x00000040 - Thing is unaffected by push/pull, wind, current, or conveyors.
      
      NOCROSS       0x00000080 - Thing cannot trigger crossable special lines.
      
      JUMPDOWN      0x00000100 - Thing, when friendly, can jump down to follow player.
      
      PUSHABLE      0x00000200 - Thing is pushable by other moving things.
      
      MAP07BOSS1    0x00000400 - Thing must be dead for MAP07 tag 666 special.
      
      MAP07BOSS2    0x00000800 - Thing must be dead for MAP07 tag 667 special.
      
      E1M8BOSS      0x00001000 - Thing must be dead for E1M8 tag 666 special.
      
      E2M8BOSS      0x00002000 - Thing must be dead for E2M8 end-level special.
      
      E3M8BOSS      0x00004000 - Thing must be dead for E3M8 end-level special.
      
      BOSS          0x00008000 - Thing is a boss; roars loud and is invincible to blast radii.
                                    
      E4M6BOSS      0x00010000 - Thing must be dead for E4M6 tag 666 special.
      
      E4M8BOSS      0x00020000 - Thing must be dead for E4M8 tag 666 special.
      
      FOOTCLIP      0x00040000 - Thing lowers when on liquid terrain.
      
      FLOATBOB      0x00080000 - Thing floats up and down on z axis.
      
      DONTDRAW      0x00100000 - Thing is totally invisible; enemies cannot see player.
      
      SHORTMRANGE   0x00200000 - Thing has a shorter than normal missile range.
      
      LONGMELEE     0x00400000 - Thing has a longer than normal melee range.
      
      RANGEHALF     0x00800000 - Thing considers itself to be half as far from target.
      
      HIGHERMPROB   0x01000000 - Thing has a higher missile attack probability.
      
      CANTLEAVEFLOORPIC
                    0x02000000 - Thing can only move into sectors with the same flat.
      
      SPAWNFLOAT    0x04000000 - Thing spawns with a randomized z coordinate.
      
      INVULNERABLE  0x08000000 - Thing is invulnerable to damage less than 10000 points.
      
      DORMANT       0x10000000 - Thing is invulnerable and inert until activated by script; see notes.
      
      SEEKERMISSILE 0x20000000 - Indicates that this thing may be used as a homing missile; see notes.
      
      DEFLECTIVE    0x40000000 - When paired with REFLECTIVE, thing will reflect at a wide angle.
      
      REFLECTIVE    0x80000000 - Thing reflects missile projectiles; see notes.
      ------------------------------------------------------------------                                   

Notes on Bits2 values:\
\

- LOGRAV:\
  Has no effect on BOUNCY things
- REMOVEDEAD:\
  Things with this flag also cannot respawn in Nightmare mode. Do not
  give the player this flag.
- DORMANT:\
  Does not function as expected unless a thing is spawned on the map at
  startup with the [DORMANT mapthing flag](editref.html#part4) set in
  the level itself. It cannot be switched on and off freely.
- SEEKERMISSILE:\
  This flag is used by reflection code to check if a missile is being
  used as a homing missile. When a homing missile with this flag is
  reflected, its tracer target will be set to the thing that fired it,
  so that it will properly home on its originator instead of its
  original target.
- REFLECTIVE:\
  Does not impart invulnerability, and unless paired with that flag, the
  object will take damage before bouncing the projectile back.

[Return to Table of Contents](#contents)

[]{#tngbits3}

------------------------------------------------------------------------

**Things: Bits3 Mnemonics (Eternity extension)**

------------------------------------------------------------------------

\


      Mnemonic      Bit mask     Meaning
      ------------------------------------------------------------------
      GHOST         0x00000001 - Thing is a Heretic-style ghost; enemies may not see player.

      THRUGHOST     0x00000002 - Missiles with this flag pass through GHOST objects.

      NODMGTHRUST   0x00000004 - Things aren't thrusted when damaged by this thing.

      ACTSEESOUND   0x00000008 - Thing may make see sound instead of active sound; 50% chance.

      LOUDACTIVE    0x00000010 - Thing makes its active sound at full volume.

      E5M8BOSS      0x00000020 - Thing is a boss on Heretic level E5M8.

      DMGIGNORED    0x00000040 - Other things will not fight this thing if damaged by it.

      BOSSIGNORE    0x00000080 - Things won't fight other species with this flag.

      SLIDE         0x00000100 - Thing can slide against walls; see notes.

      TELESTOMP     0x00000200 - Thing can telefrag other things.

      WINDTHRUST    0x00000400 - Thing is affected by Heretic-style wind sectors; see notes.
      
      FIREDAMAGE    0x00000800 - Reserved; currently has no effect.
      
      KILLABLE      0x00001000 - Thing doesn't count for kill %, but is still an enemy; see notes.
      
      DEADFLOAT     0x00002000 - Flying thing won't drop to the ground when killed.
      
      NOTHRESHOLD   0x00004000 - Thing has no threshold for acquiring a new target.
      
      FLOORMISSILE  0x00008000 - Missile can step up/down any height changes when on ground.
      
      SUPERITEM     0x00010000 - Item respawns only if the "super items respawn" DMFLAG is on.
      
      NOITEMRESP    0x00020000 - Item never respawns, and will not be crushed.
      
      SUPERFRIEND   0x00040000 - Thing never attacks other friends when friendly.
      
      INVULNCHARGE  0x00080000 - Thing is invulnerable when SKULLFLY flag is set.
      
      EXPLOCOUNT    0x00100000 - Missile will not explode if Counter 1 < Counter 2.
                                 Counter 1 will be incremented at each explosion attempt.
                                 
      CANNOTPUSH    0x00200000 - Thing cannot push things with the PUSHABLE flag.
      
      TLSTYLEADD    0x00400000 - Thing uses additive translucency style.
      
      SPACMONSTER   0x00800000 - Thing can activate parameterized line specials which bear
                                 the MONSTER activation flag.
                                 
      SPACMISSILE   0x01000000 - Thing can activate parameterized line specials which bear
                                 the MISSILE activation flag.
      
      NOFRIENDDMG   0x02000000 - Thing will not be damaged by things with the FRIEND flag.
      
      3DDECORATION  0x04000000 - Thing is a decoration which, when 3D object clipping is
                                 active, will NOT use its corrected height, even if one
                                 is specified in its EDF thingtype definition.

      ALWAYSFAST    0x08000000 - Thing has -fast/Nightmare mode attack speed behavior in
                                 any difficulty level.
                                 
      PASSMOBJ      0x10000000 - Thing is fully treated with respect to 3D object clipping.
                                 All monsters and other moving solid things should have this
                                 flag.
                                 
      DONTOVERLAP   0x20000000 - This thing type prefers not to overlap with other things
                                 vertically when possible.
      ------------------------------------------------------------------

Notes on Bits3 values:\
\

- SLIDE:\
  As noted in the documentation for Bits flag SLIDE, it does nothing.
  This flag, however, actually works, and is used by the player and by
  Heretic pods.
- WINDTHRUST:\
  This controls only which things are affected by the new Heretic wind
  special, controlled by [control linedef type
  293](editref.html#hticpush). Any thing type with this flag will
  experience thrust relative to the angle and magnitude of the control
  linedef when it is anywhere within a tagged sector.
- KILLABLE:\
  As noted in the documentation for Bits flag COUNTKILL, enemies without
  that flag normally suffer severe side-effects. This new flag
  generalizes what was previously a special case for the Lost Soul, and
  allows enemies which don\'t count toward the kill percentage to be
  otherwise normal.

[Return to Table of Contents](#contents)

[]{#frames}

------------------------------------------------------------------------

**Frame Block**

------------------------------------------------------------------------

\
[]{#framedesc}

------------------------------------------------------------------------

**Frames: Description**

------------------------------------------------------------------------

\
The DeHackEd Frame block allows you to edit the properties of frames
used both by monsters and by player weapons. This allows extensive
customization of AI and attacks.\
\
Eternity provides several extensions to the DeHackEd frame block.
Extended fields will be noted as such in their descriptions.\
\
[Return to Table of Contents](#contents)

[]{#framesyntax}

------------------------------------------------------------------------

**Frames: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
All fields in the block are optional. If a field is not provided, that
value of the frame being edited will not change.\
\


       Frame <dehacked number>
       Sprite number = <sprite number>
       Sprite subnumber = <number>
       Duration = <number>
       Next frame = <frame number>
       Unknown 1 = <number>
       Unknown 2 = <number>
       # The following fields are Eternity extensions
       Particle event = <number>
       Args1 = <number>
       Args2 = <number>
       Args3 = <number>
       Args4 = <number>
       Args5 = <number>

Explanation of special information:\
\
**dehacked number** is the DeHackEd number of the frame that this block
edits. The DeHackEd number of a frame is defined in its EDF **frame**
definition.\
\
[Return to Table of Contents](#contents)

[]{#framefields}

------------------------------------------------------------------------

**Frames: Fields**

------------------------------------------------------------------------

\

- **Sprite number = \<sprite number\>**\
  Sets the sprite to be displayed by an object or gun using this frame.
  Must be a valid sprite number between 0 and the max number of sprites
  currently defined minus 1. The number of sprites is dependent upon the
  number of sprites defined via EDF.\
  \

- **Sprite subnumber = \<number\>**\
  Sets the frame of the sprite to be displayed when this frame is
  entered. Sprite subnumbers correspond directly to the letters used to
  identify sprite frame graphics in the wad, starting with 0 = A, 1 = B,
  etc.\
  \

- **Duration = \<number\>**\
  Sets the duration of time a thing or gun will remain in this frame.
  Duration is of unit \"tics,\" and there are 35 tics per second. This
  means that a frame lasting 35 tics will last exactly 1 second. A
  Duration value of 0 allows a thing to pass through multiple frames in
  a single gametic. A Duration value of -1 means that the thing will
  remain in this frame forever.\
  \

- **Next frame = \<frame number\>**\
  Indicates what frame the thing or gun will proceed to after completing
  this one. Must be a valid frame DeHackEd number. Note that frame 0 is
  treated specially, and objects that transfer to it may be removed by
  the game engine.\
  \

- **Unknown 1 = \<number\>**\
  This field, for player gun frames, can alter the position of the gun
  graphic on screen. Use of the field for this purpose is deprecated,
  however, and is not recommended. This field also serves as a parameter
  to some MBF parameterized codepointers.\
  \

- **Unknown 2 = \<number\>**\
  As above.\
  \

- **Particle event = \<number\>**\
  This Eternity extension allows a frame to specify a particle event
  that it can trigger. Particle events are triggered immediately after
  the frame\'s codepointer function is executed.\
  \
  Particle Events Currently Available:


          Number  Effect    
          -----------------------------------------------------------
          0       None
          1       Rocket explosion particle burst
          2       BFG explosion particle burst
          -----------------------------------------------------------
          

- **Args1 - Args5**\
  These fields are an Eternity extension which allows extended
  parameters to be used by the codepointer in this frame. Parameterized
  codepointers are extremely powerful, allowing frame-specific
  customization of attacks, AI actions, sound, etc. See the [Eternity
  Engine Definitive Codepointer Reference](codeptrs.html) for further
  information.

[Return to Table of Contents](#contents)

[]{#pointer}

------------------------------------------------------------------------

**Pointer Block (Deprecated)**

------------------------------------------------------------------------

\
[]{#ptrdesc}

------------------------------------------------------------------------

**Pointers: Description**

------------------------------------------------------------------------

\
This block allows setting the codepointer of a frame to the original
codepointer contained in another frame. This form of codepointer
specification is very clumsy and error-prone, and has been deprecated in
favor of the [BEX \[CODEPTR\]](#codeptr) extension. Do not use this
block in new patches.\
\
[Return to Table of Contents](#contents)

[]{#ptrsyntax}

------------------------------------------------------------------------

**Pointers: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
Items in green are deprecated.\
The field in this block is **not** optional.\
\


       
       Pointer <xref number> (Frame <target frame number>)
       Codep Frame = <source frame number>
       

Explanation of special information:\
\
**xref number** is not used by Eternity; it can be left zero.\
\
**target frame number** is the DeHackEd number of the frame into which
the codepointer is to be copied.\
\
**source frame number** is the DeHackEd number of the frame from which
the original codepointer is to be taken. Note that even if the source
frame\'s pointer has been edited previously, the target frame will still
receive the \*original\* pointer that was in the source frame, and not
its current value.\
\
[Return to Table of Contents](#contents)

[]{#sound}

------------------------------------------------------------------------

**Sound Block**

------------------------------------------------------------------------

\
[]{#sounddesc}

------------------------------------------------------------------------

**Sounds: Description**

------------------------------------------------------------------------

\
This block, which is mostly useless, allows you to edit various
properties of the internal sound table. Most of the fields in this
section are now obsolete, and in fact, most never worked to begin with.
This entire block might become deprecated or obsolete in the future, so
avoid using it.\
\
[Return to Table of Contents](#contents)

[]{#soundsyntax}

------------------------------------------------------------------------

**Sounds: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
Items in red are obsolete.\
All fields in the block are optional. Fields not provided will not be
changed.\
\


       Sound <sound number>
       Offset = <number>
       Zero/One = <number>
       Zero 1 = <number>
       Zero 2 = <number>
       Zero 3 = <number>
       Zero 4 = <number>
       Neg. One 1 = <number>
       Neg. One 2 = <number>

Explanation of special information:\
\
**sound number** is a sound DeHackEd number. The DeHackEd number of a
sound is defined in its EDF **sound** definition. The sound entry with
DeHackEd number 0 is a dummy and should not be edited.\
\
[Return to Table of Contents](#contents)

[]{#soundfields}

------------------------------------------------------------------------

**Sounds: Fields**

------------------------------------------------------------------------

\

- **Offset = \<number\>**\
  Obsolete. This field was a pointer to the sound\'s name, stored in the
  executable. Eternity cannot support editing of this field.\
  \
- **Zero/One = \<number\>**\
  Singularity value of the sound. Positive values will cause the sound
  to be put in a class with sounds of the same value, such that only one
  sound in the class will play at a time. Values will not be documented
  here; see the EDF documentation.\
  \
- **Value = \<number\>**\
  Relative priority value of this sound. When all sound channels are
  exhausted, sounds of the lowest priority will be stopped first.\
  \
- **Zero 1 = \<number\>**\
  Obsolete. Internal field, was used as a pointer to redirect this sound
  entry to another sound. Eternity cannot support editing of this field
  via DeHackEd for security reasons.\
  \
- **Zero 2 = \<number\>**\
  A pitch value for the sound. This field is internal.\
  \
- **Zero 3 = \<number\>**\
  A volume value for the sound. This field is internal.\
  \
- **Zero 4 = \<number\>**\
  Obsolete. Internal field, used as a pointer to the cached lump data.
  Eternity cannot allow editing of this field for security reasons.\
  \
- **Neg. One 1 = \<number\>**\
  Usefulness value. Not used.\
  \
- **Neg. One 2 = \<number\>**\
  Obsolete. Was used as lump number of sound, to avoid repeated lookups.
  Eternity no longer has this field, and could not support it anyways
  due to security issues.

[Return to Table of Contents](#contents)

[]{#ammo}

------------------------------------------------------------------------

**Ammo Block**

------------------------------------------------------------------------

\
[]{#ammodesc}

------------------------------------------------------------------------

**Ammo: Description**

------------------------------------------------------------------------

\
This block allows editing of ammo type characteristics, including the
max ammo value for that type, and the amount of ammo collected in a
basic \"clip\" item.\
\
[Return to Table of Contents](#contents)

[]{#ammosyntax}

------------------------------------------------------------------------

**Ammo: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
All fields in the block are optional. Fields not provided will not be
changed.\
\


       Ammo <ammo number>
       Max ammo = <number>
       Per ammo = <number>

Explanation of special information:\
\
**ammo number** is an ammo type number, which must be one of the
following:\


       Number  Ammo type
       -----------------
       0       Bullets
       1       Shells
       2       Cells
       3       Rockets

[Return to Table of Contents](#contents)

[]{#ammofields}

------------------------------------------------------------------------

**Ammo: Fields**

------------------------------------------------------------------------

\

- **Max ammo = \<number\>**\
  Specifies the player\'s normal maximum carrying amount for this type
  of ammo. This number is doubled when the player picks up a backpack /
  bag of holding.\
  \
- **Per ammo = \<number\>**\
  This is the amount of ammo the player collects by picking up a
  \"small\" ammo item of this type. This number is doubled for the
  larger item of the same type. The basic value is doubled in the \"I\'m
  Too Young to Die\" and \"Nightmare\" difficulties.\
  \

[Return to Table of Contents](#contents)

[]{#weapons}

------------------------------------------------------------------------

**Weapon Block**

------------------------------------------------------------------------

\
[]{#weapondesc}

------------------------------------------------------------------------

**Weapons: Description**

------------------------------------------------------------------------

\
This block allows editing of player weapon characteristics.\
\
[Return to Table of Contents](#contents)

[]{#weapsyntax}

------------------------------------------------------------------------

**Weapons: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
All fields in the block are optional. Fields not provided will not be
changed.\
\


       Weapon <weapon number>
       Ammo type = <ammo number>
       Deselect frame = <frame number>
       Select frame = <frame number>
       Bobbing frame = <frame number>
       Shooting frame = <frame number>
       Firing frame = <frame number>
       # The fields below are Eternity extensions
       Ammo per shot = <number>

Explanation of special information:\
\
**weapon number** is a weapon type number, which must be one of the
following:\


       Number  Weapon type
       ------------------------
       0       Fist
       1       Pistol
       2       Shotgun
       3       Chaingun
       4       Rocket Launcher
       5       Plasma Rifle
       6       BFG 9000
       7       Chainsaw
       8       Super Shotgun

**ammo number** is an ammo type number, which must be one of the
following:\


       Number  Ammo type
       -----------------
       0       Bullets
       1       Shells
       2       Cells
       3       Rockets
       5       None

[Return to Table of Contents](#contents)

[]{#weapfields}

------------------------------------------------------------------------

**Weapons: Fields**

------------------------------------------------------------------------

\

- **Ammo type = \<number\>**\
  The type number, as given above, of the ammo type used by this weapon.
  Note that 4 is not a valid ammo type; do not use it. Also, in
  Eternity, any weapon can be safely given ammo type 5, none. In DOOM,
  giving any weapon other than the fist or chainsaw this type would
  cause the shotgun\'s max ammo to decrease on every use.\
  \
- **Deselect frame = \<frame number\>**\
  A weapon enters this frame when the player changes to another weapon
  or dies.\
  \
- **Select frame = \<frame number\>**\
  A weapon enters this frame when the player changes to it.\
  \
- **Bobbing frame = \<frame number\>**\
  A weapon stays in this frame when the player is walking around without
  firing.\
  \
- **Shooting frame = \<frame number\>**\
  A weapon enters this frame when the player hits the fire key.\
  \
- **Firing frame = \<frame number\>**\
  This frame is used to display muzzle flashes when a weapon is fired.\
  \
- **Ammo per shot = \<number\>**\
  This field is an Eternity extension which allows editing of the amount
  of ammo a weapon must have to be fired and uses on each shot (there is
  no point to these values being separate). Unless enabled through
  explicit use of this field in DeHackEd, this value is not used by the
  game engine, for compatibility reasons.

[Return to Table of Contents](#contents)

[]{#cheats}

------------------------------------------------------------------------

**Cheat Block**

------------------------------------------------------------------------

\
[]{#cheatdesc}

------------------------------------------------------------------------

**Cheats: Description**

------------------------------------------------------------------------

\
This block allows editing of some of the cheat codes.\
\
[Return to Table of Contents](#contents)

[]{#cheatsyntax}

------------------------------------------------------------------------

**Cheats: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
All fields in the block are optional. Fields not provided will not be
changed.\
\


       Cheat [0]
       Change music = <string>
       Chainsaw = <string>
       God mode = <string>
       Ammo & Keys = <string>
       Ammo = <string>
       No Clipping 1 = <string>
       No Clipping 2 = <string>
       Invincibility = <string>
       Berserk = <string>
       Invisibility = <string>
       Radiation Suit = <string>
       Auto-map = <string>
       Lite-amp Goggles = <string>
       BEHOLD menu = <string>
       Level Warp = <string>
       Player Position = <string>

Explanation of special information:\
\
**0** is a number which was put next to the \"Cheat\" token in order for
the original DeHackEd program\'s parser to be more uniform. Eternity
ignores everything on this line after the word Cheat, and so this is
optional.\
\
**string** is a cheat as it will be literally typed. Note that in old
DeHackEd files, the cheat strings were limited to the original length,
and if they were shorter, were padded out to the same length with ASCII
character 255. Eternity does not place any limitation on cheat length,
and if ASCII 255 characters are present, they will be ignored.\
\
[Return to Table of Contents](#contents)

[]{#misc}

------------------------------------------------------------------------

**Misc Block**

------------------------------------------------------------------------

\
[]{#miscdesc}

------------------------------------------------------------------------

**Misc: Description**

------------------------------------------------------------------------

\
This block allows editing of various constants and gameplay factors.\
\
[Return to Table of Contents](#contents)

[]{#miscsyntax}

------------------------------------------------------------------------

**Misc: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
All fields in the block are optional. Fields not provided will not be
changed.\
\


       Misc [0]
       Initial Health = <number>
       Initial Bullets = <number>
       Max Health = <number>
       Max Armor = <number>
       Green Armor Class = <number>
       Blue Armor Class = <number>
       Max Soulsphere = <number>
       Soulsphere Health = <number>
       Megasphere Health = <number>
       God Mode Health = <number>
       IDFA Armor = <number>
       IDFA Armor Class = <number>
       IDKFA Armor = <number>
       IDKFA Armor Class = <number>
       BFG Cells/Shot = <number>
       Monsters Infight = <number>   

Explanation of special information:\
\
**0** is a number which was put next to the \"Misc\" token in order for
the original DeHackEd program\'s parser to be more uniform. Eternity
ignores everything on this line after the word Misc, and so this is
optional.\
\
[Return to Table of Contents](#contents)

[]{#miscfields}

------------------------------------------------------------------------

**Misc: Fields**

------------------------------------------------------------------------

\

- **Initial Health = \<number\>**\
  The amount of health a player starts with at the beginning of the game
  or after dying. Default 100.\
  \
- **Initial Bullets = \<number\>**\
  The amount of bullets a player starts with at the beginning of the
  game or after dying. Default 50.\
  \
- **Max Health = \<number\>**\
  Player\'s maximum health value. Default 200.\
  \
- **Max Armor = \<number\>**\
  Player\'s maximum armor value. Default 200.\
  \
- **Green Armor Class = \<number\>**\
  Class number of the green armor. Value must be 1 or 2. Default 1.\
  \
- **Blue Armor Class = \<number\>**\
  Class number of the blue armor. Value must be 1 or 2. Default 2.\
  \
- **Max Soulsphere = \<number\>**\
  Maximum health after picking up soulsphere. Default 100.\
  \
- **Soulsphere Health = \<number\>**\
  Amount of health given by Soulsphere. Default 100.\
  \
- **Megasphere Health = \<number\>**\
  Amount of health given by Megasphere. Default 200.\
  \
- **God Mode Health = \<number\>**\
  Amount health is set to when IDDQD cheat is used. Default 100.\
  \
- **IDFA Armor = \<number\>**\
  Amount of armor given by IDFA cheat. Default 200.\
  \
- **IDFA Armor Class = \<number\>**\
  Armor class given by IDFA cheat. Value must be 1 or 2. Default 2.\
  \
- **IDKFA Armor = \<number\>**\
  Amount of armor given by IDKFA cheat. Default 200.\
  \
- **IDKFA Armor Class = \<number\>**\
  Armor class given by IDKFA cheat. Value must be 1 or 2. Default 2.\
  \
- **BFG Cells/Shot = \<number\>**\
  Ammo used per shot by BFG. Default is 40. Note: Eternity will
  propagate this value to the Ammo per shot field of the BFG weapon.\
  \
- **Monsters Infight = \<number\>**\
  Enables monsters to injure and fight their own species. This switch is
  not currently supported by Eternity, but may be added in the near
  future.

[Return to Table of Contents](#contents)

[]{#text}

------------------------------------------------------------------------

**Text Block (Deprecated)**

------------------------------------------------------------------------

\
[]{#textdesc}

------------------------------------------------------------------------

**Text: Description**

------------------------------------------------------------------------

\
Eternity provides limited support for old DeHackEd-style text
substitution. This method of text substitution is severely limited,
syntactically ugly, and not completely backward compatible. This section
has been deprecated by the [BEX \[STRINGS\] block](#strings), and by the
Eternity [\[MUSIC\]](#music), [\[SOUNDS\]](#sounds), and
[\[SPRITES\]](#sprites) blocks. Do not use this block in new files!\
\
Eternity performs old DeHackEd-style text substitution by searching
through the game\'s BEX string table for a string with a matching value.
If no such string can be found, the substitution is discarded. Many
strings which existed in DOOM are no longer available in Eternity.\
\
Note: Beginning with EE 3.33.50, it is possible for strings to be
replaced multiple times, and for strings to still be replaceable even if
another different string is given the same value. The original value of
strings is now always used to compare against the old strings, instead
of the newest value taken from the DEH/BEX patch. This significantly
improves compatibility with old patches that swap sprite, sound, and
music names.\
\
[Return to Table of Contents](#contents)

[]{#textsyntax}

------------------------------------------------------------------------

**Text: Syntax**

------------------------------------------------------------------------

\
A brief explanation of the syntax description:\
Items in angle brackets (\< \>) are required.\
Items in square brackets (\[ \]) are optional.\
Items in green are deprecated.\
The field in this block is **not** optional.\
\


       
       Text <old length> <new length>
       <old string><new string>
       

Explanation of special information:\
\
**old length** must be the EXACT length in characters, including
whitespace, new lines, tabs, etc., of the old text string that is being
replaced.\
\
**new length** must be the EXACT length in characters, including
whitespace, new lines, tabs, etc., of the new text string.\
\
**old string** is the old string to be replaced, exactly as it appears
in the executable. Case is considered irrelevant when looking for a
match.\
\
**new string** is the new string. Case and formatting characters will be
preserved.\
\
Note there is NO whitespace or any other delimiter between the two
strings.\
\
[Return to Table of Contents](#contents)

[]{#sprite}

------------------------------------------------------------------------

**Sprite Block (Obsolete)**

------------------------------------------------------------------------

\
[]{#spriteobs}

------------------------------------------------------------------------

**Sprites: Obsoletion Info**

------------------------------------------------------------------------

\
The DeHackEd sprite string redirection method is not supported by the
BEX specification or by Eternity. The syntax of this block was as
follows:


       
       Sprite <sprite number>
       Offset = <pointer>
       

This allowed the sprite name order to be rearranged, but could also
allow the game to be crashed or corrupted very easily. For this reason
and others, this feature is unsupported. Any sprite blocks will be
ignored.\
\
[Return to Table of Contents](#contents)

[]{#strings}

------------------------------------------------------------------------

**BEX Extension: \[STRINGS\] Block**

------------------------------------------------------------------------

\
[]{#stringdesc}

------------------------------------------------------------------------

**\[STRINGS\]: Description**

------------------------------------------------------------------------

\
The basic Dehacked file is understood and properly evaluated to change
such things as sounds, frame rewiring, Thing attributes, etc. No change
is required to the original .deh file for those things. However, text is
handled differently in BEX for several reasons:\
\

1.  Dehacked by the nature of the program was simply doing a search and
    replace action on the strings as found in the executable. This is
    clumsy and dangerous, though as good as can be done when dealing
    with a compiled executable program. We now have the ability to
    change the actual string values during play, a safer and better
    solution.\
    \
2.  Because Dehacked used the original string to find the place to plug
    in the replacement, there was no way to write a generic hack that
    would replace strings in all versions regardless of language. In
    BEX, the strings are identified by their names, which are the same
    in all cases.\
    \
3.  Because Dehacked was replacing strings in a physical location in the
    program file, it was limited to approximately the same size string
    as the original. Though this is generally practical, it is limiting
    in flexibility. BEX allows any length string to replace any of the
    ones in the game (this also applies to cheats).\
    \
4.  The purpose of replacing a string is to show the player some text at
    a particular point in the game. Finding the original text that was
    there and replacing it with other text worked, but logically what
    the author should have been able to do was assign strings depending
    on the desired purpose, and not have to recognize the original text
    to do so. In BEX, all strings are identified by a mnemonic name
    (actually the name of the internal variable), making it easier to
    assign a string to its desired purpose.

Detailed information is available further down in this file, explaining
the mnemonic codes that are used for all the replaceable strings. Note
that some of the strings are always displayed in the small (uppercase
only) font, so capitalization is not important for those.\
\
[Jump to the BEX String Mnemonics Table](#textmn)\
\
We urge patch authors to provide new BEX format files, for clarity,
supportability and flexibility.\
\
[Return to Table of Contents](#contents)

[]{#stringsyntax}

------------------------------------------------------------------------

**\[STRINGS\]: Syntax**

------------------------------------------------------------------------

\
To use BEX string support, start the section with a \[STRINGS\] section
marker, followed by lines that start with the key mnemonic, an equal
sign, and the value to assign to that string. You may split the value to
be assigned onto multiple lines by ending each line that is to be
continued with a backslash. Don\'t put quotes around the string if you
don\'t want them to show up in the result. A couple of examples:


       # Change the level name that shows up in the automap for MAP12:
       [STRINGS]
       HUSTR_12 = The Twelfth Night
       # and now the new red key message for switches
       PD_REDO = You need the scarlet pimpernel to \
                 turn on this machine.   
       # Note that the blank before the backslash is included in the string
       # but that the indentation before the word "turn" is not, allowing
       # the change file to be easy to read.

[Return to Table of Contents](#contents)

[]{#pars}

------------------------------------------------------------------------

**BEX Extension: \[PARS\] Block**

------------------------------------------------------------------------

\
[]{#pardesc}

------------------------------------------------------------------------

**\[PARS\]: Description**

------------------------------------------------------------------------

\
The \[PARS\] Block allows editing of par times for standard ExMy and
MAPxy levels in DOOM and DOOM II. Ultimate DOOM Episode 4 is not
supported, nor is Heretic.\
\
[Return to Table of Contents](#contents)

[]{#parsyntax}

------------------------------------------------------------------------

**\[PARS\]: Syntax**

------------------------------------------------------------------------

\
Insert the following block in your BEX file, using one or more of the
par lines depending on if you want to alter them for DOOM 1 or DOOM 2.


       [PARS]
       par <e> <m> <p>
       par <m> <p>

\<e\> is the episode number, \<m\> is the map number, and \<p\> is the
par time, in seconds. A cleaner example:


       [PARS]
       par 1 1 250
       par 1 2 300
       par 30 1000

You may have as many par times listed as you wish in the pars block.\
\
[Return to Table of Contents](#contents)

[]{#codeptr}

------------------------------------------------------------------------

**BEX Extension: \[CODEPTR\] Block**

------------------------------------------------------------------------

\
[]{#cptrdesc}

------------------------------------------------------------------------

**\[CODEPTR\]: Description**

------------------------------------------------------------------------

\
The \[CODEPTR\] block provides a new, clean, and more powerful method of
changing the codepointer (aka action) field of frames.\
\
The \[CODEPTR\] block is a list of frames, each of which has assigned to
it a codepointer mnemonic. Codepointer mnemonics are symbolic names for
the action functions they represent, and any mnemonic can be assigned to
any frame, even frames that have a zero pointer. This was not allowed in
DeHackEd.\
\
BEX also provides a new NULL mnemonic, which allows zeroing of frames
which have a codepointer in them already. This also was not allowed
under DeHackEd, for reasons unknown.\
\
[Return to Table of Contents](#contents)

[]{#cptrsyntax}

------------------------------------------------------------------------

**\[CODEPTR\]: Syntax**

------------------------------------------------------------------------

\
Insert this block into your BEX file:


       [CODEPTR]
       Frame nn = <mnemonic>
       ...

Any number of frames may be listed, one per line.\
\
The Frame number is the same as you would use in Dehacked, and the
\<mnemonic\> is a symbolic codepointer name. These mnemonics, though not
perfectly clear in some cases, are the actual names of the functions in
DOOM that are called, so it makes the most sense to use them. A complete
list of codepointer mnemonics, and full documentation of their
corresponding codepointers\' behaviors is now available in the [Eternity
Engine Definitive Codepointer Reference](codeptrs.html). Please refer to
that file for full information.\
\
When using Dehacked frame code pointer changes, Eternity will write out
a line for each one used showing what you\'d put into a BEX \[CODEPTR\]
block, in the DEHOUT.TXT file. For example:\
\
Original Dehacked file lines:


       Pointer 93 (Frame 196)
       Codep Frame = 176

Output from Eternity in the DEHOUT.TXT file, when -dehout parameter is
used:


       Line='Pointer 93 (Frame 196)'
       Processing function [2] for Pointer
       Processing Pointer at index 196: Frame
        - applied 1c53c from codeptr[176] to states[196]
       BEX [CODEPTR] -> FRAME 196 = Chase

You could then use this in the \[CODEPTR\] block of a BEX file instead
of the Dehacked lines, making it much more clear that in frame 196 you
expect the monster to Chase the player:


       FRAME 196 = Chase

In addition, you can now put any code pointer into any frame, whereas
Dehacked is limited to replacing only the existing ones.\
\
[Return to Table of Contents](#contents)

[]{#helper}

------------------------------------------------------------------------

**Eternity Extension: \[HELPER\] Block**

------------------------------------------------------------------------

\
[]{#helperdesc}

------------------------------------------------------------------------

**\[HELPER\]: Description**

------------------------------------------------------------------------

\
This block can be used to specify the type of thing the player will get
for friendly helpers by using the -dogs command-line option. Rather than
rewiring the MT_DOGS thing to make it emulate other enemies, you may now
make this simple two-line substitution.\
\
[Return to Table of Contents](#contents)

[]{#helpersyntax}

------------------------------------------------------------------------

**\[HELPER\]: Syntax**

------------------------------------------------------------------------

\


       [HELPER]
       type = <thingnum>

\<thingnum\> is the DeHackEd number of the thing type you want to
replace the dogs. If this value is out of range, dogs will spawn and a
warning message will be posted to the player. It may be desirable to
edit the height and radii of most things to make them work better as
helpers Note that the dog jumping options also affect any helper
substitute, and that all friendly things of the helper type are allowed
to jump down to follow the player.\
\
[Return to Table of Contents](#contents)

[]{#sprites}

------------------------------------------------------------------------

**Eternity Extension: \[SPRITES\] Block**

------------------------------------------------------------------------

\
[]{#spritesdesc}

------------------------------------------------------------------------

**\[SPRITES\]: Description**

------------------------------------------------------------------------

\
In Ty Halderman\'s change log he noted that sprite, sound, and music
renaming should be implemented as separate blocks. Up until now, it has
only been possible to replace these strings with the old DeHackEd-style
text blocks, which are impractical to use and have limitations.\
\
The SPRITES block can change the 4-letter prefix of any sprite type.\
\
[Return to Table of Contents](#contents)

[]{#spritessyntax}

------------------------------------------------------------------------

**\[SPRITES\]: Syntax**

------------------------------------------------------------------------

\


       [SPRITES]
       <mnemonic> = <string>

The \<mnemonic\> to use is the normal sprite name. That is, if you want
to rename the sprite that\'s normally named TROO, you use the mnemonic
TROO, even if TROO has already been changed to a new string. Changing
the sprite names will not change their corresponding mnemonics.\
\
\<string\> must be exactly 4 characters long, or the substitution will
not occur.\
\
[Return to Table of Contents](#contents)

[]{#sounds}

------------------------------------------------------------------------

**Eternity Extension: \[SOUNDS\] Block**

------------------------------------------------------------------------

\
[]{#soundsdesc}

------------------------------------------------------------------------

**\[SOUNDS\]: Description**

------------------------------------------------------------------------

\
The SOUNDS block can change the name of any sound.\
\
[Return to Table of Contents](#contents)

[]{#soundssyntax}

------------------------------------------------------------------------

**\[SOUNDS\]: Syntax**

------------------------------------------------------------------------

\


       [SOUNDS]
       <mnemonic> = <string>

The \<mnemonic\> to use is the EDF sound mnemonic defined for the sound
you wish to replace. The string to provide is the new lump name for that
sound. If you want to rename the sound that\'s normally named PISTOL,
you use the mnemonic PISTOL. Changing the sound lump names will not
change their corresponding mnemonics. \<string\> must be from 1 to 8
characters long, or the substitution will not occur.\
\
[Return to Table of Contents](#contents)

[]{#music}

------------------------------------------------------------------------

**Eternity Extension: \[MUSIC\] Block**

------------------------------------------------------------------------

\
[]{#musicdesc}

------------------------------------------------------------------------

**\[MUSIC\]: Description**

------------------------------------------------------------------------

\
The MUSIC block can change the name of any music lump used by the game.\
\
[Return to Table of Contents](#contents)

[]{#musicsyntax}

------------------------------------------------------------------------

**\[MUSIC\]: Syntax**

------------------------------------------------------------------------

\


       [MUSIC]
       <mnemonic> = <string>

The \<mnemonic\> to use is the normal music name, minus the \"D\_\" that
is normally appended to wad lumps. That is, if you want to rename the
music that\'s normally named D_ROMERO, you use the mnemonic ROMERO, even
if ROMERO has already been changed to a new string. Changing the music
names will not change their corresponding mnemonics. \<string\> must be
from 1 to 6 characters long, or the substitution will not occur.\
\
[Return to Table of Contents](#contents)

[]{#textmn}

------------------------------------------------------------------------

**BEX String Mnemonics Table**

------------------------------------------------------------------------

\
Note:\
If the original initial value is longer than about 40 characters, the
string will have been truncated and will have an ellipsis (\...) after
the closing double-quote. There should be enough in the part that is
shown to recognize it. If a string has C language printf() characters in
it, such as %s or %d, you should be sure that your replacement string
either contains the same ones in the same order, or that you leave them
out of your string. If that confuses you, you need to learn C or leave
\'em alone \<g\>. Some trailing linefeeds have been removed for clarity,
though \\n in the middle of a string indicates an internal linefeed and
your replacement can do likewise.

[]{#tpart1}

------------------------------------------------------------------------

**Part 1 - GENERAL INITIALIZATION AND PROMPTS**

------------------------------------------------------------------------

\


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    D_DEVSTR           What shows during init if -devparm is used
                         "Development mode ON."
    D_CDROM            Init: if switch -cdrom is used
                         "CD-ROM Version: default.cfg from c:\\doomdata"
    QUITMSG            Default display when you pick Quit
                         "are you sure you want to\nquit this great game?"
    LOADNET            Warning when you try to load during a network game
                         "you can't do load while in a net game!"
    QLOADNET           Warning when you try quickload during a network game
                         "you can't quickload during a netgame!"
    QSAVESPOT          Warning when you quickload without a quicksaved game
                         "you haven't picked a quicksave slot yet!"
    SAVEDEAD           Warning when not playing or dead and you try to save
                         "you can't save if you aren't playing!"
    QSPROMPT           Prompt when quicksaving your game
                         "quicksave over your game named\n\n'%s'?"
    QLPROMPT           Prompt when quickloading a game
                         "do you want to quickload the game named\n\n'%s'?"
    NEWGAME            Warning if you try to start a game during network play
                         "you can't start a new game\nwhile in a network"...
    NIGHTMARE          Nightmare mode warning
                         "are you sure? this skill level\nisn't even rem"...
    SWSTRING           Warning when you pick any episode but 1 in Shareware
                         "this is the shareware version of doom.\n\nyou '...
    MSGOFF             Message when toggling messages off (F8)
                         "Messages OFF"
    MSGON              Message when toggling messages back on (F8)
                         "Messages ON"
    NETEND             Message when you try to Quit during a network game
                         "you can't end a netgame!"
    ENDGAME            Warning when you try to end the game
                         "are you sure you want to end the game?"
    DETAILHI           Message switching to high detail (obsolete in BOOM)
                         "High detail"
    DETAILLO           Message switching to high detail (obsolete in BOOM)
                         "Low detail"
    GAMMALVL0          Message when gamma correction is set off
                         "Gamma correction OFF"
    GAMMALVL1          Message when gamma correction is set to 1
                         "Gamma correction level 1"
    GAMMALVL2          Message when gamma correction is set to 2
                         "Gamma correction level 2"
    GAMMALVL3          Message when gamma correction is set to 3
                         "Gamma correction level 3"
    GAMMALVL4          Message when gamma correction is set to 4
                         "Gamma correction level 4"
    EMPTYSTRING        Value that shows up in an unused savegame slot
                         "empty slot"
    GGSAVED            Message after a savegame has been written to disk
                         "game saved."
    SAVEGAMENAME       Saved game name, number is appended during save
                         "ETRNSAV"
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart2}

------------------------------------------------------------------------

**Part 2 - MESSAGES WHEN THE PLAYER GETS THINGS**

------------------------------------------------------------------------

\


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    GOTARMOR           Picking up the Green armor (100%)
                         "Picked up the armor."
    GOTMEGA            Picking up the Blue armor (200%)
                         "Picked up the MegaArmor!"
    GOTHTHBONUS        Picking up a health bottle (1%)
                         "Picked up a health bonus."
    GOTARMBONUS        Picking up an armor helmet (1%)
                         "Picked up an armor bonus."
    GOTSTIM            Picking up a stimpack (10%)
                         "Picked up a stimpack."
    GOTMEDINEED        Picking up a medikit (25%) when very low on health
                         "Picked up a medikit that you REALLY need!"
    GOTMEDIKIT         Picking up a medikit (25%) under normal conditions
                         "Picked up a medikit."
    GOTSUPER           Picking up the blue health orb (200%)
                         "Supercharge!"
    GOTBLUECARD        Picking up the blue keycard
                         "Picked up a blue keycard."
    GOTYELWCARD        Picking up the yellow keycard
                         "Picked up a yellow keycard."
    GOTREDCARD         Picking up the red keycard
                         "Picked up a red keycard."
    GOTBLUESKUL        Picking up the blue skull key
                         "Picked up a blue skull key."
    GOTYELWSKUL        Picking up the yellow skull key
                         "Picked up a yellow skull key."
    GOTREDSKULL        Picking up the red skull key
                         "Picked up a red skull key."
    GOTINVUL           Picking up the invulnerability sphere
                         "Invulnerability!"
    GOTBERSERK         Picking up the berserk box
                         "Berserk!"
    GOTINVIS           Picking up the invisibility sphere
                         "Partial Invisibility"
    GOTSUIT            Picking up the rad suit
                         "Radiation Shielding Suit"
    GOTMAP             Picking up the computer map
                         "Computer Area Map"
    GOTVISOR           Picking up the light-amp goggles
                         "Light Amplification Visor"
    GOTMSPHERE         Picking up the mega orb (200%/200%)
                         "MegaSphere!"
    GOTCLIP            Picking up an ammo clip
                         "Picked up a clip."
    GOTCLIPBOX         Picking up a box of ammo
                         "Picked up a box of bullets."
    GOTROCKET          Picking up a single rocket
                         "Picked up a rocket."
    GOTROCKBOX         Picking up a box of rockets
                         "Picked up a box of rockets."
    GOTCELL            Picking up an energy cell (20 units)
                         "Picked up an energy cell."
    GOTCELLBOX         Picking up an energy pack (100 units)
                         "Picked up an energy cell pack."
    GOTSHELLS          Picking up a set of 4 shells
                         "Picked up 4 shotgun shells."
    GOTSHELLBOX        Picking up a box of 20 shells
                         "Picked up a box of shotgun shells."
    GOTBACKPACK        Picking up a backpack
                         "Picked up a backpack full of ammo!"
    GOTBFG9000         Picking up the BFG (weapon 7)
                         "You got the BFG9000!  Oh, yes."
    GOTCHAINGUN        Picking up the chaingun (weapon 4)
                         "You got the chaingun!"
    GOTCHAINSAW        Picking up the chainsaw (weapon 8)
                         "A chainsaw!  Find some meat!"
    GOTLAUNCHER        Picking up the rocket launcher (weapon 5)
                         "You got the rocket launcher!"
    GOTPLASMA          Picking up the plasma gun (weapon 6)
                         "You got the plasma gun!"
    GOTSHOTGUN         Picking up the shotgun (weapon 3)
                         "You got the shotgun!"
    GOTSHOTGUN2        Picking up the double-barreled shotgun (weapon 9)
                         "You got the super shotgun!"
                         
    *New to Eternity: These are mnemonics for collectable items in Heretic.

    HGOTBLUEKEY       Picking up Heretic blue key
                         "BLUE KEY"
    HGOTYELLOWKEY     Picking up Heretic yellow key  
                         "YELLOW KEY"
    HGOTGREENKEY      Picking up Heretic green key
                         "GREEN KEY"
    HITEMHEALTH       Picking up Crystal Vile item
                         "CRYSTAL VIAL"
    HITEMBAGOFHOLDING Picking up Bag of Holding item  
                         "BAG OF HOLDING"
    HITEMSHIELD1      Picking up Silver Shield armor
                         "SILVER SHIELD"
    HITEMSHIELD2      Picking up Enchanted Shield armor
                         "ENCHANTED SHIELD"
    HITEMSUPERMAP     Picking up Map Scroll automap item
                         "MAP SCROLL"
    ------------------ -----------------------------------------------------                     

[Return to Table of Contents](#contents)

[]{#tpart3}

------------------------------------------------------------------------

**Part 3 - MESSAGES WHEN KEYS ARE NEEDED**

------------------------------------------------------------------------

\
(\*)= BOOM extensions


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    PD_BLUEO           When you don't have the blue key for a switch
                         "You need a blue key to activate this object"
    PD_REDO            When you don't have the red key for a switch
                         "You need a red key to activate this object"
    PD_YELLOWO         When you don't have the yellow key for a switch
                         "You need a yellow key to activate this object"
    PD_BLUEK           Blue key needed to open a door
                         "You need a blue key to open this door"
    PD_REDK            Red key needed to open a door
                         "You need a red key to open this door"
    PD_YELLOWK         Yellow key needed to open a door
                         "You need a yellow key to open this door"
    PD_BLUEC           Blue card key needed, not skull (*)
                         "You need a blue card to open this door"
    PD_REDC            Red card key needed, not skull (*)
                         "You need a red card to open this door"
    PD_YELLOWC         Yellow card key needed, not skull (*)
                         "You need a yellow card to open this door"
    PD_BLUES           Blue skull key needed, not card (*)
                         "You need a blue skull to open this door"
    PD_REDS            Red skull key needed, not card (*)
                         "You need a red skull to open this door"
    PD_YELLOWS         Yellow skull key needed, not card (*)
                         "You need a yellow skull to open this door"
    PD_ANY             You need a key but any of them will do (*)
                         "Any key will open this door"
    PD_ALL3            You need red, blue and yellow keys (*)
                         "You need all three keys to open this door"
    PD_ALL6            You need both skulls and cards in all 3 colors (*)
                         "You need all six keys to open this door"
                         
    * New to Eternity: These are used by locked doors instead of some of the above when in Heretic

    HPD_GREENO        Replaces PD_REDO in Heretic mode
                         "You need a green key to activate this object"
    HPD_GREENK        Replaces PD_RED(K/C/S) in Heretic mode
                         "You need a green key to open this door"
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart4}

------------------------------------------------------------------------

**Part 4 - MULTIPLAYER MESSAGING**

------------------------------------------------------------------------

\


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    HUSTR_MSGU         If you try to send a blank message?
                         "[Message unsent]"
    HUSTR_MESSAGESENT  After you send a message?  Haven't seen it
                         "[Message Sent]"

    * Chat macros 1-10
    * Original values in quotes

    HUSTR_CHATMACRO1     "I'm ready to kick butt!"
    HUSTR_CHATMACRO2     "I'm OK."
    HUSTR_CHATMACRO3     "I'm not looking too good!"
    HUSTR_CHATMACRO4     "Help!"
    HUSTR_CHATMACRO5     "You suck!"
    HUSTR_CHATMACRO6     "Next time, scumbag..."
    HUSTR_CHATMACRO7     "Come here!"
    HUSTR_CHATMACRO8     "I'll take care of it."
    HUSTR_CHATMACRO9     "Yes"
    HUSTR_CHATMACRO0     "No"

    * What shows up when you message yourself, depending
    * on how many times you've done it during the game

    HUSTR_TALKTOSELF1    "You mumble to yourself"
    HUSTR_TALKTOSELF2    "Who's there?"
    HUSTR_TALKTOSELF3    "You scare yourself"
    HUSTR_TALKTOSELF4    "You start to rave"
    HUSTR_TALKTOSELF5    "You've lost it..."

    * Prefixes for the multiplayer messages when displayed

    HUSTR_PLRGREEN       "Green: "
    HUSTR_PLRINDIGO      "Indigo: "
    HUSTR_PLRBROWN       "Brown: "
    HUSTR_PLRRED         "Red: "
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart5}

------------------------------------------------------------------------

**Part 5 - LEVEL NAMES IN THE AUTOMAP**

------------------------------------------------------------------------

\


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------

    * For DOOM - These names are made up of HUSTR_ and 
    * ExMx for the episode and map numbers.  Original
    * names are shown in quotes.  Episode 4 entries are
    * for Ultimate DOOM only.  
    * Blank lines separate sky change groupings

    HUSTR_E1M1           "E1M1: Hangar"
    HUSTR_E1M2           "E1M2: Nuclear Plant"
    HUSTR_E1M3           "E1M3: Toxin Refinery"
    HUSTR_E1M4           "E1M4: Command Control"
    HUSTR_E1M5           "E1M5: Phobos Lab"
    HUSTR_E1M6           "E1M6: Central Processing"
    HUSTR_E1M7           "E1M7: Computer Station"
    HUSTR_E1M8           "E1M8: Phobos Anomaly"
    HUSTR_E1M9           "E1M9: Military Base"

    HUSTR_E2M1           "E2M1: Deimos Anomaly"
    HUSTR_E2M2           "E2M2: Containment Area"
    HUSTR_E2M3           "E2M3: Refinery"
    HUSTR_E2M4           "E2M5: Command Center"
    HUSTR_E2M6           "E2M6: Halls of the Damned"
    HUSTR_E2M7           "E2M7: Spawning Vats"
    HUSTR_E2M8           "E2M8: Tower of Babel"
    HUSTR_E2M9           "E2M9: Fortress of Mystery"

    HUSTR_E3M1           "E3M1: Hell Keep"
    HUSTR_E3M2           "E3M2: Slough of Despair"
    HUSTR_E3M3           "E3M3: Pandemonium"
    HUSTR_E3M4           "E3M4: House of Pain"
    HUSTR_E3M5           "E3M5: Unholy Cathedral"
    HUSTR_E3M6           "E3M6: Mt. Erebus"
    HUSTR_E3M7           "E3M7: Limbo"
    HUSTR_E3M8           "E3M8: Dis"
    HUSTR_E3M9           "E3M9: Warrens"

    HUSTR_E4M1           "E4M1: Hell Beneath"
    HUSTR_E4M2           "E4M2: Perfect Hatred"
    HUSTR_E4M3           "E4M3: Sever The Wicked"
    HUSTR_E4M4           "E4M4: Unruly Evil"
    HUSTR_E4M5           "E4M5: They Will Repent"
    HUSTR_E4M6           "E4M6: Against Thee Wickedly"
    HUSTR_E4M7           "E4M7: And Hell Followed"
    HUSTR_E4M8           "E4M8: Unto The Cruel"
    HUSTR_E4M9           "E4M9: Fear"

    * For DOOM2 - These names are made up of HUSTR_ and 
    * a number for the map number.  Original names are shown in quotes.
    * Blank lines separate sky change groupings

    HUSTR_1              "level 1: entryway"
    HUSTR_2              "level 2: underhalls"
    HUSTR_3              "level 3: the gantlet"
    HUSTR_4              "level 4: the focus"
    HUSTR_5              "level 5: the waste tunnels"
    HUSTR_6              "level 6: the crusher"
    HUSTR_7              "level 7: dead simple"
    HUSTR_8              "level 8: tricks and traps"
    HUSTR_9              "level 9: the pit"
    HUSTR_10             "level 10: refueling base"
    HUSTR_11             "level 11: 'o' of destruction!"

    HUSTR_12             "level 12: the factory"
    HUSTR_13             "level 13: downtown"
    HUSTR_14             "level 14: the inmost dens"
    HUSTR_15             "level 15: industrial zone"
    HUSTR_16             "level 16: suburbs"
    HUSTR_17             "level 17: tenements"
    HUSTR_18             "level 18: the courtyard"
    HUSTR_19             "level 19: the citadel"
    HUSTR_20             "level 20: gotcha!"

    HUSTR_21             "level 21: nirvana"
    HUSTR_22             "level 22: the catacombs"
    HUSTR_23             "level 23: barrels o' fun"
    HUSTR_24             "level 24: the chasm"
    HUSTR_25             "level 25: bloodfalls"
    HUSTR_26             "level 26: the abandoned mines"
    HUSTR_27             "level 27: monster condo"
    HUSTR_28             "level 28: the spirit world"
    HUSTR_29             "level 29: the living end"
    HUSTR_30             "level 30: icon of sin"

    HUSTR_31             "level 31: wolfenstein"
    HUSTR_32             "level 32: grosse"

    * For PLUTONIA - These names are made up of PHUSTR_ and 
    * a number for the map number.  Original names are shown in quotes.
    * Blank lines separate sky change groupings

    PHUSTR_1             "level 1: congo"
    PHUSTR_2             "level 2: well of souls"
    PHUSTR_3             "level 3: aztec"
    PHUSTR_4             "level 4: caged"
    PHUSTR_5             "level 5: ghost town"
    PHUSTR_6             "level 6: baron's lair"
    PHUSTR_7             "level 7: caughtyard"
    PHUSTR_8             "level 8: realm"
    PHUSTR_9             "level 9: abattoire"
    PHUSTR_10            "level 10: onslaught"
    PHUSTR_11            "level 11: hunted"

    PHUSTR_12            "level 12: speed"
    PHUSTR_13            "level 13: the crypt"
    PHUSTR_14            "level 14: genesis"
    PHUSTR_15            "level 15: the twilight"
    PHUSTR_16            "level 16: the omen"
    PHUSTR_17            "level 17: compound"
    PHUSTR_18            "level 18: neurosphere"
    PHUSTR_19            "level 19: nme"
    PHUSTR_20            "level 20: the death domain"

    PHUSTR_21            "level 21: slayer"
    PHUSTR_22            "level 22: impossible mission"
    PHUSTR_23            "level 23: tombstone"
    PHUSTR_24            "level 24: the final frontier"
    PHUSTR_25            "level 25: the temple of darkness"
    PHUSTR_26            "level 26: bunker"
    PHUSTR_27            "level 27: anti-christ"
    PHUSTR_28            "level 28: the sewers"
    PHUSTR_29            "level 29: odyssey of noises"
    PHUSTR_30            "level 30: the gateway of hell"

    PHUSTR_31            "level 31: cyberden"
    PHUSTR_32            "level 32: go 2 it"

    * For TNT:Evilution - These names are made up of THUSTR_ and 
    * a number for the map number.  Original names are shown in quotes.
    * Blank lines separate sky change groupings

    THUSTR_1             "level 1: system control"
    THUSTR_2             "level 2: human bbq"
    THUSTR_3             "level 3: power control"
    THUSTR_4             "level 4: wormhole"
    THUSTR_5             "level 5: hanger"
    THUSTR_6             "level 6: open season"
    THUSTR_7             "level 7: prison"
    THUSTR_8             "level 8: metal"
    THUSTR_9             "level 9: stronghold"
    THUSTR_10            "level 10: redemption"
    THUSTR_11            "level 11: storage facility"

    THUSTR_12            "level 12: crater"
    THUSTR_13            "level 13: nukage processing"
    THUSTR_14            "level 14: steel works"
    THUSTR_15            "level 15: dead zone"
    THUSTR_16            "level 16: deepest reaches"
    THUSTR_17            "level 17: processing area"
    THUSTR_18            "level 18: mill"
    THUSTR_19            "level 19: shipping/respawning"
    THUSTR_20            "level 20: central processing"

    THUSTR_21            "level 21: administration center"
    THUSTR_22            "level 22: habitat"
    THUSTR_23            "level 23: lunar mining project"
    THUSTR_24            "level 24: quarry"
    THUSTR_25            "level 25: baron's den"
    THUSTR_26            "level 26: ballistyx"
    THUSTR_27            "level 27: mount pain"
    THUSTR_28            "level 28: heck"
    THUSTR_29            "level 29: river styx"
    THUSTR_30            "level 30: last call"

    THUSTR_31            "level 31: pharaoh"
    THUSTR_32            "level 32: caribbean"

    * New to Eternity:
    * For Heretic - These names are made up of HHUSTR_ and ExMx for the episode 
    * and map numbers.  Original names are shown in quotes.  Episode 4 and 5 entries 
    * are for the extended Shadow of the Serpent Riders version of Heretic. Note these 
    * are also used by the Heretic intermission code.
    * Blank lines separate sky change groupings

    HHUSTR_E1M1          "E1M1:  THE DOCKS"
    HHUSTR_E1M2          "E1M2:  THE DUNGEONS"
    HHUSTR_E1M3          "E1M3:  THE GATEHOUSE"
    HHUSTR_E1M4          "E1M4:  THE GUARD TOWER"
    HHUSTR_E1M5          "E1M5:  THE CITADEL"
    HHUSTR_E1M6          "E1M6:  THE CATHEDRAL"
    HHUSTR_E1M7          "E1M7:  THE CRYPTS"
    HHUSTR_E1M8          "E1M8:  HELL'S MAW"
    HHUSTR_E1M9          "E1M9:  THE GRAVEYARD"

    HHUSTR_E2M1          "E2M1:  THE CRATER"
    HHUSTR_E2M2          "E2M2:  THE LAVA PITS"
    HHUSTR_E2M3          "E2M3:  THE RIVER OF FIRE"
    HHUSTR_E2M4          "E2M4:  THE ICE GROTTO"
    HHUSTR_E2M5          "E2M5:  THE CATACOMBS"
    HHUSTR_E2M6          "E2M6:  THE LABYRINTH"
    HHUSTR_E2M7          "E2M7:  THE GREAT HALL"
    HHUSTR_E2M8          "E2M8:  THE PORTALS OF CHAOS"
    HHUSTR_E2M9          "E2M9:  THE GLACIER"

    HHUSTR_E3M1          "E3M1:  THE STOREHOUSE"
    HHUSTR_E3M2          "E3M2:  THE CESSPOOL"
    HHUSTR_E3M3          "E3M3:  THE CONFLUENCE"
    HHUSTR_E3M4          "E3M4:  THE AZURE FORTRESS"
    HHUSTR_E3M5          "E3M5:  THE OPHIDIAN LAIR"
    HHUSTR_E3M6          "E3M6:  THE HALLS OF FEAR"
    HHUSTR_E3M7          "E3M7:  THE CHASM"
    HHUSTR_E3M8          "E3M8:  D'SPARIL'S KEEP"
    HHUSTR_E3M9          "E3M9:  THE AQUIFER"

    HHUSTR_E4M1          "E4M1:  CATAFALQUE"
    HHUSTR_E4M2          "E4M2:  BLOCKHOUSE"
    HHUSTR_E4M3          "E4M3:  AMBULATORY"
    HHUSTR_E4M4          "E4M4:  SEPULCHER"
    HHUSTR_E4M5          "E4M5:  GREAT STAIR"
    HHUSTR_E4M6          "E4M6:  HALLS OF THE APOSTATE"
    HHUSTR_E4M7          "E4M7:  RAMPARTS OF PERDITION"
    HHUSTR_E4M8          "E4M8:  SHATTERED BRIDGE"
    HHUSTR_E4M9          "E4M9:  MAUSOLEUM"

    HHUSTR_E5M1          "E5M1:  OCHRE CLIFFS"
    HHUSTR_E5M2          "E5M2:  RAPIDS"
    HHUSTR_E5M3          "E5M3:  QUAY"
    HHUSTR_E5M4          "E5M4:  COURTYARD"
    HHUSTR_E5M5          "E5M5:  HYDRATYR"
    HHUSTR_E5M6          "E5M6:  COLONNADE"
    HHUSTR_E5M7          "E5M7:  FOETID MANSE"
    HHUSTR_E5M8          "E5M8:  FIELD OF JUDGEMENT"
    HHUSTR_E5M9          "E5M9:  SKEIN OF D'SPARIL"
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart6}

------------------------------------------------------------------------

**Part 6 - MESSAGES AS A RESULT OF TOGGLING STATES**

------------------------------------------------------------------------

\


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    AMSTR_FOLLOWON     Automap follow mode toggled on (F)
                         "Follow Mode ON"
    AMSTR_FOLLOWOFF    Automap follow mode toggled off (F)
                         "Follow Mode OFF"
    AMSTR_GRIDON       Automap grid mode toggled on (G)
                         "Grid ON"
    AMSTR_GRIDOFF      Automap grid mode toggled off (G)
                         "Grid OFF"
    AMSTR_MARKEDSPOT   Automap spot marked (M)
                         "Marked Spot"
    AMSTR_MARKSCLEARED Automap marks cleared (C)
                         "All Marks Cleared" 
    STSTR_MUS          Music changed with IDMUSnn
                         "Music Change"
    STSTR_NOMUS        Error message [IDMUS] to bad number
                         "IMPOSSIBLE SELECTION"
    STSTR_DQDON        God mode toggled on [IDDQD]
                         "Degreelessness Mode On"
    STSTR_DQDOFF       God mode toggled off [IDDQD]
                         "Degreelessness Mode Off"
    STSTR_KFAADDED     Ammo and keys added [IDKFA]
                         "Very Happy Ammo Added"
    STSTR_FAADDED      Ammo no keys added [IDK or IDKA]
                         "Ammo (no keys) Added"
    STSTR_NCON         Walk through walls toggled on [IDCLIP or IDSPISPOPD]
                         "No Clipping Mode ON"
    STSTR_NCOFF        Walk through walls off [IDCLIP or IDSPISPOPD]
                         "No Clipping Mode OFF"
    STSTR_BEHOLD       Prompt for IDBEHOLD cheat menu
                         "inVuln, Str, Inviso, Rad, Allmap, or Lite-amp"
    STSTR_BEHOLDX      Prompt after toggling the special with IDBEHOLD
                         "Power-up Toggled"
    STSTR_CHOPPERS     Message when the chainsaw is picked [IDCHOPPERS]
                         "... doesn't suck - GM"
    STSTR_CLEV         Message while changing levels [IDCLEVxx]
                         "Changing Level..."
    STSTR_COMPON       Message when turning on DOOM compatibility mode (*)
                         "Compatibility Mode On"
    STSTR_COMPOFF      Message when turning off DOOM compatibility mode (*)
                         "Compatibility Mode Off"
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart7}

------------------------------------------------------------------------

**Part 7 - EPISODE INTERMISSION TEXTS**

------------------------------------------------------------------------

\


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------

    * DOOM/Ultimate DOOM Intermissions

    E1TEXT             Message after Episode 1 of DOOM
                         "Once you beat the big badasses and\nclean out "...
    E2TEXT             Message after Episode 2 of DOOM
                         "You've done it! The hideous cyber-\ndemon lord"...
    E3TEXT             Message after Episode 3 of DOOM
                         "The loathsome spiderdemon that\nmasterminded t"...
    E4TEXT             Message after Episode 4 of Ultimate DOOM
                         "the spider mastermind must have sent forth\nit"...

    * DOOM2 Intermissions

    C1TEXT             Message after MAP06
                         "YOU HAVE ENTERED DEEPLY INTO THE INFESTED\nSTA"...
    C2TEXT             Message after MAP11
                         "YOU HAVE WON! YOUR VICTORY HAS ENABLED\nHUMANK"...
    C3TEXT             Message after MAP20
                         "YOU ARE AT THE CORRUPT HEART OF THE CITY,\nSUR"...
    C4TEXT             Message after MAP30
                         "THE HORRENDOUS VISAGE OF THE BIGGEST\nDEMON YO"...
    C5TEXT             Message when entering MAP31
                         "CONGRATULATIONS, YOU'VE FOUND THE SECRET\nLEVE"...
    C6TEXT             Message when entering MAP32
                         "CONGRATULATIONS, YOU'VE FOUND THE\nSUPER SECRE"...

    * PLUTONIA Intermissions

    P1TEXT             Message after MAP06
                         "You gloat over the steaming carcass of the\nGu"...
    P2TEXT             Message after MAP11
                         "Even the deadly Arch-Vile labyrinth could\nnot"...
    P3TEXT             Message after MAP20
                         "You've bashed and battered your way into\nthe "...
    P4TEXT             Message after MAP30
                         "The Gatekeeper's evil face is splattered\nall "...
    P5TEXT             Message when entering MAP31
                         "You've found the second-hardest level we\ngot."...
    P6TEXT             Message when entering MAP32
                         "Betcha wondered just what WAS the hardest\nlev"...

    * TNT:Evilution Intermissions

    T1TEXT             Message after MAP06
                         "You've fought your way out of the infested\nex"...
    T2TEXT             Message after MAP11
                         "You hear the grinding of heavy machinery\nahea"...
    T3TEXT             Message after MAP20
                         "The vista opening ahead looks real damn\nfamil"...
    T4TEXT             Message after MAP30
                         "Suddenly, all is silent, from one horizon\nto "...
    T5TEXT             Message when entering MAP31
                         "What now? Looks totally different. Kind\nof li"...
    T6TEXT             Message when entering MAP32
                         "Time for a vacation. You've burst the\nbowels "...

    * New to Eternity: Heretic Intermissions

    H1TEXT             Message after Episode 1
                         "with the destruction of the iron\nliches and t"...
    H2TEXT             Message after Episode 2
                         "the mighty maulotaurs have proved\nto be no ma"...
    H3TEXT             Message after Episode 3
                         "the death of d'sparil has loosed\nthe magical "...
    H4TEXT             Message after SOSR Episode 4
                         "you thought you would return to your\nown worl"...
    H5TEXT             Message after SOSR Episode 5
                         "as the final maulotaur bellows his\ndeath-agon"...
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart8}

------------------------------------------------------------------------

**Part 8 - CREATURE NAMES FOR THE FINALE**

------------------------------------------------------------------------

\


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    CC_ZOMBIE          Trooper
                         "ZOMBIEMAN"
    CC_SHOTGUN         Sargeant
                         "SHOTGUN GUY"
    CC_HEAVY           Chaingunner
                         "HEAVY WEAPON DUDE"
    CC_IMP             Imp
                         "IMP"
    CC_DEMON           Demon
                         "DEMON"
    CC_LOST            Lost Soul
                         "LOST SOUL"
    CC_CACO            Cacodemon
                         "CACODEMON"
    CC_HELL            Hell Knight
                         "HELL KNIGHT"
    CC_BARON           Baron of Hell
                         "BARON OF HELL"
    CC_ARACH           Arachnotron (baby spider)
                         "ARACHNOTRON"
    CC_PAIN            Pain Elemental
                         "PAIN ELEMENTAL"
    CC_REVEN           Revenant
                         "REVENANT"
    CC_MANCU           Mancubus
                         "MANCUBUS"
    CC_ARCH            Arch Vile
                         "ARCH-VILE"
    CC_SPIDER          Spider Mastermind
                         "THE SPIDER MASTERMIND"
    CC_CYBER           Cyberdemon
                         "THE CYBERDEMON"
    CC_HERO            Green player
                         "OUR HERO"
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart9}

------------------------------------------------------------------------

**Part 9 - INTERMISSION TILED BACKGROUNDS**

------------------------------------------------------------------------

\


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    BGFLATE1           End of DOOM Episode 1
                         "FLOOR4_8"
    BGFLATE2           End of DOOM Episode 2
                         "SFLR6_1"
    BGFLATE3           End of DOOM Episode 3
                         "MFLR8_4"
    BGFLATE4           End of DOOM Episode 4
                         "MFLR8_3"
    BGFLAT06           DOOM2 after MAP06
                         "SLIME16"
    BGFLAT11           DOOM2 after MAP11
                         "RROCK14"
    BGFLAT20           DOOM2 after MAP20
                         "RROCK07"
    BGFLAT30           DOOM2 after MAP30
                         "RROCK17"
    BGFLAT15           DOOM2 going MAP15 to MAP31
                         "RROCK13"
    BGFLAT31           DOOM2 going MAP31 to MAP32
                         "RROCK19"
    BGCASTCALL         Panel behind cast call    
                         "BOSSBACK"
                         
    * New to Eternity

    BGFLATHE1          End of Heretic Episode 1
                         "FLOOR25"
    BGFLATHE2          End of Heretic Episode 2
                         "FLATHUH1"
    BGFLATHE3          End of Heretic Episode 3
                         "FLTWAWA1"
    BGFLATHE4          End of Heretic:SOSR Episode 4
                         "FLOOR28"
    BGFLATHE5          End of Heretic:SOSR Episode 5
                         "FLOOR08"
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart10}

------------------------------------------------------------------------

**Part 10 - NEW: STARTUP BANNERS**

------------------------------------------------------------------------

\
Note: If Eternity is set to start in graphics mode, these messages will
be displayed in the console after the name of the engine is displayed,
rather than first off in text mode. Otherwise they behave as in BOOM and
MBF.


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    STARTUP1           Starting strings to be displayed during startup
                         ""
    STARTUP2           Leave them blank to not display
                         ""
    STARTUP3           You may include \n to split a line
                         ""
    STARTUP4           Have a nice time
                         ""
    STARTUP5           Burma Shave
                         ""
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart11}

------------------------------------------------------------------------

**Part 10 - NEW: OBITUARIES**

------------------------------------------------------------------------

\
These are the built-in obituary strings, which are new to Eternity. They
are shown to the player when he/she dies in certain ways. Note that
obituaries for enemy kills are defined via EDF, and cannot be edited by
DeHackEd/BEX files. The player name is always inserted before the
beginning of an obituary string, along with a space.


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    OB_SUICIDE         Displayed when player kills self via console command
                         "suicides"
    OB_FALLING         Displayed when player dies by falling too far
                         "cratered"
    OB_CRUSH           Displayed when player dies via crusher
                         "was squished"
    OB_SLIME           Displayed when player dies via damaging floor
                         "melted"
    OB_BARREL          Displayed when player killed by inanimate explosion
                         "was blown up"
    OB_COOP            Displayed when player kills player in coop mode
                         "scored a frag for the other team"
    OB_DEFAULT         Displayed on unknown or miscellaneous death
                         "died"
    OB_ROCKET_SELF     Displayed when rocket kills self
                         "should have stood back"
    OB_BFG11K_SELF     Displayed when BFG11k splash damage kills self
                         "used a BFG11k up close"
    OB_FIST            Displayed when player dies by punch in DM
                         "was punched to death"
    OB_CHAINSAW        Displayed when player dies by chainsaw in DM
                         "was mistaken for a tree"
    OB_PISTOL          Displayed when player dies by pistol in DM
                         "took a slug to the head"
    OB_SHOTGUN         Displayed when player dies by shotgun in DM
                         "couldn't duck the buckshot"
    OB_SSHOTGUN        Displayed when player dies by SSG in DM
                         "was shown some double barrels"
    OB_CHAINGUN        Displayed when player dies by chaingun in DM
                         "did the chaingun cha-cha"
    OB_ROCKET          Displayed when player dies by rocket in DM
                         "was blown apart by a rocket"
    OB_PLASMA          Displayed when player dies by plasma in DM
                         "admires the pretty blue stuff"
    OB_BFG             Displayed when player dies by BFG shot in DM
                         "was utterly destroyed by the BFG"
    OB_BFG_SPLASH      Displayed when player dies by BFG tracer in DM
                         "saw the green flash"
    OB_BETABFG         Displayed when player dies by Beta BFG in DM
                         "got a blast from the past"
    OB_BFGBURST        Displayed when player dies by Plasma Burst BFG in DM
                         "was caught in a plasma storm"
    OB_GRENADE_SELF    Displayed when grenade kills self
                         "lost track of a grenade"
    OB_GRENADE         Displayed when player dies by grenade in DM
                         "tripped on a grenade"
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#tpart12}

------------------------------------------------------------------------

**Part 10 - NEW: DEMO SEQUENCE/HELP/CREDITS**

------------------------------------------------------------------------

\
These are the names of resources used by the demo/title sequence, help
screens, and credit/about screens.


    MNEMONIC           Purpose
                         Initial original value (just the first part if long)
    ------------------ -----------------------------------------------------
    TITLEPIC           DOOM / DOOM II Titlepic resource
                         "TITLEPIC"
    TITLE              Heretic Titlepic resource
                         "TITLE"
    CREDIT             Static credits screen for original game authors
                         "CREDIT"
    HELP2              Shareware advertisement help screen
                         "HELP2"
    ORDER              Shareware advertisement help screen
                         "ORDER"
    DEMO1              First demo played in title sequence
                         "DEMO1"
    DEMO2              Second demo played in title sequence
                         "DEMO2"
    DEMO3              Third demo played in title sequence
                         "DEMO3"
    DEMO4              Fourth demo played in title sequence
                         "DEMO4"
    ------------------ -----------------------------------------------------

[Return to Table of Contents](#contents)

[]{#sndtable}

------------------------------------------------------------------------

**Sound Table**

------------------------------------------------------------------------

\
This is a current list of the default sounds defined by sounds.edf. The
number by each sound is the number that is used in Thing blocks and
elsewhere to call for this sound by number (also known as its sound
DeHackEd number). Note: Heretic sounds have not been standardized yet
and thus are not listed here. You should not attempt to use them until
they have been documented.


      Number Name
      ------------------------
      0      None
      1      "pistol"
      2      "shotgn"
      3      "sgcock"
      4      "dshtgn"
      5      "dbopn"
      6      "dbcls"
      7      "dbload"
      8      "plasma"
      9      "bfg"
      10     "sawup"
      11     "sawidl"
      12     "sawful"
      13     "sawhit"
      14     "rlaunc"
      15     "rxplod"
      16     "firsht"
      17     "firxpl"
      18     "pstart"
      19     "pstop"
      20     "doropn"
      21     "dorcls"
      22     "stnmov"
      23     "swtchn"
      24     "swtchx"
      25     "plpain"
      26     "dmpain"
      27     "popain"
      28     "vipain"
      29     "mnpain"
      30     "pepain"
      31     "slop"
      32     "itemup"
      33     "wpnup"
      34     "oof"
      35     "telept"
      36     "posit1"
      37     "posit2"
      38     "posit3"
      39     "bgsit1"
      40     "bgsit2"
      41     "sgtsit"
      42     "cacsit"
      43     "brssit"
      44     "cybsit"
      45     "spisit"
      46     "bspsit"
      47     "kntsit"
      48     "vilsit"
      49     "mansit"
      50     "pesit"
      51     "sklatk"
      52     "sgtatk"
      53     "skepch"
      54     "vilatk"
      55     "claw"
      56     "skeswg"
      57     "pldeth"
      58     "pdiehi"
      59     "podth1"
      60     "podth2"
      61     "podth3"
      62     "bgdth1"
      63     "bgdth2"
      64     "sgtdth"
      65     "cacdth"
      66     "skldth"
      67     "brsdth"
      68     "cybdth"
      69     "spidth"
      70     "bspdth"
      71     "vildth"
      72     "kntdth"
      73     "pedth"
      74     "skedth"
      75     "posact"
      76     "bgact"
      77     "dmact"
      78     "bspact"
      79     "bspwlk"
      80     "vilact"
      81     "noway"
      82     "barexp"
      83     "punch"
      84     "hoof"
      85     "metal"
      86     "chgun"
      87     "tink"
      88     "bdopn"
      89     "bdcls"
      90     "itmbk"
      91     "flame"
      92     "flamst"
      93     "getpow"
      94     "bospit"
      95     "boscub"
      96     "bossit"
      97     "bospn"
      98     "bosdth"
      99     "manatk"
      100    "mandth"
      101    "sssit"
      102    "ssdth"
      103    "keenpn"
      104    "keendt"
      105    "skeact"
      106    "skesit"
      107    "skeatk"
      108    "radio"
      109    "dgsit"
      110    "dgatk"
      111    "dgact"
      112    "dgdth"
      113    "dgpain"
      ------------------------

[Return to Table of Contents](#contents)\
\
