## Eternity Engine Editing Reference v1.91 \-- 09/02/08

[Return to the Eternity Engine Page](../etcengine.html)

- [**Table of Contents**]{#contents}
  - [Part I Linedef Types](#part1)
    - [Section 0. Terminology](#terms)
    - [Section 1. Doors](#doors)
      - [1.1 Door functions](#doorfunc)
      - [1.2 Varieties of doors](#doorvar)
      - [1.3 Door linedef types](#doorlines)
    - [Section 2. Floors](#floors)
      - [2.1 Floor targets](#floortarg)
      - [2.2 Varieties of floors](#floorvar)
      - [2.3 Floor linedef types](#floorlines)
    - [Section 3. Ceilings](#ceils)
      - [3.1 Ceiling targets](#ceiltarg)
      - [3.2 Varieties of ceilings](#ceilvar)
      - [3.3 Ceiling linedef types](#ceillines)
    - [Section 4. Platforms (Lifts)](#plats)
      - [4.1 Platform targets](#plattarg)
      - [4.2 Varieties of platforms](#platvar)
      - [4.3 Platform linedef types](#platlines)
    - [Section 5. Crusher Ceilings](#crush)
      - [5.1 Varieties of crushers](#crushvar)
      - [5.2 Crusher linedef types](#crushlines)
    - [Section 6. Stair Builders](#stairs)
      - [6.1 Varieties of stair builders](#stairvar)
      - [6.2 Regular and extended stair builder types](#stairlines)
    - [Section 7. Elevators](#elevs)
      - [7.1 Elevator targets](#elevtarg)
      - [7.2 Elevator linedef types](#elevlines)
    - [Section 7a. Lighting](#lights)
      - [7a.1 Lighting targets](#lighttarg)
      - [7a.2 Lighting linedef types](#lightlines)
    - [Section 8. Exits](#exits)
      - [8.1 Exit varieties](#exitvar)
      - [8.2 Exit linedef types](#exitlines)
    - [Section 9. Teleporters](#teles)
      - [9.1 Teleport varieties](#televar)
      - [9.2 Teleport linedef types](#telelines)
    - [Section 10. Donuts](#donuts)
      - [10.1 Donut linedef types](#donutlines)
    - [Section 11. Property Transfer](#props)
      - [213: Floor light transfer](#line213)
      - [261: Ceiling light transfer](#line261)
      - [260: Translucency transfer](#line260)
      - [242: Deep water](#line242)
      - [223: Friction transfer](#line223)
      - [271/272: Sky transfer](#skytrans)
      - [224/225: Wind/current transfer](#consttrans)
      - [226: Point pusher transfer](#pointtrans)
      - [281/282: 3DMidTex movement transfer](#3dmidtex)
      - [293/294: Heretic wind/current transfer](#hticpush)
    - [Section 12. Scrolling Walls, Flats, Objects](#scrolls)
      - [12.1 Static scrollers](#staticscroll)
      - [12.2 Simple static scrollers](#simplescroll)
      - [12.3 Dynamic scrolling](#dynscroll)
    - [Section 13. Portals](#portals)
      - [283/284/285: Plane portals](#planeportal)
      - [286/287/288: Horizon portals](#horizportal)
      - [290/291/292: Skyboxes](#skybxportal)
      - [295/296/297: Anchored portals](#anchrportal)
      - [344/345: Two-Way Anchored portals](#twowyportal)
      - [289: Portal transfer](#line289)
    - [Section 14. Script Control Linedefs](#scripts)
    - [Section 15. PolyObjects](#polyobj)
      - [15.1 Creating PolyObjects](#polyobj_create)
      - [15.2 Moving PolyObjects](#polyobj_move)
      - [15.3 PolyObject Doors](#polyobj_door)
    - [Section 16. Pillars](#pillars)
      - [Pillar varieties](#pillarvars)
      - [Pillar linedef types](#pillarlines)
    - [Section 17. Attached Surfaces](#attached)
      - 
    - [Section 18. Detailed Generalized Linedef
      Specification](#genlines)
      - [Line ranges/nomenclature](#ranges)
      - [Floors](#genfloors)
      - [Ceilings](#genceils)
      - [Doors](#gendoors)
      - [Locked Doors](#genlocked)
      - [Lifts](#genlifts)
      - [Stairs](#genstairs)
      - [Crushers](#gencrush)
    - [Section 19. Detailed Parameterized Linedef
      Specification](#paramlines)
      - [Floors](#paramfloors)
      - [Ceilings](#paramceils)
      - [Doors](#paramdoors)
      - [Stairs](#paramstairs)
      - [PolyObjects](#parampolyobj)
      - [Pillars](#parampillars)
      - [Lighting](#paramlights)
      - [Miscellaneous](#parammisc)
  - [Part II Sector Types](#part2)
    - [Section 1. Regular sector types](#sectors)
    - [Section 2. Generalized sector types](#gensectors)
  - [Part III Linedef Flags](#part3)
  - [Part IV Thing Flags](#part4)
  - [Part V Thing Types](#part5)
  - [Part VI BOOM Predefined Lumps](#part6)
    - [Lump names](#lumpnames)
    - [SWITCHES format](#switchformat)
    - [ANIMATED format](#animformat)
    - [Colormaps](#colmaps)
    - [Color translation tables](#colrngs)
    - [TXTRCONV format](#txtrconv)
    - [KEYDEFS format](#keydefs)
  - [Part VII ExtraData](#extradata)
    - [1.1 ExtraData Introduction](#edintro)
    - [1.2 ExtraData Control Objects](#edthing)
    - [1.3 ExtraData Control Linedefs](#edlines)

\
\

[]{#part1}

------------------------------------------------------------------------

### Part I Linedef Types

------------------------------------------------------------------------

[ ]{#terms}

------------------------------------------------------------------------

**Section 0. Terminology**

------------------------------------------------------------------------

\
In BOOM actions are caused to happen in the game thru linedef types.
BOOM has three kinds of linedef types:\

- Regular - the linedef types that were already in DOOM II v1.9
- Extended - linedef types not in DOOM II v1.9 but less than 8192 in
  value
- Generalized - linedef types over 8192 in value that contain bit fields
  that independently control the actions of the sector affected.

As of Eternity Engine v3.33.00, a new type of linedef special is
implemented called the Parameterized Line Special. Parameterized
specials are similar to Extended specials, but they can be given fully
customizable arguments via the [ExtraData](extradata.html) linedef field
\"args\". These line types are similar to those in the Hexen engine, and
are heavily interoperable with the Small scripting system. In addition,
parameterized specials are subject to a completely separate system for
determining how they are activated and what can activate them that is
far more flexible than that used by normal lines. A linedef with
non-zero linedef type is called a linedef trigger. Normal, extended, and
generalized linedef triggers are always activated in one of three ways -
pushing on the first sidedef of the linedef, walking over the linedef,
or shooting the linedef with an impact weapon (fists, chainsaw, pistol,
shotgun, double barreled, or chaingun).\
\
Parameterized line specials\' activation methods are controlled by flags
in the ExtraData linedef \"extflags\" field. A parameterized line can be
activated in multiple ways simultaneously, and can also be activated in
ways not available for normal lines, including being used from the back
side and impacted by projectiles. The extflags field also controls what
classes of objects can perform the activation (monsters, missiles, and
players), whether or not the special activates from the second side of
the line, and whether or not the line special is repeatable. See the
[ExtraData](extradata.html) documentation for full information.\
\
Linedefs activated by pushing on them come in two varieties. A manual
linedef affects the sector on the second sidedef of the line pushed. A
switched linedef affects all sectors that have the same tag field as the
linedef that was pushed.\
\
Nearly all switched, walkover, and gun linedefs operate on the sectors
with the same tag as that in the tag field of the linedef. Most
parameterized specials take the tag of sectors to act on as their first
parameter, freeing the line tag for use as a line identifier.\
\
Some linedefs are never activated per se, but simply create or control
an effect thru their existence and properties, usually affecting the
sectors sharing the linedef\'s tags. There are also a few special case
like line-line teleporters and exit linedefs that do not affect
sectors.\
\
Some linedefs are only triggerable once, others are triggerable as many
times as you like.\
\
Triggering types are denoted by the letters P, S, W, and G for
manual(push), switched, walkover, and gun. Their retriggerability is
denoted by a 1 or R following the letter. So the triggering types for
linedefs are:\
\


    P1 PR S1 SR W1 WR G1 GR

Note: P1/PR types are also sometimes referred to as D1/DR types.\
\
Often linedef actions depend on values in neighboring sectors. A
neighboring sector is one that shares a linedef in common, just sharing
a vertex is not sufficient.\
\
In DOOM only one action on a sector could occur at a time. BOOM supports
one floor action, one ceiling action, and one lighting action
simultaneously.\
\
[Return to Table of Contents](#contents)

[]{#doors}

------------------------------------------------------------------------

**Section 1. Doors**

------------------------------------------------------------------------

\
A door is a sector, usually placed between two rooms, whose ceiling
raises to open, and lowers to close.\
\
A door is fully closed when its ceiling height is equal to its floor
height.\
\
A door is fully open when its ceiling height is 4 less than the lowest
neighbor ceiling adjacent to it.\
\
A door may be set to an intermediate state initially, or thru the action
of a linedef trigger that affects ceilings or floors. The door is
passable to a player when its ceiling is at least 56 units higher than
its floor. In general the door is passable to a monster or a thing when
its ceiling is at least the monster or thing\'s height above the floor.\
\
If a door has a ceiling height ABOVE the fully open height, then an open
door action moves the ceiling to the fully open height instantly.\
\
If a door has a ceiling height BELOW the fully closed height (that is
the ceiling of the door sector is lower than the floor of the door
sector) a close door action moves the ceiling to the fully closed height
instantly.

[]{#doorfunc}

------------------------------------------------------------------------

**Section 1.1 Door functions**

------------------------------------------------------------------------

\

- Open, Wait, Then Close\
  On activation, door opens fully, waits a specified period, then closes
  fully. This door type is also known as a \"Raise\" door.\
  \
- Open and Stay Open\
  On activation, door opens fully and remains there.\
  \
- Close and Stay Closed\
  On activation, door closes fully, and remains there.\
  \
- Close, Wait, Then Open\
  On activation, door closes fully, waits a specified period, then opens
  fully.\
  \
- Wait, Open, Wait, Close\
  Available as a parameterized line special only, this door type waits
  for a specified countdown period, and then functions as a normal
  \"Raise\" door.\
  \
- Wait, Close and Stay Closed\
  Available as a parameterized line special only, this door type waits
  for a specified countdown period, and then closes fully and remains
  closed.

[]{#doorvar}

------------------------------------------------------------------------

**Section 1.2 Varieties of doors**

------------------------------------------------------------------------

\
A door can be triggered by pushing on it, walking over or pressing a
linedef trigger tagged to it, or shooting a linedef tagged to it. These
are called manual, walkover, switched, or gun doors resp.\
\
Since a push door (P1/PR) has no use for its tag, BOOM has extended the
functionality to include changing any tagged sectors to maximum neighbor
lighting on fully opening, then to minimum neighbor lighting on fully
closing. This is true for regular, extended, and generalized doors with
push triggers. Parameterized push doors use a separate argument to
control dynamic door lighting.\
\
A door trigger can be locked. This means that the door function will
only operate if the player is in possession of the right key(s). Regular
and extended door triggers only care if the player has a key of the
right color, they do not care which. Generalized door triggers can
distinguish between skull and card keys, and can also require any key,
or all keys in order to activate.\
\
A door can have different speeds, slow, normal, fast or turbo.
Parameterized doors can open at any speed.\
\
A door can wait for different amounts of time.\
\
A door may or may not be activatable by monsters.\
\
Any door function except Close and Stay Closed, when closing and
encountering a monster or player\'s head will bounce harmlessly off that
head and return to fully open. A Close and Stay Closed will rest on the
head until it leaves the door sector.

[]{#doorlines}

------------------------------------------------------------------------

**Section 1.3 Door linedef types**

------------------------------------------------------------------------

\


    Regular and Extended Door Types
    ---------------------------------------------------------------
    #        Class   Trig   Lock   Speed    Wait  Monst    Function

    1        Reg     PR     No     Slow     4s    Yes      Open, Wait, Then Close
    117      Reg     PR     No     Fast     4s    No       Open, Wait, Then Close
    63       Reg     SR     No     Slow     4s    No       Open, Wait, Then Close
    114      Reg     SR     No     Fast     4s    No       Open, Wait, Then Close
    29       Reg     S1     No     Slow     4s    No       Open, Wait, Then Close
    111      Reg     S1     No     Fast     4s    No       Open, Wait, Then Close
    90       Reg     WR     No     Slow     4s    No       Open, Wait, Then Close
    105      Reg     WR     No     Fast     4s    No       Open, Wait, Then Close
    4        Reg     W1     No     Slow     4s    No       Open, Wait, Then Close
    108      Reg     W1     No     Fast     4s    No       Open, Wait, Then Close

    31       Reg     P1     No     Slow     --    No       Open and Stay Open
    118      Reg     P1     No     Fast     --    No       Open and Stay Open
    61       Reg     SR     No     Slow     --    No       Open and Stay Open
    115      Reg     SR     No     Fast     --    No       Open and Stay Open
    103      Reg     S1     No     Slow     --    No       Open and Stay Open
    112      Reg     S1     No     Fast     --    No       Open and Stay Open
    86       Reg     WR     No     Slow     --    No       Open and Stay Open
    106      Reg     WR     No     Fast     --    No       Open and Stay Open
    2        Reg     W1     No     Slow     --    No       Open and Stay Open
    109      Reg     W1     No     Fast     --    No       Open and Stay Open
    46       Reg     GR     No     Slow     --    No       Open and Stay Open

    42       Reg     SR     No     Slow     --    No       Close and Stay Closed
    116      Reg     SR     No     Fast     --    No       Close and Stay Closed
    50       Reg     S1     No     Slow     --    No       Close and Stay Closed
    113      Reg     S1     No     Fast     --    No       Close and Stay Closed
    75       Reg     WR     No     Slow     --    No       Close and Stay Closed
    107      Reg     WR     No     Fast     --    No       Close and Stay Closed
    3        Reg     W1     No     Slow     --    No       Close and Stay Closed
    110      Reg     W1     No     Fast     --    No       Close and Stay Closed

    196      Ext     SR     No     Slow     30s   No       Close, Wait, Then Open
    175      Ext     S1     No     Slow     30s   No       Close, Wait, Then Open
    76       Reg     WR     No     Slow     30s   No       Close, Wait, Then Open
    16       Reg     W1     No     Slow     30s   No       Close, Wait, Then Open

    Regular and Extended Locked Door Types
    ---------------------------------------------------------------
    #        Class   Trig   Lock   Speed    Wait  Monst    Function

    26       Reg     PR     Blue   Slow     4s    No       Open, Wait, Then Close
    28       Reg     PR     Red    Slow     4s    No       Open, Wait, Then Close
    27       Reg     PR     Yell   Slow     4s    No       Open, Wait, Then Close

    32       Reg     P1     Blue   Slow     --    No       Open and Stay Open
    33       Reg     P1     Red    Slow     --    No       Open and Stay Open
    34       Reg     P1     Yell   Slow     --    No       Open and Stay Open
    99       Reg     SR     Blue   Fast     --    No       Open and Stay Open
    134      Reg     SR     Red    Fast     --    No       Open and Stay Open
    136      Reg     SR     Yell   Fast     --    No       Open and Stay Open
    133      Reg     S1     Blue   Fast     --    No       Open and Stay Open
    135      Reg     S1     Red    Fast     --    No       Open and Stay Open
    137      Reg     S1     Yell   Fast     --    No       Open and Stay Open

There are two generalized door linedef types, Generalized Door, and
Generalized Locked Door. The following tables show the possibilities for
each parameter, any combination of parameters is allowed:\
\
Slow and Fast represent the same speeds as slow and fast regular doors.
Normal is twice as fast as slow, Fast is twice normal, Turbo is twice
Fast.\
\


    Generalized Door Types
    ---------------------------------------------------------------
    #        Class   Trig   Lock   Speed    Wait  Monst    Function

    3C00H-   Gen     P1/PR  No     Slow     1s    Yes      Open, Wait, Then Close
    4000H            S1/SR         Normal   4s    No       Open and Stay Open
                     W1/WR         Fast     9s             Close and Stay Closed
                     G1/GR         Turbo    30s            Close, Wait, Then Open

\


    Generalized Locked Door Types
    ---------------------------------------------------------------
    #        Class   Trig   Lock   Speed    Wait  Monst    Function

    3800H-   Gen     P1/PR  Any    Slow     1s    No       Open, Wait, Then Close
    3C00H            S1/SR  Blue   Normal   4s             Open and Stay Open
                     W1/WR  Red    Fast     9s             Close and Stay Closed
                     G1/GR  Yell   Turbo    30s            Close, Wait, Then Open
                            BlueC
                            RedC
                            YellC
                            BlueS
                            RedS
                            YellS
                            All3
                            All6

There are currently six types of parameterized door specials, each
performing a different type of door action. See the [Parameterized Door
Types](#paramdoors) section for full information on how to use these
specials.


    Parameterized Door Types
    -----------------------------------------------------------------
    #        Class   Function                      ExtraData Name

    300      Param   Open, Wait, Then Close        Door_Raise
    301      Param   Open and Stay Open            Door_Open
    302      Param   Close and Stay Closed         Door_Close
    303      Param   Close, Wait, Then Open        Door_CloseWaitOpen
    304      Param   Wait, Open, Wait, Close       Door_WaitRaise
    305      Param   Wait, Close and Stay Closed   Door_WaitClose

[Return to Table of Contents](#contents)

[]{#floors}

------------------------------------------------------------------------

**Section 2. Floors**

------------------------------------------------------------------------

\
[]{#floortarg}

------------------------------------------------------------------------

**Section 2.1 Floor targets**

------------------------------------------------------------------------

\

- Lowest Neighbor Floor (LnF)\
  \
  This means that the floor moves to the height of the lowest
  neighboring floor including the floor itself. If the floor direction
  is up (only possible with generalized floors) motion is instant, else
  at the floor\'s speed.\
  \
- Next Neighbor Floor (NnF)\
  \
  This means that the floor moves up to the height of the lowest
  adjacent floor greater in height than the current, or down to the
  height of the highest adjacent floor less in height than the current,
  as determined by the floor\'s direction. Instant motion is not
  possible in this case. If no such floor exists, the floor does not
  move.\
  \
- Lowest Neighbor Ceiling (LnC)\
  \
  This means that the floor height changes to the height of the lowest
  ceiling possessed by any neighboring sector, including that floor\'s
  ceiling. If the target height is in the opposite direction to floor
  motion, motion is instant, otherwise at the floor action\'s speed.\
  \
- 8 Under Lowest Neighbor Ceiling (8uLnC)\
  \
  This means that the floor height changes to 8 less than the height of
  the lowest ceiling possessed by any neighboring sector, including that
  floor\'s ceiling. If the target height is in the opposite direction to
  floor motion, motion is instant, otherwise at the floor action\'s
  speed.\
  \
- Highest Neighbor Floor (HnF)\
  \
  This means that the floor height changes to the height of the highest
  neighboring floor, excluding the floor itself. If no neighbor floor
  exists, the floor moves down to -32000. If the target height is in the
  opposite direction to floor motion, the motion is instant, otherwise
  it occurs at the floor action\'s speed.\
  \
- 8 Above Highest Neighbor Floor (8aHnF)\
  \
  This means that the floor height changes to 8 above the height of the
  highest neighboring floor, excluding the floor itself. If no neighbor
  floor exists, the floor moves down to -31992. If the target height is
  in the opposite direction to floor motion, the motion is instant,
  otherwise it occurs at the floor action\'s speed.\
  \
- Ceiling\
  \
  The floor moves up until its at ceiling height, instantly if floor
  direction is down (only available with generalized types), at the
  floor speed if the direction is up.\
  \
- 24 Units (24)\
  \
  The floor moves 24 units in the floor action\'s direction. Instant
  motion is not possible with this linedef type.\
  \
- 32 Units (32)\
  \
  The floor moves 32 units in the floor action\'s direction. Instant
  motion is not possible with this linedef type.\
  \
- 512 Units (512)\
  \
  The floor moves 512 units in the floor action\'s direction. Instant
  motion is not possible with this linedef type.\
  \
- Shortest Lower Texture (SLT)\
  \
  The floor moves the height of the shortest lower texture on the
  boundary of the sector, in the floor direction. Instant motion is not
  possible with this type. In the case that there is no surrounding
  texture the motion is to -32000 or +32000 depending on direction.\
  \
- None\
  \
  Some pure texture type changes are provided for changing the floor
  texture and/or sector type without moving the floor.\
  \
- Up/Down Absolute Param\
  Only available to parameterized floor specials, this target moves the
  floor in its indicated direction by the number of units provided as a
  special argument.\
  \
- To Absolute Height\
  Only available to parameterized floor specials, this target moves the
  floor to an exact height which is provided as a parameter. This floor
  type always moves either up or down to the given height depending on
  whether it is higher or lower than the floor\'s current position. This
  means that instantaneous motion is not possible.

[]{#floorvar}

------------------------------------------------------------------------

**Section 2.2 Varieties of floors**

------------------------------------------------------------------------

\
A floor can be activated by pushing on a linedef bounding it
(generalized types only), or by pushing on a switch with the same tag as
the floor sector, or by walking over a linedef with the same tag as the
floor, or by shooting a linedef with the same tag as the floor with an
impact weapon.\
\
A floor can move either Up or Down.\
\
A floor can move with speeds of Slow, Normal, Fast, or Turbo. If the
target height specified by the floor function (see Floor Targets above)
is in the opposite direction to the floor\'s motion, then travel is
instantaneous, otherwise its at the speed specified. Parameterized floor
types can move at any speed.\
\
A floor action can be a texture change type, in which case after the
action the floor texture of the affected floor, and possibly the sector
type of the affected floor are changed to those of a model sector. The
sector type may be zeroed instead of copied from the model, or not
changed at all. These change types are referred to below as Tx (texture
only), Tx0 (type zeroed), and TxTy (texture and type changed). The model
sector for the change may be the sector on the first sidedef of the
trigger (trigger model) or the sector with floor at destination height
across the lowest numbered two-sided linedef surrounding the affected
sector (numeric model). If no model sector exists, no change occurs. If
a change occurs, floor texture is always affected, lighting is never
affected, even that corresponding to the sector\'s type, nor is any
other sector property other than the sector\'s type.\
\
Numeric model algorithm:\
\
1) Find all floors adjacent to the tagged floor at destination height\
2) Find the lowest numbered linedef separating those floors from that
tagged\
3) The sector on the other side of that linedef is the model\
\
A floor action can have the crush property, in which case players and
monsters are crushed when the floor tries to move above the point where
they fit exactly underneath the ceiling. This means they take damage
until they die, leave the sector, or the floor action is stopped. A
floor action never reverses on encountering an obstacle, even if the
crush property is not true, the floor merely remains in the same
position until the obstacle is removed or dies, then continues.

[]{#floorlines}

------------------------------------------------------------------------

**Section 2.3 Floor linedef types**

------------------------------------------------------------------------

\


    Regular and Extended Floor Types
    -------------------------------------------------------------------
    #     Class   Trig   Dir Spd   Chg  Mdl Mon Crsh  Target

    60    Reg     SR     Dn  Slow  None --  No  No    Lowest Neighbor Floor
    23    Reg     S1     Dn  Slow  None --  No  No    Lowest Neighbor Floor
    82    Reg     WR     Dn  Slow  None --  No  No    Lowest Neighbor Floor
    38    Reg     W1     Dn  Slow  None --  No  No    Lowest Neighbor Floor
          
    177   Ext     SR     Dn  Slow  TxTy Num No  No    Lowest Neighbor Floor
    159   Ext     S1     Dn  Slow  TxTy Num No  No    Lowest Neighbor Floor
    84    Reg     WR     Dn  Slow  TxTy Num No  No    Lowest Neighbor Floor
    37    Reg     W1     Dn  Slow  TxTy Num No  No    Lowest Neighbor Floor

    69    Reg     SR     Up  Slow  None --  No  No    Next Neighbor Floor
    18    Reg     S1     Up  Slow  None --  No  No    Next Neighbor Floor
    128   Reg     WR     Up  Slow  None --  No  No    Next Neighbor Floor
    119   Reg     W1     Up  Slow  None --  No  No    Next Neighbor Floor

    132   Reg     SR     Up  Fast  None --  No  No    Next Neighbor Floor
    131   Reg     S1     Up  Fast  None --  No  No    Next Neighbor Floor
    129   Reg     WR     Up  Fast  None --  No  No    Next Neighbor Floor
    130   Reg     W1     Up  Fast  None --  No  No    Next Neighbor Floor

    222   Ext     SR     Dn  Slow  None --  No  No    Next Neighbor Floor
    221   Ext     S1     Dn  Slow  None --  No  No    Next Neighbor Floor
    220   Ext     WR     Dn  Slow  None --  No  No    Next Neighbor Floor
    219   Ext     W1     Dn  Slow  None --  No  No    Next Neighbor Floor

    64    Reg     SR     Up  Slow  None --  No  No    Lowest Neighbor Ceiling
    101   Reg     S1     Up  Slow  None --  No  No    Lowest Neighbor Ceiling
    91    Reg     WR     Up  Slow  None --  No  No    Lowest Neighbor Ceiling
    5     Reg     W1     Up  Slow  None --  No  No    Lowest Neighbor Ceiling
    24    Reg     G1     Up  Slow  None --  No  No    Lowest Neighbor Ceiling

    65    Reg     SR     Up  Slow  None --  No  Yes   Lowest Neighbor Ceiling - 8
    55    Reg     S1     Up  Slow  None --  No  Yes   Lowest Neighbor Ceiling - 8
    94    Reg     WR     Up  Slow  None --  No  Yes   Lowest Neighbor Ceiling - 8
    56    Reg     W1     Up  Slow  None --  No  Yes   Lowest Neighbor Ceiling - 8

    45    Reg     SR     Dn  Slow  None --  No  No    Highest Neighbor Floor
    102   Reg     S1     Dn  Slow  None --  No  No    Highest Neighbor Floor
    83    Reg     WR     Dn  Slow  None --  No  No    Highest Neighbor Floor
    19    Reg     W1     Dn  Slow  None --  No  No    Highest Neighbor Floor

    70    Reg     SR     Dn  Fast  None --  No  No    Highest Neighbor Floor + 8     
    71    Reg     S1     Dn  Fast  None --  No  No    Highest Neighbor Floor + 8     
    98    Reg     WR     Dn  Fast  None --  No  No    Highest Neighbor Floor + 8     
    36    Reg     W1     Dn  Fast  None --  No  No    Highest Neighbor Floor + 8     

    180   Ext     SR     Up  Slow  None --  No  No    Absolute 24
    161   Ext     S1     Up  Slow  None --  No  No    Absolute 24
    92    Reg     WR     Up  Slow  None --  No  No    Absolute 24
    58    Reg     W1     Up  Slow  None --  No  No    Absolute 24

    179   Ext     SR     Up  Slow  TxTy Trg No  No    Absolute 24
    160   Ext     S1     Up  Slow  TxTy Trg No  No    Absolute 24
    93    Reg     WR     Up  Slow  TxTy Trg No  No    Absolute 24
    59    Reg     W1     Up  Slow  TxTy Trg No  No    Absolute 24

    176   Ext     SR     Up  Slow  None --  No  No    Abs Shortest Lower Texture
    158   Ext     S1     Up  Slow  None --  No  No    Abs Shortest Lower Texture
    96    Reg     WR     Up  Slow  None --  No  No    Abs Shortest Lower Texture
    30    Reg     W1     Up  Slow  None --  No  No    Abs Shortest Lower Texture

    178   Ext     SR     Up  Slow  None --  No  No    Absolute 512
    140   Reg     S1     Up  Slow  None --  No  No    Absolute 512
    147   Ext     WR     Up  Slow  None --  No  No    Absolute 512
    142   Ext     W1     Up  Slow  None --  No  No    Absolute 512

    190   Ext     SR     --  ----  TxTy Trg No  No    None
    189   Ext     S1     --  ----  TxTy Trg No  No    None
    154   Ext     WR     --  ----  TxTy Trg No  No    None
    153   Ext     W1     --  ----  TxTy Trg No  No    None

    78    Ext     SR     --  ----  TxTy Num No  No    None
    241   Ext     S1     --  ----  TxTy Num No  No    None
    240   Ext     WR     --  ----  TxTy Num No  No    None
    239   Ext     W1     --  ----  TxTy Num No  No    None

The following tables show the possibilities for generalized floor
linedef type parameters. Any combination of parameters is allowed:\
\
Slow and Fast represent the same speeds as slow and fast regular floors.
Normal is twice as fast as slow, Fast is twice normal, Turbo is twice
Fast.\
\


    Generalized Floor Types
    ---------------------------------------------------------------------------
    #      Class   Trig   Dir Spd   *Chg *Mdl Mon Crsh  Target

    6000H- Gen     P1/PR  Up  Slow   None Trg Yes Yes   Lowest Neighbor Floor
    7FFFH          S1/SR  Dn  Normal Tx   Num No  No    Next Neighbor Floor
                   W1/WR      Fast   Tx0                Lowest Neighbor Ceiling
                   G1/GR      Turbo  TxTy               Highest Neighbor Floor
                                                        Ceiling
                                                        24
                                                        32
    *Mon(ster) enabled must be No if                    Shortest Lower Texture
    Chg field is not None

    Tx = Texture copied only           Trg = Trigger Model
    Tx0 = Texture copied and Type->0   Num = Numeric Model
    TxTy = Texture and Type copied

There are currently seventeen types of parameterized floor specials,
each performing a different type of floor action. See the [Parameterized
Floor Types](#paramfloors) section for full information on how to use
these specials.


    Parameterized Floor Types
    ------------------------------------------------------------------------------
    #        Class   Function                           ExtraData Name

    306      Param   Up to Highest Neighbor Floor       Floor_RaiseToHighest
    307      Param   Down to Highest Neighbor Floor     Floor_LowerToHighest
    308      Param   Up to Lowest Neighbor Floor        Floor_RaiseToLowest
    309      Param   Down to Lowest Neighbor Floor      Floor_LowerToLowest
    310      Param   Up to Next Neighbor Floor          Floor_RaiseToNearest
    311      Param   Down to Next Neighbor Floor        Floor_LowerToNearest
    312      Param   Up to Lowest Neighbor Ceiling      Floor_RaiseToLowestCeiling
    313      Param   Down to Lowest Neighbor Ceiling    Floor_LowerToLowestCeiling
    314      Param   Up to Ceiling                      Floor_RaiseToCeiling
    315      Param   Up Abs Shortest Lower Texture      Floor_RaiseByTexture
    316      Param   Down Abs Shortest Lower Texture    Floor_LowerByTexture
    317      Param   Up Absolute Param                  Floor_RaiseByValue
    318      Param   Down Absolute Param                Floor_LowerByValue
    319      Param   To Absolute Height                 Floor_MoveToValue
    320      Param   Up Absolute Param, Instant         Floor_RaiseInstant
    321      Param   Down Absolute Param, Instant       Floor_LowerInstant
    322      Param   To Ceiling Instant                 Floor_ToCeilingInstant

[Return to Table of Contents](#contents)

[]{#ceils}

------------------------------------------------------------------------

**Section 3. Ceilings**

------------------------------------------------------------------------

\
[]{#ceiltarg}

------------------------------------------------------------------------

**Section 3.1 Ceiling Targets**

------------------------------------------------------------------------

\

- Highest Neighbor Ceiling (HnC)\
  \
  This means that the ceiling moves to the height of the highest
  neighboring ceiling NOT including the ceiling itself. If the ceiling
  direction is down (only possible with generalized ceilings) motion is
  instant, else at the ceiling\'s speed. If no adjacent ceiling exists
  the ceiling moves to -32000 units.\
  \
- Next Neighbor Ceiling (NnC)\
  \
  This means that the ceiling moves up to the height of the lowest
  adjacent ceiling greater in height than the current, or to the height
  of the highest adjacent ceiling less in height than the current, as
  determined by the ceiling\'s direction. Instant motion is not possible
  in this case. If no such ceiling exists, the ceiling does not move.\
  \
- Lowest Neighbor Ceiling (LnC)\
  \
  This means that the ceiling height changes to the height of the lowest
  ceiling possessed by any neighboring sector, NOT including itself. If
  the target height is in the opposite direction to ceiling motion,
  motion is instant, otherwise at the ceiling action\'s speed. If no
  adjacent ceiling exists the ceiling moves to 32000 units.\
  \
- Highest Neighbor Floor (HnF)\
  \
  This means that the ceiling height changes to the height of the
  highest neighboring floor, excluding the ceiling\'s floor itself. If
  no neighbor floor exists, the ceiling moves down to -32000 or the
  ceiling\'s floor, whichever is higher. If the target height is in the
  opposite direction to ceiling motion, the motion is instant, otherwise
  it occurs at the ceiling action\'s speed.\
  \
- Floor\
  \
  The ceiling moves down until its at floor height, instantly if ceiling
  direction is up (only available with generalized types), at the
  ceiling speed if the direction is down.\
  \
- 8 Above Floor (8aF)\
  \
  This means that the ceiling height changes to 8 above the height of
  the ceiling\'s floor. If the target height is in the opposite
  direction to ceiling motion, the motion is instant, otherwise it
  occurs at the ceiling action\'s speed.\
  \
- 24 Units (24)\
  \
  The ceiling moves 24 units in the ceiling action\'s direction. Instant
  motion is not possible with this linedef type.\
  \
- 32 Units (32)\
  \
  The ceiling moves 32 units in the ceiling action\'s direction. Instant
  motion is not possible with this linedef type.\
  \
- Shortest Upper Texture (SUT)\
  \
  The ceiling moves the height of the shortest upper texture on the
  boundary of the sector, in the ceiling direction. Instant motion is
  not possible with this type. In the case that there is no surrounding
  texture the motion is to -32000 or +32000 depending on direction.\
- Up/Down Absolute Param\
  Only available to parameterized ceiling specials, this target moves
  the ceiling in its indicated direction by the number of units provided
  as a special argument.\
  \
- To Absolute Height\
  Only available to parameterized ceiling specials, this target moves
  the ceiling to an exact height which is provided as a parameter. This
  ceiling type always moves either up or down to the given height
  depending on whether it is higher or lower than the ceiling\'s current
  position. This means that instantaneous motion is not possible.

[]{#ceilvar}

------------------------------------------------------------------------

**Section 3.2 Varieties of ceilings**

------------------------------------------------------------------------

\
A ceiling can be activated by pushing on a linedef bounding it
(generalized types only), or by pushing on a switch with the same tag as
the ceiling sector, or by walking over a linedef with the same tag as
the ceiling, or by shooting a linedef with the same tag as the ceiling
with an impact weapon (generalized types only).\
\
A ceiling can move either Up or Down.\
\
A ceiling can move with speeds of Slow, Normal, Fast, or Turbo. If the
target height specified by the ceiling function (see Ceiling Targets
above) is in the opposite direction to the ceiling\'s motion, then
travel is instantaneous, otherwise its at the speed specified.\
\
A ceiling action can be a texture change type, in which case after the
action the ceiling texture of the affected ceiling, and possibly the
sector type of the affected ceiling are changed to those of a model
sector. The sector type may be zeroed instead of copied from the model,
or not changed at all. These change types are referred to below as Tx
(texture only), Tx0 (type set to 0), and TxTy (texture and type copied
from model). The model sector for the change may be the sector on the
first sidedef of the trigger (trigger model) or the sector with ceiling
at destination height across the lowest numbered two-sided linedef
surrounding the affected sector (numeric model). If no model sector
exists, no change occurs. If a change occurs, ceiling texture is always
affected, lighting is never affected, even that corresponding to the
sector\'s type, nor is any other sector property other than the
sector\'s type.\
\
Numeric model algorithm:\
\
1) Find all ceilings adjacent to the tagged ceiling at destination
height\
2) Find the lowest numbered linedef separating those ceilings from that
tagged\
3) The sector on the other side of that linedef is the model\
\
A ceiling action can have the crush property (generalized types only),
in which case players and monsters are crushed when the ceiling tries to
move below the point where they fit exactly underneath the ceiling. This
means they take damage until they die, leave the sector, or the ceiling
action is stopped. A ceiling action never reverses on encountering an
obstacle, even if the crush property is not true, the ceiling merely
remains in the same position until the obstacle is removed or dies, then
continues.

[]{#ceillines}

------------------------------------------------------------------------

**Section 3.3 Ceiling linedef types**

------------------------------------------------------------------------

\


    Regular and Extended Ceiling Types
    -------------------------------------------------------------------
    #     Class   Trig   Dir Spd   *Chg *Mdl Mon Crsh Target

    43    Reg     SR     Dn  Fast  None  --  No  No   Floor 
    41    Reg     S1     Dn  Fast  None  --  No  No   Floor 
    152   Ext     WR     Dn  Fast  None  --  No  No   Floor 
    145   Ext     W1     Dn  Fast  None  --  No  No   Floor 

    186   Ext     SR     Up  Slow  None  --  No  No   Highest Neighbor Ceiling
    166   Ext     S1     Up  Slow  None  --  No  No   Highest Neighbor Ceiling
    151   Ext     WR     Up  Slow  None  --  No  No   Highest Neighbor Ceiling
    40    Reg     W1     Up  Slow  None  --  No  No   Highest Neighbor Ceiling

    187   Ext     SR     Dn  Slow  None  --  No  No   8 Above Floor
    167   Ext     S1     Dn  Slow  None  --  No  No   8 Above Floor
    72    Reg     WR     Dn  Slow  None  --  No  No   8 Above Floor
    44    Reg     W1     Dn  Slow  None  --  No  No   8 Above Floor

    205   Ext     SR     Dn  Slow  None  --  No  No   Lowest Neighbor Ceiling
    203   Ext     S1     Dn  Slow  None  --  No  No   Lowest Neighbor Ceiling
    201   Ext     WR     Dn  Slow  None  --  No  No   Lowest Neighbor Ceiling
    199   Ext     W1     Dn  Slow  None  --  No  No   Lowest Neighbor Ceiling

    206   Ext     SR     Dn  Slow  None  --  No  No   Highest Neighbor Floor
    204   Ext     S1     Dn  Slow  None  --  No  No   Highest Neighbor Floor
    202   Ext     WR     Dn  Slow  None  --  No  No   Highest Neighbor Floor
    200   Ext     W1     Dn  Slow  None  --  No  No   Highest Neighbor Floor

    Generalized Ceiling Types
    ---------------------------------------------------------------------------
    #      Class   Trig   Dir Spd   *Chg *Mdl Mon Crsh  Target

    4000H- Gen     P1/PR  Up  Slow   None Trg Yes Yes   Highest Neighbor Ceiling
    5FFFH          S1/SR  Dn  Normal Tx   Num No  No    Next Neighbor Ceiling
                   W1/WR      Fast   Tx0                Lowest Neighbor Ceiling
                   G1/GR      Turbo  TxTy               Highest Neighbor Floor
                                                        Floor
                                                        24
                                                        32
    *Mon(ster) enabled must be No if                    Shortest Upper Texture
    Chg field is not None

    Tx = Texture copied only           Trg = Trigger Model
    Tx0 = Texture copied and Type->0   Num = Numeric Model
    TxTy = Texture and Type copied

There are currently seventeen types of parameterized ceiling specials,
each performing a different type of ceiling action. See the
[Parameterized Ceiling Types](#paramceils) section for full information
on how to use these specials.


    Parameterized Ceiling Types
    -------------------------------------------------------------------------------
    #        Class   Function                           ExtraData Name

    323      Param   Up to Highest Neighbor Ceiling     Ceiling_RaiseToHighest
    324      Param   Up to HnC Instant                  Ceiling_ToHighestInstant
    325      Param   Up to Nearest Neighbor Ceiling     Ceiling_RaiseToNearest
    326      Param   Down to Nearest Neighbor Ceiling   Ceiling_LowerToNearest
    327      Param   Up to Lowest Neighbor Ceiling      Ceiling_RaiseToLowest
    328      Param   Down to Lowest Neighbor Ceiling    Ceiling_LowerToLowest
    329      Param   Up to Highest Neighbor Floor       Ceiling_RaiseToHighestFloor
    330      Param   Down to Highest Neighbor Floor     Ceiling_LowerToHighestFloor
    331      Param   Down to Floor Instant              Ceiling_ToFloorInstant
    332      Param   Down to Floor                      Ceiling_LowerToFloor
    333      Param   Up Abs Shortest Upper Texture      Ceiling_RaiseByTexture
    334      Param   Down Abs Shortest Upper Texture    Ceiling_LowerByTexture
    335      Param   Up Absolute Param                  Ceiling_RaiseByValue
    336      Param   Down Absolute Param                Ceiling_LowerByValue
    337      Param   To Absolute Height                 Ceiling_MoveToValue
    338      Param   Up Absolute Param, Instant         Ceiling_RaiseInstant
    339      Param   Down Absolute Param, Instant       Ceiling_LowerInstant

[Return to Table of Contents](#contents)

[]{#plats}

------------------------------------------------------------------------

**Section 4. Platforms (Lifts)**

------------------------------------------------------------------------

\
A platform is basically a floor action involving two heights. The simple
raise platform actions, for example, differ from the corresponding floor
actions in that if they encounter an obstacle, they reverse direction
and return to their former height.\
\
The most often used kind of platform is a lift which travels from its
current height to the target height, then waits a specified time and
returns to it former height.

[]{#plattarg}

------------------------------------------------------------------------

**Section 4.1 Platform Targets**

------------------------------------------------------------------------

\

- Lowest Neighbor Floor\
  \
  This means that the platforms \"low\" height is the height of the
  lowest surrounding floor, including the platform itself. The \"high\"
  height is the original height of the floor.\
  \
- Next Lowest Neighbor Floor\
  \
  This means that the platforms \"low\" height is the height of the
  highest surrounding floor lower than the floor itself. If no lower
  floor exists, no motion occurs as the \"low\" and \"high\" heights are
  then both equal to the floors current height.\
  \
- Lowest Neighbor Ceiling\
  \
  This means that the platforms \"low\" height is the height of the
  lowest surrounding ceiling unless this is higher than the floor
  itself. If no adjacent ceiling exists, or is higher than the floor no
  motion occurs as the \"low\" and \"high\" heights are then both equal
  to the floors current height.\
  \
- Lowest and Highest Floor\
  \
  This target sets the \"low\" height to the lowest neighboring floor,
  including the floor itself, and the \"high\" height to the highest
  neighboring floor, including the floor itself. When this target is
  used the floor moves perpetually between the two heights. Once
  triggered this type of linedef runs permanently, even if the motion is
  temporarily suspended with a Stop type. No other floor action can be
  commanded on the sector after this type is begun.\
  \
- Ceiling\
  \
  This target sets the \"high\" height to the ceiling of the sector and
  the \"low\" height to the floor height of the sector and is only used
  in the instant toggle type that switches the floor between the ceiling
  and its original height on each activation. This is also the ONLY
  instant platform type.\
  \
- Raise Next Floor\
  \
  This means that the \"high\" height is the lowest surrounding floor
  higher than the platform. If no higher adjacent floor exists no motion
  will occur.\
  \
- Raise 24 Units\
  \
  The \"low\" height is the original floor height, the \"high\" height
  is 24 more.\
  \
- Raise 32 Units\
  \
  The \"low\" height is the original floor height, the \"high\" height
  is 32 more.

[]{#platvar}

------------------------------------------------------------------------

**Section 4.2 Varieties of Platforms**

------------------------------------------------------------------------

\
A platform can be activated by pushing on a linedef bounding it
(generalized types only), or by pushing on a switch with the same tag as
the platform sector, or by walking over a linedef with the same tag as
the platform, or by shooting a linedef with the same tag as the platform
with an impact weapon.\
\
A platform can move with speeds of Slow, Normal, Fast, or Turbo. Only
the instant toggle platform moves instantly, all others move at the
platform\'s speed.\
\
A platform can have a delay, in between when it reaches \"high\" height
and returns to \"low\" height, or at both ends of the motion in the case
of perpetual lifts.\
\
A platform action can be a texture change type, in which case after the
action the floor texture of the affected floor, and possibly the sector
type of the affected floor are changed to those of a model sector. The
sector type may be zeroed instead of copied from the model, or not
changed at all. These change types are referred to below as Tx (texture
only), Tx0 (type zeroed), and TxTy (texture and type changed). The model
sector for the change is always the sector on the first sidedef of the
trigger (trigger model). If a change occurs, floor texture is always
affected, lighting is never affected, even that corresponding to the
sector\'s type, nor is any other sector property other than the
sector\'s type.

[]{#platlines}

------------------------------------------------------------------------

**Section 4.3 Platform Linedef types**

------------------------------------------------------------------------

\


    Regular and Extended Platform Types
    -------------------------------------------------------------------
    #     Class  Trig   Dly Spd  Chg  Mdl Mon Target

    66    Reg    SR     --  Slow Tx   Trg No  Raise 24 Units
    15    Reg    S1     --  Slow Tx   Trg No  Raise 24 Units
    148   Ext    WR     --  Slow Tx   Trg No  Raise 24 Units
    143   Ext    W1     --  Slow Tx   Trg No  Raise 24 Units

    67    Reg    SR     --  Slow Tx0  Trg No  Raise 32 Units
    14    Reg    S1     --  Slow Tx0  Trg No  Raise 32 Units
    149   Ext    WR     --  Slow Tx0  Trg No  Raise 32 Units
    144   Ext    W1     --  Slow Tx0  Trg No  Raise 32 Units

    68    Reg    SR     --  Slow Tx0  Trg No  Raise Next Floor
    20    Reg    S1     --  Slow Tx0  Trg No  Raise Next Floor
    95    Reg    WR     --  Slow Tx0  Trg No  Raise Next Floor
    22    Reg    W1     --  Slow Tx0  Trg No  Raise Next Floor
    47    Reg    G1     --  Slow Tx0  Trg No  Raise Next Floor

    181   Ext    SR     3s  Slow None --  No  Lowest and Highest Floor (perpetual)
    162   Ext    S1     3s  Slow None --  No  Lowest and Highest Floor (perpetual)
    87    Reg    WR     3s  Slow None --  No  Lowest and Highest Floor (perpetual)
    53    Reg    W1     3s  Slow None --  No  Lowest and Highest Floor (perpetual)

    182   Ext    SR     --  ---- ---- --  --  Stop
    163   Ext    S1     --  ---- ---- --  --  Stop
    89    Reg    WR     --  ---- ---- --  --  Stop
    54    Reg    W1     --  ---- ---- --  --  Stop

    62    Reg    SR     3s  Slow None --  No  Lowest Neighbor Floor (lift)
    21    Reg    S1     3s  Slow None --  No  Lowest Neighbor Floor (lift)
    88    Reg    WR     3s  Slow None --  No  Lowest Neighbor Floor (lift)
    10    Reg    W1     3s  Slow None --  No  Lowest Neighbor Floor (lift)

    123   Reg    SR     3s  Fast None --  No  Lowest Neighbor Floor (lift)
    122   Reg    S1     3s  Fast None --  No  Lowest Neighbor Floor (lift)
    120   Reg    WR     3s  Fast None --  No  Lowest Neighbor Floor (lift)
    121   Reg    W1     3s  Fast None --  No  Lowest Neighbor Floor (lift)

    211   Ext    SR     --  Inst None --  No  Ceiling (toggle)
    212   Ext    WR     --  Inst None --  No  Ceiling (toggle)

    Generalized Lift Types
    ---------------------------------------------------------------------------
    #      Class   Trig   Dly Spd    Mon  Target

    3400H- Gen     P1/PR  1s  Slow   Yes  Lowest Neighbor Floor
    37FFH          S1/SR  3s  Normal No   Next Lowest Neighbor Floor
                   W1/WR  5s  Fast        Lowest Neighbor Ceiling
                   G1/GR  10s Turbo       Lowest and Highest Floor (perpetual)

[Return to Table of Contents](#contents)

[]{#crush}

------------------------------------------------------------------------

**Section 5. Crusher Ceilings**

------------------------------------------------------------------------

\
A crusher ceiling is a linedef type that causes the ceiling to cycle
between its starting height and 8 above the floor, damaging monsters and
players that happen to be in between. Barrels explode when crushed.\
\
Once a crusher ceiling is started it remains running for the remainder
of the level even if temporarily suspended with a stop type. No other
ceiling action can be used in that sector thereafter.\
\
[]{#crushvar}

------------------------------------------------------------------------

**Section 5.1 Varieties of Crushers**

------------------------------------------------------------------------

\
A crusher can be activated by pushing on a linedef bounding it
(generalized types only), or by pushing on a switch with the same tag as
the crusher sector, or by walking over a linedef with the same tag as
the crusher, or by shooting a linedef with the same tag as the crusher
with an impact weapon (generalized types only).\
\
A crusher has a speed: slow, normal, fast, or turbo. The slower the
speed, the more damage the crusher does when crushing, simply thru being
applied longer. When a slow or normal crusher is moving down and
encounters something to crush, it slows down even more, by a factor of
8. This persists until it reaches bottom of stroke and starts up again.
Fast and turbo crushers do not slow down.\
\
A crusher can be silent. The regular silent crusher makes platform stop
and start noises at top and bottom of stroke. The generalized silent
crusher is completely quiet.\
\
A crusher linedef is provided to stop a crusher in its current position.
Care should be used that this doesn\'t lock the player out of an area of
the wad if the crusher is too low to pass. A crusher can be restarted,
but not changed, with any crusher linedef.

[]{#crushlines}

------------------------------------------------------------------------

**Section 5.2 Crusher Linedef Types**

------------------------------------------------------------------------

\


    Regular and Extended Crusher Types
    -------------------------------------------------------------------
    #     Class  Trig   Spd  Mon Silent Action

    184   Ext    SR     Slow No  No     Start
    49    Reg    S1     Slow No  No     Start
    73    Reg    WR     Slow No  No     Start
    25    Reg    W1     Slow No  No     Start

    183   Ext    SR     Fast No  No     Start
    164   Ext    S1     Fast No  No     Start
    77    Reg    WR     Fast No  No     Start
    6     Reg    W1     Fast No  No     Start

    185   Ext    SR     Slow No  Yes    Start
    165   Ext    S1     Slow No  Yes    Start
    150   Ext    WR     Slow No  Yes    Start
    141   Reg    W1     Slow No  Yes    Start

    188   Ext    SR     ---- --  --     Stop
    168   Ext    S1     ---- --  --     Stop
    74,   Reg    WR     ---- --  --     Stop
    57,   Reg    W1     ---- --  --     Stop

    Generalized Crusher Types
    ---------------------------------------------------------------------------
    #      Class   Trig   Spd    Mon  Silent

    2F80H- Gen     P1/PR  Slow   Yes  Yes
    2FFFH          S1/SR  Normal No   No
                   W1/WR  Fast        
                   G1/GR  Turbo       

[Return to Table of Contents](#contents)

[]{#stairs}

------------------------------------------------------------------------

**Section 6. Stair Builders**

------------------------------------------------------------------------

\
A stair builder is a linedef type that sets a sequence of sectors
defined by a complex rule to an ascending or descending sequence of
heights.\
\
The rule for stair building is as follows:\
\
1) The first step to rise or fall is the tagged sector. It rises or
falls by the stair stepsize, at the stair speed.\
\
2) To find the next step (sector) affected examine all two-sided
linedefs with first sidedef facing into the previous step for the one
with lowest linedef number. The sector on the other side of this linedef
is the next step. If no such linedef exists, stair building ceases.\
\
3) If the next step is already moving, or has already risen as part of
the stair, stair building ceases. Optionally if the floor texture of the
next step is different from the previous step\'s the stair building
stops. Otherwise the next step moves to the height of the previous step
plus or minus the stepsize depending on the direction the stairs build,
up or down. If the motion is in the same direction as the stairs build,
it occurs at stair build speed, otherwise it is instant.\
\
4) Repeat from step 2 until stair building ceases.\
\
With parameterized stairs, it is possible to have synchronized movement.
This means that each step will move at a different speed such that all
the steps reach their destination height at the same time. It is also
possible to have parameterized stairs reset to their original positions
after a given amount of time.

[]{#stairvar}

------------------------------------------------------------------------

**Section 6.1 Varieties of Stair Builders**

------------------------------------------------------------------------

\
A stair can be activated by pushing on a linedef bounding it
(generalized types only), or by pushing on a switch with the same tag as
the stair sector, or by walking over a linedef with the same tag as the
stair, or by shooting a linedef with the same tag as the stair with an
impact weapon (generalized types only).\
\
Only extended and generalized stair types are retriggerable. The
extended retriggerable stairs are mostly useless, though triggering a
stair with stepsize 8 twice might be used. The generalized retriggerable
stairs alternate building up and down on each activation which is much
more useful. For parameterized stair types, retriggering is only useful
if the staircase resets itself after building.\
\
A stair can build up or down (generalized types only).\
\
A stair can have a step size of 4, 8, 16, or 24. Only generalized types
support stepsize of 4 or 24. Parameterized stairs can build steps of any
size greater than zero.\
\
A stair can have build speeds of slow, normal, fast or turbo. Only
generalized types support speeds of normal or turbo. Parameterized
stairs can build at any speed.\
\
A stair can stop on encountering a different texture or ignore
(generalized types only) different textures and continue.\
\
Only the regular build fast, stepsize 16 stair has the property that
monsters and players can be crushed by the motion, all others do not
crush.

[]{#stairlines}

------------------------------------------------------------------------

**Section 6.2 Regular and Extended Stair Builder Types**

------------------------------------------------------------------------

\


    Regular and Extended Stairs Types
    -------------------------------------------------------------------
    #      Class   Trig   Dir Spd     Step  Ignore Mon

    258    Ext     SR     Up  Slow    8     No     No
    7      Reg     S1     Up  Slow    8     No     No
    256    Ext     WR     Up  Slow    8     No     No
    8      Reg     W1     Up  Slow    8     No     No
     
    259    Ext     SR     Up  Fast    16    No     No
    127    Reg     S1     Up  Fast    16    No     No
    257    Ext     WR     Up  Fast    16    No     No
    100    Reg     W1     Up  Fast    16    No     No

    Generalized Stairs Types
    ---------------------------------------------------------------------------
    #      Class   Trig   Dir Spd     Step  Ignore Mon

    3000H- Gen     P1/PR  Up  Slow    4     Yes    Yes
    33FFH          S1/SR  Dn  Normal  8     No     No 
                   W1/WR      Fast    16 
                   G1/GR      Turbo   24 

There are currently four types of parameterized stair specials, each
performing a different type of stair action. See the [Parameterized
Stair Types](#paramstairs) section for full information on how to use
these specials.


    Parameterized Stair Types
    -------------------------------------------------------------------------------
    #      Class   Function                           ExtraData Name

    340    Param   Build stairs up, DOOM method       Stairs_BuildUpDoom
    341    Param   Build stairs down, DOOM method     Stairs_BuildDownDoom
    342    Param   Build stairs up sync, DOOM method  Stairs_BuildUpDoomSync
    343    Param   Build stairs dn sync, DOOM method  Stairs_BuildDownDoomSync

[Return to Table of Contents](#contents)

[]{#elevs}

------------------------------------------------------------------------

**Section 7. Elevators**

------------------------------------------------------------------------

\
An elevator is a linedef type that moves both floor and ceiling
together. All elevator linedefs are extended, there are no regular or
generalized elevator types. Instant elevator motion is not possible, and
monsters cannot activate elevators. All elevator triggers are either
switched or walkover.

[]{#elevtarg}

------------------------------------------------------------------------

**Section 7.1 Elevator Targets**

------------------------------------------------------------------------

\

- Next Highest Floor\
  \
  The elevator floor moves to the lowest adjacent floor higher than the
  elevator\'s floor, the ceiling staying the same height above the
  floor. If there is no higher floor the elevator doesn\'t move.\
  \
- Next Lowest Floor\
  \
  The elevator floor moves to the highest adjacent floor lower than the
  current floor, the ceiling staying the same height above the floor. If
  there is no lower floor the elevator doesn\'t move.\
  \
- Current Floor\
  \
  The elevator floor moves to the height of the floor on the first
  sidedef of the triggering line, the ceiling remaining the same height
  above the elevator floor.

[]{#elevlines}

------------------------------------------------------------------------

**Section 7.2 Elevator Linedef Types**

------------------------------------------------------------------------

\


    Extended Elevator types
    -------------------------------------------------------------------
    #     Class  Trig  Spd    Target

    230   Ext    SR    Fast   Next Highest Floor
    229   Ext    S1    Fast   Next Highest Floor
    228   Ext    WR    Fast   Next Highest Floor
    227   Ext    W1    Fast   Next Highest Floor

    234   Ext    SR    Fast   Next Lowest Floor
    233   Ext    S1    Fast   Next Lowest Floor
    232   Ext    WR    Fast   Next Lowest Floor
    231   Ext    W1    Fast   Next Lowest Floor

    238   Ext    SR    Fast   Current Floor
    237   Ext    S1    Fast   Current Floor
    236   Ext    WR    Fast   Current Floor
    235   Ext    W1    Fast   Current Floor

[Return to Table of Contents](#contents)

[]{#lights}

------------------------------------------------------------------------

**Section 7a. Lighting**

------------------------------------------------------------------------

\
The lighting linedef types change the lighting in the tagged sector. All
are regular, extended, or parameterized types, there are no generalized
lighting types.

[]{#lighttarg}

------------------------------------------------------------------------

**Section 7a.1 Lighting Targets**

------------------------------------------------------------------------

\

- Lights to Minimum Neighbor\
  \
  Each tagged sector is set to the minimum light level found in any
  adjacent sector. The tagged sectors are changed in numerical order,
  and this may influence the result.\
  \
- Lights to Maximum Neighbor\
  \
  Each tagged sector is set to the maximum light level found in any
  adjacent sector. The tagged sectors are changed in numerical order,
  and this may influence the result.\
  \
- Blinking\
  \
  Each tagged sector begins blinking between two light levels. The
  brighter level is the light level in the tagged sector. The darker
  level is the minimum neighbor light level, or 0 if all neighbor
  sectors have lighting greater than or equal to the sector\'s at the
  time of activation. The blinking is non-synchronous, beginning 1-9
  gametics after activation, with a dark period of 1 sec (35 gametics)
  and a bright period of 1/7 sec (5 gametics).\
  \
- 35 Units\
  \
  Each tagged sector is set to a light level of 35 units.\
  \
- 255 Units\
  \
  Each tagged sector is set to a light level of 255 units.\
- Raise by Param\
  \
  Available with parameterized lines only. Raises tagged sectors\' light
  levels by a given amount.\
- Lower by Param\
  \
  Available with parameterized lines only. Lowers tagged sectors\' light
  levels by a given amount.\
- Change to Param\
  \
  Available with parameterized lines only. Changes all tagged sectors\'
  light levels to the given level.\
- Fade\
  \
  Available with parameterized lines only. Fades all tagged sectors\'
  light levels up or down to the given level.\
- Glow\
  \
  Available with parameterized lines only. Causes all tagged sectors to
  fade continuously between two given light levels at a given speed.\
- Flicker\
  \
  Available with parameterized lines only. Causes all tagged sectors to
  blink between two given light levels at random. Sector flickering is
  not synchronized.\
- Strobe\
  \
  Available with parameterized lines only. Causes all tagged sectors to
  blink between two given light levels on a period between given minimum
  and maximum amounts of time. Sector strobing is not synchronized.

[]{#lightlines}

------------------------------------------------------------------------

**Section 7a.2 Lighting Linedef Types**

------------------------------------------------------------------------

\


    Regular and Extended Lighting types
    -------------------------------------------------------------------
    #     Class  Trig  Target

    139   Reg    SR    35 Units
    170   Ext    S1    35 Units
    79    Reg    WR    35 Units
    35    Reg    W1    35 Units

    138   Reg    SR    255 Units
    171   Ext    S1    255 Units
    81    Reg    WR    255 Units
    13    Reg    W1    255 Units

    192   Ext    SR    Maximum Neighbor
    169   Ext    S1    Maximum Neighbor
    80    Reg    WR    Maximum Neighbor
    12    Reg    W1    Maximum Neighbor

    194   Ext    SR    Minimum Neighbor
    173   Ext    S1    Minimum Neighbor
    157   Ext    WR    Minimum Neighbor
    104   Reg    W1    Minimum Neighbor

    193   Ext    SR    Blinking
    172   Ext    S1    Blinking
    156   Ext    WR    Blinking
    17    Reg    W1    Blinking

There are currently seven types of parameterized lighting specials, each
performing a different type of lighting action. See the [Parameterized
Lighting](#paramlights) section for full information on how to use these
specials.


    Parameterized Stair Types
    -------------------------------------------------------------------------------
    #      Class   Function                           ExtraData Name

    368    Param   Raise light by given amount        Light_RaiseByValue
    369    Param   Lower light by given amount        Light_LowerByValue
    370    Param   Change light to given level        Light_ChangeToValue
    371    Param   Fade light to given level          Light_Fade
    372    Param   Fade light between two levels      Light_Glow
    373    Param   Flicker light between two levels   Light_Flicker
    374    Param   As above, but with given period    Light_Strobe

[Return to Table of Contents](#contents)

[]{#exits}

------------------------------------------------------------------------

**Section 8. Exits**

------------------------------------------------------------------------

\
An exit linedef type ends the current level bringing up the intermission
screen with its report of kills/items/secrets or frags in the case of
deathmatch. Obviously, none are retriggerable, and none require tags,
since no sector is affected.\
\
[]{#exitvar}

------------------------------------------------------------------------

**Section 8.1 Exit Varieties**

------------------------------------------------------------------------

\
An exit can be activated by pushing on a switch with the exit type or by
walking over a linedef with the exit type, or by shooting a linedef with
the exit type with an impact weapon.\
\
An exit can be normal or secret. The normal exit simply ends the level
and takes you to the next level via the intermission screen. A secret
exit only works in a special level depending on the IWAD being played.
In DOOM the secret exits can be on E1M3, E2M5, or E3M6. In DOOM II they
can be in levels 15 and 31. If a secret exit is used in any other level,
it brings you back to the start of the current level. In DOOM the secret
exits go from E1M3 to E1M9, E2M5 to E2M9, and E3M6 to E3M9. In DOOM II
they go from 15 to 31 and from 31 to 32. In DOOM II a normal exit from
31 or 32 goes to level 16.

[]{#exitlines}

------------------------------------------------------------------------

**Section 8.2 Exit Linedef Types**

------------------------------------------------------------------------

\


    Regular and Extended Exit types
    -------------------------------------------------------------------
    #     Class  Trig  Type

    11    Reg    S1    Normal
    52    Reg    W1    Normal
    197   Ext    G1    Normal

    51    Reg    S1    Secret
    124   Reg    W1    Secret
    198   Ext    G1    Secret

[Return to Table of Contents](#contents)

[]{#teles}

------------------------------------------------------------------------

**Section 9. Teleporters**

------------------------------------------------------------------------

\
A teleporter is a linedef that when crossed or switched makes the player
or monster appear elsewhere in the level. There are only regular and
extended teleporters, no generalized teleporters exist (yet).
Teleporters are the only direction sensitive walkover triggers in DOOM
(to allow them to be exited without teleporting again). They must be
crossed from the first sidedef side in order to work.\
\
The place that the player or monster appears is set in one of two ways.
The more common is by a teleport thing in the sector tagged to the
teleport line. The newer completely player preserving line-line silent
teleport makes the player appear relative to the exit line (identified
by having the same tag as the entry line) with the same orientation,
position, momentum and height that they had relative to the entry line
just before teleportation. A special kind of line-line teleporter is
also provided that reverses the player\'s orientation by 180 degrees.\
\
In the case of several lines tagged the same as the teleport line, or
several sectors tagged the same, or several teleport things in the
tagged sector, in all cases the lowest numbered one is used.\
\
In DOOM crossing a W1 teleport line in the wrong direction used it up,
and it was never activatable. In BOOM this is not the case, it does not
teleport but remains active until next crossed in the right direction.\
\
If the exit of a teleporter is blocked a monster (or anything besides a
player) will not teleport thru it. However if the exit of teleporter is
blocked a player will, including into a down crusher, a monster, or
another player. In the latter two cases the monster/player occupying the
spot dies messily, and this is called a telefrag.

[]{#televar}

------------------------------------------------------------------------

**Section 9.1 Teleport Varieties**

------------------------------------------------------------------------

\
A teleporter can be walkover or switched, and retriggerable or not.\
\
A teleporter can have destination set by a teleport thing in the tagged
sector, or by a line tagged the same as the teleport line. These are
called thing teleporters and line teleporters resp.\
\
A teleporter can preserve orientation or set orientation from the
position, height, and angle of the teleport thing. Note a thing
destination teleporter always sets position, though it may preserve
orientation otherwise.\
\
A teleporter can produce green fog and a whoosh noise, or it can be
silent, in which case it does neither.\
\
A teleporter can transport monsters only, or both players and monsters.

[]{#telelines}

------------------------------------------------------------------------

**Section 9.2 Teleport Linedef types**

------------------------------------------------------------------------

\


    Regular and Extended Teleport types
    -------------------------------------------------------------------
    #     Class  Trig  Silent Mon Plyr Orient    Dest

    195   Ext    SR    No     Yes Yes  Set       TP thing in tagged sector
    174   Ext    S1    No     Yes Yes  Set       TP thing in tagged sector
    97    Reg    WR    No     Yes Yes  Set       TP thing in tagged sector
    39    Reg    W1    No     Yes Yes  Set       TP thing in tagged sector

    126   Reg    WR    No     Yes No   Set       TP thing in tagged sector
    125   Reg    W1    No     Yes No   Set       TP thing in tagged sector
    269   Ext    WR    Yes    Yes No   Set       TP thing in tagged sector
    268   Ext    W1    Yes    Yes No   Set       TP thing in tagged sector

    210   Ext    SR    Yes    Yes Yes  Preserve  TP thing in tagged sector
    209   Ext    S1    Yes    Yes Yes  Preserve  TP thing in tagged sector
    208   Ext    WR    Yes    Yes Yes  Preserve  TP thing in tagged sector
    207   Ext    W1    Yes    Yes Yes  Preserve  TP thing in tagged sector

    244   Ext    WR    Yes    Yes Yes  Preserve  Line with same tag
    243   Ext    W1    Yes    Yes Yes  Preserve  Line with same tag
    263   Ext    WR    Yes    Yes Yes  Preserve  Line with same tag (reversed)
    262   Ext    W1    Yes    Yes Yes  Preserve  Line with same tag (reversed)

    267   Ext    WR    Yes    Yes No   Preserve  Line with same tag 
    266   Ext    W1    Yes    Yes No   Preserve  Line with same tag 
    265   Ext    WR    Yes    Yes No   Preserve  Line with same tag (reversed) 
    264   Ext    W1    Yes    Yes No   Preserve  Line with same tag (reversed)

[Return to Table of Contents](#contents)

[]{#donuts}

------------------------------------------------------------------------

**Section 10. Donuts**

------------------------------------------------------------------------

\
A donut is a very specialized linedef type that lowers a pillar in a
surrounding pool, while raising the pool and changing its texture and
type.\
\
The tagged sector is the pillar, and its lowest numbered line must be
two-sided. The sector on the other side of that is the pool. The pool
must have a two-sided line whose second sidedef does not adjoin the
pillar, and the sector on the second side of the lowest numbered such
linedef is the model for the pool\'s texture change. The model sector
floor also provides the destination height both for lowering the pillar
and raising the pool.\
\
No generalized donut linedefs exist, and all are switched or walkover.

[]{#donutlines}

------------------------------------------------------------------------

**Section 10.1 Donut Linedef types**

------------------------------------------------------------------------

\


    Regular and Extended Donut types
    -------------------------------------------------------------------
    #     Class  Trig

    191   Ext    SR
    9     Reg    S1
    155   Ext    WR
    146   Ext    W1

[Return to Table of Contents](#contents)

[]{#props}

------------------------------------------------------------------------

**Section 11. Property Transfer**

------------------------------------------------------------------------

\
These linedefs are special purpose and are used to transfer properties
from the linedef itself or the sector on its first sidedef to the tagged
sector(s). None are triggered, they simply exist.\
\


    Extended Property Transfer Linedefs
    -------------------------------------------------------------------
    #     Class  Trig Description

[]{#line213}


    213   Ext    --   Set Tagged Floor Lighting to Lighting on 1st Sidedef's Sector

Used to give the floor of a sector a different light level from the
remainder of the sector. For example bright lava in a dark room.\
\
[]{#line261}


    261   Ext    --   Set Tagged Ceiling Lighting to Lighting on 1st Sidedef's Sector

Used to give the ceiling of a sector a different light level from the
remainder of the sector.\
\
[]{#line260}


    260   Ext    --   Make Tagged Lines (or this line if tag==0) Translucent

Used to make 2s normal textures translucent. If tag==0, then this
linedef\'s normal texture is made translucent if it\'s 2s, and the
default translucency map TRANMAP is used as the filter. If tag!=0, then
all similarly-tagged 2s linedefs\' normal textures are made translucent,
and if this linedef\'s first sidedef contains a valid lump name for its
middle texture (as opposed to a texture name), and the lump is 64K long,
then that lump will be used as the translucency filter instead of the
default TRANMAP, allowing different filters to be used in different
parts of the same maps. If the first side\'s normal texture is not a
valid translucency filter lump name, it must be a valid texture name,
and will be displayed as such on this linedef.\
\
[]{#line242}


    242   Ext    --   Set Tagged Lighting, Flats Heights to 1st Sidedef's Sector,
                      and set colormap based on sidedef textures.

This allows the tagged sector to have two levels \-- an actual floor and
ceiling, and another floor or ceiling where more flats are rendered.
Things will stand on the actual floor or hang from the actual ceiling,
while this function provides another rendered floor and ceiling at the
heights of the sector on the first sidedef of the linedef. Typical use
is \"deep water\" that can be over the player\'s head.\
\


         ----------------------------------  < real sector's ceiling height
        |         real ceiling             | < control sector's ceiling texture
        |                                  |
        |                                  | < control sector's lightlevel
        |              A                   |
        |                                  | < upper texture as colormap
        |                                  |
        |                                  | < control sector's floor texture
         ----------------------------------  < control sector's ceiling height
        |         fake ceiling             | < real sector's ceiling texture
        |                                  |
        |                                  | < real sector's lightlevel
        |              B                   |
        |                                  | < normal texture as colormap
        |                                  |
        |          fake floor              | < real sector's floor texture
         ----------------------------------  < control sector's floor height
        |                                  | < control sector's ceiling texture
        |                                  |
        |                                  | < control sector's lightlevel
        |              C                   |
        |                                  | < lower texture as colormap
        |                                  |
        |          real floor              | < control sector's floor texture
         ----------------------------------  < real sector's floor height

Boom sectors controlled by a 242 linedef are partitioned into 3 spaces.
The viewer\'s xyz coordinates uniquely determine which space they are
in.\
\
If they are in space B (normal space), then the floor and ceiling
textures and lightlevel from the real sector are used, and the colormap
from the 242 linedef\'s first sidedef\'s normal texture is used
(COLORMAP is used if it\'s invalid or missing). The floor and ceiling
are rendered at the control sector\'s heights.\
\
If they are in space C (\"underwater\"), then the floor and ceiling
textures and lightlevel from the control sector are used, and the lower
texture in the 242 linedef\'s first sidedef is used as the colormap.\
\
If they are in space A (\"head over ceiling\"), then the floor and
ceiling textures and lightlevel from the control sector are used, and
the upper texture in the 242 linedef\'s first sidedef is used as the
colormap.\
\
If only two of these adjacent partitions in z-space are used, such as
underwater and normal space, one has complete control over floor
textures, ceiling textures, light level, and colormaps, in each of the
two partitions. The control sector determines the textures and lighting
in the more \"unusual\" case (e.g. underwater).\
\
It\'s also possible for the fake floor to extend below the real floor,
in which case an invisible platform / stair effect is created. In that
case, the picture looks like this (barring any ceiling effects too):\
\


         ----------------------------------  < real sector's ceiling texture
        |   real ceiling = fake ceiling    |
        |                                  |
        |                                  |
        |              B                   | < real sector's lightlevel
        |                                  | < normal texture's colormap
        |                                  |
        |          real floor              |
         ----------------------------------  < invisible, no texture drawn
        |                                  |
        |                                  |
        |                                  | < real sector's lightlevel
        |              C                   | < normal texture's colormap
        |                                  |
        |                                  |
        |          fake floor              | < real sector's floor texture
         ----------------------------------  < fake sector's floor height

In this case, since the viewer is always at or above the fake floor, no
colormap/lighting/texture changes occur \-- the fake floor just gets
drawn at the control sector\'s height, but at the real sector\'s
lighting and texture, while objects stand on the higher height of the
real floor.\
\
It\'s the viewer\'s position relative to the fake floor and/or fake
ceiling, which determines whether the control sector\'s lighting and
textures should be used, and which colormap should be used. If the
viewer is always between the fake floor and fake ceiling, then no
colormap, lighting, or texture changes occur, and the view just sees the
real sector\'s textures and light level drawn at possibly different
heights.\
\
If the viewer is below the fake floor height set by the control sector,
or is above the fake ceiling height set by the control sector, then the
corresponding colormap is used (lower or upper texture name), and the
textures and lighting are taken from the control sector rather than the
real sector. They are still stacked vertically in standard order \-- the
control sector\'s ceiling is always drawn above the viewer, and the
control sector\'s floor is always drawn below the viewer.\
\
The kaleidescope effect only occurs when F_SKY1 is used as the control
sector\'s floor or ceiling. If F_SKY1 is used as the control sector\'s
ceiling texture, then under water, only the control sector\'s floor
appears, but it \"envelops\" the viewer. Similarly, if F_SKY1 is used as
the control sector\'s floor texture, then when the player\'s head is
over a fake ceiling, the control sector\'s ceiling is used throughout.\
\
F_SKY1 causes HOM when used as a fake ceiling between the viewer and
normal space. Since there is no other good use for it, this kaleidescope
is an option turned on by F_SKY1. Note that this does not preclude the
use of sky REAL ceilings over deep water \-- this is the control
sector\'s ceiling, the one displayed when the viewer is underwater, not
the real one.\
\
A colormap has the same size and format as Doom\'s COLORMAP. Extra
colormaps may defined in Boom by adding them between C_START and C_END
markers in wads. Colormaps between C_START and C_END are automatically
merged by Boom with any previously defined colormaps.\
\
WATERMAP is a colormap predefined by Boom which can be used to provide a
blue-green tint while the player is under water. WATERMAP can be
modified by pwads.\
\
Ceiling bleeding may occur if required upper textures are not used.\
\
[]{#line223}


    223   Ext    --   Length Sets Friction in tagged Sector,Sludge<100, Ice>100

The length of the linedef with type 223 controls the amount of friction
experienced by the player in the tagged sector, when in contact with the
floor. Lengths less than 100 are sticker than normal, lengths greater
than 100 are slipperier than normal. The effect is only present in the
tagged sector when its friction enable bit (bit 8) in the sector type is
set. This allows the flat to be changed in conjunction with turning the
effect on or off thru texture/type changes.

=== New to MBF ===\
\
[]{#skytrans}


    271   Ext    --   Transfer sky texture to tagged sectors 
    272   Ext    --   Transfer sky texture to tagged sectors, flipped

These linedefs transfer wall textures to skies. F_SKY1 must still be
used as the floor or ceiling texture in the sectors for which sky is
desired, but the presence of a 271 or 272 property-transfer linedef can
change the sky texture to something other than a level-based default.\
\
Every sector with F_SKY1 floor or ceiling which shares the same sector
tag as the 271 or 272 linedef will use a sky texture based on the upper
texture of the first sidedef in the 271 or 272 linedef. Sectors with
F_SKY1 floors or ceilings which are not tagged with 271 or 272 linedefs,
behave just like Doom.\
\
Horizontal offsets or scrolling in the transferred texture, is converted
into rotation of the sky texture. Vertical offsets or scrolling is
transferred as well, allowing for precise adjustments of sky position.
Unpegging in the sky transfer linedef does not affect the sky texture.\
\
Horizontal scrolling of the transferred upper wall texture is converted
into rotation of the sky texture, but it occurs at a very slow speed
relative to the moving texture, allowing for long-period rotations (such
as one complete revolution per Doom real-time hour).\
\
Effects other than sky-transfer effects are not excluded from the
affected sector(s), and tags can be shared so long as the effects are
unambiguous. For example, a wall-scrolling linedef can share a sector
tag with both its affectee linedef (the one being scrolled), and with
the sector that the latter controls. There is no ambiguity because one
effect (scrolling) applies to a linedef, while the other effect (sky
transfer) applies to a sector.\
\
If a sector underneath a special sky needs to be set up to have a
different purpose (for example, if it is a lift or a stairbuilder), then
two tags will need to be created, and the transfer linedef and any
scrolling linedefs will need to be duplicated as well, so that the same
effect as far as the sky goes, is duplicated in two separate sector
tags. This will not affect sky appearance, but it will allow a special
sector which needs a unique tag, to sit under such a sky.\
\
Animated textures may be transferred to skies as well.\
\
In Doom, skies were flipped horizontally. To maintain compatibility with
this practice, the 272 linedef flips the wall image horizontally. The
271 linedef does not flip the wall image, and it is intended to make it
easier to take existing non-flipped wall textures and transfer them to
skies.\
\
Sky textures which are different must be separated by non-sky textures,
or else the results are undefined.\
Note for Eternity Engine v3.31 public beta 6 and later: Sky textures
will be rendered using the maximum of the texture height and the height
of the tallest patch in the texture, in order to properly support tall
skies that do not require stretching for mlook. Because of this,
textures which are meant to cut off a taller patch at the texture height
will not appear as expected when used as sky textures. Avoid using sky
textures which contain patches taller than the texture to avoid this
problem.

[]{#consttrans}

------------------------------------------------------------------------

**Constant pushers**

------------------------------------------------------------------------

Two types of constant pushers are available, wind and current. Depending
on whether you are above, on, or below (special water sectors) the
ground level, the amount of force varies.\
\
The length of the linedef defines the \'full\' magnitude of the force,
and the linedef\'s angle defines the direction.\
\


         line type         above  on   under
         ---------         -----  --   -----
         wind       224    full  half  none
         current    225    none  full  full

\
The linedef should be tagged to the sector where you want the effect.
The special type of the sector should have bit 9 set (0x200). If this
bit is turned off, the effect goes away. For example, a fan creating a
wind could be turned off, and the wind dies, by changing the sector type
and clearing that bit.\
\
Constant pushers can be combined with scrolling effects and point
pushers.\
\


    224  Ext     --   Length/Direction Sets Wind Force in tagged Sectors

    225  Ext     --   Length/Direction Sets Current Force in tagged Sectors

\
[]{#pointtrans}

------------------------------------------------------------------------

**Point pushers**

------------------------------------------------------------------------

Two types of point pushers are available, push and pull.\
\
This implementation ignores sector boundaries and provides the effect in
a circular area whose center is defined by a Thing of type 5001 (push)
or 5002 (pull). You don\'t have to set any option flags on these Things.
A new linedef type of 226 is used to control the effect, and this line
should be tagged to the sector with the 5001/5002 Thing.\
\
The length of the linedef defines the \'full\' magnitude of the force,
and the force is inversely proportional to distance from the point
source. If the length of the controlling linedef is L, then the force is
reduced to zero at a distance of 2L.\
\
The angle of the controlling linedef is not used.\
\
The sector where the 5001/5002 Things reside must have bit 9 set (0x200)
in its type. If this is turned off, the effect goes away.\
\
Point pushers can be combined with scrolling effects and constant
pushers.\
\


    226  Ext     --   Length Sets Point Source Wind/Current Force in Tagged Sectors

=== New to Eternity ===\
\
[]{#3dmidtex}

------------------------------------------------------------------------

Moving 3DMidTex Lines

------------------------------------------------------------------------

\
These line types allow lines with the 3DMidTex flag (linedef flag 1024)
to be scrolled vertically along with the floor or ceiling action of a
sector.\
\
The floor or ceiling movement of the sector on the first side of one of
these lines will be transferred to the y offsets of both sidedefs of all
like-tagged 3DMixTex lines. If the transfer sector or the tagged lines
are blocked by a moving object such as an enemy, they will obey the
transfer sector\'s rules for movement when blocked (for example,
3DMidTex lines tagged to a DR door sector will bounce off of monster\'s
heads).\
\
The new linedefs are as follows:\
\


    281  Ext     --   Floor movement of sector on 1st side is transferred to all
                      like-tagged 3DMidTex lines as vertical scrolling
                   
    282  Ext     --   Ceiling movement of sector on 1st side is transferred to all
                      like-tagged 3DMidTex lines as vertical scrolling

[]{#hticpush}

------------------------------------------------------------------------

Heretic wind/current transfer

------------------------------------------------------------------------

\
These lines allow Heretic-style wind and current effects to be used
independent of automatic Heretic map translation. BOOM-style Heretic
levels should use these lines in combination with scrollers and
generalized damage sector types to replicate Heretic sector types 4 and
20 through 51.\
\
Line type 293 duplicates Heretic wind specials, which affect only things
with the Bits3 WINDTHRUST flag when the things are anywhere within the
tagged sectors.\
\
Line type 294 duplicates Heretic current specials, which affect only the
player, and only when he/she is on the ground.\
\
The effect, applied in every tagged sector, is to push all things in the
direction of the control linedef with a force proportional to its
length. The length of the line is scaled by a factor of 512, giving the
following equivalencies for Heretic sector types:


    Heretic Sector Type    Equivalent Line Setup
    ---------------------------------------------------------------------------
    4                      294, East (0 degrees), length  112

    20 - 24                294, East (0 degrees), lengths 20, 40, 100, 120, 140
    25 - 29                294, North (90 deg.),  lengths 20, 40, 100, 120, 140
    30 - 34                294, South (270 deg.), lengths 20, 40, 100, 120, 140
    35 - 39                294, West (180 deg.),  lengths 20, 40, 100, 120, 140

    40 - 42                293, East (0 degrees), lengths 20, 40, 100
    43 - 45                293, North (90 deg.),  lengths 20, 40, 100
    46 - 48                293, South (270 deg.), lengths 20, 40, 100
    49 - 51                293, West (180 deg.),  lengths 20, 40, 100
    ---------------------------------------------------------------------------

The new linedefs are as follows:\
\


    293  Ext     --   Heretic wind in line direction with push magnitude proportional
                      to line length.
                   
    294  Ext     --   Heretic current in line direction with push magnitude proportional
                      to line length.

[Return to Table of Contents](#contents)

[]{#scrolls}

------------------------------------------------------------------------

**Section 12. Scrolling Walls, Flats, Objects**

------------------------------------------------------------------------

\
[]{#staticscroll}

------------------------------------------------------------------------

**Section 12.1 Static Scrollers**

------------------------------------------------------------------------

\
The most basic kind of scrolling linedef causes some part of the tagged
sector or tagged wall to scroll, in proportion to the length of the
linedef the trigger is on, and according to the direction that trigger
linedef lies. For each 32 units of length of the trigger, the tagged
object scrolls 1 unit per frame. Since the length of a linedef doesn\'t
change during gameplay, these types are static, the scrolling effect
remains constant during gameplay. However, these effects can be combined
with, and/or canceled by, other scrollers.\
\

- 250 \-- Scroll Tagged Ceiling\
  \
  The ceiling of the tagged sector scrolls in the direction of the
  linedef trigger, 1 unit per frame for each 32 units of linedef trigger
  length. Objects attached to the ceiling do not move.\
  \
- 251 \-- Scroll Tagged Floor\
  \
  The floor of the tagged sector scrolls in the direction of the linedef
  trigger, 1 unit per frame for each 32 units of linedef trigger length.
  Objects resting on the floor do not move.\
  \
- 252 \-- Carry Objects on Tagged Floor\
  \
  Objects on the floor of the tagged sector move in the direction of the
  linedef trigger, 1 unit per frame for each 32 units of linedef trigger
  length. The floor itself does not scroll.\
  \
- 253 \-- Scroll Tagged Floor, Carry Objects\
  \
  Both the floor of the tagged sector and objects resting on that floor
  move in the direction of the linedef trigger, 1 unit per frame for
  each 32 units of linedef trigger length.\
  \
- 254 \-- Scroll Tagged Wall, Same as Floor/Ceiling\
  \
  Walls with the same tag as the linedef trigger scroll at the same rate
  as a floor or ceiling controlled by one of 250-253, allowing their
  motion to be synchronized. When the linedef trigger is not parallel to
  the wall, the component of the linedef in the direction perpendicular
  to the wall causes the wall to scroll vertically. The length of the
  component parallel to the wall sets the horizontal scroll rate, the
  length of the component perpendicular to the wall sets the vertical
  scroll rate.

[]{#simplescroll}

------------------------------------------------------------------------

**Section 12.2 Simple Static Scrollers**

------------------------------------------------------------------------

\
For convenience three simpler static scroll functions are provided for
when you just need a wall to scroll and don\'t need to control its rate
and don\'t want to bother with proportionality.\
\

- 255 \-- Scroll Wall Using Sidedef Offsets\
  \
  For simplicity, a static scroller is provided that scrolls the first
  sidedef of a linedef, based on its x- and y- offsets. No tag is used.
  The x offset controls the rate of horizontal scrolling, 1 unit per
  frame per x offset, and the y offset controls the rate of vertical
  scrolling, 1 unit per frame per y offset.\
  \
- 48 \-- Animated wall, Scrolls Left\
  \
  A linedef with this type scrolls its first sidedef left at a constant
  rate of 1 unit per frame.\
  \
- 85 \-- Animated wall, Scrolls Right\
  \
  A linedef with this type scrolls its first sidedef right at a constant
  rate of 1 unit per frame.

[]{#dynscroll}

------------------------------------------------------------------------

**Section 12.3 Dynamic Scrolling**

------------------------------------------------------------------------

\
To achieve dynamic scrolling effects, the position or rate of scrolling
is controlled by the relative position of the floor and ceiling of the
sector on the scrolling trigger\'s first sidedef. The direction of
scrolling remains controlled by the direction of the linedef trigger.
Either the floor or ceiling may move in the controlling sector, or both.
The control variable is the amount of change in the sum of the floor and
ceiling heights.\
\
All scroll effects are additive, and thus two or more effects may
reinforce and/or cancel each other.\
\
Displacement Scrollers\
\
In the first kind, displacement scrolling, the position of the scrolled
objects or walls changes proportionally to the motion of the floor or
ceiling in the sector on the first sidedef of the scrolling trigger. The
proportionality is set by the length of the linedef trigger. If it is 32
units long, the wall, floor, ceiling, or object moves 1 unit for each
unit the floor or ceiling of the controlling sector moves. If it is 64
long, they move 2 units per unit of relative floor/ceiling motion in the
controlling sector and so on.\
\

- 245 \-- Scroll Tagged Ceiling w.r.t. 1st Side\'s Sector\
  \
  The tagged sector\'s ceiling texture scrolls in the direction of the
  scrolling trigger line, when the sector on the trigger\'s first
  sidedef changes height. The amount moved is the height change times
  the trigger length, divided by 32. Objects attached to the ceiling do
  not move.\
  \
- 246 \-- Scroll Tagged Floor w.r.t. 1st Side\'s Sector\
  \
  The tagged sector\'s floor texture scrolls in the direction of the
  scrolling trigger line when the sector on the trigger\'s first sidedef
  changes height. The amount moved is the height change times the
  trigger length, divided by 32. Objects on the floor do not move.\
  \
- 247 \-- Push Objects on Tagged Floor wrt 1st Side\'s Sector\
  \
  Objects on the tagged sector\'s floor move in the direction of the
  scrolling trigger line when the sector on the trigger\'s first sidedef
  changes height. The amount moved is the height change times the
  trigger length, divided by 32.\
  \
- 248 \-- Push Objects & Tagged Floor wrt 1st Side\'s Sector\
  \
  The tagged sector\'s floor texture, and objects on the floor, move in
  the direction of the scrolling trigger line when the sector on the
  trigger\'s first sidedef changes height. The amount moved is the
  height change times the trigger length, divided by 32.\
  \
- 249 \-- Scroll Tagged Wall w.r.t 1st Side\'s Sector\
  \
  Walls with the same tag as the linedef trigger scroll at the same rate
  as a floor or ceiling controlled by one of 245-249, allowing their
  motion to be synchronized. When the linedef trigger is not parallel to
  the wall, the component of the linedef in the direction perpendicular
  to the wall causes the wall to scroll vertically. The length of the
  component parallel to the wall sets the horizontal scroll
  displacement, the length of the component perpendicular to the wall
  sets the vertical scroll displacement. The distance scrolled is the
  controlling sector\'s height change times the trigger length, divided
  by 32.\

Accelerative Scrollers\
\
The second kind of dynamic scrollers, accelerative scrollers, also react
to height changes in the sector on the first sidedef of the linedef
trigger, but the RATE of scrolling changes, not the displacement. That
is, changing the controlling sector\'s height speeds up or slows down
the scrolling by the change in height times the trigger\'s length,
divided by 32.\
\
An on/off scroller can be made by using an accelerative scroller and any
linedef that changes the controlling sector\'s heights. If a scroll
effect which is initially on is desired, then the accelerative scroller
should be combined with a static one which turns the scroll effect on
initially. The accelerative scroller would then need to be set up to
cancel the static scroller\'s effect when the controlling sector\'s
height changes.\
\

- 214 \-- Accel Tagged Ceiling w.r.t. 1st Side\'s Sector\
  \
  The tagged sector\'s ceiling\'s rate of scroll changes in the
  direction of the trigger linedef (use vector addition if already
  scrolling) by the change in height of the sector on the trigger\'s
  first sidedef times the length of the linedef trigger, divided by 32.
  For example, if the ceiling is motionless originally, the linedef
  trigger is 32 long, and the ceiling of the controlling sector moves 1
  unit, the tagged ceiling will start scrolling at 1 unit per frame.\
  \
- 215 \-- Accel Tagged Floor w.r.t. 1st Side\'s Sector\
  \
  The tagged sector\'s floor\'s rate of scroll changes in the direction
  of the trigger linedef (use vector addition if already scrolling) by
  the change in height of the sector on the trigger\'s first sidedef
  times the length of the linedef trigger, divided by 32. For example,
  if the floor is motionless originally, the linedef trigger is 32 long,
  and the ceiling of the controlling sector moves 1 unit, the tagged
  floor will start scrolling at 1 unit per frame.\
  \
- 216 \-- Accel Objects on Tagged Floor wrt 1st Side\'s Sector\
  \
  Objects on the tagged sector\'s floor\'s rate of motion changes in the
  direction of the trigger linedef (use vector addition if already
  moving) by the change in height of the sector on the trigger\'s first
  sidedef times the length of the linedef trigger divided by 32. For
  example, if the objects are motionless originally, the linedef trigger
  is 32 long, and the ceiling of the controlling sector moves 1 unit,
  the objects on the tagged floor will start moving at 1 unit per
  frame.\
  \
  \
- 217 \-- Accel Objects&Tagged Floor wrt 1st Side\'s Sector\
  \
  The tagged sector\'s floor, and objects on it, change its rate of
  motion in the direction of the trigger linedef (use vector addition if
  already moving) by the change in height of the sector on the
  trigger\'s first sidedef times the length of the linedef trigger,
  divided by 32. For example, if the floor and objects are motionless
  originally, the linedef trigger is 32 long, and the ceiling of the
  controlling sector moves 1 unit, the objects and the tagged floor will
  start moving at 1 unit per frame.\
  \
- 218 \-- Accel Tagged Wall w.r.t 1st Side\'s Sector\
  \
  Walls with the same tag as the linedef trigger increase their scroll
  rate in sync with a floor or ceiling controlled by one of 214-217.
  When the linedef trigger is not parallel to the wall, the component of
  the linedef in the direction perpendicular to the wall causes the wall
  to increase its vertical scroll rate. The length of the component
  parallel to the wall sets the change in horizontal scroll rate, the
  length of the component perpendicular to the wall sets the change in
  vertical scroll rate. The rate change is the height change times the
  trigger length, divided by 32.

[Return to Table of Contents](#contents)

[]{#portals}

------------------------------------------------------------------------

**Section 13. Portals**

------------------------------------------------------------------------

\
Portals turn a set of floors, ceilings, or linedefs into \"windows\"
onto another part of the map, allowing for some otherwise impossible
visual effects. Portals are not interactive, however \-- objects cannot
travel through them, enemies cannot see through them or fire into them,
etc.

[]{#planeportal}

------------------------------------------------------------------------

**Plane Portals**

------------------------------------------------------------------------

\
A plane portal linedef creates a portal which displays an infinite
plane. The flat and light level used are determined from the ceiling of
the sector on the front side of the plane portal control linedef.\
\
Plane portal linedefs are as follows:


    283  Ext     --   Applies plane portal to ceilings of all like-tagged sectors.
                   
    284  Ext     --   Applies plane portal to floors of all like-tagged sectors.
                      
    285  Ext     --   Applies plane portal to floors and ceilings of all like-tagged sectors.

[]{#horizportal}

------------------------------------------------------------------------

**Horizon Portals**

------------------------------------------------------------------------

\
A horizon portal linedef creates a portal which displays two infinite
planes which converge on a fixed horizon at the center of the screen.
The flat and light level of the two planes come from the floor and
ceiling of the sector on the front side of the horizon portal control
linedef.\
\
Horizon portal linedefs are as follows:


    286  Ext     --   Applies horizon portal to ceilings of all like-tagged sectors.
                   
    287  Ext     --   Applies horizon portal to floors of all like-tagged sectors.
                      
    288  Ext     --   Applies horizon portal to floors and ceilings of all like-tagged sectors.

[]{#skybxportal}

------------------------------------------------------------------------

**Skyboxes**

------------------------------------------------------------------------

\
A skybox is a portal which renders part of the map from the point of
view of a control object. To setup a skybox, an EESkyboxCam object
(doomednum 5006) must be placed in a sector. Then, one of the following
skybox portal lines must be placed in that same sector such that it is
the sector on the front side of the line. When viewed in the map, the
surroundings of the control object will be seen at their real, fixed
distance from that point, although the player\'s viewing angle will
still be applied (so the scenery will turn, but not move).\
\
Skybox linedefs are as follows:


    290  Ext     --   Applies skybox portal to ceilings of all like-tagged sectors.
                   
    291  Ext     --   Applies skybox portal to floors of all like-tagged sectors.
                      
    292  Ext     --   Applies skybox portal to floors and ceilings of all like-tagged sectors.

[]{#anchrportal}

------------------------------------------------------------------------

**Anchored Portals**

------------------------------------------------------------------------

\
Anchored portals are the most complicated portal type. They define a
portal similar to a skybox, but which moves relative to the player\'s
point of view. This allows an anchored portal to appear as if it is a
seamless part of the surrounding map.\
\
To create an anchored portal, first one of the below anchored portal
control linedefs should be placed into the sector which will appear
inside the portal. Then, the sector in which the portal will appear
should be tagged the same as the control linedef. This is the same as
for all other types. However, an anchor line is also required so that
the renderer can guage the offset between the real map area and the
portal sector. This line, with specials 298 or 299, must be placed in
the real sector, and must share the same tag as the sector and control
linedef.\
\
Here is an illustration of an anchored portal setup:


    +---+              +---+
    | A |-298 linedef  | B |-295 linedef
    +---+              +---+

Sector A is the normal sector in which the player will walk. Sector B is
the portal sector which will be drawn into the portal. The 298 linedef,
the 295 linedef, and sector A all share a common tag. Eternity will
guage the distance between the portal linedef and the anchor linedef and
will know how far to offset the camera in order to render the portal. In
this case, Eternity will render sector B over the ceiling of sector A,
and the two will appear to be placed directly on top of each other.\
\
As of Eternity Engine v3.31 public beta 7, the 296 anchored portal type
uses line special 299 for its anchor line. This was revised to allow
separate anchored portals to be used for the floor and ceiling of the
same sector, which would not previously work. 295 and 297 still use the
298 anchor type.\
\
Note that the areas drawn within the anchored portal will render at the
player\'s current viewheight, so if the view within sector B in this
example were above the ceiling or beneath the floor of sector B, a HOM
effect would appear in the portal instead of the expected view. Be sure
that areas inside an anchored portal are large enough both in terms of
area and height so that the offset camera position always remains within
valid space regardless of where the player is viewing the portal from.\
\
Anchored portal linedefs are as follows:


    295  Ext     --   Applies anchored portal to ceilings of all like-tagged sectors.
                   
    296  Ext     --   Applies anchored portal to floors of all like-tagged sectors.
                      
    297  Ext     --   Applies anchored portal to floors and ceilings of all like-tagged sectors.

    298  Ext     --   Anchor line, required for use alongside specials 295 and 297.

    299  Ext     --   Anchor line, required for use alongside special 296.

Warning: As of Eternity Engine v3.33.33, normal anchored portals should
no longer be used for two-way setups, where both areas of an anchored
portal are visible from each other. Use the new two-way types discussed
below instead for this situation. If the lines above are used instead,
the player may see seemingly spontaneous error messages and feel
annoying pauses during gameplay.

[]{#twowyportal}

------------------------------------------------------------------------

**Two-Way Anchored Portals**

------------------------------------------------------------------------

\
For two-way anchored portal setups, where each area involved in the
portal can be seen from the other area, you are now required to use the
following set of linedefs instead of those outlined in the section
above. This change is necessary to support visibility of normal portals
even when the player\'s viewpoint would be beneath the floor or ceiling.
Unfortunately, such a change causes the normal anchored portals to begin
drawing each other repeatedly when used in a two-way setup. Although
this will no longer crash Eternity, it is considered an error and will
be displayed to the player as such. To avoid this, use only these types
in such a situation.


    344  Ext     --   Applies two-way anchored portal to ceilings of all like-tagged sectors.
                   
    345  Ext     --   Applies two-way anchored portal to floors of all like-tagged sectors.
                      
    346  Ext     --   Anchor line, required for use alongside special 344 only.

    347  Ext     --   Anchor line, required for use alongside special 345 only.

[]{#line289}

------------------------------------------------------------------------

**289: Portal Transfer**

------------------------------------------------------------------------

\
The portal transfer linedef allows the effects of any type of portal to
be transferred to the front side of a one-sided line (it will \*not\*
work on 2S lines). To accomplish this, simply give the 289 line the same
tag used by the portal control linedef which is being targetted. Any
number of 289 lines may share a portal, and the portal may still be used
normally. If the normal effect of the portal (applied to a floor or
ceiling) is not needed, it does not have to be tagged to a sector.\
\
An example:


    +------+---+                 +------+
    |       \ B|<- B tag = 1     |      |
    |        \ |                 |  C   |
    |   A      +                 |      |
    |          |                 |  *   |<- Skybox line special 290 , tag = 1
    +----------+                 +------+
      ^                             ^
      Line special 289, tag = 1     Control thing 5006

In this example, sector C will be visible in sector B\'s ceiling as a
skybox. However, it will also appear on the line in sector A with
special 289, giving it a window-like effect.


    289  Ext     --   Transfers a like-tagged portal to the first side of this linedef.

[Return to Table of Contents](#contents)

[]{#scripts}

------------------------------------------------------------------------

**Section 14. Script Control Linedefs**

------------------------------------------------------------------------

\
These linedefs are used to start Small Levelscripts or ACS scripts. See
the [Small Usage Documentation](small_usagedoc.html) for full
information on how to use these linedefs along with a Small script to
add dynamic actions to your maps.\


    280 WR Start script with tag number
    273 WR Start script, 1-way trigger
    274 W1 Start script with tag number
    275 W1 Start script, 1-way trigger
    276 SR Start script with tag number
    277 S1 Start script with tag number
    278 GR Start script with tag number
    279 G1 Start script with tag number

    Parameterized Types
    ---------------------------------------------------------
    #   ExtraData Name   Parameters

    365 ACS_Execute      scriptnum, mapnum, arg1, arg2, arg3
    366 ACS_Suspend      scriptnum, mapnum
    367 ACS_Terminate    scriptnum, mapnum

    ---------------------------------------------------------

These linedefs exist for all activation models. The two one-way script
activators will only be activated when the player crosses them from the
first side, as with other line types which function in that manner.
Crossing from the second side will have no effect.\
\
[Return to Table of Contents](#contents)

[]{#polyobj}

------------------------------------------------------------------------

**Section 15. PolyObjects**

------------------------------------------------------------------------

\
PolyObjects are special sets of one-sided linedefs which, unlike any
other lines, can be moved on the map during gameplay. Unlike the Hexen
and zdoom implementation of PolyObjects, Eternity does not restrict
these objects to one per subsector, and they are also allowed to move
any distance from their spawn point, even across subsector boundaries.
The only real restriction is that PolyObjects should remain separated by
a plane from all other surfaces, including other PolyObjects and normal
walls.\
\
PolyObjects fully clip normal objects such as the player and enemies,
optionally doing crushing damage depending upon the type of spawn spot
used. They also block bullet tracers and monsters\' lines of sight. They
do not, however, clip sound propagation, so it may be necessary to use
sound blocking lines near areas where PolyObjects are meant to function
as a normal wall until they move.\
\
Although Eternity does allow PolyObjects to move between different
subsectors, this will only work properly if the subsectors on both sides
of a line have the same properties (floor & ceiling heights, light
level, and flat) \-- otherwise, temporary rendering oddities such as
bleeding or disappearing lines may occur. However, due to its
implementation of PolyObjects via true dynamic seg generation, Eternity
does not require a \"PolyObject-aware\" node builder.\
\
Finally, unlike zdoom, Eternity allows PolyObjects to be used in
DOOM-format maps via the use of [ExtraData](#extradata). See the
subsections below for more information on how to create and move
PolyObjects.\
\
[Return to Table of Contents](#contents)

[]{#polyobj_create}

------------------------------------------------------------------------

**15.1 Creating PolyObjects**

------------------------------------------------------------------------

\
PolyObjects are a combination of a set of 1S lines, which are typically
placed in a \"control sector\" area outside of the main map, and exactly
two mapthings: one spawn spot, and one anchor point. There are two
different methods for grouping lines into a PolyObject.

Method 1: The Polyobj_StartLine Special\
\
The Polyobj_StartLine special marks the first line in a PolyObject which
is made up of a cyclic set of linedefs (that is, each line \"points
toward\" the next line, ending at the first vertex of the first line).\
\
Diagrammatic Example:


        [Control Sector]                     [Main Map Area]
        +-------------------+          +-------------------------------+
        |                   |          |                               |
        |        |          |          |                               |
        |      +--->+       |          |                               |
        |      ^    |       |          |                               |
        |      |    |_      |          |                               |
        |   A -| B  |       |          |              C                |
        |      |    v       |          |                               |
        |      +<---+       |          |                               |
        |         |         |          |                               |
        |                   |          |                               |
        +-------------------+          +-------------------------------+

Assume that the linedef A is assigned the Polyobj_StartLine special and
has argument #1 set to \"1\" \-- this is its PolyObject ID number, which
must be unique amongst all on the map, and must be a number greater than
zero.\
\
Starting from line A, the engine will go from vertex to vertex, adding
the first line found which shares the current line\'s second vertex as
its first vertex. In this example, since the object is closed as is
required, the process will end at the first vertex of line A, and there
will be four lines in the PolyObject (note that unlike Hexen, there is
NO limit to the number of lines that can be placed in one PolyObject).\
\
Now, once the lines are added, it is necessary to define the control
objects. Points B and C are the two required objects:

- B: This is the PolyObject\'s anchor point (type EEPolyObjAnchor with
  DoomEd #9300). It defines the point relative to which all the lines in
  the PolyObject will be translated to the spawn point. This object\'s
  angle must be set to the same value as the StartLine\'s first argument
  (the PolyObject ID). There must be one and only one of these objects
  for each PolyObject ID.
- C: This is the PolyObject\'s spawn point (one of EEPolyObjSpawnSpot
  \[9301\] or EEPolyObjSpawnSpotCrush \[9302\]). It defines the point
  where the PolyObject will initially spawn on the map. The anchor point
  will be translated to this exact location, and all lines in the
  PolyObject will maintain their relative positions to it. This object
  must also have its angle field set to the same value as both the
  StartLine\'s first argument and the anchor point\'s angle field. There
  must be one and only one of these objects for each PolyObject ID. Use
  9302 to cause the PolyObject to do crushing damage to things that
  block it while it is in motion.

Note that when you use this method, the other lines belonging to the
PolyObject can be given their own line specials, including ones which
affect the PolyObject itself. When the player presses the PolyObject,
the specials will work as expected.\
\
The complete arguments to Polyobj_StartLine are as follows:

- polyobj_id : the unique ID number (greater than zero) of the
  PolyObject
- mirror_id : the ID number of a PolyObject that you want to mirror
  every action that affects this PolyObject. This number cannot be the
  same as the PolyObject\'s own ID.
- sndseq_num : Number of an EDF sound sequence that this PolyObject will
  use when moving.

In order to use this line special in a DOOM-format map, it is necessary
to use [ExtraData](#extradata). In that case, you must give the line you
want to have the Polyobj_StartLine special the ExtraData linedef control
special (#270) instead, and then specify Polyobj_StartLine and its
arguments in its corresponding ExtraData linedef record.

Method 2: The Polyobj_ExplicitLine Special\
\
The Polyobj_ExplicitLine special demarcates every line that will be
added to a PolyObject and the exact order in which the lines will be
added. This affords a bit more flexibility in the construction of
PolyObjects at the price of not allowing any other line specials to be
placed on the object itself.\
\
Diagrammatic Example:


        [Control Sector]                     [Main Map Area]
        +-------------------+          +-------------------------------+
        |        2          |          |                               |
        |        |          |          |                               |
        |      +--->+       |          |                               |
        |      ^    |       |          |                               |
        |      |    |_ 4    |          |                               |
        |   1 -| B  |       |          |              C                |
        |      |    v       |          |                               |
        |      +<---+       |          |                               |
        |         |         |          |                               |
        |         3         |          |                               |
        +-------------------+          +-------------------------------+

Assume that the linedefs labeled 1 through 4 are assigned the
Polyobj_ExplicitLine special, all have argument #1 set to \"1\" \-- this
is its PolyObject ID number, which must be unique amongst all on the
map, and must be a number greater than zero \-- and all have argument #2
set to the integer values 1 through 4, corresponding to their labels in
the diagram.\
\
The game engine will search through all linedefs in the entire map for
ones with the current PolyObject ID in argument #1 and a value greater
than zero in argument #2. The lines will be collected and then sorted by
the value in their second argument and added to the PolyObject in that
order. Note that unlike Hexen, it is not necessary for the numbers in
the second argument to be sequential (ie, a sequence such as { 5, 10,
15, 20 } would also work).\
\
Now, once the lines are added, it is necessary to define the control
objects. This is exactly the same as it is for the StartLine special
above.

- B: This is the PolyObject\'s anchor point (type EEPolyObjAnchor with
  DoomEd #9300). It defines the point relative to which all the lines in
  the PolyObject will be translated to the spawn point. This object\'s
  angle must be set to the same value as the ExplicitLines\' first
  argument (the PolyObject ID). There must be one and only one of these
  objects for each PolyObject ID.
- C: This is the PolyObject\'s spawn point (one of EEPolyObjSpawnSpot
  \[9301\] or EEPolyObjSpawnSpotCrush \[9302\]). It defines the point
  where the PolyObject will initially spawn on the map. The anchor point
  will be translated to this exact location, and all lines in the
  PolyObject will maintain their relative positions to it. This object
  must also have its angle field set to the same value as both the
  ExplicitLines\' first argument and the anchor point\'s angle field.
  There must be one and only one of these objects for each PolyObject
  ID. Use 9302 to cause the PolyObject to do crushing damage to things
  that block it while it is in motion.

The complete arguments to Polyobj_ExplicitLine are as follows:

- polyobj_id : the unique ID number (greater than zero) of the
  PolyObject
- number : specifies the order in which the line will be added to the
  PolyObject. This number should be unique amongst all lines to be added
  to the same object, and must be a value greater than zero.
- mirror_id : the ID number of a PolyObject that you want to mirror
  every action that affects this PolyObject. This number cannot be the
  same as the PolyObject\'s own ID.
- sndseq_num : Number of an EDF sound sequence that this PolyObject will
  use when moving. NOTE: Sound sequences have not yet been implemented,
  so this argument will do nothing and is reserved for future use. Leave
  it zero for now.

In order to use this line special in a DOOM-format map, it is necessary
to use [ExtraData](#extradata). In that case, you must give the lines
you want to have the Polyobj_ExplicitLine special the ExtraData linedef
control special (#270) instead, and then specify Polyobj_ExplicitLine
and its arguments in all the corresponding ExtraData linedef records.\
\
[Return to Table of Contents](#contents)

[]{#polyobj_move}

------------------------------------------------------------------------

**15.2 Moving PolyObjects**

------------------------------------------------------------------------

\
There are two types of basic motion which PolyObjects can currently
undergo: translation and rotation. Using \"override\" actions, it is
possible to combine the two to have a PolyObject which rotates while
moving in the xy plane.

Translating PolyObjects\
\
To move a PolyObject in the xy plane, use one of the following line
specials:

- Polyobj_Move
- Polyobj_OR_Move

Both of these line specials use the same arguments:\

- polyobj_id : Specifies which PolyObject to intially move. If the
  object is already moving, nothing will happen unless the action is
  Polyobj_OR_Move. If this object has a mirror, the same action will be
  recursively applied to every mirror PolyObject, reversing the angle of
  movement for each subsequent mirror.
- speed : Speed of the motion in eighths of a unit per tic.
- angle : Byte angle specifying the direction of motion. To convert from
  degrees to byte angles, use this formula:\
  byteangle = (degrees \* 256) / 360\
  Round or chop the result to an integer. 0 means East, 64 means North,
  128 means West, and 192 means South.
- dist : The total distance to move the object in units.

All PolyObjects will inflict thrust on objects which block them with
force proportional to the speed of motion. If the PolyObject was spawned
with the EEPolyObjSpawnSpotCrush object (DoomEd num 9302), then if the
object does not fit at the location it is being pushed toward, it will
be damaged 3 hitpoints per tic (105 damage per second).

Rotating PolyObjects\
\
To rotate a PolyObject, use one of the following line specials:

- Polyobj_RotateRight
- Polyobj_OR_RotateRight
- Polyobj_RotateLeft
- Polyobj_OR_RotateLeft

All these line specials use the same arguments:\

- polyobj_id : Specifies which PolyObject to initially rotate. If the
  object is already moving, nothing will happen unless the action is
  Polyobj_OR_RotateRight or Polyobj_OR_RotateLeft. If this object has a
  mirror, the same action will be recursively applied to every mirror
  PolyObject, reversing the direction of rotation for each subsequent
  mirror.
- aspeed : Angular speed of the rotation in byte angles per tic. Same as
  for the \"angle\" parameter to Move actions.
- adist : Angular distance to rotate in byte angles. Same as for aspeed,
  except with two special caveats:
  - The byte angle value 0 (zero) means to rotate exactly 360 degrees.
  - The byte angle value 255 means to rotate perpetually. This type of
    action never ends and thus only override actions can subsequently be
    applied to the PolyObject.

[Return to Table of Contents](#contents)

[]{#polyobj_move}

------------------------------------------------------------------------

**15.3 PolyObject Doors**

------------------------------------------------------------------------

\
There are two distinct types of actions provided to allow PolyObjects to
function as doors. There are no override versions of door actions, and
applying other overrides to currently moving PolyObject doors will most
likely cause the doors to malfunction and move erratically.\
\
All PolyObject doors accept a delay parameter, and this specifies the
amount of time the door will wait before closing. A door which is
closing will reopen if blocked by an object.

- Polyobj_DoorSlide\
  This action creates a sliding door from a PolyObject and takes the
  following arguments:
  - polyobj_id : The ID number of PolyObject to affect. Mirroring works
    for doors as well and is the most common use for PolyObject
    mirroring.
  - speed : Speed of the door\'s opening and closing in eighths of a
    unit per tic.
  - angle : Byte angle of door\'s initial motion. This angle is reversed
    when the door closes.
  - dist : Distance the door moves when opening, and again when closing,
    in units.
  - delay : Amount of time before the door attempts to close after fully
    opening in tics. If the door is forced to reopen due to being
    blocked, it will also wait this amount of time before once again
    closing.
- Polyobj_DoorSwing\
  This action creates a swinging door from a PolyObject and takes the
  following arguments:
  - polyobj_id : The ID number of PolyObject to affect. Mirroring works
    for doors as well and is the most common use for PolyObject
    mirroring.
  - aspeed : Angular speed in byte angles per tic.
  - adist : Angular distance to rotate when opening, and again but in
    the opposite direction when closing.
  - delay : Amount of time before the door attempts to close after fully
    opening in tics. If the door is forced to reopen due to being
    blocked, it will also wait this amount of time before once again
    closing.

  Swinging doors currently only rotate left. A right-rotating swinging
  door special will be added in the near future.

[Return to Table of Contents](#contents)

[]{#pillars}

------------------------------------------------------------------------

**Section 16. Pillars**

------------------------------------------------------------------------

\
A pillar is a linedef type that moves both floor and ceiling in opposite
directions. All pillar linedefs are parameterized; there are no regular,
extended, or generalized pillar types. Instant pillar motion is not
possible.

[]{#pillarvars}

------------------------------------------------------------------------

**Section 16.1 Pillar varieties**

------------------------------------------------------------------------

\

- Build\
  \
  The pillar\'s floor and ceiling move up and down respectively until
  they meet at a parameter-specified height. If the specified height is
  0, the floor and ceiling will meet at a point directly half-way
  between their current heights. The speed parameter for this type
  specifies the speed of the surface which must move the greater
  distance. The other surface\'s speed will be set such that it arrives
  at the destination at the same time. No crushing damage is done by
  this type. If motion is blocked, both surfaces will wait until the
  obstruction is removed.\
  \
- Build and Crush\
  \
  This is exactly the same as build, but variable crushing damage will
  be inflicted on all objects that are blocking the pillar\'s motion.\
  \
- Open\
  \
  The pillar\'s floor and ceiling move apart (down and up respectively),
  opening a closed pillar. The distances that the floor and ceiling
  should move are specified separately as parameters. If the floor
  distance is zero, the floor will move to the lowest surrounding floor.
  If the ceiling distance is zero, the ceiling will move to the highest
  surrounding ceiling. The speed parameter specifies the speed of the
  surface which must move the greatest distance. The other surface\'s
  speed will be set such that it arrives at its destination at the same
  time. This type of action can only be performed on a sector which has
  the floor and ceiling heights equal.

[Return to Table of Contents](#contents)

[]{#elevlines}

------------------------------------------------------------------------

**Section 16.2 Pillar linedef types**

------------------------------------------------------------------------

\
There are currently three types of parameterized pillar specials, each
performing a different type of pillar action. See the [Parameterized
Pillar Types](#parampillars) section for full information on how to use
these specials.


    Parameterized Stair Types
    -------------------------------------------------------------------------------
    #      Class   Function                           ExtraData Name

    362    Param   Build pillar                       Pillar_Build
    363    Param   Build pillar and crush             Pillar_BuildAndCrush
    364    Param   Open pillar                        Pillar_Open

[Return to Table of Contents](#contents)

[]{#attached}

------------------------------------------------------------------------

**Section 17. Attached Surfaces**

------------------------------------------------------------------------

\
Attached surfaces cause multiple sectors to move together. This allows
for the creation of complex lifts, doors, or elevators comprised of
multiple sectors which will remain synchronized even if any sector\'s
movement is blocked by a player or other object.\
\
Attached surface linedefs are as follows: \* 379 \--
Attach_SetCeilingControl The sector on the front side of this line will
have its ceiling used to control the movement of the attached surfaces
that share this line\'s tag number. \* 380 \-- Attach_SetFloorControl
The sector on the front side of this line will have its floor used to
control the movement of the attached surfaces that share this line\'s
tag. \* 381 \-- Attach_FloorToControl The sector on the front side of
this line will have its floor movement controlled by the surface whose
control line shares this line\'s tag. \* 382 \-- Attach_CeilingToControl
The sector on the front side of this line will have its ceiling movement
controlled by the surface whose control line shares this line\'s tag. \*
383 \-- Attach_MirrorFloorToControl The sector on the front side of this
line will have its floor movement controlled inversely by the surface
whose control line shares this line\'s tag. \* 384 \--
Attach_MirrorCeilingToControl The sector on the front side of this line
will have its ceiling movement controlled inversely by the surface whose
control line shares this line\'s tag. [Return to Table of
Contents](#contents)

[]{#genlines}

------------------------------------------------------------------------

**Section 18. Detailed Generalized Linedef Specification**

------------------------------------------------------------------------

\
BOOM has added generalized linedef types that permit the parameters of
linedef actions to be nearly independently chosen.\
\
To support these, DETH has been modified to support them via dialogs for
each generalized type; each dialog allows the parameters for that type
to be independently specified by choice from a array of radio buttons. A
parser has also been added to support reading back the function of a
generalized linedef from its type number.\
\
NOTE to wad authors:\
\
This requires a lot of type numbers, 20608 in all so far. Some editors
may object to the presence of these new types thru assuming that a
lookup table of size 256 suffices (or some other reason). For those that
must continue to use such an editor, it may be necessary to stick to the
old linedef types, which are still present.\
\
We are also providing command line tools to set these linedef types,
independent of which editor you happen to use, but they are a lot less
slick, and more difficult to use than DETH. See TRIGCALC.EXE and
CLED.EXE in the EDITUTIL.ZIP package of the BOOM distribution.\
\


    == Generalized Linedef Ranges =======================================

    There are types for Floors, Ceilings, Doors, Locked Doors, Lifts,
    Stairs, and Crushers. The allocation of linedef type field numbers is 
    according to the following table:

    Type            Start      Length   (Dec)
    -----------------------------------------------------------------
    Floors          0x6000     0x2000  (8192)
    Ceilings        0x4000     0x2000  (8192)
    Doors           0x3c00     0x0400  (1024)
    Locked Doors    0x3800     0x0400  (1024)
    Lifts           0x3400     0x0400  (1024)
    Stairs          0x3000     0x0400  (1024)
    Crushers        0x2F80     0x0080   (128)
    -----------------------------------------------------------------
    Totals:  0x2f80-0x7fff     0x5080  (20608)

    ======================================================================

The following sections define the placement and meaning of the bit
fields within each linedef category. Fields in the description are
listed in increasing numeric order.\
\
Some nomenclature:\
\
Target height designations:\
\
H means highest, L means lowest, N means next, F means floor, C means
ceiling, n means neighbor, Cr means crush, sT means shortest lower
texture.\
\
Texture change designations:\
\


    c0n change texture, change sector type to 0, numeric model change
    c0t change texture, change sector type to 0, trigger model change
    cTn change texture only, numeric model change
    cTt change texture only, trigger model change
    cSn change texture and sector type to model's, numeric model change
    cSt change texture and sector type to model's, trigger model change

A trigger model change uses the sector on the first side of the trigger
for its model. A numeric model change looks at the sectors adjoining the
tagged sector at the target height, and chooses the one across the
lowest numbered two sided line for its model. If no model exists, no
change occurs. Note that in DOOM II v1.9, no model meant an illegal
sector type was generated.


    ------------------------------------------------------------------
    generalized floors (8192 types)

    field   description                       NBits   Mask     Shift
    ------------------------------------------------------------------

    trigger W1/WR/S1/SR/G1/GR/D1/DR            3     0x0007      0
    speed   slow/normal/fast/turbo             2     0x0018      3
    model   trig/numeric -or- nomonst/monst    1     0x0020      5
    direct  down/up                            1     0x0040      6
    target  HnF/LnF/NnF/LnC/C/sT/24/32         3     0x0380      7
    change  nochg/zero/txtonly/type            2     0x0c00      10
    crush   no/yes                             1     0x1000      12

    DETH Nomenclature:

    W1[m] F->HnF DnS [c0t] [Cr]
    WR[m] F->LnF DnN [c0n] 
    S1[m] F->NnF DnF [cTt]  
    SR[m] F->LnC DnT [cTn]          
    G1[m] F->C   UpS [cSt]          
    GR[m] FbysT  UpN [cSn]          
    D1[m] Fby24  UpF                        
    DR[m] Fby32  UpT                        

    Notes:

    1) When change is nochg, model is 1 when monsters can activate trigger
       otherwise monsters cannot activate it.
    2) The change fields mean the following:
       nochg   - means no texture change or type change
       zero    - means sector type is zeroed, texture copied from model
       txtonly - means sector type unchanged, texture copied from model
       type    - means sector type and floor texture are copied from model
    3) down/up specifies the "normal" direction for moving. If the
       target specifies motion in the opposite direction, motion is instant.
       Otherwise it occurs at speed specified by speed field.
    4) Speed is 1/2/4/8 units per tic
    5) If change is nonzero then model determines which sector is copied.
       If model is 0 its the sector on the first side of the trigger.
       if model is 1 (numeric) then the model sector is the sector at
       destination height on the opposite side of the lowest numbered
       two sided linedef around the tagged sector. If it doesn't exist
       no change occurs.

    ------------------------------------------------------------------
    generalized ceilings (8192 types)

    field   description                        NBits   Mask     Shift
    ------------------------------------------------------------------

    trigger W1/WR/S1/SR/G1/GR/D1/DR             3     0x0007      0
    speed   slow/normal/fast/turbo              2     0x0018      3
    model   trig/numeric -or- nomonst/monst     1     0x0020      5
    direct  down/up                             1     0x0040      6
    target  HnC/LnC/NnC/HnF/F/sT/24/32          3     0x0380      7
    change  nochg/zero/txtonly/type             2     0x0c00      10
    crush   no/yes                              1     0x1000      12

    DETH Nomenclature:

    W1[m] C->HnC DnS [Cr] [c0t] 
    WR[m] C->LnC DnN      [c0n] 
    S1[m] C->NnC DnF      [cTt] 
    SR[m] C->HnF DnT      [cTn]     
    G1[m] C->F   UpS      [cSt]     
    GR[m] CbysT  UpN      [cSn]     
    D1[m] Cby24  UpF                        
    DR[m] Cby32  UpT                        

    Notes:

    1) When change is nochg, model is 1 when monsters can activate trigger
       otherwise monsters cannot activate it.
    2) The change fields mean the following:
       nochg   - means no texture change or type change
       zero    - means sector type is zeroed, texture copied from model
       txtonly - means sector type unchanged, texture copied from model
       type    - means sector type and ceiling texture are copied from model
    3) down/up specifies the "normal" direction for moving. If the
       target specifies motion in the opposite direction, motion is instant.
       Otherwise it occurs at speed specified by speed field.
    4) Speed is 1/2/4/8 units per tic
    5) If change is nonzero then model determines which sector is copied.
       If model is 0 its the sector on the first side of the trigger.
       if model is 1 (numeric) then the model sector is the sector at
       destination height on the opposite side of the lowest numbered
       two sided linedef around the tagged sector. If it doesn't exist
       no change occurs.

    ------------------------------------------------------------------
    generalized doors (1024 types)

    field   description                       NBits    Mask     Shift
    ------------------------------------------------------------------

    trigger W1/WR/S1/SR/G1/GR/D1/DR            3      0x0007      0
    speed   slow/normal/fast/turbo             2      0x0018      3
    kind    odc/o/cdo/c                        2      0x0060      5
    monster n/y                                1      0x0080      7
    delay   1/4/9/30 (secs)                    2      0x0300      8

    DETH Nomenclature:

    W1[m] OpnD{1|4|9|30}Cls S
    WR[m] Opn               N
    S1[m] ClsD{1|4|9|30}Opn F
    SR[m] Cls               T
    G1[m]
    GR[m]
    D1[m]
    DR[m]

    Notes:

    1) The odc (Open, Delay, Close) and cdo (Close, Delay, Open) kinds use
       the delay field. The o (Open and Stay) and c (Close and Stay) kinds 
       do not.
    2) The precise delay timings in gametics are: 35/150/300/1050
    3) Speed is 2/4/8/16 units per tic

    ------------------------------------------------------------------
    generalized locked doors (1024 types)

    field   description                       NBits    Mask     Shift
    ------------------------------------------------------------------

    trigger W1/WR/S1/SR/G1/GR/D1/DR            3      0x0007      0
    speed   slow/normal/fast/turbo             2      0x0018      3
    kind    odc/o                              1      0x0020      5
    lock    any/rc/bc/yc/rs/bs/ys/all          3      0x01c0      6
    sk=ck   n/y                                1      0x0200      9

    DETH Nomenclature:

    W1[m] OpnD{1|4|9|30}Cls S Any
    WR[m] Opn               N R{C|S|K}
    S1[m]                   F B{C|S|K}
    SR[m]                   T Y{C|S|K}
    G1[m]                     All{3|6}
    GR[m]
    D1[m]
    DR[m]

    Notes:

    1) Delay for odc kind is constant at 150 gametics or about 4.333 secs
    2) The lock field allows any key to open a door, or a specific key to
       open a door, or all keys to open a door.
    3) If the sk=ck field is 0 (n) skull and cards are different keys,
       otherwise they are treated identically. Hence an "all" type door
       requires 3 keys if sk=ck is 1, and 6 keys if sk=ck is 0.
    4) Speed is 2/4/8/16 units per tic

    -------------------------------------------------------------------
    generalized lifts (1024 types)

    field   description                       NBits    Mask     Shift
    -------------------------------------------------------------------

    trigger W1/WR/S1/SR/G1/GR/D1/DR            3      0x0007      0
    speed   slow/normal/fast/turbo             2      0x0018      3
    monster n/y                                1      0x0020      5
    delay   1/3/5/10 (secs)                    2      0x00c0      6
    target  LnF/NnF/LnC/LnF<->HnF(perp.)       2      0x0300      8

    DETH Nomenclature:

    W1[m] Lft  F->LnFD{1|3|5|10}    S
    WR[m]      F->NnFD{1|3|5|10}    N
    S1[m]      F->LnCD{1|3|5|10}    F
    SR[m]      HnF<->LnFD{1|3|5|10} T
    G1[m]
    GR[m]
    D1[m]
    DR[m]

    Notes:

    1) The precise delay timings in gametics are: 35/105/165/350
    2) Speed is 1/2/4/8 units per tic
    3) If the target specified is above starting floor height, or does not
       exist the lift does not move when triggered. NnF is Next Lowest
       Neighbor Floor.
    4) Starting a perpetual lift between lowest and highest neighboring floors
       locks out all other actions on the sector, even if it is stopped with
       the non-extended stop perpetual floor function.

    -------------------------------------------------------------------
    generalized stairs (1024 types)

    field   description                       NBits    Mask     Shift
    -------------------------------------------------------------------

    trigger W1/WR/S1/SR/G1/GR/D1/DR            3      0x0007      0
    speed   slow/normal/fast/turbo             2      0x0018      3
    monster n/y                                1      0x0020      5
    step    4/8/16/24                          2      0x00c0      6
    dir     dn/up                              1      0x0100      8
    igntxt  n/y                                1      0x0200      9

    DETH Nomenclature:

    W1[m] Stair Dn s4  S [Ign]
    WR[m]       Up s8  N
    S1[m]          s16 F
    SR[m]          s24 T
    G1[m]
    GR[m]
    D1[m]
    DR[m]

    Notes:

    1) Speed is .25/.5/2/4 units per tic
    2) If igntxt is 1, then the staircase will not stop building when
       a step does not have the same texture as the previous.
    3) A retriggerable stairs builds up and down alternately on each
       trigger.

    -------------------------------------------------------------------
    generalized crushers (128 types)

    field   description                       NBits    Mask     Shift
    -------------------------------------------------------------------

    trigger W1/WR/S1/SR/G1/GR/D1/DR            3      0x0007      0
    speed   slow/normal/fast/turbo             2      0x0018      3
    monster n/y                                1      0x0020      5
    silent  n/y                                1      0x0040      6

    DETH Nomenclature:

    W1[m] Crusher S [Silent]
    WR[m]         N
    S1[m]         F
    SR[m]         T
    G1[m]
    GR[m]
    D1[m]
    DR[m]

    Notes:

    1) Speed is 1/2/4/8 units per second, faster means slower damage as usual.
    2) If silent is 1, the crusher is totally quiet, no start/stop sounds

[Return to Table of Contents](#contents)

[]{#paramlines}

------------------------------------------------------------------------

**Section 19. Detailed Parameterized Linedef Specification**

------------------------------------------------------------------------

\


    Parameterized Floor Types
    ------------------------------------------------------------------------------
    #    Function                           ExtraData Name

    306  Up to Highest Neighbor Floor       Floor_RaiseToHighest
    307  Down to Highest Neighbor Floor     Floor_LowerToHighest
    308  Up to Lowest Neighbor Floor        Floor_RaiseToLowest
    309  Down to Lowest Neighbor Floor      Floor_LowerToLowest
    310  Up to Next Neighbor Floor          Floor_RaiseToNearest
    311  Down to Next Neighbor Floor        Floor_LowerToNearest
    312  Up to Lowest Neighbor Ceiling      Floor_RaiseToLowestCeiling
    313  Down to Lowest Neighbor Ceiling    Floor_LowerToLowestCeiling
    314  Up to Ceiling                      Floor_RaiseToCeiling
    315  Up Abs Shortest Lower Texture      Floor_RaiseByTexture
    316  Down Abs Shortest Lower Texture    Floor_LowerByTexture
    317  Up Absolute Param                  Floor_RaiseByValue
    318  Down Absolute Param                Floor_LowerByValue
    319  To Absolute Height                 Floor_MoveToValue
    320  Up Absolute Param, Instant         Floor_RaiseInstant
    321  Down Absolute Param, Instant       Floor_LowerInstant
    322  To Ceiling Instant                 Floor_ToCeilingInstant

    Parameters
    ---------------------------------------------------------------
    Floor_RaiseToHighest         tag, speed, change, crush
    Floor_LowerToHighest         tag, speed, change
    Floor_RaiseToLowest          tag, change, crush
    Floor_LowerToLowest          tag, speed, change
    Floor_RaiseToNearest         tag, speed, change, crush
    Floor_LowerToNearest         tag, speed, change
    Floor_RaiseToLowestCeiling   tag, speed, change, crush
    Floor_LowerToLowestCeiling   tag, speed, change
    Floor_RaiseToCeiling         tag, speed, change, crush
    Floor_RaiseByTexture         tag, speed, change, crush
    Floor_LowerByTexture         tag, speed, change
    Floor_RaiseByValue           tag, speed, height, change, crush
    Floor_LowerByValue           tag, speed, height, change
    Floor_MoveToValue            tag, speed, height, change, crush
    Floor_RaiseInstant           tag, height, change, crush
    Floor_LowerInstant           tag, height, change
    Floor_ToCeilingInstant       tag, change, crush

    Values
    ---------------------------------------------------------------
    tag    : Tag of the sector(s) to affect. A tag of zero means to
             affect the sector on the second side of the line.
    speed  : Speed of floor in eights of a unit per tic.
    change : This parameter takes the following values:
             0 : No texture or type change.
             1 : Copy texture, zero type; trigger model.
             2 : Copy texture, zero type; numeric model.
             3 : Copy texture, preserve type; trigger model.
             4 : Copy texture, preserve type; numeric model.
             5 : Copy texture and type; trigger model.
             6 : Copy texture and type; numeric model.
    crush  : Amount of crushing damage floor inflicts at each
             crushing event (when gametic % 4 = 0). If this
             amount is less than or equal to zero, no crushing
             damage is done.
    height : An integer number of units, either the amount to 
             move the floor by or the exact z coordinate to move
             the floor toward. Negative numbers are valid for 
             the latter case.
    ------------------------------------------------------------------------------

    Parameterized Ceiling Types
    ------------------------------------------------------------------------------
    #    Function                           ExtraData Name

    323  Up to Highest Neighbor Ceiling     Ceiling_RaiseToHighest
    324  Up to HnC Instant                  Ceiling_ToHighestInstant
    325  Up to Nearest Neighbor Ceiling     Ceiling_RaiseToNearest
    326  Down to Nearest Neighbor Ceiling   Ceiling_LowerToNearest
    327  Up to Lowest Neighbor Ceiling      Ceiling_RaiseToLowest
    328  Down to Lowest Neighbor Ceiling    Ceiling_LowerToLowest
    329  Up to Highest Neighbor Floor       Ceiling_RaiseToHighestFloor
    330  Down to Highest Neighbor Floor     Ceiling_LowerToHighestFloor
    331  Down to Floor Instant              Ceiling_ToFloorInstant
    332  Down to Floor                      Ceiling_LowerToFloor
    333  Up Abs Shortest Upper Texture      Ceiling_RaiseByTexture
    334  Down Abs Shortest Upper Texture    Ceiling_LowerByTexture
    335  Up Absolute Param                  Ceiling_RaiseByValue
    336  Down Absolute Param                Ceiling_LowerByValue
    337  To Absolute Height                 Ceiling_MoveToValue
    338  Up Absolute Param, Instant         Ceiling_RaiseInstant
    339  Down Absolute Param, Instant       Ceiling_LowerInstant

    Parameters
    ---------------------------------------------------------------
    Ceiling_RaiseToHighest       tag, speed, change
    Ceiling_ToHighestInstant     tag, change, crush
    Ceiling_RaiseToNearest       tag, speed, change
    Ceiling_LowerToNearest       tag, speed, change, crush
    Ceiling_RaiseToLowest        tag, speed, change
    Ceiling_LowerToLowest        tag, speed, change, crush
    Ceiling_RaiseToHighestFloor  tag, speed, change
    Ceiling_LowerToHighestFloor  tag, speed, change, crush
    Ceiling_ToFloorInstant       tag, change, crush
    Ceiling_LowerToFloor         tag, speed, change, crush
    Ceiling_RaiseByTexture       tag, speed, change
    Ceiling_LowerByTexture       tag, speed, change, crush
    Ceiling_RaiseByValue         tag, speed, height, change
    Ceiling_LowerByValue         tag, speed, height, change, crush
    Ceiling_MoveToValue          tag, speed, height, change, crush
    Ceiling_RaiseInstant         tag, height, change
    Ceiling_LowerInstant         tag, height, change, crush

    Values
    ---------------------------------------------------------------
    tag    : Tag of the sector(s) to affect. A tag of zero means to
             affect the sector on the second side of the line.
    speed  : Speed of ceiling in eighths of a unit per tic.
    change : This parameter takes the following values:
             0 : No texture or type change.
             1 : Copy texture, zero type; trigger model.
             2 : Copy texture, zero type; numeric model.
             3 : Copy texture, preserve type; trigger model.
             4 : Copy texture, preserve type; numeric model.
             5 : Copy texture and type; trigger model.
             6 : Copy texture and type; numeric model.
    crush  : Amount of crushing damage ceiling inflicts at each
             crushing event (when gametic % 4 = 0). If this
             amount is less than or equal to zero, no crushing
             damage is done.
    height : An integer number of units, either the amount to 
             move the floor by or the exact z coordinate to move
             the floor toward. Negative numbers are valid for 
             the latter case.
    ------------------------------------------------------------------------------

    Parameterized Door Types
    ------------------------------------------------------------------------------
    #    Function                     ExtraData Name

    300  Open, Wait, Then Close       Door_Raise
    301  Open and Stay Open           Door_Open
    302  Close and Stay Closed        Door_Close
    303  Close, Wait, Then Open       Door_CloseWaitOpen
    304  Wait, Open, Wait, Close      Door_WaitRaise
    305  Wait, Close and Stay Closed  Door_WaitClose

    Parameters
    ---------------------------------------------------------------
    Door_Raise          tag, speed, delay, lighttag 
    Door_Open           tag, speed, lighttag
    Door_Close          tag, speed, lighttag
    Door_CloseWaitOpen  tag, speed, delay, lighttag
    Door_WaitRaise      tag, speed, delay, countdown, lighttag
    Door_WaitClose      tag, speed, countdown, lighttag

    Values
    ---------------------------------------------------------------
    tag       : Tag of the sector(s) to affect as a door. A tag of 
                zero means to affect the sector on the second side
                of the line.
    speed     : Speed of door in eights of a unit per tic.
    delay     : Delay time in tics.
    lighttag  : Tag used to determine what sector will receive the 
                BOOM dynamic door light effect when this door opens
                or closes.
    countdown : Delay before action occurs in tics.
    ------------------------------------------------------------------------------

    Parameterized Stair Types
    ------------------------------------------------------------------------------
    #    Function                            ExtraData Name

    340  Build stairs up, DOOM method        Stairs_BuildUpDoom
    341  Build stairs down, DOOM method      Stairs_BuildDownDoom
    342  Build stairs up sync*, DOOM method  Stairs_BuildUpDoomSync
    343  Build stairs dn sync*, DOOM method  Stairs_BuildDownDoomSync

    * Synchronized stairs have each step move at a different speed so that all the
      steps reach the destination height at the same time. This stair building
      style is only available with these parameterized line types.

    Parameters
    ---------------------------------------------------------------
    Stairs_BuildUpDoom        tag, speed, stepsize, delay, reset
    Stairs_BuildDownDoom      tag, speed, stepsize, delay, reset
    Stairs_BuildUpDoomSync    tag, speed, stepsize, reset
    Stairs_BuildDownDoomSync  tag, speed, stepsize, reset

    Values
    ---------------------------------------------------------------
    tag       : Tag of first sector to raise or lower as a step.
                When the stair building method is DOOM, subsequent
                steps are found by searching across two-sided
                lines where the first side belongs to the current
                sector and the floor texture is the same. The
                Hexen stair building method is not yet supported.
    speed     : Speed of steps in eights of a unit per tic. If the
                build style is synchronized, only the first step
                moves at this speed. Other steps will move at a
                speed which allows them to reach their 
                destination height at the same time.
    stepsize  : Step size in units.
    delay     : Delay time in tics between steps
    reset     : If greater than zero, the stairs will wait this
                amount of time in tics after completely building
                and will then reset by repeating the same stair
                building action but in the reverse direction. If
                zero, the stairs will never reset.
    ------------------------------------------------------------------------------

    Parameterized PolyObject Types
    ------------------------------------------------------------------------------
    #    Function                            ExtraData Name

    348  Marks first line in PolyObject*     Polyobj_StartLine
    349  Explicitly includes a line*         Polyobj_ExplicitLine
    350  Open PolyObject as sliding door     Polyobj_DoorSlide
    351  Open PolyObject as swinging door    Polyobj_DoorSwing
    352  Move PolyObject in xy plane         Polyobj_Move
    353  Move PolyObject w/override**        Polyobj_OR_Move
    354  Rotate PolyObject right             Polyobj_RotateRight
    355  Rotate PolyObject right w/override  Polyobj_OR_RotateRight
    356  Rotate PolyObject left              Polyobj_RotateLeft
    357  Rotate PolyObject left w/override   Polyobj_OR_RotateLeft

    * There are two different methods of creating a PolyObject. In the first,
      the "first" line is given the Polyobj_StartLine special, and subsequent
      lines are added to the PolyObject by following segs from vertex to vertex.
      To use this, all the lines around the PolyObject must run in the same
      direction, and the last line must end at the same vertex which starts the
      first line (in other words, it must be closed).
      
      With the second method, every line to be included must be given the
      Polyobj_ExplicitLine special and must be given a linenum param which is
      unique amongst all lines that will added to that polyobject. Polyobjects
      of this sort should also be closed for proper rendering, but the game
      engine cannot and will not verify that this is the case.
      
      Only PolyObjects created with the Polyobj_StartLine method can have other
      line specials on the PolyObject's lines themselves.

    ** Override types will take effect even if an action is already affecting the
      specified polyobject. Normal actions will fail in this case.

    Parameters
    ------------------------------------------------------------------
    Polyobj_StartLine       polyobj_id, mirror_id, sndseq_id
    Polyobj_ExplicitLine    polyobj_id, linenum, mirror_id, sndseq_id
    Polyobj_DoorSlide       polyobj_id, speed, angle, dist, delay
    Polyobj_DoorSwing       polyobj_id, aspeed, adist, delay
    Polyobj_Move            polyobj_id, speed, angle, dist
    Polyobj_OR_Move         polyobj_id, speed, angle, dist
    Polyobj_RotateRight     polyobj_id, aspeed, adist
    Polyobj_OR_RotateRight  polyobj_id, aspeed, adist
    Polyobj_RotateLeft      polyobj_id, aspeed, adist
    Polyobj_OR_RotateLeft   polyobj_id, aspeed, adist

    Values
    ---------------------------------------------------------------
    polyobj_id : ID number of the PolyObject of which this line is
                 a part. PolyObject ID numbers are defined in the
                 angle field of their EEPolyObjSpawnSpot object
                 (DoomEd numbers 9301 or 9302). Every PolyObject
                 must be given a valid unique ID number greater
                 than zero.
    mirror_id  : The ID number of a PolyObject that wants to mirror
                 every action that this PolyObject makes. This
                 number cannot refer to the self-same PolyObject,
                 nor should PolyObjects attempt to mirror each
                 other in a cycle since this will create problems
                 with override actions. Angle of motion is always
                 reversed when applying an action to a mirroring
                 PolyObject. If a mirror is already in motion for
                 a non-override action, the mirror will not be
                 affected even if the main object was moved. Mirror
                 polyobjects can themselves define a mirror, and
                 an action will affect all mirroring polyobjects
                 in the chain.
    sndseq_id  : Reserved for future use, currently has no effect.
                 Will be used to select the EDF sound sequence this
                 PolyObject uses when moving.
    linenum    : For Polyobj_ExplicitLine only, specifies the
                 order in which lines should be added to the
                 PolyObject. Each line must have a number greater
                 than zero which is unique within all lines that
                 belong to that PolyObject. Although, unlike Hexen,
                 there is no requirement that the numbers be in
                 consecutive order or that they start at 1.
    speed      : Linear speed of PolyObject motion in eighths of a
                 unit per tic.
    angle      : Byte angle of linear motion. *
    dist       : Total linear distance to move in units.
    delay      : Time to wait before closing in tics.
    aspeed     : Byte angle specifying speed in degrees per tic. *
    adist      : Byte angle specifying total angular distance to
                 rotate. *, **
    ---------------------------------------------------------------

    * Byte angles follow this system:
        0 =   0 degrees (East)
       64 =  90 degrees (North)
      128 = 180 degrees (West)
      192 = 270 degrees (South)
      
      To convert from degrees to byte angles, use the following calculation:
      
      byteangle = (degrees * 256) / 360
     
      Chop or round the result to an integer.
      
      Valid byte angles fall between 0 and 255, so adjust any angles less than
      0 or greater than or equal to 360 before performing this conversion.
      
    ** For the adist parameter to Polyobj_Rotate-type specials (excluding
      Polyobj_DoorSwing), the values 0 and 255 have the following special
      meanings:
      0   == The PolyObject will rotate exactly 360 degrees.
      255 == The PolyObject will rotate perpetually.
      All other byte angle values have their normal meaning.
    ------------------------------------------------------------------------------

    Parameterized Pillar Types
    ------------------------------------------------------------------------------
    #    Function                            ExtraData Name

    362  Build pillar                        Pillar_Build
    363  Build pillar and crush              Pillar_BuildAndCrush
    364  Open pillar                         Pillar_Open

    Parameters
    ---------------------------------------------------------------
    Pillar_Build              tag, speed, height
    Pillar_BuildAndCrush      tag, speed, height, crush
    Pillar_Open               tag, speed, fdist, cdist

    Values
    ---------------------------------------------------------------
    tag       : Tag of sector to close or open as a pillar. For the
                action to succeed, neither surface may have an 
                action running on it already. If the tag is zero,
                the sector on the backside of a 2S line with this
                special will be used.
    speed     : Speed of the surface with the greatest distance to
                move in eighths of a unit per tic. The other surface
                will move at a speed which causes it to reach its
                destination at the same time.
    height    : For Build and BuildAndCrush types, this is the 
                height relative to the floor where the floor and 
                ceiling should meet. If this value is zero, the 
                floor and ceiling will meet exactly half-way.
    crush     : For BuildAndCrush type only, this specifies the 
                amount of damage to inflict per crush event.
    fdist     : For Open type only, this is the distance the floor
                should move down. If this value is zero, the floor
                will move to its lowest neighboring floor.
    cdist     : For Open type only, this is the distance the
                ceiling should move up. If this value is zero, the
                ceiling will move to its highest neighboring 
                ceiling.          
    ------------------------------------------------------------------------------
     
    Parameterized Lighting Types
    ------------------------------------------------------------------------------
    #    Function                            ExtraData Name

    368  Raise light by value                Light_RaiseByValue
    369  Lower light by value                Light_LowerByValue
    370  Set light to value                  Light_ChangeToValue
    371  Fade light to value                 Light_FadeToValue
    372  Fade continuously between values    Light_Glow
    373  Flicker between levels randomly     Light_Flicker
    374  Flicker with specified periodicity  Light_Strobe

    Parameters
    ---------------------------------------------------------------------
    Light_RaiseByValue        tag, level
    Light_LowerByValue        tag, level
    Light_ChangeToValue       tag, level
    Light_Fade                tag, destlevel, speed
    Light_Glow                tag, maxlevel, minlevel, speed
    Light_Flicker             tag, maxlevel, minlevel
    Light_Strobe              tag, maxlevel, minlevel, maxtime, mintime

    Values
    ---------------------------------------------------------------
    tag       : Tag of sector(s) to change light levels within.            
                If the tag is zero, the sector on the backside of a 
                2S line with this special will be used.
    level     : A light level between 0 and 255.
    destlevel : A light level between 0 and 255.
    speed     : Speed of light level changes in levels per gametic.
    maxlevel  : The high light level of a perpetual fade or flicker.
    minlevel  : The low light level of a perpetual fade or flicker.
    maxtime   : Maximum amount of time in gametics between flickers.
    mintime   : Minimum amount of time in gametics between flickers.            

    ------------------------------------------------------------------------------

    Miscellaneous Parameterized Line Specials
    ------------------------------------------------------------------------------
    #    Function                            ExtraData Name

    365  Start ACS script                    ACS_Execute
    366  Suspend ACS script                  ACS_Suspend
    367  Terminate ACS script                ACS_Terminate
    375  Start earthquake                    Radius_Quake

    Parameters
    ---------------------------------------------------------------------
    ACS_Execute      scriptnum, mapnum, arg1, arg2, arg3
    ACS_Suspend      scriptnum, mapnum
    ACS_Terminate    scriptnum, mapnum

        scriptnum    = ACS script number to execute/suspend/terminate
        mapnum       = Map number on which to schedule action, or 0
                       for current map
        arg1 - arg3  = Arguments passed on to called script        
        
        
    Radius_Quake     intensity, duration, damageradius, quakeradius, tid

        intensity    = "Richter" scale value from 1 to 9 (out-of-range
                       values are allowed, but may cause HOM to appear).
        duration     = length of earthquake in gametics
        damageradius = if greater than 0, the number of 64-unit blocks
                       from the epicenter in which players will take 
                       damage and experience additional thrust
        quakeradius  = number of 64-unit blocks from the epicenter in
                       which players will experience view-shaking
        tid          = tid of mapthing(s) to use as earthquake epicenters                

    ------------------------------------------------------------------------------

[Return to Table of Contents](#contents)

[]{#part2}

------------------------------------------------------------------------

### Part II Sector Types

------------------------------------------------------------------------

\
BOOM is backward compatible with DOOM\'s sector types. All types 0-17
have the same meaning they did under DOOM. Types 18-31 are reserved for
extended sector types, that work like DOOM\'s and share the limitation
that only one type can be active in a sector at once. No extended sector
types are implemented at this time.

[]{#sectors}

------------------------------------------------------------------------

**Section 1. Regular Sector types**

------------------------------------------------------------------------

\
From Matt Fell\'s Unofficial DOOM Spec the DOOM sector types are:\
\


    Dec Hex Class   Description
    -------------------------------------------------------------------
     0  00  -       Normal, no special characteristic.
     1  01  Light   random off
     2  02  Light   blink 0.5 second
     3  03  Light   blink 1.0 second
     4  04  Both    -10/20% health AND light blink 0.5 second
     5  05  Damage  -5/10% health
     7  07  Damage  -2/5% health
     8  08  Light   oscillates
     9  09  Secret  a player must stand in this sector to get credit for
                      finding this secret. This is for the SECRETS ratio
                      on inter-level screens.
    10  0a  Door    30 seconds after level start, ceiling closes like a door.
    11  0b  End     -10/20% health. If a player's health is lowered to less
                      than 11% while standing here, then the level ends! Play
                      proceeds to the next level. If it is a final level
                      (levels 8 in DOOM 1, level 30 in DOOM 2), the game ends!
    12  0c  Light   blink 0.5 second, synchronized
    13  0d  Light   blink 1.0 second, synchronized
    14  0e  Door    300 seconds after level start, ceiling opens like a door.
    16  10  Damage  -10/20% health

    17  11  Light   flickers on and off randomly

Note that BOOM will NEVER exit the game on an illegal sector type, as
was the case with DOOM. The sector type will merely be ignored.\
\
[Return to Table of Contents](#contents)

[]{#gensectors}

------------------------------------------------------------------------

**Section 2. Generalized sector types**

------------------------------------------------------------------------

\
BOOM also provides generalized sector types, based on bit fields, that
allow several sector type properties to be independently specified for a
sector. Texture change linedefs can be used to switch some or all of
these properties dynamically, outside lighting.\
\
Bits 0 thru 4 specify the lighting type in the sector, the same codes
that DOOM used are employed:\
\


    Dec Bits 4-0   Description
    -------------------------------------------------------------------
    0   00000      Normal lighting
    1   00001      random off
    2   00010      blink 0.5 second
    3   00011      blink 1.0 second
    4   00100      -10/20% health AND light blink 0.5 second
    8   01000      oscillates
    12  01100      blink 0.5 second, synchronized
    13  01101      blink 1.0 second, synchronized
    17  10001      flickers on and off randomly

Bits 5 and 6 set the damage type of the sector, with the usual 5/10/20
damage units per second.\
\


    Dec Bits 6-5   Description
    -------------------------------------------------------------------
    0   00         No damage
    32  01         5  units damage per sec (halve damage in TYTD skill)
    64  10         10 units damage per sec
    96  11         20 units damage per sec

Bit 7 makes the sector count towards the secrets total at game end\
\


    Dec Bit 7      Description
    -------------------------------------------------------------------
    0    0         Sector is not secret
    128  1         Sector is secret

Bit 8 enables the ice/mud effect controlled by linedef 223\
\


    Dec Bit 8      Description
    -------------------------------------------------------------------
    0    0         Sector friction disabled
    256  1         Sector friction enabled

Bit 9 enables the wind effects controlled by linedefs 224-226\
\


    Dec Bit 9      Description
    -------------------------------------------------------------------
    0    0         Sector wind disabled
    512  1         Sector wind enabled

=== New to SMMU ===\
\
Bits 10 and 11 are now implemented as of SMMU v3.21.\
Bit 10 suppresses all sounds within the sector, while Bit 11 disables
any sounds due to floor or ceiling motion by the sector. Bit 11 is
especially useful in silencing constructs such as pseudo-3D bridges,
which give off an undesirable plat sound when they move.\
\


    Dec Bit 10     Description
    -------------------------------------------------------------------
    0    0         Sounds made in sector function normally
    1024 1         Sounds made in sector are suppressed

    Dec Bit 11     Description
    -------------------------------------------------------------------
    0    0         Sounds made by sector floor/ceiling movement normal
    2048 1         Sounds made by sector floor/ceiling movement suppressed

[Return to Table of Contents](#contents)

[]{#part3}

------------------------------------------------------------------------

### Part III Linedef Flags

------------------------------------------------------------------------

\
Only one new linedef flag is implemented in BOOM, called PassThru, which
allows a push or switch linedef trigger to pass the push action thru it
to ones within range behind it. In this way BOOM is capable of setting
off many actions with a single push. Note that the limitation of one
floor action, one ceiling action, and one lighting action per sector
affect still applies however.\
The new linedef flag is bit 9, value 512, in the linedef flags word.\
\


    Dec Bit 9      Description
    -------------------------------------------------------------------
    0    0         Line absorbs all push actions as normal
    512  1         Line passes push actions through to lines behind it

=== New to Eternity v3.31 ===\
\
Eternity, starting with v3.31, implements a new line flag called
3DMidTex, which causes 2S linedefs using it to clip objects using the
height of their middle texture. Within the range of the texture\'s
height, the lines are effectively solid and block players, monsters, and
projectiles. Below or above, objects can pass freely. This enables very
easy pseudo-3D effects.\
The new linedef flag is bit 10, value 1024, in the linedef flags word.\
\


    Dec Bit 10     Description
    -------------------------------------------------------------------
    0    0         Linedef is normal with respect to clipping
    1024 1         2S linedef clips objects with respect to mid texture

=== New to Eternity v3.33.33 ===\
\
As of Eternity Engine v3.33.33, line flag #11 with decimal value 2048 is
considered reserved. If this bit is set on a line, all of its extended
line flags from BOOM or later will be set to zero. This is required to
repair erroneous maps such as Ultimate DOOM\'s E2M7.\
\


    Dec Bit 11     Description
    -------------------------------------------------------------------
    0    0         Extended flags are normal
    2048 1         Extended flags are cleared

[Return to Table of Contents](#contents)

[]{#part4}

------------------------------------------------------------------------

### Part IV Thing Flags

------------------------------------------------------------------------

\
BOOM has implemented two new thing flags, \"not in DM\" and \"not in
COOP\", which in combination with the existing \"not in Single\" flag,
usually called \"Multiplayer\", allow complete control over a level\'s
inventory and monsters, in any game mode.\
\
If you want a thing to be only available in Single play, you set both
the \"not in DM\" and \"not in COOP\" flags. Other combinations are
similar.\
\
\"not in DM\" is bit 5, value 32, in the thing flags word.\
\"not in COOP\" is bit 6, value 64, in the thing flags word.\
\
=== New to MBF ===\
\
MBF introduces one new mapthing flag\
\
\"friendly\" is bit 7, value 128, in the thing flags word.\
\
This flag may be placed on any object, but will only have an effect on
sentient enemies. When given this flag, a thing will use MBF\'s friendly
entity logic, and will help the player to defeat enemies in the level.\
\
To use this flag, you must manually add the value 128 to the thing\'s
flags using either DETH\'s \"enter value\" menu, or a tool such as CLED
which can manipulate any level data field.\
\
Bit 8, value 256, has been reserved for detecting editors which set
flags they do not understand. It should not be set or else all extended
flags will be cleared.\
\
=== New to Eternity ===\
\
Eternity introduces one new mapthing flag\
\
\"dormant\" is bit 9, value 512, in the thing flags word.\
\
Things given this flag are invincible and will remain in their spawn
state until a script calls the objawaken function on them, from which
point onward they act normally.\
\
[Return to Table of Contents](#contents)

[]{#part5}

------------------------------------------------------------------------

### Part V Thing Types

------------------------------------------------------------------------

\
BOOM has implemented two new thing types as well, **BoomPushPoint**
(5001), and **BoomPullPoint** (5002). These control the origin of the
point source wind effect controlled by linedef types 225 and 226, and
set whether the wind blows towards or away from the origin depending on
which is used.\
\
=== New to MBF ===\
\
MBF introduces the following new mapthing types:\
\
**MBFHelperDog** (DoomEd #888, DeHackEd #140)\
A dog similar to the one found in Wolfenstein. This dog is the one
normally spawned with the player when the -dogs command-line option is
used. It is not friendly by default, but must be given the \"friendly\"
thing flag if you wish to make it so. Otherwise it will act as a normal
enemy.\
\
=== New to SMMU ===\
\
SMMU introduces the following new mapthing types:\
\
**SMMUCameraSpot** (5003)\
A general-purpose camera place-holder. These cameras will be randomly
chosen during \"cool demo playback\" and for a view of the level during
the intermission.\
\
=== New to Eternity ===\
\
Eternity introduces the following new mapthing types. Note that this
list does not currently contain any Heretic mapthings, which may be
documented here or elsewhere in the future. Note that there are also
some mapthing types within the engine which are not currently
documented. Maps should refrain from using those things until they are
made official, as they could change or be deleted later.\
\
**EEEnviroSequence** (1200 - 1300)\
This group of objects are all translated into EEEnviroSequence with
DoomEdNum 1300. These objects are used to add environmental sound
sequences to the environmental sequence manager. For types 1200 through
1299, the sequence number to add is type - 1200. For type 1300, the
sequence number is the value in args\[0\] as specified via ExtraData or
the Hexen map format.\
\
**EESectorSequence** (1400 - 1500)\
This group of objects are all translated into EESectorSequence with
DoomEdNum 1500. These objects are used to assign sound sequences to
sectors by number. For types 1400 through 1499, the sequence number is
type - 1400. For type 1500, the sequence number is the value in
args\[0\] as specified via ExtraData or the Hexen map format. For type
1500, if args\[0\] is 0, the sector is reset to default sound sequence
behavior, even if the MapInfo variable \"noautosequences\" has been set
to \"true\".\
\
**ExtraData Thing Control Object** (5004)\
This doomednum is used to reference an ExtraData mapthing record, the
number of which must be placed in the options field of this mapthing.
There is no EDF thingtype which corresponds to the ExtraData control
point; it is only used during map startup to spawn the appropriate
ExtraData-defined thing at the location and angle of this mapthing. In
the cases that an ExtraData control object references an invalid
ExtraData mapthing record, attempts to spawn another ExtraData control
object, or references an unknown EDF thingtype mnemonic, the \"Unknown\"
object described below will be spawned as a placeholder. If an EDF
thingtype is used which has a doomednum of -1, no thing will be spawned
at all. See the [ExtraData Reference](extradata.html) for full
information.\
\
**Unknown** (5005)\
This thing type is used as a placeholder by EDF, ExtraData, and
scripting. It may be spawned when an unknown thing type is requested,
and its definition is required by the EDF language, so it is guaranteed
to always exist. It uses DoomEd #5005 so that it can be used by
ExtraData, but it will not generally be useful in maps.\
\
**EESkyboxCam** (5006)\
This thing type determines the origin for rendering of a skybox portal.
See the [Skybox Portal](#skybxportal) documentation for complete
information.\
\
**EEParticleDrip** (5007)\
A parameterized particle drip will be spawned at this mapthing\'s
location with properties determined by the mapthing\'s ExtraData/Hexen
arguments list. See the particlefx field documentation in the [EDF
Documentation](edf_ref.html) for more information on thingtype particle
effects, and see the [ExtraData Documentation](extradata.html) for full
information on the ExtraData system.\
\
**EEMapSpot** (9001)\
This is a Hexen/zdoom-compatible mapspot for scripting purposes. It is
especially handy for playing sounds or spawning objects at a particular
location without needing to remember the location\'s x/y/z coordinates.\
\
**EEMapSpotGravity** (9013)\
This object is exactly the same as EEMapSpot, but it will fall under
gravity, allowing it to move up or down with sector floors/ceilings.\
\
**EEParticleFountain** (9027 - 9033)\
A particle fountain will be spawned at the mapthing\'s location. The
fountain color depends on the mapthing number used, as follows:\

- 9027: Red\
- 9028: Green\
- 9029: Blue\
- 9030: Yellow\
- 9031: Purple\
- 9032: Black\
- 9033: White

**EEPolyObjAnchor** (9300)\
This is a Hexen/zdoom-compatible PolyObject anchor spot. Each PolyObject
defined in a map must have one and only one of these objects, and the
object\'s angle field must be set to the PolyObject\'s integer id
number. This spot determines the point relative to which lines will be
translated and rotated.\
\
**EEPolyObjSpawnSpot** (9301)\
This is a Hexen/zdoom-compatible PolyObject spawn spot. Each PolyObject
defined in a map must have one and only one of these objects, and the
object\'s angle field must be set to the PolyObject\'s integer id
number. This spot determines the exact location at which the PolyObject
will start at the beginning of play on the map. The anchor spot will be
translated exactly to this point, and all lines in the PolyObject are
translated relative to it.\
\
**EEPolyObjSpawnSpotCrush** (9302)\
Exactly the same as EEPolyObjSpawnSpot, but PolyObjects which use this
object instead will inflict damage upon shootable things which block the
course of their movement at a rate of 3 HP/gametic (105 damage per
second).\
\
**EEAmbience** (14001 - 14065)\
This group of objects are all translated into EEAmbience with DoomEdNum
14065. These objects define control points that utilize EDF ambience
definitions. For types 14001 through 14064, the ambience index used by
that object is type - 14000. For type 14065, the ambience index used by
the object is the value in args\[0\] as specified via ExtraData or the
Hexen map format.\
\
[Return to Table of Contents](#contents)

[]{#part6}

------------------------------------------------------------------------

### Part VI BOOM Predefined Lumps

------------------------------------------------------------------------

\
[Most predefined lumps used in BOOM and MBF are still present in
Eternity, although not all are used. These lumps can be found in
eternity.wad and are no longer stored inside the executable, primarily
for reasons of code size and compile time.\
]{#lumpnames}\


    Menu text
    ---------
    M_KEYBND       Key Bindings entry in setup menu
    M_AUTO         Automap entry in setup menu
    M_CHATM        Chat strings entry in setup menu (Replaces M_CHAT, New to SMMU)
    M_ENEM         Enemies options entry in setup menu
    M_STAT         Status bar and Hud entry in setup menu
    M_WEAP         Weapons entry in setup menu
    M_COLORS       Palette color picker diagram
    M_PALNO        Xed-out symbol for unused automap features
    M_BUTT1        Setup reset button off state (currently unused)
    M_BUTT2        Setup reset button on state (currently unused)
    M_LDSV         Load/Save menu graphic (new to SMMU)
    M_FEAT         Features menu graphic (new to SMMU)
    M_MULTI        Multiplayer options menu graphic (new to SMMU)
    M_ABOUT        About menu graphic (new to SMMU)
    M_DEMOS        Demos menu graphic (new to SMMU)
    M_WAD          Wad loading menu graphic (new to SMMU)
    M_SOUND        Sound options menu graphic (new to SMMU)
    M_VIDEO        Video options menu graphic (new to SMMU)
    M_MOUSE        Mouse options menu graphic (new to SMMU)
    M_HUD          Heads-up options menu graphic (new to SMMU)
    M_TCPIP        TCP/IP connect menu graphic (new to SMMU)
    M_SERIAL       Serial connect menu graphic (new to SMMU)
    M_PLAYER       Player options menu graphic (new to SMMU)
    M_COMPAT       Compatibility options menu graphic (new to MBF)
    M_SLIDEM       Small slider gfx, middle segment (new to SMMU)
    M_SLIDEL       Small slider gfx, left end (new to SMMU)
    M_SLIDEO       Small slider gfx, caret (new to SMMU)
    M_SLIDER       Small slider gfx, right end (new to SMMU)
    MBFTEXT        Marine's Best Friend menu graphic (new to MBF, currently unused)
    M_ETCOPT       Eternity options menu graphic (new to Eternity)

    Font chars
    ----------
    STCFN096       ` character for standard DOOM font
    STCFN123       left end of console horizontal divider bar  (new to SMMU)
    STCFN124       center of console horizontal divider bar    (new to SMMU)
    STCFN125       right end of console horizontal divider bar (new to SMMU)

    STBR123        HUD font bargraph char, full, 4 vertical bars
    STBR124        HUD font bargraph char, partial, 3 vertical bars
    STBR125        HUD font bargraph char, partial, 2 vertical bars
    STBR126        HUD font bargraph char, partial, 1 vertical bars
    STBR127        HUD font bargraph char, empty, 0 vertical bars

    DIG0         HUD font 0 char
    DIG1         HUD font 1 char
    DIG2         HUD font 2 char
    DIG3         HUD font 3 char
    DIG4         HUD font 4 char
    DIG5         HUD font 5 char
    DIG6         HUD font 6 char
    DIG7         HUD font 7 char
    DIG8         HUD font 8 char
    DIG9         HUD font 9 char
    DIGA         HUD font A char
    DIGB         HUD font B char
    DIGC         HUD font C char
    DIGD         HUD font D char
    DIGE         HUD font E char
    DIGF         HUD font F char
    DIGG         HUD font G char
    DIGH         HUD font H char
    DIGI         HUD font I char
    DIGJ         HUD font J char
    DIGK         HUD font K char
    DIGL         HUD font L char
    DIGM         HUD font M char
    DIGN         HUD font N char
    DIGO         HUD font O char
    DIGP         HUD font P char
    DIGQ         HUD font Q char
    DIGR         HUD font R char
    DIGS         HUD font S char
    DIGT         HUD font T char
    DIGU         HUD font U char
    DIGV         HUD font V char
    DIGW         HUD font W char
    DIGX         HUD font X char
    DIGY         HUD font Y char
    DIGZ         HUD font Z char
    DIG45        HUD font - char
    DIG47        HUD font / char
    DIG58        HUD font : char
    DIG91        HUD font [ char
    DIG93        HUD font ] char

    Status bar
    ----------
    STKEYS6        Both blue keys graphic
    STKEYS7        Both yellow keys graphic
    STKEYS8        Both red keys graphic

    Box chars
    ---------
    BOXUL        Upper left corner of box
    BOXUC        Center of upper border of box
    BOXUR        Upper right corner of box
    BOXCL        Center of left border of box
    BOXCC        Center of box
    BOXCR        Center of right border of box
    BOXLL        Lower left corner of box
    BOXLC        Center of lower border of box
    BOXLR        Lower right corner of box

    Misc HUD Graphics
    -----------------
    HU_FRAGS     Frags display header graphic (new to SMMU)
    HU_FRAGBX    Frags display name plate graphic (new to SMMU)
    CROSS1       + crosshair graphic (new to SMMU)
    CROSS2       Angle crosshair graphic (new to SMMU)
    VPO          Visplane overflow indicator (new to SMMU)
    OPENSOCK     Network connection error indicator (new to SMMU)

    Misc Graphics
    -------------
    UDTTL00      Hires Ultimate DOOM title screen, upper left (new to SMMU)
    UDTTL01      Hires Ultimate DOOM title screen, lower left (new to SMMU)
    UDTTL10      Hires Ultimate DOOM title screen, upper rght (new to SMMU)
    UDTTL11      Hires Ultimate DOOM title screen, lower rght (new to SMMU)
    D2TTL00      Hires DOOM II title screen, upper left (new to SMMU)
    D2TTL01      Hires DOOM II title screen, lower left (new to SMMU)
    D2TTL10      Hires DOOM II title screen, upper rght (new to SMMU)
    D2TTL11      Hires DOOM II title screen, lower rght (new to SMMU)
    DLGBACK      Default dialog background graphic (new to Eternity)

    Lumps missing in v1.1
    ---------------------
    STTMINUS       minus sign for neg frags in status bar
    WIMINUS        minus sign for neg frags on end screen
    M_NMARE        nightmare skill menu string
    STBAR          status bar background
    DSGETPOW       sound for obtaining power-up

    Sprites
    -------
    TNT1A0         Invisible sprite for push/pull controller things
    DOGS*          MBF helper dog sprite (new to MBF)
    PLS1*          BFG 2704 plasma 1 sprite (new to MBF)
    PLS2*          BFG 2704 plasma 2 sprite (new to MBF)
    SLDG*          FLOOR_SLUDGE TerrainType fx sprite (new to Eternity)
    LVAS*          FLOOR_LAVA TerrainType fx sprite (new to Eternity)
    SPSH*          FLOOR_WATER TerrainType fx sprite (new to Eternity)

    Misc Sounds
    -----------
    DSDGSIT        MBF helper dog alert (new to MBF)
    DSDGATK        MBF helper dog attack (new to MBF)
    DSDGACT        MBF helper dog idle (new to MBF)
    DSDGDTH        MBF helper dog death (new to MBF)
    DSDGPAIN       MBF helper dog pain (new to MBF)
    DSPLFEET       Light player impact (new to Eternity)
    DSFALLHT       Falling damage impact death (new to Eternity)
    DSPLFALL       Falling damage player scream (new to Eternity)
    DSTHUNDR       MapInfo global thunder effect (new to Eternity)
    DSMUCK         FLOOR_SLUDGE TerrainType sound (new to Eternity)
    DSBURN         FLOOR_LAVA TerrainType sound (new to Eternity)
    DSGLOOP        FLOOR_WATER TerrainType sound (new to Eternity)

    Flats
    -----
    METAL          Complimentary brown metal flat (new to SMMU)
    F_SKY2         Dummy flat for Hexen-style sky sectors (new to Eternity)

    Maps
    ----
    START          DOOM II Mode Start Map (new to SMMU)

    Switches and Animations
    -----------------------
    SWITCHES       Definition of switch textures recognized
    ANIMATED       Definition of animated textures and flats recognized

[SWITCHES format:]{#switchformat}\
\
The SWITCHES lump makes the names of the switch textures used in the
game known to the engine. It consists of a list of records each 20 bytes
long, terminated by an entry (not used) whose last 2 bytes are 0.\
\


    9 bytes        Null terminated string naming "off" state texture for switch
    9 bytes        Null terminated string naming "off" state texture for switch
    2 bytes        Short integer for the IWADs switch will work in
        1 = shareware
        2 = registered/retail DOOM or shareware
        3 = DOOM II, registered/retail DOOM, or shareware
        0 = terminates list of known switches

[ANIMATED format:]{#animformat}\
\
The ANIMATED lump makes the names of the animated flats and textures
known to the engine. It consists of a list of records, each 23 bytes
long, terminated by a record (not used) whose first byte is -1 (255).\
\


    1 byte         -1 to terminate list, 0 if a flat, 1 if a texture
    9 bytes        Null terminated string naming last texture/flat in animation
    9 bytes        Null terminated string naming first texture/flat in animation
    4 bytes        Animation speed, number of frames between animation changes

The utility package contains a program, SWANTBLS.EXE that will generate
both the SWITCHES and ANIMATED lumps from a text file. An example is
provided, DEFSWANI.DAT that generates the standard set of switches and
animations for DOOM.\
\
=== New to SMMU ===\
\
Setting the animation speed for a flat to a number higher than 65535
will\
cause the flat to use the swirling liquid effect rather than animation.\
\


    Colormaps
    ---------
    WATERMAP       Custom greenish colormap for underwater use
    LAVAMAP        Custom red colormap for lava (new to SMMU)

The WATERMAP lump has the same format as COLORMAP, 34 tables of 256
bytes each, see the UDS for details. Note that colormaps you add to a
PWAD must be between C_START and C_END markers.\
\
The utility package contains a program, CMAPTOOL.EXE, that will generate
custom colormaps for use with BOOM. It also includes 7 premade
colormaps, and an add-on wad containing them.\
\


    Color translation tables
    ------------------------
    CRBRICK        Translates red range to brick red
    CRTAN          Translates red range to tan
    CRGRAY         Translates red range to gray
    CRGREEN        Translates red range to green
    CRBROWN        Translates red range to brown
    CRGOLD         Translates red range to dark yellow
    CRRED          Translates red range to red range (no change)
    CRBLUE         Translates red range to light blue
    CRBLUE2        Translates red range to dark blue (status bar numerals)
    CRORANGE       Translates red range to orange
    CRYELLOW       Translates red range to light yellow

These tables are used to translate the red font characters to other
colors. They are made replaceable in order to support custom palettes in
TC\'s better. Note however that the font character graphics must be
defined using the palette indices for the standard red range of the
palette 176-191.\
\
=== Eternity Note ===\
\
The default text translation range in Heretic is gray rather than red,
and replacement fonts should use the gray range in the palette to
support color translations.\
\


    Data Lumps and Scripts
    ----------------------
    TXTRCONV       DOOM -> DOOM II texture conversion table (new to SMMU)
    KEYDEFS        Default keybindings (new to Eternity)
    STARTSCR       Compiled Small script for start map (new to Eternity)

[TXTRCONV format:]{#txtrconv}\
\
The TXTRCONV lump is used to translate DOOM textures to DOOM II
textures, for the purpose of playing ExMy levels under DOOM II (accessed
via the map command). The format of this lump is currently very strict.
The pairs of texture names must be aligned precisely, with the first
texture name (\<= 8 character) starting in column 0, followed by spaces
up to column 16, where the second DOOM II replacement texture name must
start. Only one pair must be on a line, and lines must be terminated
with a line break. This lump is ASCII text.\
\
[KEYDEFS format:]{#keydefs}\
\
KEYDEFS contains the engine\'s default key bindings. This lump is
written to file as keys.csc when a file by that name cannot be found.
Editing of this defaults lump is not generally desirable, but it is a
simple console script, the syntax of which is expounded upon in the
[Eternity Engine Console Reference](console_ref.html). This lump is
ASCII text.\
\
[Return to Table of Contents](#contents)

[]{#extradata}

------------------------------------------------------------------------

### Part VII ExtraData

------------------------------------------------------------------------

\

[]{#edintro}

------------------------------------------------------------------------

**Section 1.1. ExtraData Introduction**

------------------------------------------------------------------------

\
ExtraData is a new data specification language for the Eternity Engine
that allows arbitrary extension of mapthings, lines, and sectors with
any number of new fields, with data provided in more or less any format.
The use of a textual input language forever removes any future problems
caused by binary format limitations. The ExtraData parser is based on
the libConfuse configuration file parser library by Martin Hedenfalk,
which is also used by GFS and EDF.\
\
ExtraData requires special control thing types and line specials to be
placed on the things or lines to be affected. Within these control
objects, the number of the ExtraData record to use is specified.
Basically speaking, ExtraData replaces the information of a thing or
line in your map with information you specify in the ExtraData text
script.\
\
This documentation only covers the most basic usage of ExtraData. For
full syntax and semantics, it is critical that you consult the [Eternity
Engine ExtraData Documentation](extradata.html).\
\
ExtraData scripts must be attached to the maps which use them via the
use of [MapInfo](mapinfo.html).\
\
[Return to Table of Contents](#contents)

[]{#edthing}

------------------------------------------------------------------------

**Section 1.2. ExtraData Control Objects**

------------------------------------------------------------------------

\
The ExtraData control object has a **doomednum** of 5004. Each control
object specifies its corresponding ExtraData mapthing record number as
an integer value in its mapthing options field. If your editor does not
allow entering arbitrary values into the options field of mapthings, you
will need to use the BOOM command-line editor, CLED. As of the initial
writing time of this document, both DETH and Doom Builder support
entering arbitrary integer values for the options field. Check your
editor\'s documentation or user interface to verify if it supports this.

The x, y, and angle fields of the ExtraData control object are passed on
to the thing which will be spawned at the control point\'s location, so
those fields of the control object should be set normally. The true type
and options fields of the thing to be spawned, along with any extended
fields, are specified in the ExtraData record which the control thing
references.

Any number of ExtraData control objects can reference the same ExtraData
record. In the event that a control object references a non-existent
ExtraData record, the ExtraData script for a level is missing, or the
thing type referenced by an ExtraData record isn\'t valid, an Unknown
thing will be spawned at the control point\'s location. See the [EDF
Documentation](edf_ref.html) for information on the required Unknown
thingtype definition. Note that you cannot spawn ExtraData control
objects using ExtraData. Attempting this will also result in an Unknown
thing.\
\
Diagram:


      +-----------------------------+          mapthing
      |                             |          {
      |                             |             recordnum = 1
      |                             |             type = thing:DoomImp
      |                             |             options = EASY|NORMAL|HARD
      |                             |             tid = 666
      |                             |             height = 240
      |             O               |             args = { 0, 1, 2, 3, 4 }
      |                             |          }
      |                             |
      |                             |
      |                             |
      |                             |
      +-----------------------------+

Assume that in the diagram, the point marked O is a thing of type 5004
with its options field set to 1. This object will use the ExtraData
**mapthing** section defined to the right of the diagram, provided that
such a section exists in the level\'s ExtraData script. This particular
object will become an Imp who appears in all difficulty levels, has a
Small/ACS scripting thing ID of 666, spawns 240 units above the ground,
and has all five of its argument values set to different numbers. Note
that the fields **tid**, **height**, and **args** are values that would
otherwise only be available in the Hexen map format. ExtraData allows
use of all Hexen-format fields in DOOM, and may also provide access to
even more custom Eternity Engine fields in the future.\
\
[Return to Table of Contents](#contents)

[]{#edlines}

------------------------------------------------------------------------

**Section 1.3. ExtraData Control Linedefs**

------------------------------------------------------------------------

\
Linedef records in ExtraData are associated only with lines which bear
the ExtraData Control Line Special, #270. You can place this special
into the normal \"special\" field of a line using virtually any map
editor. Editors with Eternity-specific configurations should support
this special natively.

Each control linedef specifies its corresponding ExtraData linedef
record number as an integer value in its linedef tag field. If your
editor does not allow entering arbitrary values up to 32767 into the tag
field of linedefs, you will need to use the BOOM command-line editor,
CLED. As of the initial writing time of this document, both DETH and
Doom Builder support entering arbitrary integer values for the tag
field. Check your editor\'s documentation or user interface to verify if
it supports this.

The flags, sidedefs, textures, and location of the ExtraData control
linedef are used normally by the line and cannot be altered from within
ExtraData. The true special and tag fields of the linedef, along with
any extended fields, are specified in the ExtraData record which the
control linedef references.

Any number of ExtraData control linedefs can reference the same
ExtraData record. In the event that a control linedef references a
non-existent ExtraData record or the ExtraData script for a level is
missing, the special and tag of the ExtraData control linedef will both
be set to zero.\
\
Diagram:


      +-----------------------------+          linedef
      |                             |          {
      |                             |             recordnum = 1
      |                             |             special = Polyobj_RotateRight
      |                             |             args = { 1, 4, 64 }
      |                             |             extflags = PLAYER|CROSS|REPEAT
      |                             |          }
      |   +-------------------+     |
      |             1               |
      |                             |
      |                             |
      |                             |
      |                             |
      +-----------------------------+

Assume that in the diagram, the line marked 1 is a 2S linedef with
special 270 and with its tag field set to 1. This line will use the
ExtraData **linedef** section defined to the right of the diagram,
provided that such a section exists in the level\'s ExtraData script.
This particular line will become a Polyobj_RotateRight trigger that
turns PolyObject #1 90 degrees at approximately 5 degrees per gametic
and which can be activated any number of times but only when crossed by
the player. Note that the **args** field would otherwise only be
available in the Hexen map format, and that the **extflags** field,
which affects only parameterized line specials, is new to the Eternity
Engine. ExtraData enables the use of both Hexen and Eternity extended
linedef features within the DOOM map format.

[Return to Table of Contents](#contents)
