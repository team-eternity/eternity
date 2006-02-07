/*
    Eternity Engine Small Header
    Native functions for cameras
*/

#if defined _cam_included
  #endinput
#endif
#define _cam_included

// call to set the chasecam on or off
// * true = on, false = off
native _ToggleChasecam(bool:onoff);

// call to check if the chasecam is on or off
// * returns true if on, false if off
native _IsChasecamOn();

