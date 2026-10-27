------------------------------------------------------------------------

**Eternity Engine Codepointer Reference \-- 10/08/08**

------------------------------------------------------------------------

[Return to the Eternity Engine Page](../etcengine.html)

This reference exposes full information on all \"codepointers\", also
known as action routines, available in the Eternity Engine, including
both those from DOOM and those added in later ports.

- [**Table of Contents**]{#contents}
  - [Changes](#changes)
  - [Note on Damage Formulae](#damage)
  - [Player Gun Frame Code Pointers](#pgp)
    - [Weapon Maintenance Pointers](#wmp)
      - [Light0](#)
      - [Light1](#)
      - [Light2](#)
      - [WeaponReady](#)
      - [Lower](#)
      - [Raise](#)
      - [ReFire](#)
      - [CheckReload](#)
      - [GunFlash](#)
    - [Player Attack Pointers](#pap)
      - [Punch](#)
      - [FirePistol](#)
      - [FireShotgun](#)
      - [FireShotgun2](#)
      - [FireCGun](#)
      - [FireMissile](#)
      - [Saw](#)
      - [FirePlasma](#)
      - [FireBFG](#)
    - [Miscellaneous Weapon Pointers](#mwp)
      - [BFGsound](#)
      - [OpenShotgun2](#)
      - [LoadShotgun2](#)
      - [CloseShotgun2](#)
    - [Parameterized Player Attack Pointers](#ppap)
      - [FireCustomBullets](#)
      - [FirePlayerMissile](#)
      - [CustomPlayerMelee](#)
      - [PlayerThunk](#)
    - [Weapon Frame Scripting Pointers](#pfrscript)
      - [WeaponCtrJump](#)
      - [WeaponCtrSwitch](#)
      - [WeaponSetCtr](#)
      - [WeaponCtrOp](#)
      - [WeaponCopyCtr](#)
  - [General Codepointers](#genptr)
    - [Miscellaneous Pointers](#genmisc)
      - [BFGSpray](#)
      - [BFG11KHit](#)
      - [BouncingBFG](#)
      - [BFGBurst](#)
      - [Mushroom](#)
      - [Nailbomb](#)
      - [SetFlags](#)
      - [UnSetFlags](#)
      - [Fall](#)
      - [Die](#)
      - [BrainScream](#)
      - [BrainExplode](#)
      - [SpawnFly](#)
      - [Fire](#)
      - [Spawn](#)
      - [Turn](#)
      - [LineEffect](#)
      - [Stop](#)
      - [SteamSpawn](#)
    - [Heretic Miscellaneous Pointers](#hticmisc)
      - [SpawnGlitter](#)
      - [AccelGlitter](#)
      - [SpawnAbove](#)
      - [MummySoul](#)
      - [BlueSpark](#)
      - [GenWizard](#)
      - [Sor2DthInit](#)
      - [Sor2DthLoop](#)
      - [PodPain](#)
      - [RemovePod](#)
      - [MakePod](#)
      - [DripBlood](#)
    - [Miscellaneous / Sound Pointers](#miscsnd)
      - [StartFire](#)
      - [FireCrackle](#)
      - [SkelWhoosh](#)
      - [FatRaise](#)
      - [SpawnSound](#)
    - [Heretic Misc / Sound Pointers](#hticsnd)\
      - [Sor1Pain](#)
    - [Object Sound Pointers](#objsound)\
      - [PlaySound](#)
      - [Pain](#)
      - [Scream](#)
      - [XScream](#)
      - [PlayerScream](#)
      - [BrainPain](#)
      - [BrainAwake](#)
      - [VileStart](#)
      - [AmbientThinker](#)
    - [AI Pointers](#aiptr)\
      - [Look](#)
      - [Chase](#)
      - [VileChase](#)
      - [FaceTarget](#)
      - [KeepChasing](#)
      - [RandomWalk](#)
    - [Heretic AI Pointers](#hticai)\
      - [Sor1Chase](#)
    - [AI Pointers with Walking Sounds](#aiwalk)\
      - [Metal](#)
      - [BabyMetal](#)
      - [Hoof](#)
    - [Blast Radius Attack Pointers](#blast)\
      - [Explode](#)
      - [Detonate](#)
    - [Heretic Blast Radius Attack Pointers](#hticblast)\
      - [HticExplode](#)
    - [Monster Attack Pointers](#monatk)\
      - [PosAttack](#)
      - [SPosAttack](#)
      - [VileTarget](#)
      - [VileAttack](#)
      - [SkelFist](#)
      - [SkelMissile](#)
      - [FatAttack1](#)
      - [FatAttack2](#)
      - [FatAttack3](#)
      - [CPosAttack](#)
      - [TroopAttack](#)
      - [SargAttack](#)
      - [HeadAttack](#)
      - [BruisAttack](#)
      - [SkullAttack](#)
      - [BspiAttack](#)
      - [CyberAttack](#)
      - [PainAttack](#)
      - [BrainSpit](#)
    - [Parameterized Monster Attacks](#monparam)\
      - [BetaSkullAttack](#)
      - [Scratch](#)
      - [MissileAttack](#)
      - [MissileSpread](#)
      - [BulletAttack](#)
      - [ThingSummon](#)
    - [Heretic Monster Attack Pointers](#hticmon)\
      - [MummyAttack](#)
      - [MummyAttack2](#)
      - [ClinkAttack](#)
      - [WizardAtk1](#)
      - [WizardAtk2](#)
      - [WizardAtk3](#)
      - [Srcr2Decide](#)
      - [Srcr2Attack](#)
      - [KnightAttack](#)
      - [BeastAttack](#)
      - [BeastPuff](#)
      - [SnakeAttack](#)
      - [SnakeAttack2](#)
      - [Srcr1Attack](#)
      - [VolcanoBlast](#)
      - [VolcBallImpact](#)
      - [MinotaurAtk1](#)
      - [MinotaurDecide](#)
      - [MinotaurAtk2](#)
      - [MinotaurAtk3](#)
      - [MinotaurCharge](#)
      - [MntrFloorFire](#)
      - [LichFire](#)
      - [LichWhirlwind](#)
      - [LichAttack](#)
      - [LichIceImpact](#)
      - [LichFireGrow](#)
      - [ImpChargeAtk](#)
      - [ImpMeleeAtk](#)
      - [ImpMissileAtk](#)
    - [Homing Missile Maintenance Pointers](#home)\
      - [Tracer](#)
      - [GenTracer](#)
    - [Heretic Homing Missile Maintenance Pointers](#htichome)\
      - [HticTracer](#)
      - [WhirlwindSeek](#)
    - [Line of Sight Checking Pointers](#los)\
      - [CPosRefire](#)
      - [SpidRefire](#)
    - [Special Monster Death Effects](#specmon)\
      - [PainDie](#)
      - [KeenDie](#)
      - [BossDeath](#)
      - [BrainDie](#)
      - [KillChildren](#)
    - [Heretic Special Monster Death Effects](#hticspecmon)\
      - [HticBossDeath](#)
      - [SorcererRise](#)
      - [ImpDeath](#)
      - [ImpXDeath1](#)
      - [ImpXDeath2](#)
      - [ImpExplode](#)
    - [Massacre Cheat Specials](#nukespecs)\
      - [PainNukeSpec](#)
      - [SorcNukeSpec](#)
    - [Frame Scripting Pointers](#frscript)\
      - [RandomJump](#)
      - [HealthJump](#)
      - [TargetJump](#)
      - [CounterJump](#)
      - [CounterSwitch](#)
      - [SetCounter](#)
      - [CounterOp](#)
      - [CopyCounter](#)
      - [SetTics](#)
      - [AproxDistance](#)
    - [Small Scripting System](#small)\
      - [StartScript](#)
      - [PlayerStartScript](#)
    - [EDF-Related Pointers](#edf)\
      - [ShowMessage](#)

[]{#changes}

------------------------------------------------------------------------

**Changes**

------------------------------------------------------------------------

- 04/07/26: moved to markdown, under git control. Further updates will be on git.
- 10/08/08: Revamped almost the entire document.
- 10/22/06: Added AmbientThinker and SteamSpawn.

[Return to Table of Contents](#contents)

[]{#damage}

------------------------------------------------------------------------

**Note on Naming and Conventions:**

------------------------------------------------------------------------

Normally codepointers have the `A_` prefix, but for [Dehacked](dehref.md) the prefix is usually
omitted. For [EDF](edf_ref.html), especially with the Decorate state syntax, the convention is to include the
prefix. In this document the codepointers are listed without the prefix.

An alternative name for codepointer is "action function".

Parameterized codepointers will be presented as functions, e.g.

**SomeCodepointer**(*anArg*, *anotherArg*, *yetAnother*)

**Bold** text means literal identifiers. They may also appear as `code text`. *Italics* are
placeholders for actual values you will enter for the parameters in EDF. Unless otherwise specified,
all arguments are optional and have default values. Omit the parentheses if there are no arguments
(e.g. **SomeCodepointer**).

When working directly in Dehacked patches, you only set the
**SomeCodepointer** name in the `[CODEPTRS]` `Frame =` assignment, and each arg will be designated
by `Args1 = ...`, `Args2 = ...` etc. in the `Frame` block. For names of things, frames, sounds etc.
you specify their Dehacked numbers instead.

------------------------------------------------------------------------

**Note on Damage Formulae:**

------------------------------------------------------------------------

Most damage in DOOM uses a random number, a modulus value, and a
multiplier. In general, damage formulae will appear like this:

(rnd%modulus + 1) \* multiplier

where rnd is a random number between 0 and 255, unless otherwise noted.

(rnd%modulus) + 1 results in a range of numbers from 1 to modulus, so if
the following formula is shown:

(rnd%8 + 1) \* 3

then the range of damage values is multiples of 3 from 3 to 24, since
1\*3 = 3 and 8\*3 = 24. Where damage values involve the summation of
several random \"dice\" throws or other, more complicated formulae, the
damage range will be shown explicitly for convenience.

[Return to Table of Contents](#contents)

[]{#pgp}

------------------------------------------------------------------------

**Player Gun Frame Code Pointers**

------------------------------------------------------------------------

[]{#wmp}

------------------------------------------------------------------------

**Weapon Maintenance Pointers**

------------------------------------------------------------------------

- **Light0**\
  Type: Weapon maintenance, normal\
  Purpose: Turns off any lighting flashes caused by firing a weapon.\
  \
- **Light1**\
  Type: Weapon maintenance, normal\
  Purpose: Causes a first-level light flash to occur for the player
  shooting the weapon.\
  Notes: Weapon flashes do not function on levels that use special
  global colormaps, or if [EMAPINFO](mapinfo.md) **fullbright** variable is set to **true**. Light must be subsequently returned to normal by
  using the Light0 code pointer above.\
  \
  \
- **Light2**\
  Type: Weapon maintenance, normal\
  Purpose: Causes a second-level light flash to occur for the player
  shooting the weapon.\
  Notes: Weapon flashes do not function on levels that use special
  global colormaps, or if [EMAPINFO](mapinfo.md) **fullbright** variable is set to **true**. Light must be subsequently returned to normal by
  using the Light0 code pointer above.\
  \
  \
- **WeaponReady**(*flags*)
  Type: Weapon maintenance, optionally parameterized

  Arguments:
  - *flags*: optional flag, can be **WRF_NOBOB** (1).

  Purpose: Readies a weapon for firing or change, should be used after
  the weapon's raise sequence and fire sequence.

  This can play the **weaponinfo** **readysound** like the chainsaw.

  If **WRF_NOBOB** is set, it will block the weapon from bobbing on player movement.


- **Lower**\
  Type: Weapon maintenance, normal\
  Purpose: Lowers the current weapon and changes to a newly selected
  one, unless the player is dead. Should be called from the weapon\'s
  lowering sequence.\
  \
  \
- **Raise**\
  Type: Weapon maintenance, normal\
  Purpose: Raises a new weapon to normal position. Should be called from
  a weapon\'s raise sequence.\
  \
  \
- **ReFire**\
  Type: Weapon maintenance, normal\
  Purpose: Allows a weapon to restart its firing sequence without
  returning to its ready sequence. If the player does not have enough
  ammo to fire again, the weapon will be lowered and replaced.\
  \
  \
- **CheckReload**\
  Type: Weapon maintenance, normal\
  Purpose: Checks the ammo for a weapon to see if a weapon change should
  occur before the reload frames are entered. Used in particular by the
  super shotgun.\
  Notes: This code pointer does not function properly in BOOM, MBF, or
  SMMU, and will fail to make the weapon lower before completing its
  reload frames. This problem has been repaired in Eternity.\
  \
  \
- **GunFlash**\
  Type: Weapon maintenance, normal\
  Purpose: Shows the flash weapon state, and triggers Boom weapon recoil, if enabled in settings.
  This codepointer is needed when the flash needs to appear at other moments than when the actual
  ammo is fired.

[Return to Table of Contents](#contents)

[]{#pap}

------------------------------------------------------------------------

**Player Attack Pointers**

------------------------------------------------------------------------

- **Punch**\
  Type: Player attack, normal\
  Purpose: Performs the standard player punch attack. Hits for damage
  with the formula (rnd%10 + 1) \* 2 when the player is normal. If the
  player is berserk, this value is multiplied by 10. The player can only
  hit when within melee range (64 units). If a hit is successful, the
  player will turn to directly face the target and will emit the
  \"punch\" sound effect.\
  \
  \
- **FirePistol**\
  Type: Player attack, normal\
  Purpose: Fires a single bullet for standard player tracer damage —
  (rnd%3 + 1) \* 5 — Accuracy will be perfect on the first shot, but
  will use \"never\" accuracy if the gun is fired repeatedly. The player
  object will transfer to its second attack frame when this pointer is
  used. The player will emit the \"pistol\" sound effect.\
  \
  \
- **FireShotgun**\
  Type: Player attack, normal\
  Purpose: Fires player shotgun attack. Seven tracers are fired using
  \"never\" accuracy (a moderate horizontal spread with no vertical
  error), each doing standard bullet damage of (rnd%3 + 1) \* 5. The
  player object will transfer to its second attack frame when this
  pointer is used. The player will emit the \"shotgn\" sound effect.\
  \
  \
- **FireShotgun2**\
  Type: Player attack, normal\
  Purpose: Fires player super shotgun attack. Twenty tracers are fired
  using \"ssg\" accuracy (wide horizontal spread combined with moderate
  vertical error), each tracer doing standard bullet damage of
  (rnd%3 + 1) \* 5. The player object will transfer to its second attack
  state, and will emit the \"dshtgn\" sound effect.\
  \
  \
- **FireCGun**\
  Type: Player attack, normal\
  Purpose: Fires player chaingun attack. Fires one tracer for standard
  bullet damage of (rnd%3 + 1) \* 5. Accuracy will be perfect on the
  first shot, but will use \"never\" accuracy on subsequent shots. The
  player object will transfer to its second attack state, and will emit
  the \"chgun\" sound effect.\
  Notes: If no DSCHGUN sound lump is present amongst all WAD files, the
  \"chgun\" sound effect will be dynamically remapped to DSPISTOL. This
  behavior is new as of SMMU.

  Unlike **FirePistol**, this will not fire if no ammo is left, but will play the "pistol" sound
  anyway.
  \
  \
- **FireMissile**\
  Type: Player attack, normal\
  Purpose: Fires a single RocketShot projectile.\
  \
  \
- **Saw**\
  Type: Player attack, normal\
  Purpose: Performs a player chainsaw attack. Hits for (rnd%10 + 1) \* 2
  damage once per call. Player must be within melee range (64
  units) in order to hit with this attack. If the attack misses, the
  player will emit the \"sawful\" sound effect. If the attack hits, the
  player will emit the \"sawhit\" sound effect, and his/her current
  angle will be set directly toward the target, and then will be
  randomly deflected 4.5 degrees to the right or left. The JUSTATTACKED
  flag will be set for the player object, causing slight forward
  movement.\
  \
  \
- **FirePlasma**\
  Type: Player attack, normal\
  Purpose: Fires a single PlasmaShot projectile. It has a 50% chance to the flashing state to the
  one following the designated.
  \
  \
- **FireBFG**\
  Type: Player attack, normal\
  Purpose: Fires a single BFGShot projectile.

[Return to Table of Contents](#contents)

[]{#mwp}

------------------------------------------------------------------------

**Miscellaneous Weapon Pointers**

------------------------------------------------------------------------

- **BFGsound**\
  Type: Weapon miscellaneous, normal\
  Purpose: Player object will emit the \"bfg\" sound.\
  \
  \
- **OpenShotgun2**\
  Type: Weapon miscellaneous, normal\
  Purpose: Player object will emit the \"dbopn\" sound effect.\
  \
  \
- **LoadShotgun2**\
  Type: Weapon miscellaneous, normal\
  Purpose: Player object will emit the \"dbload\" sound effect.\
  \
  \
- **CloseShotgun2**\
  Type: Weapon miscellaneous, normal\
  Purpose: Player object will emit the \"dbcls\" sound effect, and then
  control will transfer to the ReFire code pointer, which will check for
  weapon refire.

[Return to Table of Contents](#contents)

[]{#ppap}

------------------------------------------------------------------------

**Parameterized Player Attack Pointers**

------------------------------------------------------------------------

- **FireCustomBullets**(*sound*, *accuracy*, *numBullets*, *dmgfactor*, *dmgmod*, *flash*, *horizontalSpread*, *verticalSpread*, *puffType*)

  Type: Player attack, parameterized\
  \
  Parameter Information:
  - *sound* = name of sound to play (default of **none** = no sound)
  - *accuracy* = Select accuracy type (default of 0 = 1)
    - 1 or **always** = Always accurate
    - 2 or **first** = Accurate on first fire only (\"never\" accurate afterward)
    - 3 or **never** = "Never" accurate\
      (moderate horizontal spread with no vertical error)
    - 4 or **ssg** = Super Shotgun accuracy\
      (wide horizontal spread with moderate vertical error)
    - 5 or **monster** = Monster accuracy (very wide horizontal spread)
    - 6 or **custom** = Custom accuracy. Use *horizontalSpread* and *verticalSpread*
  - *numBullets* = Number of bullet tracers to fire (default of 0 = 0)
  - *dmgfactor* = Damage factor (Damage formula is: *dmgfactor* \*
    ((rnd%*dmgmod*) + 1))
  - *dmgmod* = Damage modulus\
    (Clamped into range of 1 to 256, default 1)
  - *flash* = name of weapon flash state to use instead of weapon's default (default of 0: use
    weapon's definition)
  - *horizontalSpread*, *verticalSpread* = custom spread angles if *accuracy* is **custom**, in
    degrees. Defaults 0.
  - *puffType* = gunshot **pufftype** name to use instead of game's default.

  \
  Purpose: Extremely powerful codepointer that allows construction of
  custom player bullet weapons. The sound, accuracy, number of tracers,
  and randomized damage range of the tracers are all selectable via the
  argument fields of the frame calling this codepointer. It is possible
  to recreate any of the other bullet attacks in the game using this
  pointer, as well as create many totally new ones.\
  New to Eternity.\

  \
  \

- **FirePlayerMissile**(*thing*, *flags*)

  Type: Player attack, Homing missile firing pointer, parameterized\

  Parameter Information:
  - *thing* = name of thing type to shoot (must be valid, no default)
  - *flags* = a collection of zero or more of those flags:
    - **homing** (1) = may seek targets
    - **noammo** (2) = do not consume ammo

  Purpose: Allows a custom player projectile attack. The type of missile
  to fire is given as the thing type's name in *thing*. If this type is invalid, this codepointer does nothing. If *flags* has **homing** and the missile
  fired has a homing maintenance codepointer in its frames, the missile will home on the player's
  current autoaiming target, if one exists. New to Eternity.

  Notes: A missile cannot home unless its frames contain a homing
  maintenance codepointer. These pointers include:
  - **Tracer**
  - **GenTracer**
  - **HticTracer**
  - **WhirlwindSeek**

  \
  \

- **CustomPlayerMelee**(*dmgfactor*, *dmgmod*, *berserkMultiplier*, *deflection*, *sound*, *range*, *pufftype*)

  Type: Player attack, parameterized

  Parameter Information:
  - *dmgfactor* = Damage factor (Damage formula is: *dmgfactor* \*
    ((rnd%*dmgmod*) + 1))
  - *dmgmod* = Damage modulus (Clamped into range of 1 to 256, default 1)
  - *berserkMultiplier* = Berserk multiplier (default 0, x\*damage when player is
    berserk)
  - *deflection* = Select angle deflection type (default of 0 = 1)
    - **none** (1) = None
    - **punch** (2) = Punch (player will face object on hit)
    - **chainsaw** (3) = Chainsaw (player will face object and be wiggled around)
  - *sound* = DeHackEd number of sound to play (default of **none** = no sound)
  - *range* = range of attack (default 64)
  - *pufftype* = optional name of puff-type override (default none)

  Purpose: Extremely powerful codepointer that allows construction of
  custom player melee attacks. Two major classes of attacks are
  possible, punch-style and chainsaw-style, depending on the parameters
  given. Note that the Berserk multiplier can be applied to any attack,
  but that if it is 0, you will do zero damage when berserk! The Berserk
  multiplier should be 1 when no berserk bonus is desired.

  New to Eternity.
  \
  \

- **PlayerThunk**\
  Type: Player attack, parameterized\
  \
  Parameter Information:

  - Args1 = Codepointer to call (no default, must be valid)
  - Args2 = Select target facing behavior (default of 0 = 0)
    - 0 = FaceTarget pointer has no effect
    - 1 = FaceTarget pointer is active
  - Args3 = DeHackEd number of state to put player in temporarily
    (default of 0 = 0; negative value = no state change)
  - Args4 = Select targeting behavior (default of 0 = 0)
    - 0 = Player targets last attacker, if any
    - 1 = Player targets current autoaim target, if any\
      (attack fails if none)
  - Args5 = Select ammo usage (default of 0 = 0)
    - 0 = No ammo is used
    - 1 = Normal amount of ammo is used, if applicable (see note below
      about autoaim targeting)

  \
  Purpose: Allows player gun frames to call a select set of monster
  code- pointers which have been determined as safe for use in this
  context. If the index parameter indicates an invalid codepointer, or
  any of the other parameters have invalid values, nothing will occur.
  New to Eternity.\
  \
  Notes: If targeting behavior is set to autoaim, and ammo usage is on,
  no ammo is used unless the player follows through with an attack on
  his current target. Otherwise, ammo would be used despite no attack
  occuring. This may become an additional ammo usage option in the
  future.\
  \
  Behavior of monster codepointers when applied to the player will in
  general be exactly as described for monsters. The Args3 field is
  provided for use with parameterized codepointers, so that they can
  retrieve args values from a different frame. The player will NOT
  display a sprite, call an action function, or cause a particle event
  from this state.\
  \
  \*\*\* The Args3 parameter\'s behavior has been changed slightly as of
  EE v3.31 public beta 5. Formerly, the default value zero meant no
  state change would occur, but this was inappropriate. Now, a negative
  number means no state, and zero means whatever state is defined to
  have DeHackEd number zero (S_NULL by default).\
  \
  Special Information: This codepointer is only usable via EDF. The
  first argument must be specified in an EDF frame block\'s args list
  using the following syntax:\
  \

      args = { bexptr:<codepointer mnemonic> }

  where \<codepointer mnemonic\> is the name of a codepointer which
  indicates it can be used with PlayerThunk. Any other codepointers will
  not be executed by PlayerThunk.

[Return to Table of Contents](#contents)

[]{#pfrscript}

------------------------------------------------------------------------

**Weapon Frame Scripting Pointers**

------------------------------------------------------------------------

General Notes for Weapon Frame Scripting Codepointers:

These pointers allow a degree of programmatic control over an weapon\'s
behavior, and are therefore termed \"frame scripting\" codepointers.

Counter fields may have values from -32768 to 32767. Values above or
below this range will wrap around.

- **WeaponCtrJump**\
  Type: Weapon frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Frame DeHackEd number to which weapon may jump\
    (default of 0 = frame 0)
  - Args2 = Type of comparison to perform on counter\
    (default of 0 = 0)
    - 0 = Jump if counter \< Args3
    - 1 = Jump if counter \<= Args3
    - 2 = Jump if counter \> Args3
    - 3 = Jump if counter \>= Args3
    - 4 = Jump if counter equals Args3
    - 5 = Jump if counter does not equal Args3
    - 6 = Jump if counter & Args3 (bitwise AND operation)
    - 7 - 13 = The value in Args3 will be interpreted as a counter
      number (0 - 2). The resulting operation is the counter in Args3
      compared against the value of the counter in Args4, subtracting 7
      from the operation number (ie. operation 7 is \"Jump if counter1
      \< counter2\").
  - Args3 = Value to compare against counter / Counter number holding
    value (default of 0 = 0)
  - Args4 = Counter number on the current weapon to use (default of 0 =
    0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args5 = PSprite to affect (default of 0 = 0)
    - 0 = Main Weapon
    - 1 = Gun Flash

  \
  Purpose: Causes the indicated psprite to transfer to the indicated
  frame if the value in the indicated internal counter field compares as
  requested to the provided value. This allows \"frame scripting\" of
  actions which vary depending on the current value of a weapon\'s
  counters. If the jump does not occur, the object will fall through to
  the next frame. If the current frame\'s tics are set to -1,
  fall-through will not occur.\
  New to Eternity Engine v3.33.33.\
  \
  \
- **WeaponCtrSwitch**\
  Type: Weapon frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Counter number to use (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args2 = Frame DeHackEd number of first state in set\
    (default of 0 = frame 0)
  - Args3 = Number of states in set\
    (No default, must be valid)
  - Args4 = PSprite to affect (default of 0 = 0)
    - 0 = Main Weapon
    - 1 = Gun Flash

  \
  Purpose: This powerful codepointer implements an N-way branch where
  the value of the indicated counter determines to which frame the jump
  will occur. The frames to be jumped to must be defined in a
  consecutive block in EDF. The first frame in this set is deemed to be
  frame #0, and the last frame in the set is frame #Args3 - 1. The value
  of the counter is considered to be zero-based, so that if it is equal
  to zero, the object will jump to frame #0 (the frame you provided in
  Args2). The entire range of frames must be valid, and if it is not, no
  jump will occur. If the counter value is not in the range from 0 to
  Args3 - 1, no jump will occur. Finally, if the counter number in Args1
  is invalid, no jump will occur.\
  New to Eternity Engine v3.33.33.\
  \
  \
- **WeaponSetCtr**\
  Type: Weapon frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Counter number to set (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args2 = Value to utilize (default of 0 = 0)
  - Args3 = Operation to perform (default of 0 = 0)
    - 0 : Args1 = Args2 (direct assignment)
    - 1 : Args1 += Args2 (addition)
    - 2 : Args1 -= Args2 (subtraction)
    - 3 : Args1 \*= Args2 (multiplication)
    - 4 : Args1 /= Args2 (division)
    - 5 : Args1 %= Args2 (modulus)
    - 6 : Args1 &= Args2 (bitwise AND)
    - 7 : Args1 &= \~Args2 (compound bitwise AND / bitwise NOT)
    - 8 : Args1 \|= Args2 (bitwise OR)
    - 9 : Args1 \^= Args2 (bitwise XOR)
    - 10 : Args1 = rnd (random value, Args2 is not used)
    - 11 : Args1 = rnd % Args2 (random value modulus)
    - 13 : Args1 \<\<= Args2 (bitshift left)
    - 14 : Args1 \>\>= Args2 (bitshift right)

  \
  Purpose: Sets the indicated counter on this weapon to a value
  resulting from an operation with the current counter value and the
  value provided in Args2. New to Eternity Engine v3.33.33.\
  Notes: For operation 4, division, Args2 may not be equal to zero, and
  no operation will occur if it is. For operations 5 and 11, modulus and
  random modulus, Args2 must be greater than zero. No operation will
  occur if it is less than or equal to zero. Operation 7 inverts the
  bits in Args2 before bitwise-ANDing it with the value in the counter.
  For operation 10, the value of Args2 is not used, and may simply be
  allowed to default. All random numbers are between 0 and 255
  inclusive. Operations 12 and 15-18 are not defined for this
  codepointer and will do nothing.\
  \
  \
  \
- **WeaponCtrOp**\
  Type: Weapon frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Counter number to use for operand 1 (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args2 = Counter number to use for operand 2 (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args3 = Counter number to set (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args4 = Operation to perform (no default)
    - 1 : Args3 = Args1 + Args2 (addition)
    - 2 : Args3 = Args1 - Args2 (subtraction)
    - 3 : Args3 = Args1 \* Args2 (multiplication)
    - 4 : Args3 = Args1 / Args2 (division)
    - 5 : Args3 = Args1 % Args2 (modulus)
    - 6 : Args3 = Args1 & Args2 (bitwise AND)
    - 8 : Args3 = Args1 \| Args2 (bitwise OR)
    - 9 : Args3 = Args1 \^ Args2 (bitwise XOR)
    - 12 : Args3 = Args1 \* ((rnd % Args2) + 1) (HITDICE calculation)
    - 13 : Args3 = Args1 \<\< Args2 (bitshift left)
    - 14 : Args3 = Args1 \>\> Args2 (bitshift right)
    - 15 : Args3 = \|Args1\| (absolute value)
    - 16 : Args3 = -Args1 (negate)
    - 17 : Args3 = !Args1 (logical NOT)
    - 18 : Args3 = \~Args1 (bitwise invert)

  \
  Purpose: Sets the indicated counter on this weapon to a value
  resulting from an operation with one or two provided counter values.
  The operand and destination counters may be the same counter number.\
  New to Eternity Engine v3.33.33.\
  Notes: For operation 4, division, Args2 may not be equal to zero, and
  no operation will occur if it is. For operations 5 and 12, modulus and
  HITDICE calculation, Args2 must be greater than zero. No operation
  will occur if it is less than or equal to zero. Operation 12, HITDICE,
  is the operation used to calculate standard damage values in DOOM,
  where Args1 acts as a multiplier and Args2 acts as the modulus. All
  random numbers are between 0 and 255 inclusive. Operations 0, 7, 10,
  and 11 are not defined for this codepointer and will do nothing.
  Operations 15 through 18 are unary and do not use the Args2 parameter,
  but it must still be a valid counter number. You may allow it to
  default to zero.\
  \
  \
  \
- **WeaponCopyCtr**\
  Type: Weapon frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Source counter number (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args2 = Destination counter number (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2

  \
  Purpose: Copies a weapon\'s counter value to another one of its
  counters.\
  New to Eternity Engine v3.33.33.\

[Return to Table of Contents](#contents)

[]{#genptr}

------------------------------------------------------------------------

**General Code Pointers**

------------------------------------------------------------------------

[]{#genmisc}

------------------------------------------------------------------------

**Miscellaneous Pointers**

------------------------------------------------------------------------

- **BFGSpray**\
  Type: Miscellaneous, normal\
  Purpose: Causes BFG explosion effects, which differ depending upon the
  player\'s selected type of BFG:
  1.  Normal BFG:\
      40 tracers will emanate from the object which fired the projectile
      in the general direction in which the projectile was fired, each
      doing damage that is the sum of 15 rolls of an 8-sided die
      (Absolute damage range per tracer is 15 to 120, absolute damage
      range of ALL tracers is 600 to 4800).
  2.  BFG 11K:\
      Control is transferred to the BFG11KHit codepointer
  3.  Bouncing BFG:\
      Control is transferred to the BouncingBFG codepointer
  4.  Plasma Burst BFG:\
      Control is transferred to the BFGBurst codepointer

  \
  \

- **BFG11KHit**\
  Type: Miscellaneous, normal\
  Purpose: Explosion effect of BFG 11k weapon. Designed for more
  balanced play in deathmatch, this BFG works very differently. First,
  if the BFG ball explodes closer than 96 units to its originator, the
  originator will take damage proportional to that distance. The damage
  calculation is the sum of floor(48 - (distance / 2)) rolls of an
  8-sided die. At 95 units away, the damage is 0. At 0 units away, the
  damage ranges from 48 to 384. The originator is hit directly for this
  damage, and a BFGFlash object is spawned at his/her location. 40
  tracers will be fired out in a circular pattern from the explosion
  location. These tracers cannot hit the originator of the missile.
  These tracers will hit for damage equal to the sum of 20 rolls of an
  8-sided die (damage range per tracer is 20 to 160, damage range of all
  tracers together is 800 to 6400). When a target is hit, an BFGFlash
  object will be spawned at its location.\
  New to Eternity.\
  \
  \

- **BouncingBFG**\
  Type: Miscellaneous, normal\
  Purpose: Explosion effect of Bouncing BFG weapon. This BFG does not do
  any tracer-based damage. Instead, 40 test traces are fired in a
  circular pattern from the point of explosion. The first of these
  tracers to hit a living object selects that object as the target. If
  the same object was targetted just prior in this weapon\'s propagation
  process, it will not be targetted again and the process will stop. If
  this is the 16th time the process has repeated, the process will also
  stop. If a valid target is acquired, a new BFGShot projectile will be
  fired from the current projectile\'s location toward the target. This
  bouncing can occur up to 16 times, as noted before. New to Eternity.\
  Notes: This effect will not work unless the original projectile was
  fired by the FireBFG player gun codepointer.\
  \
  \

- **BFGBurst**\
  Type: Miscellaneous, normal\
  Purpose: Explosion effect of the Plasma Burst BFG weapon. This BFG
  does not do tracer-based damage. Instead, 40 EEBetaPlasma3 projectiles
  will be fired from the point of explosion in a circular pattern, with
  randomized small positive or negative z momentum, such that they tend
  to bounce off the floor. The new projectiles cannot hit the originator
  of the BFG projectile. Based on the BFG from the unreleased MagDOOM
  source port by Cephaler.\
  New to Eternity.\
  \
  \

- **Mushroom**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Unknown 1 = Fixed-point launch angle factor (default of 0 = 4 units)
  - Unknown 2 = Fixed-point missile momentum factor (default of 0 = 1/2
    unit)
  - Args1 = DeHackEd number of thing type to spawn (default of 0 =
    MancubusShot)

  \
  Purpose: Causes a mushroom cloud effect. The object will first call
  the Explode codepointer for primary blast radius damage. Then, a
  number of MancubusShot projectiles proportional to the object\'s
  missile damage field will be launched into the air in a circular
  pattern, using the angle and speed parameters provided in the current
  frame. This codepointer is new to MBF.\
  Notes: The Args1 parameter is new to Eternity Engine v3.31 beta 4. It
  allows the thing type fired to be overridden. If this parameter is not
  provided, the default MancubusShot will be used.\
  Thunk: Yes.\
  \
  \

- **Nailbomb**\
  Type: Miscellaneous, normal\
  Purpose: Causes a blast radius attack with radius and max damage value
  of 128, and then fires 30 bullet tracers in a circular spread for 10
  damage each. New to SMMU.\
  Thunk: Yes.\
  \
  \

- **SetFlags**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Args1 = Select Bits Field (no default)
    - 1 = Bits
    - 2 = Bits2
    - 3 = Bits3
  - Args2 = Value to Logical-OR with selected Bits field (default of 0 =
    0)

  \
  Purpose: Adds the flags set in Args2 to the object\'s Bits field
  selected by Args1. This \"turns on\" effects controlled by those bits.
  New to Eternity.\
  Thunk: Yes.\
  \
  \

- **UnSetFlags**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Args1 = Select Bits Field (no default)
    - 1 = Bits
    - 2 = Bits2
    - 3 = Bits3
  - Args2 = Value to Inverse-Logical-AND with selected Bits field
    (default of 0 = 0)

  \
  Purpose: Removes the flags set in Args2 from the object\'s Bits field
  selected by Args1. This \"turns off\" effects controlled by those
  bits. New to Eternity.\
  Thunk: Yes.\
  \
  \

- **Fall**\
  Type: Miscellaneous, normal\
  Purpose: Removes the solid flag from the object, allowing it to be
  walked over.\
  Thunk: Yes.\
  \
  \

- **Die**\
  Type: Miscellaneous, normal\
  Purpose: Causes the object to die by damaging it for its current
  health. This codepointer is new to MBF.\
  Thunk: Yes.\
  \
  \

- **BrainScream**\
  Type: Miscellaneous, normal\
  Purpose: Boss Brain Explosion. Spawns RocketShot objects from the
  point (object.x - 196, object.y - 320, z) to the point (object.x +
  320, object.y - 320, z), stepping 8 units along the x axis for each
  spawn, and using a random z coordinate of 128 + (rnd \* 2). Each
  RocketShot object will be given a random z momentum of (512 \* rnd),
  will be set to frame S_BRAINEXPLODE1 (#799), and will have its
  state\'s length modified by a random amount of 0 to 7 gametics, not
  allowing the frame length to be less than 1 tic. The object will then
  play the \"bosdth\" sound effect at full volume.\
  Thunk: Yes.\
  \
  \

- **BrainExplode**\
  Type: Miscellaneous, normal\
  Purpose: Boss Brain Explosion, called by rockets spawned by
  BrainScream. Another RocketShot object will be spawned within a small
  distance of the one calling this pointer, and it will be given
  randomized z momentum, be transferred to frame S_BRAINEXPLODE1 (#799),
  and have its frame\'s length randomly modified.\
  Notes: This effect will continue ad infinitum. This may slow the game
  down eventually, especially if multiple objects start this effect.\
  Thunk: Yes.\
  \
  \

- **SpawnFly**\
  Type: Miscellaneous, normal\
  Purpose: Boss spawn cube spawning function. If the spawn cube has not
  reached its target spot, which is judged by a precomputed amount of
  time having passed, nothing will occur. Otherwise, a BossSpawnFire
  object will be spawned at the object\'s (x,y) location on the floor of
  its sector, the \"telept\" sound will be played, and one of the
  following monsters will be spawned at the same point, with the
  approximate probability listed for that type:


                         Type     Probability   Type number (*)
                 * DoomImp --------- 19.5% --------  1
                 * Demon ----------- 15.6% --------  2
                 * Spectre --------- 11.7% --------  3
                 * Cacodemon ------- 11.7% --------  5
                 * Mancubus -------- 11.7% --------  9
                 * HellKnight ------  9.3% -------- 10
                 * Arachnotron -----  7.8% --------  8
                 * PainElemental ---  3.9% --------  4
                 * Revenant --------  3.9% --------  7
                 * BaronOfHell -----  3.9% -------- 11
                 * Archvile --------  0.7% --------  6

  Friendliness of the spawn cube object will be transferred to the
  spawned monster. The spawned monster will telefrag anything occupying
  its location. The spawned monster will immediately awaken if it can
  either see a target, or a target has made sound in the area at any
  time in the past. The spawn cube object will remove itself after the
  spawning operation takes place.\
  Notes: Spawn cubes can occasionally miss their target. This is due to
  round-off error in the calculation used to determine the amount of
  time the cube should travel. If this happens, the cube will travel in
  a straight path for an undetermined amount of time. (\*) \-- This
  number indicates the order of the thing types in the EDF
  boss_spawner_types list. EDF allows the thing types spawned by this
  codepointer to be edited. See the [EDF Reference](edf_ref.html) for
  more information.\
  \
  \

- **Fire**\
  Type: Miscellaneous, normal\
  Purpose: Moves the object in front of the object it is tracing, if the
  object which it was spawned by has not lost sight of the traced
  object. This object must have been spawned by the VileAttack
  codepointer or no effects will occur.\
  \
  \

- **Spawn**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Unknown 1 = DeHackEd number of thing type to spawn\
    (must be valid, no default)
  - Unknown 2 = Integer amount of units to add to z coordinate of
    object\
    (default = 0)

  \
  Purpose: Spawns an object of the provided type at the calling
  object\'s location. Friendliness of the calling object will be
  transferred to the spawned object. This codepointer is new to MBF.\
  Notes: This codepointer will no longer crash on invalid thing type
  numbers as of Eternity Engine v3.31 public beta 3. If the type
  indicated is not valid, the pointer will do nothing. Also note that if
  both the source and spawned object are solid, they will become stuck
  to each other. This codepointer is more suitable for boss-brain-like
  effects.\
  Thunk: Yes.\
  \
  \

- **Turn**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Unknown 1 = Integer number of degrees to turn (default of 0 = 0
    degrees)

  \
  Purpose: The object will turn from its present angle the provided
  number of degrees. New to MBF.\
  Thunk: Yes.\
  \
  \

- **Face**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Unknown 1 = Integer angle to face (default of 0 = 0 degrees)

  \
  Purpose: Makes the object face toward the indicated angle. New to
  MBF.\
  Notes: 0 (zero) degrees is equivalent to east, or toward the right
  side of the automap.\
  Thunk: Yes.\
  \
  \

- **LineEffect**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Unknown 1 = Line special (default of 0 = 0)
  - Unknown 2 = Sector tag (default of 0 = 0)

  \
  Purpose: Allows objects to trigger linedef specials from frames. This
  effect can trigger any W1/WR/S1/SR line effect. It does not work for
  G1/GR line effects. If a W1 or S1 line effect is triggered, the object
  will not be able to trigger any additional line effects. If the line
  special provided is not a valid type, nothing will occur. Note that
  the action will occur as if a player has activated the line, and not a
  monster. New to MBF.\
  Notes: As of Eternity Engine v3.31 Delta, this codepointer cannot
  cause accidental corruption of memory under any circumstances. While
  exceedingly rare, this could have occurred occasionally in MBF and
  SMMU.\
  Warning: This codepointer may cause undesired side effects in some
  situations by affecting the first line defined in the map and/or the
  sector(s) which it references. Use with caution!\
  \
  \

- **Stop**\
  Type: Miscellaneous\
  Purpose: Causes an object to completely stop all movement. New to
  Eternity.\
  Thunk: Yes.\
  \
  \

- **SteamSpawn**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Args1 = DeHackEd number of thing type to spawn
  - Args2 = Horizontal angular range (centered on thing\'s angle)
  - Args3 = Vertical angle
  - Args4 = Vertical angular rangle (centered on Args3)
  - Args5 = Speed

  \
  Purpose: Spawns and thrusts objects within randomized horizontal and
  vertical angular sweeps. Angular ranges should be values between 0
  and 359. New to Eternity Engine v3.33.50.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#hticmisc}

------------------------------------------------------------------------

**Heretic Miscellaneous Pointers**

------------------------------------------------------------------------

- **SpawnGlitter**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Args1 = DeHackEd number of object type to spawn (must be valid, no
    default)
  - Args2 = Amount of z momentum to apply to object in eights of a unit
    per tic.\
    (default of 0 = +1/4 unit per tic)

  \
  Purpose: Teleport glitter spawning function. Can be used to make a
  fountain of any type of object. One object will be spawned at a
  randomized coordinate within a square around the source object, and
  will be given z momentum specified in Args2. If the thing type given
  in Args1 is invalid, this codepointer will do nothing.\
  New to Eternity.\
  Thunk: Yes.\
  \
  \
- **AccelGlitter**\
  Type: Miscellaneous, normal\
  Purpose: Accelerates the object upward by increasing its z momentum by
  50%.\
  New to Eternity.\
  Thunk: Yes.\
  \
  \
- **SpawnAbove**\
  Type: Miscellaneous, parameterized\
  \
  Parameter Information:
  - Args1 = DeHackEd number of thing type to spawn (must be valid, no
    default)
  - Args2 = Frame DeHackEd number to which thing will transfer\
    (default of 0 = 0, negative values = no frame transfer)
  - Args3 = Integer amount to add to thing\'s z coordinate\
    (default of 0 = 0 units)

  \
  Purpose: Special spawning function to spawn an object above the source
  object. The object will be spawned at precisely the source\'s (x,y)
  position, and at the object\'s z coordinate plus the amount provided
  in Args3. It will then be set to the frame specified by Args2, if any.
  If the thing type in Args1 or the frame number in Args2 are invalid,
  the pointer will do nothing. Note that a negative value can be
  provided for the frame number to indicate that no frame transition is
  to take place. New to Eternity.\
  Thunk: Yes.\
  \
  \
- **MummySoul**\
  Type: Miscellaneous, normal\
  Purpose: Spawns a GolemSoul object 10 units above the object and gives
  it upward momentum of 1 unit. New to Eternity.\
  Thunk: Yes.\
  \
  \
- **BlueSpark**\
  Type: Miscellaneous, normal\
  Purpose: Spawns D\'Sparil\'s blue projectile spark trail. Two
  DsparilSpark objects are spawned at the object\'s location and are
  given randomized x, y, and z momenta. New to Eternity.\
  Thunk: Yes.\
  \
  \
- **GenWizard**\
  Type: Miscellaneous, normal\
  Purpose: DsparilShot2 Disciple spawning pointer. The pointer will
  attempt to spawn a Disciple object at its (x,y) location and at its z
  minus 1/2 the height of a Disciple object. If after spawning, the
  object does not fit or cannot move, it will be removed from the level
  immediate- ly. Otherwise, the MBF friendliness flag will be
  transferred from the source object to the new object, the source
  object will be stopped and set to its death state, a HereticTeleFog
  object will be spawned at its location, and the Heretic TELEPT sound
  effect will be played.\
  New to Eternity.\
  \
  \
- **Sor2DthInit**\
  Type: Miscellaneous, normal\
  Purpose: Part of D\'Sparil (2nd form) death sequence. The object\'s
  Counter 0 field will be set to 7 so that the death frame sequence
  loops 7 times, and all living entities with the same friendliness as
  the dying object will be massacred (that is, if a friendly D\'Sparil
  dies, all friends die, and if an enemy D\'Sparil dies, all enemies
  die). New to Eternity.\
  Thunk: Yes.\
  \
  \
- **Sor2DthLoop**\
  Type: Miscellaneous, normal\
  Purpose: Part of D\'Sparil (2nd form) death sequence. Each call to
  this codepointer decrements Counter 0 field set by Sor2DthInit by one.
  If the field is still greater than zero, nothing will happen.
  Otherwise, the object will be transferred to the S_SOR2_DIE4 frame and
  the death loop ends. New to Eternity.\
  \
  \
- **PodPain**\
  Type: Miscellaneous, normal\
  Purpose: Called when pods are damaged. There is a 50% chance that this
  codepointer will do nothing. Provided it takes action, there is a
  16/256 chance that the codepointer will perform its action twice
  rather than once. Depending upon whether it chooses once or twice, it
  will spawn one or two PodGoo objects at the source object\'s (x,y)
  location and its z + 48 units. The objects will be given randomized
  momenta away from the pod. New to Eternity.\
  Thunk: Yes.\
  \
  \
- **RemovePod**\
  Type: Miscellaneous, normal\
  Purpose: Part of pod generator code, called when a pod is destroyed.
  This codepointer uses an internal field of the object to find the pod
  generator which spawned this pod. If that field is valid, it will
  decrement the PodGenerator\'s Counter 0 field so that the generator
  will be able to spawn more pods in the future.\
  New to Eternity.\
  Thunk: Yes.\
  \
  \
- **MakePod**\
  Type: Miscellaneous, normal\
  Purpose: Part of pod generator code, called by pod generators to spawn
  pods. The object\'s Counter 0 field is used to track the number of
  pods currently owned by this generator. If that number is higher than
  16, no more pods will be spawned (this limit is necessary to avoid
  overcrowding of areas with pods). Provided that there are not too many
  pods, the generator will attempt to create a new Pod object at its
  location. If the generator\'s location is blocked by a solid object,
  the pod will be removed immediately. Otherwise, the pod object will
  transfer to the S_POD_GROW1 frame and will emit the Heretic NEWPOD
  sound effect. It will then be given random momenta, and one of its
  internal fields will be set to point back to the pod generator so that
  the generator can later be notified of the pod\'s destruction. New to
  Eternity.\
  Thunk: Yes.\
  \
  \
- **DripBlood**\
  Type: Miscellaneous, normal\
  Purpose: A HereticBlood object will be spawned at a random location
  within a small distance of the source object, will be given randomized
  x and y momenta, and will be given the LOGRAV Bits2 flag. New to
  Eternity.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#miscsnd}

------------------------------------------------------------------------

**Miscellaneous / Sound Pointers**

------------------------------------------------------------------------

- **StartFire**\
  Type: Miscellaneous / sound, normal\
  Purpose: Plays the \"flamst\" sound and calls the Fire codepointer on
  the object. Part of the Archvile flame attack sequence.\
  \
  \
- **FireCrackle**\
  Type: Miscellaneous / sound, normal\
  Purpose: Plays the \"flame\" sound and calls the Fire codepointer on
  the object. Part of the Archvile flame attack sequence.\
  \
  \
- **SkelWhoosh**\
  Type: Miscellaneous / sound, normal\
  Purpose: Causes the object to face its target and play the \"skeswg\"
  sound effect.\
  Thunk: Yes.\
  \
  \
- **FatRaise**\
  Type: Miscellaneous / sound, normal\
  Purpose: Causes the object to face its target and play the \"manatk\"
  sound effect.\
  Thunk: Yes.\
  \
  \
- **SpawnSound**\
  Type: Miscellaneous / sound, normal\
  Purpose: The object will emit the \"boscub\" sound effect, and then
  control will transfer to the SpawnFly codepointer.

[Return to Table of Contents](#contents)

[]{#hticsnd}

------------------------------------------------------------------------

**Heretic Misc / Sound Pointers**

------------------------------------------------------------------------

- **Sor1Pain**\
  Type: Miscellaneous / sound, normal\
  Purpose: The object\'s Counter 0 field will be set to 20, so that it
  will walk faster when using the Sor1Chase codepointer. Control will
  then transfer to the Pain codepointer for normal pain sound playing.\
  New to Eternity.

[Return to Table of Contents](#contents)

[]{#objsound}

------------------------------------------------------------------------

**Object Sound Pointers**

------------------------------------------------------------------------

- **PlaySound**\
  Type: Object sound pointer, parameterized\
  \
  Parameter Information:
  - Unknown 1 = Sound DeHackEd number to play (must be valid, no
    default)
  - Unknown 2 = Select sound volume
    - 0 = normal (default)
    - 1 = full volume

  \
  Purpose: Causes the object to emit the sound indicated by the provided
  sound DeHackEd number, at the indicated volume level. New to MBF.\
  Notes: As of Eternity Engine v3.31 public beta 3, this codepointer
  will no longer crash the game if an invalid sound number is provided.
  If the sound number is invalid, nothing will happen.\
  Thunk: Yes.\
  \
  \
- **Pain**\
  Type: Object sound pointer, normal\
  Purpose: Plays the object\'s pain sound, if it has one.\
  Thunk: Yes.\
  \
  \
- **Scream**\
  Type: Object sound pointer, normal\
  Purpose: Plays the object\'s death sound, if it has one. If the object
  uses certain sounds as its death sound, it may play one of a set of
  random sounds as documented here:
  1.  If the object uses any of the \"podth1\", \"podth2\", or
      \"podth3\" sounds, it will play one of the three sounds at random.
  2.  If the object uses either of the \"bgdth1\" or \"bgdth2\" sounds,
      it will play one of the two sounds at random.

  If the object has the BOSS flag in its Bits2 field, it will play its
  death sound at full volume. This is used by most DOOM and Heretic
  bosses.\
  Thunk: Yes.\
  \
  \
- **XScream**\
  Type: Object sound pointer, normal\
  Purpose: Plays the \"slop\" sound. If the object is a player, falling
  damage is active, and the player has died from falling, the \"fallht\"
  sound will be played (this sound is subject to remapping by player
  skins).\
  Thunk: Yes.\
  \
  \
- **PlayerScream**\
  Type: Object sound pointer, normal\
  Purpose: Plays player death sounds. Normally plays \"pldeth\" sound.
  However, in games other than the shareware game, if the player dies
  with -50 or less health, it will play the \"pdiehi\" sound. If
  Eternity\'s falling damage effect is active, and the player has died
  from falling, the \"fallht\" sound will be played. Note that all of
  these sounds are subject to remapping by player skins.\
  Thunk: Yes.\
  \
  \
- **BrainPain**\
  Type: Object sound pointer, normal\
  Purpose: Plays the \"bospn\" sound effect at full volume.\
  Thunk: Yes.\
  \
  \
- **BrainAwake**\
  Type: Object sound pointer, normal\
  Purpose: Plays the \"bossit\" sound effect at full volume.\
  Notes: In DOOM, this codepointer was also responsible for initializing
  the internal list of Boss Spawn Spot objects. If the number of objects
  was incorrect, this code pointer could crash the game. However, this
  behavior has been moved, as of BOOM, to an internal level init
  function, and the limit on spawn spots has also been removed.\
  Thunk: Yes.\
  \
  \
- **VileStart**\
  Type: Object sound pointer, normal\
  Purpose: Plays the \"vilatk\" sound effect.\
  Thunk: Yes.\
  \
  \
- **AmbientThinker**\
  Type: Object sound pointer, normal\
  Purpose: Drives EDF ambience effects controlled by the EEAmbience
  object. This pointer expects to find the EDF ambience sequence index
  number in the args\[0\] field of the thing. If that number is invalid,
  this codepointer does nothing. It also utilizes the counter #0 field
  of the object as a ticker. In order to work properly, this codepointer
  must be called once every gametic (ie, in a frame of duration 1). New
  to Eternity Engine v3.33.50.\
  Thunk: Absolutely not.

[Return to Table of Contents](#contents)

[]{#aiptr}

------------------------------------------------------------------------

**AI Pointers**

------------------------------------------------------------------------

- **Look**\
  Type: AI pointer, normal\
  Purpose: Causes the object to look for targets. Various gameplay
  elements have an effect on acquiring a target. Friendly monsters will
  look for enemies first, but if they find none, will look for a player
  to follow. Enemies will look for players first, then friendly
  monsters. If none are found, they will remain asleep. Upon waking, a
  monster will play its alert sound. Use of certain alert sounds by a
  monster will cause it to randomly use one of a set of sounds, as
  documented here:
  1.  If object uses the \"posit1\", \"posit2\", or \"posit3\" sounds,
      it will use any one of these three sounds at random.
  2.  If object uses the \"bgsit1\" or \"bgsit2\" sounds, it will use
      one of these two sounds at random.

  If the object has the BOSS flag in its Bits2 field, it will play its
  alert sound at full-volume. This is used by most DOOM and Heretic
  bosses by default. After playing the sound, the object will transfer
  to its \"first walking\" state.\
  Notes: This codepointer should only be called from an object\'s
  initial spawn state. Calling it from a walking state can cause an
  infinite state cycle, which will cause an error message to appear.\
  \
  \
- **Chase**\
  Type: AI pointer, normal\
  Purpose: Causes an object to pursue its target.\
  \
  - If the object has lost its target (ie. it has died or been removed),
    the object will look for a new target (possibly remembering a prior
    one). If no targets are found, the object will return to its initial
    spawn state.\
    \
  - If the object has a target, it may attempt to attack:\
    \
    - If an object has just attacked prior to this, it will not attack
      again, unless the game skill is at \"Nightmare!\" level or is in
      \"fast\" mode. Instead, the enemy will choose a new direction to
      walk in.\
      \
    - If the object has a melee attack and is within 64 units of its
      target, it will transfer to its melee attack state.\
      \
    - If the object does not have a melee attack or is not close enough
      to use it, is within normal missile range (2048 units), and timing
      or skill level permitting, it will transfer to its missile attack
      state.

    \
  - Depending on the actor\'s internal \"threshold\" value, which
    decrements slowly as it chases a target, it may choose a new target
    to attack when it cannot attack its current one.\
    \
    - Friendly objects may choose a new player to follow, or may look
      for other friendly objects who are in critical condition (low
      health). If a friendly object finds a friend in trouble, it may
      adopt that friend\'s target as its own.\
      \
    - Enemies will look for new players first, and then friendly
      monsters. If advanced strafing logic is active, and the monster is
      moving, it may decide to strafe away from its target.

    \
  - Once all moving and attack logic has been passed, the object has a
    3/256 (1.17%) chance of playing its active (roaming) sound, if it
    has one.\
    \
    - If an object has the ACTSEESOUND flag in its Bits3 field, it has a
      50% chance of playing its alert sound instead of its active
      sound.\
      \
    - If an object has the LOUDACTIVE flag in its Bits3 field, it will
      play which ever sound it chooses at full volume.

  \
  Notes: This codepointer should only be called from the walking frames
  of a sentient object. There may be undesired effects or undefined
  behavior in other circumstances.\
  \
  As of Eternity Engine v3.33.01, this codepointer will not crash the
  game if used in an object\'s spawn state or attack states. However, an
  error message may be printed until the object leaves the offending
  state(s), as using the pointer in this context is NOT appropriate.\
  \
  \
- **VileChase**\
  Type: AI pointer, normal\
  Purpose: Causes an object to look for monster corpses as it walks.
  When a corpse is encountered which has a resurrection state, is
  currently in a frame with a -1 tics value, and is not currently
  blocked by any other creature, the object will stop beside it and
  transfer to frame #266 (S_VILE_HEAL). The object being resurrected
  will emit the DSSLOP sound and will be placed into its first
  resurrection frame. The resurrected object\'s normal height, radius,
  health, and flags information will be restored. Friendliness of the
  object performing the resurrection will be transferred to the
  resurrected object (friendly objects resurrect friends, and vice
  versa). If no corpses are found to resurrect, control transfers to the
  Chase codepointer for normal attack and walking logic.\
  Notes: If comp_vile is set, crushed creatures will not have their
  height or radius information restored, and will become \"ghosts.\"\
  \
  \
- **FaceTarget**\
  Type: AI pointer, normal\
  Purpose: Makes an object face its target, if it has one. If the
  object\'s target is partially invisible (Bits field SHADOW), totally
  invisible (Bits2 field DONTDRAW), or a Heretic ghost (Bits3 field
  GHOST), the object will face toward the target, and then have a random
  amount between -45 and 45 degrees added to its angle.\
  Thunk: Yes.\
  \
  \
- **KeepChasing**\
  Type: AI pointer, normal\
  Purpose: Allows a monster to keep walking during its attack frames.
  Chase cannot be used in this context because it may cause the monster
  to acquire a new target while trying to attack its current one. Note
  the monster\'s attack frames will need to have a short duration for
  the motion to seem smooth. New to Eternity.\
  \
  \
- **RandomWalk**\
  Type: AI pointer, normal\
  Purpose: Causes an object to walk around in randomly chosen
  directions, whether it has no target or has acquired a target via
  being damaged. Neither friends nor enemies using this pointer will
  follow the player at any time. This pointer has a 24/256 chance of
  setting the object back to its spawn state once movement in one
  direction has stopped. If the spawn state doesn\'t call RandomWalk,
  this allows the object to periodically stop movement. It is possible
  for a thing using RandomWalk to transfer to use of normal AI via Look
  and Chase, but note that if a thing using RandomWalk walks into a
  sector where alerting sound has occurred at any time during gameplay,
  it would awaken instantly if the Look pointer was then used. Things
  using this pointer will not continue to walk into obstructions, but
  will choose a different random direction in which to walk. Things
  using this pointer never attack while walking, though it is possible
  to set up retaliatory attacks from a thing\'s other states, such as
  the pain state, in concert with the TargetJump codepointer.\
  New to Eternity Engine v3.33.01

[Return to Table of Contents](#contents)

[]{#hticai}

------------------------------------------------------------------------

**Heretic AI Pointers**

------------------------------------------------------------------------

- **Sor1Chase**\
  Type: AI pointer, normal\
  Purpose: D\'Sparil first form walking logic. If the object\'s Counter
  0 field has been set to a positive value by the Sor1Pain codepointer,
  the object will subtract 3 from its current tics value, causing it to
  walk faster (its tics value will not be set less than 1, to avoid
  state cycles). The Counter 0 field will be decremented by 1. Control
  then transfers to the Chase codepointer for normal walking logic.\
  New to Eternity.

[Return to Table of Contents](#contents)

[]{#aiwalk}

------------------------------------------------------------------------

**AI Pointers with Walking Sounds**

------------------------------------------------------------------------

- **Metal**\
  Type: AI pointer / Walking sound, normal\
  Purpose: The object will emit the \"metal\" sound effect, and then
  control will transfer to the Chase codepointer for normal walking and
  attack logic.\
  \
  \
- **BabyMetal**\
  Type: AI pointer / Walking sound, normal\
  Purpose: The object will emit the \"bspwlk\" sound effect, and then
  control will transfer to the Chase codepointer for normal walking and
  attack logic.\
  \
  \
- **Hoof**\
  Type: AI pointer / Walking sound, normal\
  Purpose: The object will emit the \"hoof\" sound effect, and then
  control will transfer to the Chase codepointer for normal walking and
  attack logic.

[Return to Table of Contents](#contents)

[]{#blast}

------------------------------------------------------------------------

**Blast Radius Attack Pointers**

------------------------------------------------------------------------

- **Explode**\
  Type: Blast radius attack, normal\
  Purpose: Spawns a blast radius at the object\'s location 128 units in
  radius, with maximum damage of 128, decreasing linearly with distance
  from the blast point. If the blast radius is 128 units or closer to
  the floor, a TerrainTypes hit will occur.\
  Thunk: Yes.\
  \
  \
- **Detonate**\
  Type: Blast radius attack, normal\
  Purpose: Spawns a blast radius at the object\'s location with a radius
  and maximum damage value equal to the object\'s missile damage value.
  Other effects are the same as Explode. New to MBF.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#hticblast}

------------------------------------------------------------------------

**Heretic Blast Radius Attack Pointers**

------------------------------------------------------------------------

- **HticExplode**\
  Type: Blast radius attack, parameterized\
  \
  Parameter Information:
  - Args1 = Select special explosion effect (default of 0 = 0)
    - 0 = Blast with radius and max damage of 128
    - 1 = D\'Sparil blue spark effect\
      Radius and max damage = 80 + (rnd % 32)
    - 2 = Maulotaur floor fire effect\
      Radius and max damage = 24
    - 3 = Timebomb of the Ancients effect\
      Object z increases 32 units; object translucency is disabled;
      Radius and max damage = 128

  \
  Purpose: Performs various Heretic explosion effects, selected by
  Args1. Higher argument values are reserved for addition of more
  effects and should not be used. If the blast radius is damage units or
  less above the floor, a TerrainTypes hit will occur. Special effects 2
  and 3 are new to Eternity Engine v3.31 Delta.\
  New to Eternity.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#monatk}

------------------------------------------------------------------------

**Monster Attack Pointers**

------------------------------------------------------------------------

- **PosAttack**\
  Type: Monster attack, normal\
  Purpose: Causes a Zombieman pistol attack. If the object\'s target is
  valid, it will turn to face it, play the \"pistol\" sound effect, and
  fire a single tracer with \"monster\" accuracy (very wide horizontal
  spread) for standard monster tracer damage of (rnd%5 + 1) \* 3.\
  Thunk: Yes.\
  \
  \
- **SPosAttack**\
  Type: Monster attack, normal\
  Purpose: Causes a Sergeant shotgun attack. If the object\'s target is
  valid, it will turn to face it, play the \"shotgn\" sound effect, and
  fire 3 tracers with \"monster\" accuracy for standard monster tracer
  damage of (rnd%5 + 1) \* 3.\
  Thunk: Yes.\
  \
  \
- **VileTarget**\
  Type: Monster attack, normal\
  Purpose: Causes the object to face its target, and spawns a VileFire
  object at the target\'s location. The Fire codepointer will be called
  on the VileFire object after it is spawned.\
  \
  \
- **VileAttack**\
  Type: Monster attack, normal\
  Purpose: If the object\'s target is valid and can currently be seen by
  the object, an Archvile explosion attack will occur. The object will
  face its target, emit the \"barexp\" sound, and damage the object
  directly for 20 points of damage. This attack will also impart an
  upward velocity to the target of (1000 / mass) units per tic. If the
  object has a currently spawned VileFire object, that object will emit
  a blast radius of 70 units, with maximum damage of 70 at the center of
  the explosion. This explosion can damage the attacking object if it is
  not immune to blast radius damage.\
  Thunk: Yes.\
  \
  \
- **SkelFist**\
  Type: Monster attack, normal\
  Purpose: If the object\'s target is valid and is within melee range
  (64 units) of the object, it will face its target, emit the DSSKEPCH
  sound and damage its target directly for (rnd%10 + 1) \* 6 damage.\
  Thunk: Yes.\
  \
  \
- **SkelMissile**\
  Type: Monster attack, Homing missile firing pointer, normal\
  Purpose: If the object\'s target is valid, it will face toward it and
  fire a TracerShot missile at it. The missile\'s tracer target will be
  set the to the object it was fired at.\
  Notes: Missiles fired by this pointer will be fired 16 units higher
  than the normal default of 32 units.\
  Thunk: Yes.\
  \
  \
- **FatAttack1**\
  Type: Monster attack, normal\
  Purpose: If the object\'s target is valid, it will face it, and then
  turn an additional +11.25 degrees. It will fire two MancubusShot
  missiles, one directly at the target, and another at +11.25 degrees.\
  Thunk: Yes.\
  \
  \
- **FatAttack2**\
  Type: Monster attack, normal\
  Purpose: If the object\'s target is valid, it will face it, and then
  turn an additional -11.25 degrees. It will fire two MancubusShot
  missiles, one directly at the target, and another at -11.25 degrees.\
  Thunk: Yes.\
  \
  \
- **FatAttack3**\
  Type: Monster attack normal\
  Purpose: If the object\'s target is valid, it will face it and fire
  two MancubusShot missiles, one at its angle plus 5.625 degrees, and
  the other at its angle minus 5.625 degrees.\
  Thunk: Yes.\
  \
  \
- **CPosAttack**\
  Type: Monster attack, normal\
  Purpose: Causes a Chaingunner chaingun attack. If the object\'s target
  is valid, it will turn to face it, play the \"chgun\" sound effect,
  and fire a single tracer with \"monster\" accuracy (very wide
  horizontal spread) for standard monster tracer damage of (rnd%5 + 1)
  \* 3.\
  Thunk: Yes.\
  \
  \
- **TroopAttack**\
  Type: Monster attack, normal\
  Purpose: Imp attack codepointer. If the object\'s target is valid, it
  will face toward it. If the target is within melee range (64 units) of
  the object, the object will emit the \"claw\" sound effect and damage
  the target directly for (rnd%8 + 1) \* 3 damage. Otherwise, the object
  will fire a DoomImpShot projectile at the target.\
  Thunk: Yes.\
  \
  \
- **SargAttack**\
  Type: Monster attack, normal\
  Purpose: Demon attack codepointer. If the object\'s target is valid
  and is within melee range (64 units) of the target, it will face the
  target and damage it directly for (rnd%10 + 1) \* 4 damage.\
  Thunk: Yes.\
  \
  \
- **HeadAttack**\
  Type: Monster attack, normal\
  Purpose: Cacodemon attack codepointer. If the object\'s target is
  valid, it will face toward it. If the target is within melee range (64
  units) of the object, the object will damage the target directly for
  (rnd%6 + 1) \* 10 damage. Otherwise, the object will fire a
  CacodemonShot projectile at the target.\
  Thunk: Yes.\
  \
  \
- **BruisAttack**\
  Type: Monster attack, normal\
  Purpose: Baron attack codepointer. If the object\'s target is valid,
  it will face toward it. If the target is within melee range (64 units)
  of the object, the object will emit the \"claw\" sound effect, and
  will damage the target directly for (rnd%8 + 1) \* 10 damage.
  Otherwise, the object will fire a BaronShot projectile at the target.\
  Thunk: Yes.\
  \
  \
- **SkullAttack**\
  Type: Monster attack, normal\
  Purpose: Lost soul attack codepointer. If the object\'s target is
  valid, it will face toward it and, if it has an attack sound, will
  emit it. The object\'s SKULLFLY Bits flag will be set, and it will
  hurdle toward its target at a speed of 20 units per second. Notes:
  Eternity will not crash if an object with no attack sound uses this
  codepointer. This was a common problem in earlier ports.\
  Thunk: Yes.\
  \
  \
- **BspiAttack**\
  Type: Monster attack, normal\
  Purpose: Arachnotron attack codepointer. If the object\'s target is
  valid, it will face toward it and fire one ArachnotronShot
  projectile.\
  Thunk: Yes.\
  \
  \
- **CyberAttack**\
  Type: Monster attack, normal\
  Purpose: Cyberdemon attack codepointer. If the object\'s target is
  valid, it will face toward it and fire one RocketShot projectile.\
  Thunk: Yes.\
  \
  \
- **PainAttack**\
  Type: Monster attack, normal\
  Purpose: Pain Elemental attack codepointer. If the object\'s target is
  valid, it will face toward it. If the comp_pain variable is asserted,
  the monster will check the number of lost souls on the level. If that
  number is higher than 20, no attack will occur. If the flag is not
  set, there is no limitation. The monster will then spawn one lost soul
  immediately in front of it and face it toward its own target. If the
  comp_skull variable is off, the lost soul will be destroyed
  immediately if it is spawned over a blocking line. The lost soul\'s
  target will be set equal to the monster\'s, and then the SkullAttack
  codepointer will be called on it such that it performs a lost soul
  attack.\
  Notes: The MBF friendliness flag\'s state is transferred to the
  spawned lost soul. Friendly Pain Elementals shoot friendly lost souls;
  enemy PE\'s fire enemies.\
  Thunk: Yes.\
  \
  \
- **BrainSpit**\
  Type: Monster attack, normal\
  Purpose: Boss brain spawn cube attack. If there are no boss spawn
  spots on the level, this function will do nothing. If the game is on
  the an easy skill level, this function will do nothing on every other
  call. Otherwise, a boss spawn spot will be selected by its map
  spawning order, and a BossSpawnCube object will be fired at it like a
  missile. The object will then emit the \"bospit\" sound effect.\
  Notes: The MBF friendliness flag will be transferred to the spawn cube
  from the firing object. This is so that the cube will later spawn
  either an enemy or friendly monster, depending on the friendliness of
  its originating boss brain.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#monparam}

------------------------------------------------------------------------

**Parameterized Monster Attacks**

------------------------------------------------------------------------

- **BetaSkullAttack**\
  Type: Monster attack, parameterized\
  \
  Parameter Information:
  - Object\'s damage field = damage multiplier

  \
  Purpose: If the object\'s target is invalid or is of the same type as
  this object, no attack will occur. Otherwise, the object will play its
  attack sound if it has one, and will face its target and damage it
  directly for (rnd%8 + 1) multiplied by its missile damage field. This
  is basically a long-range scratch. New to MBF.\
  Notes: As of Eternity Engine v3.31 public beta 3, this codepointer
  will not cause an error if the object does not have an attack sound.\
  Thunk: Yes.\
  \
  \
- **Scratch**\
  Type: Monster attack, parameterized\
  \
  Parameter Information:
  - Unknown 1 = Amount of damage to inflict (default of 0 = 0)
  - Unknown 2 = DeHackEd number of sound to play (default of 0 = no
    sound will be played)
  - Args1 = Special mode toggle (default of 0 = 0)
    - 0 = Compatibility mode (Use value in \"Unknown 1\" as normal)
    - 1 = Use object damage field
    - 2 = Use counter specified in Args2
  - Args2 = Counter field number for special mode 2 (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2

  \
  Purpose: If the object\'s target is valid and is within melee range
  (64 units), it will damage the target for the indicated amount. If the
  Unknown 2 parameter is non-zero and is a valid sound DeHacked number,
  the indicated sound will played along with the attack. New to MBF.\
  Notes: As of Eternity Engine v3.31 public beta 3, this codepointer
  will no longer crash the game if an invalid sound number is provided.
  No sound will be played if the number is invalid.\
  \
  Parameters Args1 and Args2 are new to Eternity Engine v3.31 Delta, and
  allow getting a damage value from other sources. If Args1 is 1, the
  object\'s damage value will be used. If Args1 is 2, the value of the
  counter specified in Args2 will be used. When combined with SetCounter
  or CounterOp, this allows you to perform your own custom damage
  calculations. This change does not break compatibility, as the Args1
  default value is 0.\
  Thunk: Yes.\
  \
  \
- **MissileAttack**\
  Type: Monster attack, Homing missile firing pointer, parameterized\
  \
  Parameter Information:
  - Args1 = DeHackEd number of thing type to fire (no default, must be
    valid)
  - Args2 = Select homing property (default of 0 = 0)
    - 0 = will not home
    - 1 = may home
  - Args3 = Amount to add to standard z missile firing height (32
    units)\
    (default of 0 = 0, can be negative)
  - Args4 = Amount to add to actor angle in degrees\
    (default of 0 = 0, can be negative)
  - Args5 = DeHackEd number of state to enter for optional melee attack\
    (default of 0 = 0, negative values = no melee attack)

  \
  Purpose: Very powerful parameterized monster projectile attack
  pointer. Can be used to create standard attacks or angular spreads,
  with or without homing. Can fire missiles at a customized height, and
  can optionally chain to another state when the object is within melee
  range of its target, allowing situational attacks like those used by
  the Imp, Baron, and Cacodemon. The Args5 parameter should be set to -1
  unless a valid frame DeHackEd number is provided. New to Eternity
  Engine v3.31 public beta 4.\
  Notes: Missiles must have a homing maintenance pointer in their frames
  in order to home.\
  Thunk: Yes.\
  \
  \
- **MissileSpread**\
  Type: Monster attack, parameterized\
  \
  Parameter Information:
  - Args1 = DeHackEd number of thing type to fire (no default, must be
    valid)
  - Args2 = Number of missiles to fire (MUST be greater than or equal to
    2)
  - Args3 = Amount to add to standard z missile firing height (32
    units)\
    (default of 0 = 0, can be negative)
  - Args4 = Total angular sweep of attack in degrees\
    (default of 0 = 0)
  - Args5 = DeHackEd number of state to enter for optional melee attack\
    (default of 0 = 0, negative values = no melee attack)

  \
  Purpose: Convenience codepointer for firing a barrage of missiles in
  an angular sweep. The object will face its target and begin firing
  missiles at its angle minus half the Args4 sweep value. It will then
  step Args4 / (Args2 - 1) degrees for each additional missile,
  spreading them out evenly, centered on the actor\'s angle. Thus, the
  angular sweep is the total separation between the first and last
  missiles in degrees. This codepointer can only be used to fire 2 or
  more missiles. Providing a value of 0 or 1 will cause it to stop
  without attacking. The Args5 parameter can be used to specify a frame
  to use for an optional melee attack, used when the object is within
  melee range of its target. This parameter should be set to -1 unless a
  valid frame DeHackEd number is provided.\
  New to Eternity Engine v3.31 public beta 6.\
  Thunk: Yes.\
  \
  \
- **BulletAttack**\
  Type: Monster attack, parameterized\
  \
  Parameter Information:
  - Args1 = DeHackEd number of sound to play (default of 0 = no sound)
  - Args2 = Select accuracy (default of 0 = 1)
    - 1 = Always accurate
    - 2 = Never accurate\
      (moderate horizontal spread with no vertical error)
    - 3 = Super Shotgun accuracy\
      (wide horizontal spread with moderate vertical error)
    - 4 = Monster accuracy (very wide horizontal spread)
  - Args3 = Number of bullet tracers to fire (default of 0 = 0)
  - Args4 = Damage factor (Damage formula is: dmgfactor \*
    ((rnd%dmgmod) + 1))
  - Args5 = Damage modulus\
    (Forced into range of 1 to 256)

  \
  Purpose: Powerful codepointer which allows construction of custom
  monster bullet attacks. This pointer is capable of emulating any of
  the built-in player or monster bullet attacks, as well creating many
  completely new ones (including a super shotgun attack for monsters).
  New to Eternity Engine v3.31 public beta 6.\
  Notes: The Args2 parameter for this pointer differs slightly from the
  Args2 parameter to the FireCustomBullets player codepointer. The value
  which allows \"first fire only\" accuracy is not available for
  monsters.\
  Thunk: Yes.\
  \
  \
- **ThingSummon**\
  Type: Monster attack, parameterized\
  \
  Parameter Information:
  - Args1 = DeHackEd num of thing type to summon (default of 0 = thing
    0)
  - Args2 = Prestep distance (added to distance between thing and
    summoner)
  - Args3 = Z distance of object above summoner\'s z coordinate.
  - Args4 = One of the following values:
    - 0: If the object can\'t fit, it will be killed.
    - 1: If the object can\'t fit, it will be removed.
  - Args5 = One of the following values:
    - 0: The object is normal.
    - 1: The object is made a child of the summoner.

  \
  Purpose: Powerful codepointer which allows custom Pain Elemental style
  attacks. Objects will not be spawned in locations where they are
  stuck, or across blocking lines. There is no limit on the number of
  objects spawned when using this pointer. New to Eternity Engine
  v3.33.33.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#hticmon}

------------------------------------------------------------------------

**Heretic Monster Attack Pointers**

------------------------------------------------------------------------

- **MummyAttack**\
  Type: Heretic monster attack, normal\
  Purpose: Golem attack. If the object\'s target is invalid, nothing
  will occur. Otherwise, it will emit its attack sound if it has one. If
  its target is within melee range (64 units), it will damage its target
  directly for (rnd%8 + 1) \* 2 damage, and play the MUMAT2 sound
  effect. If the target was not within melee range, no attack occurs and
  the MUMAT1 sound effect is played. New to Eternity.\
  Thunk: Yes.\
  \
  \

- **MummyAttack2**\
  Type: Heretic monster attack, Homing missile firing pointer, normal\
  Purpose: Golem Leader attack. If the object\'s target is invalid,
  nothing will occur. Otherwise, if the object\'s target is within melee
  range (64 units), it will receive direct damage for (rnd%8 + 1) \* 2
  HP. Otherwise, a GolemShot projectile will be fired at the target, and
  will be set to home on it. New to Eternity.\
  Thunk: Yes.\
  \
  \

- **ClinkAttack**\
  Type: Heretic monster attack, normal\
  Purpose: Sabreclaw attack. If the object\'s target is invalid, nothing
  will occur. Otherwise, the object will play its attack sound if it has
  one. If the object\'s target is within melee range (64 units), it will
  be damaged directly for (rnd%7 + 3) points. New to Eternity.\
  Thunk: Yes.\
  \
  \

- **WizardAtk1**\
  Type: Heretic monster attack, normal\
  Purpose: Disciple attack. The object will face its target and turn off
  the GHOST Bits3 flag. New to Eternity.\
  Thunk: Yes.\
  \

- **WizardAtk2**\
  Type: Heretic monster attack, normal\
  Purpose: Disciple attack. The object will face its target and turn on
  the GHOST Bits3 flag. New to Eternity.\
  Thunk: Yes.\
  \

- **WizardAtk3**\
  Type: Heretic monster attack, normal\
  Purpose: The object will turn off the GHOST Bits3 flag. If its target
  is invalid, nothing further will be done. Otherwise, the object will
  play its attack sound if it has one. If the target is within melee
  range (64 units), it will be damaged directly for (rnd%8 + 1) \* 4
  points. Otherwise, 3 DiscipleShot projectiles will be fired, one
  directly ahead, and the other two 5.625 degrees to the left and right.
  New to Eternity.\
  Thunk: Yes.\
  \
  \

- **Srcr2Decide**\
  Type: Heretic monster attack, normal\
  Purpose: D\'Sparil (2nd form) teleportation pointer. D\'Sparil
  teleports more and more as his life is lower. The exact probability
  formula is complicated, but is 192/256 at maximum. When D\'Sparil
  teleports, a DsparilTeleSpot object is chosen at random as a
  destination. D\'Sparil transfers to the S_SOR2_TELE1 frame and is
  teleported to the destination spot. A DsparilTeleFade object (which by
  default looks like D\'Sparil himself) is spawned at his original
  location, and the Heretic TELEPT sound is played at both locations.\
  New to Eternity.\
  \
  \

- **Srcr2Attack**\
  Type: Heretic monster attack, normal\
  Purpose: D\'Sparil (2nd form) attack pointer. If the object\'s target
  is invalid, nothing will occur. Otherwise, the object\'s attack sound
  will be played if it has one. If the target is within melee range, the
  target will be damaged directly for (rnd%8 + 1) \* 20 points.
  Otherwise, one of two missile attacks will occur. The chance of the
  first attack, Disciple spawning, is dependent upon the object\'s
  health. If its current health is half or more of its spawn health,
  then the chance is 48/256. If it is less than half, the chance
  increases to 96/256. If Disciple spawning is chosen, two DsparilShot2
  projectiles will be spawned at the object\'s angle plus and minus 45
  degrees, and the MBF friendliness flag is transferred to the
  projectiles so that they spawn Disciples with the same friendliness.
  If Disciple spawning is not chosen, a DsparilShot1 projectile will be
  fired at the target.\
  New to Eternity.\
  Thunk: Yes.\
  \
  \

- **KnightAttack**\
  Type: Heretic monster attack, normal\
  Purpose: Heretic Death Knight attack. If the object\'s target is
  invalid, nothing will occur. If the object\'s target is within melee
  range (64 units), it will be hit directly for (rnd%8 + 1) \* 3 damage,
  and the object will emit the Heretic KGTAT2 sound effect. Otherwise,
  the object will emit its attack sound, if it has one, and one of two
  projectiles will be fired. If the object using this pointer is a
  DeathKnightGhost object, KnightAxeRed will always be thrown.
  Otherwise, there is a 40/256 chance that KnightAxeRed will be thrown.
  If KnightAxeRed is not thrown, KnightAxe will be thrown instead. New
  to Eternity.\
  Thunk: Yes.\
  \
  \

- **BeastAttack**\
  Type: Heretic monster attack, normal\
  Purpose: Heretic Weredragon attack. If the object\'s target is
  invalid, nothing will occur. Otherwise, the object will emit its
  attacksound, if it has one. Then, if the target is within melee range
  (64 units), it will be hit directly for (rnd%8 + 1) \* 3 damage. If
  the target is further away, a WeredragonShot missile will be fired at
  it.\
  New to Eternity.\
  Thunk: Yes.\
  \
  \

- **BeastPuff**\
  Type: Heretic monster attack, parameterized\
  \
  Parameter Information:
  - Args1 = Momentum application toggle (default of 0 = 0)
    - 0 = momentum away from parent is applied to object
    - 1 = no momentum will be applied

  \
  Purpose: Spawns damaging smoke trail behind WeredragonShot
  projectiles. There is a 64/256 chance that no action will be taken.
  Provided that action is taken, a WeredragonSmoke object will be
  spawned within a small random distance of the source object and will
  be marked as if though it was fired by the object who fired the source
  object. This prevents the source object from taking damage, and
  properly credits it with any kill. The Args1 parameter is new to
  Eternity Engine v3.31 Delta, and allows the application of momentum to
  the smoke object to be disabled. Note that when momentum is not
  applied, the smoke will not do any damage.\
  New to Eternity.\
  Thunk: Yes.\
  \
  \

- **SnakeAttack**\
  Type: Heretic monster attack, normal\
  Purpose: Ophidian attack 1. If the object\'s target is invalid, the
  object will be set to its seestate. Otherwise, the object will emit
  its attack sound, if it has one, will face its target, and will fire
  one OphidianShotA missile. New to Eternity.\
  \
  \

- **SnakeAttack2**\
  Type: Heretic monster attack, normal\
  Purpose: Ophidian attack 2. If the object\'s target is invalid, the
  object will be set to its seestate. Otherwise, the object will emit
  its attack sound, if it has one, will face its target, and will fire
  one OphidianShotB missile. New to Eternity.\
  \
  \

- **Srcr1Attack**\
  Type: Heretic monster attack, normal\
  Purpose: D\'Sparil first form attack. If the object\'s target is
  invalid, nothing will occur. Otherwise the object will play its attack
  sound, if it has one. If the object is within melee range of the
  target, it will damage the target directly for ((rnd % 8) + 1) \* 8
  damage. Otherwise, it will decide on a missile attack. If the object
  has more than 66% of its spawn health, it will fire one SRDsparilShot1
  missile. Otherwise, it will fire three SRDsparilShot1 missiles, one
  straight ahead and the other two at +/- 3 degrees from its angle.
  Additionally, if the thing\'s health is less than 33% of its spawn
  health, and it has not just attacked before, it will transfer to frame
  S_SRCR1_ATK4 in order to attack again. New to Eternity.\
  \
  \

- **VolcanoBlast**\
  Type: Heretic monster attack, normal\
  Purpose: Volcano eruption. A random number of VolcanoBall objects
  between 1 and 3 will be spawned at the object\'s location and at a
  height of 44 units above the object. Each VolcanoBall object will be
  given a randomized angle and 1 unit of momentum along that angle. Each
  object will then be given a randomized positive z momentum. Each
  object will emit the ht_bstatk sound.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \

- **VolcBallImpact**\
  Type: Heretic monster attack, normal\
  Purpose: Splits VolcanoBall objects into multiple VolcanoBallSmall
  objects. If the object is on the floor of its sector, it will have the
  Bits GRAVITY and Bits2 LOGRAV flags cleared, and will have its
  position set to 28 units above the floor. The object will then emit a
  blast radius with radius and maximum damage of 25. Four
  VolcanoBallSmall objects will then be spawned and will have this
  object\'s target passed on to them. Each successive VolcanoBallSmall
  object will be thrown out at 90 degree increments, will be given 0.7
  momentum along that angle, and will be given a small, randomized
  positive z momentum.\
  New to Eternity Engine v3.31 Delta.\
  \
  \

- **MinotaurAtk1**\
  Type: Heretic monster attack, normal\
  Purpose: Maulotaur melee attack. If the object\'s target is invalid,
  nothing will occur. Otherwise, the object will emit the ht_stfpow
  sound. If the object is within melee range, it will damage its target
  directly for (rnd % 8 + 1) \* 4 damage. If the target is a player, the
  player will have his view height temporarily reduced by 16 units.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \

- **MinotaurDecide**\
  Type: Heretic monster attack, normal\
  Purpose: Chooses a Maulotaur attack. If the object\'s target is
  invalid, nothing will occur. Otherwise, the object will emit the
  ht_minsit sound. An attack will then be chosen as follows:
  - If the target\'s head height is between the object\'s floor and
    head, the target is between 64 and 512 units away, and a random
    result yields less than 150, the object will transfer to frame
    S_MNTR_ATK4_1 without calling any codepointer in that state, the
    Bits SKULLFLY flag is set on the object, the object faces its
    target, the object is given 13 momentum along its angle, and the
    object\'s Counter 0 field is set to 17.
  - Provided the above attack does not occur, if the target is on the
    floor of its sector, is less than 576 units away, and a random
    result yields less than 220, the object will transfer to frame
    S_MNTR_ATK3_1 and its Counter 1 field will be set to 0.
  - If neither of the above actions is chosen, the object will face its
    target.

  New to Eternity Engine v3.31 Delta.\
  \
  \

- **MinotaurAtk2**\
  Type: Heretic monster attack, normal\
  Purpose: Maulotaur fire array. If the object\'s target is invalid,
  nothing will occur. Otherwise, the object will emit the ht_minat2
  sound. If the target is within melee range, it will be hit directly
  for (rnd % 8 + 1) \* 5 damage. Otherwise, 5 MinotaurShot missiles will
  be fired, each at the object\'s z + 40 units. The first is fired along
  the object\'s current angle. The other 4 are fired at +/-2.8125 and
  +/-5.625 degrees from the object\'s angle and with the same z momentum
  as the first missile. The first missile will emit the ht_minat2
  sound.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \

- **MinotaurAtk3**\
  Type: Heretic monster attack, normal\
  Purpose: Maulotaur floor fire attack. If the object\'s target is
  invalid, nothing will occur. Otherwise, if the object\'s target is
  within melee range, it will be hit directly for (rnd % 8 + 1) \* 5
  damage. Otherwise, a MaulotaurFloorFire missile will be spawned on the
  object\'s sector\'s floor and the object will emit the ht_minat2
  sound. If a random result yields less than 192 and the object\'s
  Counter 1 field is equal to 0, the object will transfer to frame
  S_MNTR_ATK3_4 in order to attack again, and its Counter 1 field will
  be set to zero.\
  New to Eternity Engine v3.31 Delta.\
  \
  \

- **MinotaurCharge**\
  Type: Heretic monster attack, normal\
  Purpose: Maulotaur charge maintenance. If the object\'s Counter 0
  field is greater than zero, a HereticPhoenixPuff object will be
  spawned at the object\'s location and will be given positive z
  momentum of 2 units per tic. The object\'s Counter 0 field will then
  be decremented. Otherwise, the object will have the Bits SKULLFLY flag
  cleared and it will be set to its first walking frame.\
  New to Eternity Engine v3.31 Delta.\
  \
  \

- **MntrFloorFire**\
  Type: Heretic monster attack, normal\
  Purpose: Floor fire maintenence. The object will be forced to the
  floor of its sector. A MaulotaurFloorFlame object will be spawned at a
  small randomized distance from this object on the floor, and this
  object\'s target will be transferred to the new object. The flame
  object will be given the smallest possible amount of x momentum and
  then will be checked for an immediate missile collision.\
  New to Eternity Engine v3.31 Delta.\
  \
  \

- **LichFire**\
  Type: Heretic monster attack, normal\
  Purpose: Iron Lich fire attack. Called by LichAttack. If the object\'s
  target is invalid, nothing will occur. Otherwise, an IronLichShot3
  object will be fired at the standard z missile firing height, and will
  be set to frame S_LICHFX3_4 so that it does not call the LichFireGrow
  pointer. The object will then emit the ht_hedat1 sound. 5 more
  IronLichShot3 objects will then be spawned at the first object\'s z
  height, are given the same target, momenta, and angle as the first
  object, have their damage set to zero, and have their Counter 0 field
  set to (i + 1) \* 2, where i is the zero-based number of the missile
  currently being spawned.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \

- **LichWhirlwind**\
  Type: Heretic monster attack, Homing missile firing pointer, normal
  Purpose: Iron Lich whirlwind attack. Called by LichAttack. If the
  object\'s target is invalid, nothing will occur. Otherwise, a
  LichWhirlwind object will be spawned at the actor\'s z height and will
  be set to home on the object\'s target. The LichWhirlwind object\'s
  Counter 0 field is set to 20 \* 35, its Counter 1 field is set to 50,
  and its Counter 2 field is set to 60. The Counter 1 and Counter 2
  fields are required by the Bits3 flag EXPLOCOUNT, which keeps this
  missile from exploding unless Counter 1 is greater than Counter 2.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \

- **LichAttack**\
  Type: Heretic monster attack, normal\
  Purpose: Iron Lich attack decision. If the object\'s target is
  invalid, nothing will occur. Otherwise, the object will face its
  target. If the target is within melee range, the target will be hit
  directly for (rnd % 8 + 1) \* 6 damage. Otherwise, one of the
  following attacks will be chosen as shown in the following table, with
  probabilities depending on the distance to the target:


                 Attack     Distance < 512   Distance >= 512
                 -------------------------------------------
                 Ice                   20%               60%
                 Fire                  40%               20%
                 Whirlwind             40%               20%

  If ice is chosen, an IronLichShot1 missile is fired at the standard
  missile firing height and the object emits the ht_hedat2 sound. If
  fire is chosen, control transfers to the LichFire codepointer.
  Otherwise, control transfers to the LichWhirlwind codepointer.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \

- **LichIceImpact**\
  Type: Heretic monster attack, normal\
  Purpose: Iron Lich ice ball impact. 8 IronLichShot2 missiles will be
  spawned at the object\'s exact location, each being fired at a 45
  degree increment from the last. Each of the new missiles is given the
  same target as the parent object. The missiles are given -0.6 z
  momentum so that they slowly descend.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \

- **LichFireGrow**\
  Type: Heretic monster attack, normal\
  Purpose: Iron Lich fire pillar maintenance. The object\'s z coordinate
  is increased by 9 units. The object\'s Counter 0 field is decremented.
  If the Counter 0 field is equal to 0, the object\'s normal damage
  value is restored and it is set to frame S_LICHFX3_4.\
  New to Eternity Engine v3.31 Delta.\
  \
  \

- **ImpChargeAtk**\
  Type: Heretic monster attack, normal\
  Purpose: Imp charge attack. If the actor\'s target is invalid, or a
  random result yields more than 64, nothing will occur. Otherwise, the
  object will play its attack sound if it has one, then it will face its
  target. The object\'s SKULLFLY Bits flag will be set, and it will
  hurdle toward its target at a speed of 12 units per second.\
  New to Eternity Engine v3.31 Delta.\
  \
  \

- **ImpMeleeAtk**\
  Type: Heretic monster attack, normal\
  Purpose: Imp scratch attack. If the object\'s target is invalid,
  nothing will occur. Otherwise, the object will emit its attack sound
  if it has one, and if the target is within melee range, it will be hit
  directly for (rnd % 8) + 5 damage.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \

- **ImpMissileAtk**\
  Type: Heretic monster attack, normal\
  Purpose: Imp Leader fireball attack. If the object\'s target is
  invalid, nothing will occur. Otherwise, the object will emit its
  attack sound if it has one. If the target is within melee range, it
  will be hit directly for (rnd % 8) + 5 damage, otherwise a
  HereticIMpShot projectile will be fired at the target at the normal
  missile firing height.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#home}

------------------------------------------------------------------------

**Homing Missile Maintenance Pointers**

------------------------------------------------------------------------

- **Tracer**\
  Type: Homing missile maintenance pointer, normal\
  Purpose: Performs homing maintenance for Revenant missiles. If the
  missile was fired by a homing-missile-firing codepointer, it may home
  on its target. However, this pointer will cause the missile to fail to
  home depending upon the gametic at which it is called. If the gametic
  is not divisible by 3, no actions will occur. Otherwise, one
  BulletPuff and one TracerSmoke object will be spawned behind the
  missile, the TracerSmoke object will be given upward momentum, and the
  missile will home on its target.\
  Notes: As mentioned above, missiles do not always home when using this
  codepointer. Also, missiles must be fired by a homing-missile- firing
  codepointer or else no homing will take place.\
  \
  \
- **GenTracer**\
  Type: Homing missile maintenance pointer, normal\
  Purpose: Performs generic homing maintenance. If a missile was fired
  by a homing-missile-firing codepointer, it will home on its target. No
  other special effects will occur. New to Eternity.

[Return to Table of Contents](#contents)

[]{#htichome}

------------------------------------------------------------------------

**Heretic Homing Missile Maintenance Pointers**

------------------------------------------------------------------------

- **HticTracer**\
  Type: Homing missile maintenance pointer, parameterized\
  \
  Parameter Information:
  - Args1 = Threshold angle, integer
  - Args2 = Max turn angle, integer

  \
  Purpose: Performs Heretic-style homing missile maintenance. The
  threshold and max turn parameters affect when the missile will home
  and how far at maximum that it can turn, respectively. Different
  monsters use different values for these, and they can feasibly be set
  to any value from 0 to 359 degrees. Vertical adjustment of Heretic-
  style homing missiles is very conservative. Vertical momentum will
  only be adjusted if the missile is outside the height range of its
  target. As a result, the missile can hit the ground easily.\
  New to Eternity.\
  \
  \
- **WhirlwindSeek**\
  Type: Homing missile maintenance pointer, normal\
  Purpose: Performs homing and movement logic maintenance for Iron Lich
  whirlwinds.\
  The object\'s Counter 0 value is decremented by 3. If the resulting
  value is less than zero, the object\'s momenta are set to zero, the
  object is set to its deathstate, and the MISSILE flag is removed from
  the object.\
  The object\'s Counter 1 value is decremented by three. If the
  resulting value is less than 0, the object\'s Counter 1 value is set
  to 58 + (rnd % 32). The object will emit the ht_hedat3 sound when this
  occurs.\
  If the object\'s tracer target has died or has acquired the Bits3
  GHOST flag, it will check its originator for a new target. If its
  originator has a new target which is alive and, if the originator is
  friendly, is not also a friend, this object will acquire the
  originator\'s target as its new tracer target. If the tracer target is
  valid, it will be homed on via the effects of HticTracer using a
  threshold value of 10 degrees and a maxturn value of 30 degrees.\
  New to Eternity Engine v3.31 Delta.

[Return to Table of Contents](#contents)

[]{#los}

------------------------------------------------------------------------

**Line of Sight Checking Pointers**

------------------------------------------------------------------------

- **CPosRefire**\
  Type: Line of sight checking pointer, normal\
  Purpose: Checks the line of sight between the attacking object and its
  target. If the line has been blocked, the object will transfer back to
  its first walking frame. If the object is friendly and has hit a
  friend by accident, it may stop firing. If the object is friendly and
  is retaliating against a friend\'s attack, it will fire only a small,
  random number of shots before stopping.\
  Thunk: Yes.\
  \
  \
- **SpidRefire**\
  Type: Line of sight checking pointer, normal\
  Purpose: Checks the line of sight between the attacking object and its
  target. If the line has been blocked, the object will transfer back to
  its first walking frame. If the object is friendly and has hit a
  friend by accident, it may stop firing. If the object is friendly and
  is retaliating against a friend\'s attack, it will fire only a small,
  random number of shots before stopping.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#specmon}

------------------------------------------------------------------------

**Special Monster Death Effects**

------------------------------------------------------------------------

- **PainDie**\
  Type: Monster death / attack, normal\
  Purpose: The Fall codepointer will be called on the object, and then
  it will perform 3 consecutive Lost Soul spawnings following the same
  procedure as that of the PainAttack codepointer, but each at a
  different angle.\
  Thunk: Yes.\
  \
  \

- **KeenDie**\
  Type: Monster death / \"Magic\" codepointer, normal\
  Purpose: The Fall codepointer will be called on the object. It will
  then check the level to see if all objects of the type which called
  this pointer are dead. If so, all sectors tagged 666 will be opened as
  normal, stay-open doors.\
  Notes: This \"magic\" pointer works on all maps and with any type of
  monster.\
  \
  \

- **BossDeath**\
  Type: \"Magic\" codepointer, normal\
  Purpose: Performs level completion actions for the DOOM and DOOM II
  game modes. The action performed is currently specific to certain
  maps. All members of all species bearing the listed Bits2 flag must be
  dead for the action to occur (this flag feature is new to Eternity,
  and allows customization of episode boss monster types).


                 MAP      FLAG REQUIRED  ACTION TAKEN ON DEATH OF ALL FLAG BEARERS
                 -------  -------------  -------------------------------------------
                 E1M8     E1M8BOSS       Lowers floors tagged 666 to lowest neighbor
                 E2M8     E2M8BOSS       Exits the level if player(s) still alive
                 E3M8     E3M8BOSS       Exits the level if player(s) still alive
                 E4M6     E4M6BOSS       Opens sectors tagged 666 as blazing doors
                 E4M8     E4M8BOSS       Lowers floors tagged 666 to lowest neighbor
                 MAP07    MAP07BOSS1     Lowers floors tagged 666 to lowest neighbor
                 MAP07    MAP07BOSS2     Raises floors tagged 667 by lower texture

  \

- **BrainDie**\
  Type: \"Magic\" codepointer, normal\
  Purpose: Causes a normal exit from the level. Notes: This pointer does
  not check for other boss brain objects, nor does it check to make sure
  that players are still alive before exiting the level.\
  Thunk: Yes.\
  \
  \

- **KillChildren**\
  Type: Monster Death / \"Magic\" codepointer, parameterized\
  Parameter Information:
  - Args1 = One of the following values:
    - 0: All child things will be killed.
    - 1: All child things will be removed from the game.

  \
  Purpose: Kills or removes all things marked as children of the current
  thing. Things are only marked as children when summoned by specific
  codepointers, currently including ThingSummon. New to Eternity Engine
  v3.33.33.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#hticspecmon}

------------------------------------------------------------------------

**Heretic Special Monster Death Effects**

------------------------------------------------------------------------

- **HticBossDeath**\
  Type: \"Magic\" codepointer, normal\
  Purpose: Performs level completion actions for the Heretic game mode.
  The action performed is currently specific to certain maps. All
  members of all species bearing the listed Bits2/Bits3 flag must be
  dead for the action to occur (this flag feature is new to Eternity,
  and allows customization of episode boss monster types).


                 MAP      FLAG REQUIRED     ACTION TAKEN ON DEATH OF ALL FLAG BEARERS
                 -------  ----------------  -------------------------------------------
                 E1M8     E1M8BOSS          Lowers all floors tagged 666
                 E2M8     E2M8BOSS          Lowers all floors tagged 666
                 E3M8     E3M8BOSS          Lowers all floors tagged 666
                 E4M8     E4M8BOSS          Lowers all floors tagged 666
                 E5M8     E5M8BOSS (Bits3)  Lowers all floors tagged 666

  On all episodes other than episode 1, all monsters with the same
  friendliness as the dying object will be massacred as well.\
  New to Eternity.\
  \
  \

- **SorcererRise**\
  Type: Monster death / attack, normal\
  Purpose: D\'Sparil first form death. Removes the SOLID flag from this
  object, and then spawns a Dsparil object at its location. The Dsparil
  object will then be placed into the S_SOR2_RISE1 frame.\
  New to Eternity.\
  \
  \

- **ImpDeath**\
  Type: Monster death\
  Purpose: Heretic imp normal death setup. The SOLID Bits flag will be
  cleared from the object, and the FOOTCLIP Bits2 flag will be set. If
  the object is at or below its sector\'s floor height and it possesses
  a crashstate, it will transfer to its crashstate.\
  New to Eternity Engine v3.31 Delta.\
  \
  \

- **ImpXDeath1**\
  Type: Monster death\
  Purpose: Heretic imp extreme death setup 1. The SOLID Bits flag will
  be cleared from the object, and the NOGRAVITY Bits flag and FOOTCLIP
  Bits2 flag will both be set. The object\'s Counter 0 field will be set
  to 666. New to Eternity Engine v3.31 Delta.\
  \
  \

- **ImpXDeath2**\
  Type: Monster death\
  Purpose: Heretic imp extreme death setup 2. The NOGRAVITY Bits flag
  will be cleared from the object. If the object is at or below its
  sector\'s floor height and it possesses a crashstate, it will transfer
  to its crashstate. New to Eternity Engine v3.31 Delta.\
  \
  \

- **ImpExplode**\
  Type: Monster death\
  Purpose: Heretic imp crashstate action. One HereticImpChunk1 and one
  HereticImpChunk2 object will be spawned at the object\'s location and
  given randomized x and y momenta. Both objects are given positive z
  momentum of 9 units per tic. If the object\'s Counter 0 field is equal
  to 666, the object will transfer to the S_IMP_XCRASH1 frame. New to
  Eternity Engine v3.31 Delta.

[Return to Table of Contents](#contents)

[]{#nukespecs}

------------------------------------------------------------------------

**Massacre Cheat Specials**

------------------------------------------------------------------------

- **PainNukeSpec**\
  Type: Massacre cheat special, normal\
  Purpose: Special action taken for Pain Elementals when the \"killem\"
  cheat is activated by the player. The PainDie codepointer is called on
  the object, so that Lost Souls will be spawned while the cheat is
  still killing monsters, and then the object will be transfered to
  state S_PAIN_DIE6. Specified via EDF only. New to Eternity.\
  \
  \
- **SorcNukeSpec**\
  Type: Massacre cheat special, normal\
  Purpose: Special action taken for D\'Sparil\'s first form when the
  \"killem\" cheat is activated by the player. The Fall codepointer is
  called on the object, and then it is set to state S_SRCR1_DIE17 using
  an internal function which causes the codepointer for that state to
  not be called. This prevents spawning of D\'Sparil\'s second form.
  Specified via EDF only. New to Eternity.

[Return to Table of Contents](#contents)

[]{#frscript}

------------------------------------------------------------------------

**Frame Scripting Pointers**

------------------------------------------------------------------------

General Notes for Frame Scripting Codepointers:

These pointers allow a degree of programmatic control over an object\'s
behavior, and are therefore termed \"frame scripting\" codepointers.
Note that some other codepointers utilize the Counter fields of a thing,
and so using those pointers along with these will cause interference.
Codepointer descriptions now make note of which Counter fields they
utilize, so be sure to check for conflicts.

Counter fields may have values from -32768 to 32767. Values above or
below this range will wrap around.

- **RandomJump**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Unknown 1 = Frame DeHackEd number to which object may jump\
    (default of 0 = frame 0)
  - Unknown 2 = Chance out of 256 that jump will occur (default of 0 =
    0%)

  \
  Purpose: Causes the object to transfer to the indicated frame with the
  indicated probability. If the jump does not occur, the object will
  fall through to the next frame. If the current frame\'s tics are set
  to -1, fall-through will not occur. New to MBF.\
  Notes: As of Eternity Engine v3.31 public beta 3, this codepointer
  will no longer crash the game if an invalid frame number is supplied.
  If the frame number is invalid, no jump will occur.\
  \
  \
- **HealthJump**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Frame DeHackEd number to which object may jump\
    (default of 0 = frame 0)
  - Args2 = Type of comparison to perform on object\'s health\
    (default of 0 = 0)
    - 0 = Jump if health \< Args3
    - 1 = Jump if health \<= Args3
    - 2 = Jump if health \> Args3
    - 3 = Jump if health \>= Args3
    - 4 = Jump if health equals Args3
    - 5 = Jump if health does not equal Args3
    - 6 = Jump if health & Args3 (bitwise AND operation)
    - 7 - 13 = The value in Args3 will be interpreted as a counter
      number (0 - 2). The resulting operation is the health compared
      against the value of the requested counter, subtracting 6 from the
      operation number (ie. operation 7 is \"Jump if health \<
      counter\").
  - Args3 = Value to compare health against / Counter number holding
    value\
    (default of 0 = 0)

  \
  Purpose: Causes the object to transfer to the indicated frame if the
  value in Args3 (or the given counter, for operations 7 through 13)
  compares as requested to the object\'s current health. This allows
  \"frame scripting\" of actions which vary depending on how much damage
  an enemy or other object has received. If the jump does not occur, the
  object will fall through to the next frame. If the current frame\'s
  tics are set to -1, fall-through will not occur.\
  New to Eternity v3.31 public beta 7.\
  Notes: Options 6 and 13, \"bitwise and\", are most useful for
  performing an action only if the object\'s health is even or odd. To
  perform an action if the health value is odd, set Args3 to 1, and to
  perform an action if it is even, set Args3 to 2. Use of other values
  will result in periodic chances for an action, and will require
  experimentation.\
  \
  \
- **TargetJump**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Frame DeHackEd number to which object may jump\
    (default of 0 = frame 0)

  \
  Purpose: Causes the object to transfer to the indicated frame if the
  object\'s target meets the following criteria:
  1.  The target must be valid.
  2.  The target must be alive (health \> 0).
  3.  If this object has the Bits3 flag \"SUPERFRIEND\" set, its target
      must not be a friend.

  If the jump does not occur, the object will fall through to the next
  frame. If the current frame\'s tics are set to -1, fall-through will
  not occur.\
  New to Eternity v3.33.01\
  \
  \
- **CounterJump**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Frame DeHackEd number to which object may jump\
    (default of 0 = frame 0)
  - Args2 = Type of comparison to perform on counter\
    (default of 0 = 0)
    - 0 = Jump if counter \< Args3
    - 1 = Jump if counter \<= Args3
    - 2 = Jump if counter \> Args3
    - 3 = Jump if counter \>= Args3
    - 4 = Jump if counter equals Args3
    - 5 = Jump if counter does not equal Args3
    - 6 = Jump if counter & Args3 (bitwise AND operation)
    - 7 - 13 = The value in Args3 will be interpreted as a counter
      number (0 - 2). The resulting operation is the counter in Args3
      compared against the value of the counter in Args4, subtracting 7
      from the operation number (ie. operation 7 is \"Jump if counter1
      \< counter2\").
  - Args3 = Value to compare against counter / Counter number holding
    value (default of 0 = 0)
  - Args4 = Counter number to use (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2

  \
  Purpose: Causes the object to transfer to the indicated frame if the
  value in the indicated internal counter field compares as requested to
  the provided value. This allows \"frame scripting\" of actions which
  vary depending on the current value of a thing\'s counters. If the
  jump does not occur, the object will fall through to the next frame.
  If the current frame\'s tics are set to -1, fall-through will not
  occur.\
  New to Eternity Engine v3.31 Delta.\
  \
  \
- **CounterSwitch**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Counter number to use (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args2 = Frame DeHackEd number of first state in set\
    (default of 0 = frame 0)
  - Args3 = Number of states in set\
    (No default, must be valid)

  \
  Purpose: This powerful codepointer implements an N-way branch where
  the value of the indicated counter determines to which frame the jump
  will occur. The frames to be jumped to must be defined in a
  consecutive block in EDF. The first frame in this set is deemed to be
  frame #0, and the last frame in the set is frame #Args3 - 1. The value
  of the counter is considered to be zero-based, so that if it is equal
  to zero, the object will jump to frame #0 (the frame you provided in
  Args2). The entire range of frames must be valid, and if it is not, no
  jump will occur. If the counter value is not in the range from 0 to
  Args3 - 1, no jump will occur. Finally, if the counter number in Args1
  is invalid, no jump will occur.\
  New to Eternity Engine v3.31 Delta.\
  \
  \
- **SetCounter**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Counter number to set (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args2 = Value to utilize (default of 0 = 0)
  - Args3 = Operation to perform (default of 0 = 0)
    - 0 : Args1 = Args2 (direct assignment)
    - 1 : Args1 += Args2 (addition)
    - 2 : Args1 -= Args2 (subtraction)
    - 3 : Args1 \*= Args2 (multiplication)
    - 4 : Args1 /= Args2 (division)
    - 5 : Args1 %= Args2 (modulus)
    - 6 : Args1 &= Args2 (bitwise AND)
    - 7 : Args1 &= \~Args2 (compound bitwise AND / bitwise NOT)
    - 8 : Args1 \|= Args2 (bitwise OR)
    - 9 : Args1 \^= Args2 (bitwise XOR)
    - 10 : Args1 = rnd (random value, Args2 is not used)
    - 11 : Args1 = rnd % Args2 (random value modulus)
    - 13 : Args1 \<\<= Args2 (bitshift left)
    - 14 : Args1 \>\>= Args2 (bitshift right)

  \
  Purpose: Sets the indicated counter on this thing to a value resulting
  from an operation with the current counter value and the value
  provided in Args2. New to Eternity Engine v3.31 Delta.\
  Notes: For operation 4, division, Args2 may not be equal to zero, and
  no operation will occur if it is. For operations 5 and 11, modulus and
  random modulus, Args2 must be greater than zero. No operation will
  occur if it is less than or equal to zero. Operation 7 inverts the
  bits in Args2 before bitwise-ANDing it with the value in the counter.
  For operation 10, the value of Args2 is not used, and may simply be
  allowed to default. All random numbers are between 0 and 255
  inclusive. Operations 12 and 15-18 are not defined for this
  codepointer and will do nothing.\
  Thunk: Yes.\
  \
  \
- **CounterOp**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Counter number to use for operand 1 (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args2 = Counter number to use for operand 2 (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args3 = Counter number to set (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args4 = Operation to perform (no default)
    - 1 : Args3 = Args1 + Args2 (addition)
    - 2 : Args3 = Args1 - Args2 (subtraction)
    - 3 : Args3 = Args1 \* Args2 (multiplication)
    - 4 : Args3 = Args1 / Args2 (division)
    - 5 : Args3 = Args1 % Args2 (modulus)
    - 6 : Args3 = Args1 & Args2 (bitwise AND)
    - 8 : Args3 = Args1 \| Args2 (bitwise OR)
    - 9 : Args3 = Args1 \^ Args2 (bitwise XOR)
    - 12 : Args3 = Args1 \* ((rnd % Args2) + 1) (HITDICE calculation)
    - 13 : Args3 = Args1 \<\< Args2 (bitshift left)
    - 14 : Args3 = Args1 \>\> Args2 (bitshift right)
    - 15 : Args3 = \|Args1\| (absolute value)
    - 16 : Args3 = -Args1 (negate)
    - 17 : Args3 = !Args1 (logical NOT)
    - 18 : Args3 = \~Args1 (bitwise invert)

  \
  Purpose: Sets the indicated counter on this thing to a value resulting
  from an operation with one or two provided counter values. The operand
  and destination counters may be the same counter number.\
  New to Eternity Engine v3.31 Delta.\
  Notes: For operation 4, division, Args2 may not be equal to zero, and
  no operation will occur if it is. For operations 5 and 12, modulus and
  HITDICE calculation, Args2 must be greater than zero. No operation
  will occur if it is less than or equal to zero. Operation 12, HITDICE,
  is the operation used to calculate standard damage values in DOOM,
  where Args1 acts as a multiplier and Args2 acts as the modulus. All
  random numbers are between 0 and 255 inclusive. Operations 0, 7, 10,
  and 11 are not defined for this codepointer and will do nothing.
  Operations 15 through 18 are unary and do not use the Args2 parameter,
  but it must still be a valid counter number. You may allow it to
  default to zero.\
  Thunk: Yes.\
  \
  \
- **CopyCounter**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Source counter number (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2
  - Args2 = Destination counter number (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2

  \
  Purpose: Copies a thing\'s counter value to another one of its
  counters.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \
- **SetTics**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Base tics amount (35 tics per second) (default of 0 = 0)\
    -OR-\
    Counter number (default of 0 = 0)
  - Args2 = Randomizer modulus value (default of 0 = no randomization)
  - Args3 = Counter toggle (default of 0 = 0)
    - 0 = value in Args1 is a constant tics amount
    - 1 = value in Args1 is a counter number

  \
  Purpose: Allows setting a thing\'s current tics value to a counter,
  constant, or optionally randomized amount. If the randomizer modulus
  in Args2 is greater than zero, a random amount of time between 0 and
  Args2 - 1 tics will be added to the base amount. If the counter toggle
  parameter is 1, the Args1 parameter must be a valid counter number, or
  no action will occur.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.\
  \
  \
- **AproxDistance**\
  Type: Frame scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Destination counter number (default of 0 = 0)
    - 0 = Counter 0
    - 1 = Counter 1
    - 2 = Counter 2

  \
  Purpose: Returns an approximate distance measurement between the
  object and its current target in the indicated counter. If the object
  does not have a valid target, the value returned is -1. Combined with
  CounterJump, this can be used to branch to different attacks depending
  on a monster\'s distance to its target.\
  New to Eternity Engine v3.31 Delta.\
  Thunk: Yes.

[Return to Table of Contents](#contents)

[]{#small}

------------------------------------------------------------------------

**Small Scripting System**

------------------------------------------------------------------------

General Notes for Small Scripting Codepointers:

These codepointers allow execution of Small functions in either of the
Gamescript or Levelscript VMs. See the [Small Usage
Documentation](small_usagedoc.html) and the [Small Function
Reference](small_funcref.html) for full information.

- **StartScript**\
  Type: Small scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Number of public Script function to execute (default of 0 =
    \"Script0\")
  - Args2 = Select VM (default of 0 = 0)
    - 0 = Gamescript
    - 1 = Levelscript
  - Args3 = Parameter to script function #1
  - Args4 = Parameter to script function #2
  - Args5 = Parameter to script function #3

  \
  Purpose: Calls an appropriately named public Small function in the
  indicated VM, passing Args3 - Args5 as integer parameters. To be
  invoked via this function, a Small function must be declared as
  public, and must be named using the \"Script#\" syntax, where \# is
  the integer provided in Args1. The function should accept 3 integer
  parameters, although you are not required to use them inside the
  function.\
  Notes: If the indicated VM is not loaded, nothing will happen. If the
  VM is loaded but the indicated function does not exist, an error
  message will be printed to the console.\
  \
  \
- **PlayerStartScript**\
  Type: Small scripting, parameterized\
  \
  Parameter Information:
  - Args1 = Number of public Script function to execute (default of 0 =
    \"Script0\")
  - Args2 = Select VM (default of 0 = 0)
    - 0 = Gamescript
    - 1 = Levelscript
  - Args3 = Parameter to script function #1
  - Args4 = Parameter to script function #2
  - Args5 = Parameter to script function #3

  \
  Purpose: Calls an appropriately named public Small function in the
  indicated VM, passing Args3 - Args5 as integer parameters. To be
  invoked via this function, a Small function must be declared as
  public, and must be named using the \"Script#\" syntax, where \# is
  the integer provided in Args1. The function should accept 3 integer
  parameters, although you are not required to use them inside the
  function.\
  Notes: If the indicated VM is not loaded, nothing will happen. If the
  VM is loaded but the indicated function does not exist, an error
  message will be printed to the console.\
  WARNING: This codepointer is a Player Gun Frame codepointer, and
  CANNOT be used by monsters. Use the above StartScript pointer for
  monster frames instead.

[]{#edf}

------------------------------------------------------------------------

**EDF-Related Pointers**

------------------------------------------------------------------------

These pointers interact directly with parts of the EDF system. For more
information on EDF, see the [EDF Documentation](edf_ref.html).

- **ShowMessage**\
  Type: EDF utility, parameterized\
  \
  Parameter Information:
  - Args1 = ID number of EDF string to display as a message (default of
    0 = 0)
  - Args2 = Message display method (default of 0 = 0)
    - 0 = Console
    - 1 = Normal message
    - 2 = Center message

  \
  Purpose: Displays a text message defined as an EDF string to all
  players.\
  New to Eternity Engine v3.33.00.\
  Notes: If an EDF string with the indicated ID does not exist, nothing
  will happen.\
  Thunk: Yes.

[Return to Table of Contents](#contents)
