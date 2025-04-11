//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
// Purpose: Freeverb algorithm implementation
//          Based on original public domain implementation by Jezar at Dreampoint
// Authors: James Haley, Max Waine
//

#include "z_zone.h"

#include "e_reverbs.h"
#include "i_sound.h"
#include "s_reverb.h"

//
// Defines and constants
//

#define NUMCOMBS     8
#define NUMALLPASSES 4
#define MUTED        0
#define FIXEDGAIN    0.015
#define SCALEWET     3
#define SCALEDRY     2
#define SCALEDAMP    0.4
#define SCALEROOM    0.28
#define OFFSETROOM   0.7
#define INITIALROOM  0.5
#define INITIALDAMP  0.5
#define INITIALWET   1.0/SCALEWET
#define INITIALDRY   0
#define INITIALWIDTH 1
#define INITIALMODE  0
#define INITIALDELAY 0
#define FREEZEMODE   0.5
#define STEREOSPREAD 23

#define COMBTUNINGL1 1116
#define COMBTUNINGR1 1116+STEREOSPREAD
#define COMBTUNINGL2 1188
#define COMBTUNINGR2 1188+STEREOSPREAD
#define COMBTUNINGL3 1277
#define COMBTUNINGR3 1277+STEREOSPREAD
#define COMBTUNINGL4 1356
#define COMBTUNINGR4 1356+STEREOSPREAD
#define COMBTUNINGL5 1422
#define COMBTUNINGR5 1422+STEREOSPREAD
#define COMBTUNINGL6 1491
#define COMBTUNINGR6 1491+STEREOSPREAD
#define COMBTUNINGL7 1557
#define COMBTUNINGR7 1557+STEREOSPREAD
#define COMBTUNINGL8 1617
#define COMBTUNINGR8 1617+STEREOSPREAD

#define ALLPASSTUNINGL1 556
#define ALLPASSTUNINGR1 556+STEREOSPREAD
#define ALLPASSTUNINGL2 441
#define ALLPASSTUNINGR2 441+STEREOSPREAD
#define ALLPASSTUNINGL3 341
#define ALLPASSTUNINGR3 341+STEREOSPREAD
#define ALLPASSTUNINGL4 225
#define ALLPASSTUNINGR4 225+STEREOSPREAD

//=============================================================================
//
// denorms
//

static inline void undenormalize(double &d)
{
   static const double anti_denormal = 1e-18;
   d += anti_denormal;
   d -= anti_denormal;
}

//=============================================================================
//
// delay
//

#define MAXDELAY 250u
#define MAXSR    44100u

static double delayBuffer[MAXDELAY*MAXSR/1000];

static size_t delaySize;
static size_t readPos;
static size_t writePos;

static void delay_clearBuffer()
{
   for(size_t i = 0; i < delaySize; i++)
      delayBuffer[i] = 0.0;
}

static void delay_set(size_t delayms, size_t sr = MAXSR)
{
   if(delayms > MAXDELAY)
      delayms = MAXDELAY;
   if(sr > MAXSR)
      sr = MAXSR;
   size_t curDelaySize = delaySize;
   delaySize = delayms * sr / 1000;

   if(delaySize != curDelaySize)
   {
      readPos  = 0;
      writePos = delaySize - 1;
      delay_clearBuffer();
   }
}

static void delay_writeSample(double sample)
{
   delayBuffer[writePos] = sample;
   if(++writePos >= delaySize)
      writePos = 0;
}

static double delay_readSample()
{
   double ret = delayBuffer[readPos];
   if(++readPos >= delaySize)
      readPos = 0;
   return ret;
}

//=============================================================================
//
// comb
//

class comb
{
public:
   double  feedback;
   double  filterstore;
   double  damp1;
   double  damp2;
   double *buffer;
   int     bufsize;
   int     bufidx;

   void setbuffer(double *buf, int size)
   {
      buffer  = buf;
      bufsize = size;
   }

   double process(double input)
   {
      double output;
      
      output = buffer[bufidx];
      undenormalize(output);

      filterstore = (output * damp2) + (filterstore * damp1);
      undenormalize(filterstore);

      buffer[bufidx] = input + (filterstore * feedback);

      if(++bufidx >= bufsize)
         bufidx = 0;

      return output;
   }

   void mute()
   {
      for(int i = 0; i < bufsize; i++)
         buffer[i] = 0;
   }

   void setdamp(double val)
   {
      damp1 = val;
      damp2 = 1 - val;
   }
};

//=============================================================================
//
// allpass
//

class allpass
{
public:
   double  feedback;
   double *buffer;
   int     bufsize;
   int     bufidx;

   void setbuffer(double *buf, int size)
   {
      buffer  = buf;
      bufsize = size;
   }

   double process(double input)
   {
      double output, bufout;

      bufout = buffer[bufidx];
      undenormalize(bufout);

      output = -input + bufout;
      buffer[bufidx] = input + (bufout * feedback);

      if(++bufidx >= bufsize)
         bufidx = 0;

      return output;
   }

   void mute()
   {
      for(int i = 0; i < bufsize; i++)
         buffer[i] = 0;
   }
};

//=============================================================================
//
// Equalizer
//

#define SND_PI       3.14159265
#define INITIALEQ    false
#define INITIALLG    1.0
#define INITIALMG    1.0
#define INITIALHG    1.0
#define INITIALLF    250.0
#define INITIALHF    4000.0

struct EQSTATE
{
   // Filter #1 (Low band)

   double  lf;       // Frequency
   double  f1p0;     // Poles ...
   double  f1p1;
   double  f1p2;
   double  f1p3;

   // Filter #2 (High band)

   double  hf;       // Frequency
   double  f2p0;     // Poles ...
   double  f2p1;
   double  f2p2;
   double  f2p3;

   // Sample history buffer

   double  sdm1;     // Sample data minus 1
   double  sdm2;     //                   2
   double  sdm3;     //                   3

   // Gain Controls

   double  lg;       // low  gain
   double  mg;       // mid  gain
   double  hg;       // high gain
};

struct eqparams_t
{
   double lowfreq;
   double highfreq;
   double lowgain;
   double midgain;
   double highgain;
};

static double do_3band(EQSTATE &es, double sample)
{
   static double vsa = (1.0 / 4294967295.0);
   
   // Locals
   double l, m, h;    // Low / Mid / High - Sample Values

   // Filter #1 (lowpass)
   es.f1p0  += (es.lf * (sample  - es.f1p0)) + vsa;
   es.f1p1  += (es.lf * (es.f1p0 - es.f1p1));
   es.f1p2  += (es.lf * (es.f1p1 - es.f1p2));
   es.f1p3  += (es.lf * (es.f1p2 - es.f1p3));

   l         = es.f1p3;

   // Filter #2 (highpass)
   es.f2p0  += (es.hf * (sample  - es.f2p0)) + vsa;
   es.f2p1  += (es.hf * (es.f2p0 - es.f2p1));
   es.f2p2  += (es.hf * (es.f2p1 - es.f2p2));
   es.f2p3  += (es.hf * (es.f2p2 - es.f2p3));

   h         = es.sdm3 - es.f2p3;

   // Calculate midrange (signal - (low + high))
   m         = es.sdm3 - (h + l); // haleyjd 07/05/10: which is right?

   // Scale, Combine and store
   l        *= es.lg;
   m        *= es.mg;
   h        *= es.hg;

   // Shuffle history buffer
   es.sdm3   = es.sdm2;
   es.sdm2   = es.sdm1;
   es.sdm1   = sample;                

   return (l + m + h);
}

static void init_3band(const eqparams_t &params, EQSTATE &eql, EQSTATE &eqr)
{
   // flush out state of equalizers
   memset(&eql, 0, sizeof(eql));
   memset(&eqr, 0, sizeof(eqr));

   // Set Low/Mid/High gains 
   eql.lg = eqr.lg = params.lowgain;
   eql.mg = eqr.mg = params.midgain;
   eql.hg = eqr.hg = params.highgain;

   // Calculate filter cutoff frequencies
   eql.lf = eqr.lf = 2 * sin(SND_PI * (params.lowfreq  / (double)MAXSR));
   eql.hf = eqr.hf = 2 * sin(SND_PI * (params.highfreq / (double)MAXSR));
}

static void clear_3band(EQSTATE &eq)
{
   eq.sdm1 = eq.sdm2 = eq.sdm3 = 0.0;
}

//=============================================================================
//
// revmodel
//

class revmodel
{
public:
   double gain;
   double roomsize, roomsize1;
   double damp, damp1;
   double wet, wet1, wet2;
   double dry;
   double width;
   double mode;
   size_t delay;
   bool   doEQ;

   // equalizer
   EQSTATE eql, eqr;
   eqparams_t eqparams;

   // comb filters
   comb combL[NUMCOMBS];
   comb combR[NUMCOMBS];

   // allpass filters
   allpass allpassL[NUMALLPASSES];
   allpass allpassR[NUMALLPASSES];

   // Buffers for the combs
   double bufcombL1[COMBTUNINGL1];
   double bufcombR1[COMBTUNINGR1];
   double bufcombL2[COMBTUNINGL2];
   double bufcombR2[COMBTUNINGR2];
   double bufcombL3[COMBTUNINGL3];
   double bufcombR3[COMBTUNINGR3];
   double bufcombL4[COMBTUNINGL4];
   double bufcombR4[COMBTUNINGR4];
   double bufcombL5[COMBTUNINGL5];
   double bufcombR5[COMBTUNINGR5];
   double bufcombL6[COMBTUNINGL6];
   double bufcombR6[COMBTUNINGR6];
   double bufcombL7[COMBTUNINGL7];
   double bufcombR7[COMBTUNINGR7];
   double bufcombL8[COMBTUNINGL8];
   double bufcombR8[COMBTUNINGR8];

   // Buffers for the allpasses
   double bufallpassL1[ALLPASSTUNINGL1];
   double bufallpassR1[ALLPASSTUNINGR1];
   double bufallpassL2[ALLPASSTUNINGL2];
   double bufallpassR2[ALLPASSTUNINGR2];
   double bufallpassL3[ALLPASSTUNINGL3];
   double bufallpassR3[ALLPASSTUNINGR3];
   double bufallpassL4[ALLPASSTUNINGL4];
   double bufallpassR4[ALLPASSTUNINGR4];

   revmodel()
   {
      combL[0].setbuffer(bufcombL1, COMBTUNINGL1);
      combR[0].setbuffer(bufcombR1, COMBTUNINGR1);
      combL[1].setbuffer(bufcombL2, COMBTUNINGL2);
      combR[1].setbuffer(bufcombR2, COMBTUNINGR2);
      combL[2].setbuffer(bufcombL3, COMBTUNINGL3);
      combR[2].setbuffer(bufcombR3, COMBTUNINGR3);
      combL[3].setbuffer(bufcombL4, COMBTUNINGL4);
      combR[3].setbuffer(bufcombR4, COMBTUNINGR4);
      combL[4].setbuffer(bufcombL5, COMBTUNINGL5);
      combR[4].setbuffer(bufcombR5, COMBTUNINGR5);
      combL[5].setbuffer(bufcombL6, COMBTUNINGL6);
      combR[5].setbuffer(bufcombR6, COMBTUNINGR6);
      combL[6].setbuffer(bufcombL7, COMBTUNINGL7);
      combR[6].setbuffer(bufcombR7, COMBTUNINGR7);
      combL[7].setbuffer(bufcombL8, COMBTUNINGL8);
      combR[7].setbuffer(bufcombR8, COMBTUNINGR8);
      allpassL[0].setbuffer(bufallpassL1, ALLPASSTUNINGL1);
      allpassR[0].setbuffer(bufallpassR1, ALLPASSTUNINGR1);
      allpassL[1].setbuffer(bufallpassL2, ALLPASSTUNINGL2);
      allpassR[1].setbuffer(bufallpassR2, ALLPASSTUNINGR2);
      allpassL[2].setbuffer(bufallpassL3, ALLPASSTUNINGL3);
      allpassR[2].setbuffer(bufallpassR3, ALLPASSTUNINGR3);
      allpassL[3].setbuffer(bufallpassL4, ALLPASSTUNINGL4);
      allpassR[3].setbuffer(bufallpassR4, ALLPASSTUNINGR4);

      // Set default values
      allpassL[0].feedback = 0.5;
      allpassR[0].feedback = 0.5;
      allpassL[1].feedback = 0.5;
      allpassR[1].feedback = 0.5;
      allpassL[2].feedback = 0.5;
      allpassR[2].feedback = 0.5;
      allpassL[3].feedback = 0.5;
      allpassR[3].feedback = 0.5;
      
      // set initial parameters
      wet      = INITIALWET * SCALEWET;
      roomsize = (INITIALROOM * SCALEROOM) + OFFSETROOM;
      dry      = INITIALDRY * SCALEDRY;
      damp     = INITIALDAMP * SCALEDAMP;
      width    = INITIALWIDTH;
      mode     = INITIALMODE;
      delay    = INITIALDELAY;
      delay_set(delay);
      doEQ = INITIALEQ;
      eqparams.lowgain  = INITIALLG;
      eqparams.midgain  = INITIALMG;
      eqparams.highgain = INITIALHG;
      eqparams.lowfreq  = INITIALLF;
      eqparams.highfreq = INITIALHF;
      update();

      // Buffer will be full of rubbish - so we MUST mute them
      mute();
   }

   double getMode()
   {
      return (mode >= FREEZEMODE);
   }

   void mute()
   {
      if(getMode() >= FREEZEMODE)
         return;

      for(int i = 0; i < NUMCOMBS; i++)
      {
         combL[i].mute();
         combR[i].mute();
      }
      for(int i = 0; i < NUMALLPASSES; i++)
      {
         allpassL[i].mute();
         allpassR[i].mute();
      }

      delay_clearBuffer();
      clear_3band(eql);
      clear_3band(eqr);
   }

   void processReplace(float *inputL, float *inputR, 
                       float *outputL, float *outputR,
                       int numsamples, int skip)
   {
      double outL, outR, input;

      while(numsamples-- > 0)
      {
         outL = outR = 0;
         input = (*inputL + *inputR) * gain;

         // pre-delay
         if(delay)
         {
            delay_writeSample(input);
            input = delay_readSample();
         }

         // accumulate comb filters in parallel
         for(int i = 0; i < NUMCOMBS; i++)
         {
            outL += combL[i].process(input);
            outR += combR[i].process(input);
         }

         // feed through allpasses in series
         for(int i = 0; i < NUMALLPASSES; i++)
         {
            outL = allpassL[i].process(outL);
            outR = allpassR[i].process(outR);
         }

         // equalization pass
         if(doEQ)
         {
            outL = do_3band(eql, outL);
            outR = do_3band(eqr, outR);
         }

         // calculate output replacing anything already there
         *outputL = (float)(outL * wet1 + outR * wet2 + *inputL * dry);
         *outputR = (float)(outR * wet1 + outL * wet2 + *inputR * dry);

         // increment sample pointers
         inputL  += skip;
         inputR  += skip;
         outputL += skip;
         outputR += skip;
      }
   }

   void processMix(float *inputL, float *inputR, 
                   float *outputL, float *outputR,
                   int numsamples, int skip)
   {
      double outL, outR, input;

      while(numsamples-- > 0)
      {
         outL = outR = 0;
         input = (*inputL + *inputR) * gain;

         // pre-delay
         if(delay)
         {
            delay_writeSample(input);
            input = delay_readSample();
         }

         // accumulate comb filters in parallel
         for(int i = 0; i < NUMCOMBS; i++)
         {
            outL += combL[i].process(input);
            outR += combR[i].process(input);
         }

         // feed through allpasses in series
         for(int i = 0; i < NUMALLPASSES; i++)
         {
            outL = allpassL[i].process(outL);
            outR = allpassR[i].process(outR);
         }

         // equalization pass
         if(doEQ)
         {
            outL = do_3band(eql, outL);
            outR = do_3band(eqr, outR);
         }

         // calculate output mixing with anything already there
         *outputL += (float)(outL * wet1 + outR * wet2 + *inputL * dry);
         *outputR += (float)(outR * wet1 + outL * wet2 + *inputR * dry);

         // increment sample pointers
         inputL  += skip;
         inputR  += skip;
         outputL += skip;
         outputR += skip;
      }
   }

   void update()
   {
      wet1 = wet * (width / 2 + 0.5);
      wet2 = wet * ((1 - width) / 2);

      if(mode >= FREEZEMODE)
      {
         roomsize1 = 1;
         damp1     = 0;
         gain      = MUTED;
      }
      else
      {
         roomsize1 = roomsize;
         damp1     = damp;
         gain      = FIXEDGAIN;
      }

      for(int i = 0; i < NUMCOMBS; i++)
      {
         combL[i].feedback = roomsize1;
         combL[i].setdamp(damp1);
         combR[i].feedback = roomsize1;
         combR[i].setdamp(damp1);
      }

      init_3band(eqparams, eql, eqr);
   }

   void setRoomSize(double value)
   {
      roomsize = (value * SCALEROOM) + OFFSETROOM;
   }

   double getRoomSize() const { return (roomsize - OFFSETROOM) / SCALEROOM; }

   void setDamp(double value)
   {
      damp = value * SCALEDAMP;
   }

   double getDamp() const { return damp / SCALEDAMP; }

   void setWet(double value)
   {
      wet = value * SCALEWET;
   }

   double getWet() const { return wet / SCALEWET; }

   void   setDry(double value) { dry = value * SCALEDRY; }
   double getDry() const       { return dry / SCALEDRY;  }

   void setWidth(double value)
   {
      width = value;
   }

   void setMode(double value)
   {
      mode = value;
   }

   void setDelay(size_t value)
   {
      size_t curdelay = delay;
      delay = value;
      if(delay != curdelay)
         delay_set(delay);
   }
};

static revmodel reverb;

//=============================================================================
//
// External Interface
//

void S_SuspendReverb()
{
   reverb.mute();
}

void S_ResumeReverb()
{
   reverb.mute();
}

//
// S_ReverbSetState
//
// Set the reverberation state from an ereverb definition.
//
void S_ReverbSetState(ereverb_t *ereverb)
{
   // if moving into an area with no effect, stop the reverb engine.
   if(!(ereverb->flags & REVERB_ENABLED))
   {
      reverb.mute();
      s_reverbactive = false;
      return;
   }

   // copy in parameters from the EDF reverb definition
   reverb.setRoomSize(ereverb->roomsize);
   reverb.setDamp(ereverb->dampening);
   reverb.setWet(ereverb->wetscale);
   reverb.setDry(ereverb->dryscale);
   reverb.setWidth(ereverb->width);
   reverb.setDelay((size_t)ereverb->predelay);

   if(ereverb->flags & REVERB_EQUALIZED)
   {
      reverb.doEQ = true;
      reverb.eqparams.lowfreq  = ereverb->eqlowfreq;
      reverb.eqparams.highfreq = ereverb->eqhighfreq;
      reverb.eqparams.lowgain  = ereverb->eqlowgain;
      reverb.eqparams.midgain  = ereverb->eqmidgain;
      reverb.eqparams.highgain = ereverb->eqhighgain;
   }
   else
      reverb.doEQ = false;

   reverb.update();

   s_reverbactive = true;
}

//
// Mix the reverb engine's output into the input sound stream.
//
void S_ProcessReverb(float *stream, const int samples, const int skip)
{
   reverb.processMix(stream, stream+1, stream, stream+1, samples, skip);
}

//
// S_ProcessReverbReplace
//
// Replace the input sound stream with the reverb engine's output.
//
void S_ProcessReverbReplace(float *stream, int samples)
{
   reverb.processReplace(stream, stream+1, stream, stream+1, samples, 2);
}

// EOF

