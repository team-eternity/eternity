/*
    Eternity Engine Small Header
    Particle Effects
*/

#if defined _particle_included
  #endinput
#endif
#define _particle_included

// causes an explosion at the indicated location with the requested colors
native _PtclExplosionPos(Fixed:x, Fixed:y, Fixed:z, color1, color2);

// causes an explosion at all things with the given tid and the requested
// colors
native _PtclExplosionThing(tid, color1, color2);