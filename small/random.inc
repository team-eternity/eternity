/*
    Eternity Engine Small Header
    Native functions for random number generation
    
    All functions here return a pseudo-random integer between 0 and 
    255. Higher-period RNGs can be obtained by OR'ing together the 
    results of multiple calls.
*/

#if defined _rand_included
  #endinput
#endif
#define _rand_included

// gameplay random number generator -- use this for anything 
// potentially demo-sync critical
native _P_Random();

// non-gameplay RNG -- this is safe for cameras, particles, and 
// perhaps screen graphics -- do NOT use this if the thing in 
// question has any effect on gameplay
native _M_Random();

