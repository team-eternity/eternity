//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Stephen McGranahan, et al.
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
//------------------------------------------------------------------------------
//
// Purpose: The actual span drawing functions.
// Here find the main potential for optimization,
// e.g. inline assembly, different algorithms.
//
// Authors: Stephen McGranahan, James Haley, Ioan Chera, Max Waine
//

#include "z_zone.h"
#include "doomstat.h"
#include "w_wad.h"
#include "r_draw.h"
#include "r_main.h"
#include "v_video.h"
#include "mn_engin.h"
#include "d_gi.h"
#include "r_plane.h"

/*
// Template structure for inlining constant values for orthogonal spans
template<int xshift, int yshift, int xmask>
struct spanconsts
{
    static constexpr inline int XShift() { return xshift; }
    static constexpr inline int YShift() { return yshift; }
    static constexpr inline int XMask()  { return xmask;  }
};

// Structure for inlining accesses to the cb_span_t structure for orthogonal spans
struct spanvalues
{
    inline static int XShift() { return span.xshift; }
    inline static int YShift() { return span.yshift; }
    inline static int XMask()  { return span.xmask;  }
};

// Template structure for inlining constant values for slope spans
template<int xshift, int xmask, int ymask>
struct slopeconsts
{
    static constexpr inline int XShift() { return xshift; }
    static constexpr inline int XMask()  { return xmask;  }
    static constexpr inline int YMask()  { return ymask;  }
};

// Structure for inlining accesses to the cb_span_t structure for slope spans
struct slopevalues
{
    inline static int XShift() { return span.xshift; }
    inline static int XMask()  { return span.xmask;  }
    inline static int YMask()  { return span.ymask;  }
};
*/

//==============================================================================
//
// R_DrawSpan_*
//
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//

// SoM: Why didn't I see this earlier? the spot variable is a waste now
// because we don't have the uber complicated math to calculate it now,
// so that was a memory write we didn't need!

// SoM: we only need 6 bits for the integer part (0 thru 63) so the rest
// can be used for the fraction part. This allows calculation of the memory
// address in the texture with two shifts, an OR and one AND. (see below)
// for texture sizes > 64 the amount of precision we can allow will decrease,
// but only by one bit per power of two (obviously)
// Ok, because I was able to eliminate the variable spot below, this function
// is now FASTER than doom's original span renderer. Whodathunkit?

template<int xshift, int yshift, int xmask>
static void R_DrawSpanSolid_8(const cb_span_t &span)
{
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    while(count-- > 0)
    {
        *dest  = colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]];
        dest  += linesize;
        xf    += xs;
        yf    += ys;
    }
}

static void R_DrawSpanSolid_8_GEN(const cb_span_t &span)
{
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    unsigned int xshift = span.xshift;
    unsigned int xmask  = span.xmask;
    unsigned int yshift = span.yshift;

    while(count-- > 0)
    {
        *dest  = colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]];
        dest  += linesize;
        xf    += xs;
        yf    += ys;
    }
}

#if 0
// SoM: Archive
// This is the optimized version of the original flat drawing function.
static void R_DrawSpan_OLD()
{
    unsigned int position;
    unsigned int step;

    byte *source;
    byte *colormap;
    byte *dest;

    unsigned int count;

    position = ((span.yfrac << 10) & 0xffff0000) | ((span.xfrac >> 6) & 0xffff);
    step     = ((span.ystep << 10) & 0xffff0000) | ((span.xstep >> 6) & 0xffff);

    source   = (byte *)(span.source);
    colormap = span.colormap;
    dest     = ylookup[span.y] + columnofs[span.x1];
    count    = span.x2 - span.x1 + 1;

    // SoM: So I went back and realized the error of ID's ways and this is even
    // faster now! This is by far the fastest way because it uses the exact same
    // number of ops mine does except it only has one addition and one memory
    // write for the position variable. Now we only have two writes to memory
    // (one for the pixel, one for position)

    while(count >= 4)
    {
        dest[0]   = colormap[source[(position >> 26) | ((position >> 4) & 4032)]];
        position += step;

        dest[1]   = colormap[source[(position >> 26) | ((position >> 4) & 4032)]];
        position += step;

        dest[2]   = colormap[source[(position >> 26) | ((position >> 4) & 4032)]];
        position += step;

        dest[3]   = colormap[source[(position >> 26) | ((position >> 4) & 4032)]];
        position += step;

        dest  += 4;
        count -= 4;
    }

    while(count)
    {
        *dest++   = colormap[source[(position >> 26) | ((position >> 4) & 4032)]];
        position += step;
        count--;
    }
}
#endif

// SoM: Removed the original span drawing code.

//==============================================================================
//
// TL Span Drawers
//
// haleyjd 06/21/08: TL span drawers are needed for double flats and for portal
// visplane layering.
//

template<int xshift, int yshift, int xmask>
static void R_DrawSpanTL_8(const cb_span_t &span)
{
    unsigned int              t;
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    while(count-- > 0)
    {
        t      = span.bg2rgb[*dest] + span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]];
        t     |= 0x01f07c1f;
        *dest  = RGB32k[0][0][t & (t >> 15)];
        dest  += linesize;
        xf    += xs;
        yf    += ys;
    }
}

static void R_DrawSpanTL_8_GEN(const cb_span_t &span)
{
    unsigned int              t;
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    unsigned int xshift = span.xshift;
    unsigned int xmask  = span.xmask;
    unsigned int yshift = span.yshift;

    while(count-- > 0)
    {
        t      = span.bg2rgb[*dest] + span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]];
        t     |= 0x01f07c1f;
        *dest  = RGB32k[0][0][t & (t >> 15)];
        dest  += linesize;
        xf    += xs;
        yf    += ys;
    }
}

// Additive blending

template<int xshift, int yshift, int xmask>
static void R_DrawSpanAdd_8(const cb_span_t &span)
{
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    while(count-- > 0)
    {
        unsigned int a, b;

        a      = span.bg2rgb[*dest] + span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]];
        b      = a;
        a     |= 0x01f07c1f;
        b     &= 0x40100400;
        a     &= 0x3fffffff;
        b      = b - (b >> 5);
        a     |= b;
        *dest  = RGB32k[0][0][a & (a >> 15)];
        dest  += linesize;
        xf    += xs;
        yf    += ys;
    }
}

static void R_DrawSpanAdd_8_GEN(const cb_span_t &span)
{
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    unsigned int xshift = span.xshift;
    unsigned int xmask  = span.xmask;
    unsigned int yshift = span.yshift;

    while(count-- > 0)
    {
        unsigned int a, b;

        a      = span.bg2rgb[*dest] + span.fg2rgb[colormap[source[((xf >> xshift) & xmask) | (yf >> yshift)]]];
        b      = a;
        a     |= 0x01f07c1f;
        b     &= 0x40100400;
        a     &= 0x3fffffff;
        b      = b - (b >> 5);
        a     |= b;
        *dest  = RGB32k[0][0][a & (a >> 15)];
        dest  += linesize;
        xf    += xs;
        yf    += ys;
    }
}

//==============================================================================
//
// Masked drawers (normal and TL -- additive doesn't need it because black is
// already transparent there)
//

// Keep this a macro to easily change to a byte set if needed
#define MASK(alpham, i) ((alpham)[(i)>>3] & 1 << ((i) & 7))

template<int xshift, int yshift, int xmask>
static void R_DrawSpanSolidMasked_8(const cb_span_t &span)
{
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    const byte *alpham = (byte *)span.alphamask;
    unsigned    i;

    while(count >= 4)
    {
        i = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            dest[0] = colormap[source[i]];
        xf += xs;
        yf += ys;
        i   = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            dest[linesize] = colormap[source[i]];
        xf += xs;
        yf += ys;
        i   = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            dest[linesize * 2] = colormap[source[i]];
        xf += xs;
        yf += ys;
        i   = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            dest[linesize * 3] = colormap[source[i]];
        xf    += xs;
        yf    += ys;
        dest  += linesize * 4;
        count -= 4;
    }
    while(count-- > 0)
    {
        i = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            *dest = colormap[source[i]];
        xf   += xs;
        yf   += ys;
        dest += linesize;
    }
}
static void R_DrawSpanSolidMasked_8_GEN(const cb_span_t &span)
{
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    unsigned int xshift = span.xshift;
    unsigned int xmask  = span.xmask;
    unsigned int yshift = span.yshift;

    const byte *alpham = (byte *)span.alphamask;
    unsigned    i;

    while(count >= 4)
    {
        i = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            dest[0] = colormap[source[i]];
        xf += xs;
        yf += ys;
        i   = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            dest[linesize] = colormap[source[i]];
        xf += xs;
        yf += ys;
        i   = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            dest[linesize * 2] = colormap[source[i]];
        xf += xs;
        yf += ys;
        i   = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            dest[linesize * 3] = colormap[source[i]];
        xf    += xs;
        yf    += ys;
        dest  += linesize * 4;
        count -= 4;
    }
    while(count-- > 0)
    {
        i = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
            *dest = colormap[source[i]];
        xf   += xs;
        yf   += ys;
        dest += linesize;
    }
}
template<int xshift, int yshift, int xmask>
static void R_DrawSpanTLMasked_8(const cb_span_t &span)
{
    unsigned int              t;
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    const byte *alpham = (byte *)span.alphamask;
    unsigned    i;

    while(count-- > 0)
    {
        i = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
        {
            t      = span.bg2rgb[*dest] + span.fg2rgb[colormap[source[i]]];
            t     |= 0x01f07c1f;
            *dest  = RGB32k[0][0][t & (t >> 15)];
        }
        xf   += xs;
        yf   += ys;
        dest += linesize;
    }
}
static void R_DrawSpanTLMasked_8_GEN(const cb_span_t &span)
{
    unsigned int              t;
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    unsigned int xshift = span.xshift;
    unsigned int xmask  = span.xmask;
    unsigned int yshift = span.yshift;

    const byte *alpham = (byte *)span.alphamask;
    unsigned    i;

    while(count-- > 0)
    {
        i = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
        {
            t      = span.bg2rgb[*dest] + span.fg2rgb[colormap[source[i]]];
            t     |= 0x01f07c1f;
            *dest  = RGB32k[0][0][t & (t >> 15)];
        }
        xf   += xs;
        yf   += ys;
        dest += linesize;
    }
}
template<int xshift, int yshift, int xmask>
static void R_DrawSpanAddMasked_8(const cb_span_t &span)
{
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    const byte *alpham = (byte *)span.alphamask;
    unsigned    i;

    while(count-- > 0)
    {
        i = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
        {
            unsigned int a, b;

            a      = span.bg2rgb[*dest] + span.fg2rgb[colormap[source[i]]];
            b      = a;
            a     |= 0x01f07c1f;
            b     &= 0x40100400;
            a     &= 0x3fffffff;
            b      = b - (b >> 5);
            a     |= b;
            *dest  = RGB32k[0][0][a & (a >> 15)];
        }
        xf   += xs;
        yf   += ys;
        dest += linesize;
    }
}
static void R_DrawSpanAddMasked_8_GEN(const cb_span_t &span)
{
    unsigned int              xf = span.xfrac, yf = span.yfrac;
    const unsigned int        xs = span.xstep, ys = span.ystep;
    const lighttable_t *const colormap = span.colormap;
    int                       count    = span.x2 - span.x1 + 1;

    const byte *const source = static_cast<const byte *>(span.source);
    byte             *dest   = R_ADDRESS(span.x1, span.y);

    unsigned int xshift = span.xshift;
    unsigned int xmask  = span.xmask;
    unsigned int yshift = span.yshift;

    const byte *alpham = (byte *)span.alphamask;
    unsigned    i;

    while(count-- > 0)
    {
        i = ((xf >> xshift) & xmask) | (yf >> yshift);
        if(MASK(alpham, i))
        {
            unsigned int a, b;

            a      = span.bg2rgb[*dest] + span.fg2rgb[colormap[source[i]]];
            b      = a;
            a     |= 0x01f07c1f;
            b     &= 0x40100400;
            a     &= 0x3fffffff;
            b      = b - (b >> 5);
            a     |= b;
            *dest  = RGB32k[0][0][a & (a >> 15)];
        }
        xf   += xs;
        yf   += ys;
        dest += linesize;
    }
}

//==============================================================================
//
// Slope span drawers
//

enum class maskedSlope_e : bool
{
    NO  = false,
    YES = true
};

struct Sampler
{
    struct Solid
    {
        inline static byte Sample(const byte fgColor, const byte bgColor, const cb_span_t &span) { return fgColor; }
    };

    struct Translucent
    {
        inline static byte Sample(const byte fgColor, const byte bgColor, const cb_span_t &span)
        {
            int t;

            t  = span.bg2rgb[bgColor] + span.fg2rgb[fgColor];
            t |= 0x01f07c1f;
            return RGB32k[0][0][t & (t >> 15)];
        }
    };

    struct Additive
    {
        inline static byte Sample(const byte fgColor, const byte bgColor, const cb_span_t &span)
        {
            unsigned int a, b;

            a  = span.bg2rgb[bgColor] + span.fg2rgb[fgColor];
            b  = a;
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            return RGB32k[0][0][a & (a >> 15)];
        }
    };
};

#if 0
// Perfect *slow* render
while(count--)
{
    float mul = 1.0f / id;

    int      u    = (int)(iu * mul);
    int      v    = (int)(iv * mul);
    unsigned texl = (v & 63) * 64 + (u & 63);

    *dest = src[texl];
    dest++;

    iu += ius;
    iv += ivs;
    id += ids;
}
#endif

#define SPANJUMP 16
#define INTERPSTEP (0.0625f)

#define DO_SLOPE_SAMPLE() \
    do                                                                                \
    {                                                                                 \
        colormap = cb_slopespan_t::colormap[mapindex++];                              \
        const unsigned int i = ((vfrac >> xshift) & xmask) | ((ufrac >> 16) & ymask); \
        if constexpr(masked == maskedSlope_e::NO)                                     \
            *dest = Sampler::Sample(colormap[src[i]], *dest, span);                   \
        else                                                                          \
        {                                                                             \
            if(MASK(alpham, i))                                                       \
                *dest = Sampler::Sample(colormap[src[i]], *dest, span);               \
        }                                                                             \
        dest  += linesize;                                                            \
        ufrac += ustep;                                                               \
        vfrac += vstep;                                                               \
    } while(false)

template<maskedSlope_e masked, typename Sampler, int xshift, int xmask, int ymask>
inline static void R_drawSlope_8(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    double iu = slopespan.iufrac, iv = slopespan.ivfrac;
    double ius = slopespan.iustep, ivs = slopespan.ivstep;
    double id = slopespan.idfrac, ids = slopespan.idstep;

    const byte *colormap;
    int         count;
    fixed_t     mapindex = slopespan.x1;

    if((count = slopespan.x2 - slopespan.x1 + 1) < 0)
        return;

    const byte *const src  = static_cast<const byte *>(slopespan.source);
    byte             *dest = R_ADDRESS(slopespan.x1, slopespan.y);

    const byte *alpham = static_cast<const byte *>(span.alphamask);

    while(count >= SPANJUMP)
    {
        double       ustart, uend;
        double       vstart, vend;
        double       mulstart, mulend;
        unsigned int ustep, vstep, ufrac, vfrac;
        int          incount;

        mulstart  = 65536.0f / id;
        id       += ids * SPANJUMP;
        mulend    = 65536.0f / id;

        // IMPORTANT: use this function to properly handle negative numbers. Merely casting is
        // non-standard between CPUs and will glitch out.
        ufrac  = R_doubleToUint32(ustart = iu * mulstart);
        vfrac  = R_doubleToUint32(vstart = iv * mulstart);
        iu    += ius * SPANJUMP;
        iv    += ivs * SPANJUMP;
        uend   = iu * mulend;
        vend   = iv * mulend;

        ustep = R_doubleToUint32((uend - ustart) * INTERPSTEP);
        vstep = R_doubleToUint32((vend - vstart) * INTERPSTEP);

        incount = SPANJUMP;
        while(incount--)
        {
            DO_SLOPE_SAMPLE();
        }

        count -= SPANJUMP;
    }
    if(count > 0)
    {
        double       ustart, uend;
        double       vstart, vend;
        double       mulstart, mulend;
        unsigned int ustep, vstep, ufrac, vfrac;
        int          incount;

        mulstart  = 65536.0f / id;
        id       += ids * count;
        mulend    = 65536.0f / id;

        ufrac  = R_doubleToUint32(ustart = iu * mulstart);
        vfrac  = R_doubleToUint32(vstart = iv * mulstart);
        iu    += ius * count;
        iv    += ivs * count;
        uend   = iu * mulend;
        vend   = iv * mulend;

        ustep = R_doubleToUint32((uend - ustart) / count);
        vstep = R_doubleToUint32((vend - vstart) / count);

        incount = count;
        while(incount--)
        {
            DO_SLOPE_SAMPLE();
        }
    }
}

template<maskedSlope_e masked, typename Sampler>
inline static void R_drawSlope_8_GEN(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    double iu = slopespan.iufrac, iv = slopespan.ivfrac;
    double ius = slopespan.iustep, ivs = slopespan.ivstep;
    double id = slopespan.idfrac, ids = slopespan.idstep;

    const byte *colormap;
    int         count;
    fixed_t     mapindex = slopespan.x1;

    if((count = slopespan.x2 - slopespan.x1 + 1) < 0)
        return;

    const byte *const src  = static_cast<const byte *>(slopespan.source);
    byte             *dest = R_ADDRESS(slopespan.x1, slopespan.y);

    const byte *alpham = static_cast<const byte *>(span.alphamask);

    unsigned int xshift = span.xshift;
    unsigned int xmask  = span.xmask;
    unsigned int ymask  = span.ymask;

    while(count >= SPANJUMP)
    {
        double       ustart, uend;
        double       vstart, vend;
        double       mulstart, mulend;
        unsigned int ustep, vstep, ufrac, vfrac;
        int          incount;

        mulstart  = 65536.0f / id;
        id       += ids * SPANJUMP;
        mulend    = 65536.0f / id;

        ufrac  = R_doubleToUint32(ustart = iu * mulstart);
        vfrac  = R_doubleToUint32(vstart = iv * mulstart);
        iu    += ius * SPANJUMP;
        iv    += ivs * SPANJUMP;
        uend   = iu * mulend;
        vend   = iv * mulend;

        ustep = R_doubleToUint32((uend - ustart) * INTERPSTEP);
        vstep = R_doubleToUint32((vend - vstart) * INTERPSTEP);

        incount = SPANJUMP;
        while(incount--)
        {
            DO_SLOPE_SAMPLE();
        }

        count -= SPANJUMP;
    }
    if(count > 0)
    {
        double       ustart, uend;
        double       vstart, vend;
        double       mulstart, mulend;
        unsigned int ustep, vstep, ufrac, vfrac;
        int          incount;

        mulstart  = 65536.0f / id;
        id       += ids * count;
        mulend    = 65536.0f / id;

        ufrac  = R_doubleToUint32(ustart = iu * mulstart);
        vfrac  = R_doubleToUint32(vstart = iv * mulstart);
        iu    += ius * count;
        iv    += ivs * count;
        uend   = iu * mulend;
        vend   = iv * mulend;

        ustep = R_doubleToUint32((uend - ustart) / count);
        vstep = R_doubleToUint32((vend - vstart) / count);

        incount = count;
        while(incount--)
        {
            DO_SLOPE_SAMPLE();
        }
    }
}

template<int xshift, int xmask, int ymask>
static void R_drawSlopeSolid_8(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8<maskedSlope_e::NO, Sampler::Solid, xshift, xmask, ymask>(slopespan, span);
}

static void R_drawSlopeSolid_8_GEN(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8_GEN<maskedSlope_e::NO, Sampler::Solid>(slopespan, span);
}

//==============================================================================
//
// TL Slope Drawers
//

template<int xshift, int xmask, int ymask>
static void R_drawSlopeTL_8(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8<maskedSlope_e::NO, Sampler::Translucent, xshift, xmask, ymask>(slopespan, span);
}

static void R_drawSlopeTL_8_GEN(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8_GEN<maskedSlope_e::NO, Sampler::Translucent>(slopespan, span);
}

// Additive blending

template<int xshift, int xmask, int ymask>
static void R_drawSlopeAdd_8(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8<maskedSlope_e::NO, Sampler::Additive, xshift, xmask, ymask>(slopespan, span);
}

static void R_drawSlopeAdd_8_GEN(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8_GEN<maskedSlope_e::NO, Sampler::Additive>(slopespan, span);
}

//==============================================================================
//
// Masked drawers (normal and TL -- additive doesn't need it because black is
// already transparent there)
//

template<int xshift, int xmask, int ymask>
static void R_drawSlopeSolidMasked_8(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8<maskedSlope_e::YES, Sampler::Solid, xshift, xmask, ymask>(slopespan, span);
}

static void R_drawSlopeSolidMasked_8_GEN(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8_GEN<maskedSlope_e::YES, Sampler::Solid>(slopespan, span);
}

template<int xshift, int xmask, int ymask>
static void R_drawSlopeTLMasked_8(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8<maskedSlope_e::YES, Sampler::Translucent, xshift, xmask, ymask>(slopespan, span);
}

static void R_drawSlopeTLMasked_8_GEN(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8_GEN<maskedSlope_e::YES, Sampler::Translucent>(slopespan, span);
}

template<int xshift, int xmask, int ymask>
static void R_drawSlopeAddMasked_8(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8<maskedSlope_e::YES, Sampler::Additive, xshift, xmask, ymask>(slopespan, span);
}

static void R_drawSlopeAddMasked_8_GEN(const cb_slopespan_t &slopespan, const cb_span_t &span)
{
    R_drawSlope_8_GEN<maskedSlope_e::YES, Sampler::Additive>(slopespan, span);
}

#undef SPANJUMP
#undef INTERPSTEP

//==============================================================================
//
// Span Engine Objects
//

// clang-format off

spandrawer_t r_spandrawer =
{
    // Orthogonal span drawers
    {
        // R_DrawSpan<32-2*n, 32-n, n 1s then n 0s> // 2^n x 2^n
        // NB: n 1s then n 0s can be represented as ((1 << n) - 1) << n
        
        // Solid
        {
            R_DrawSpanSolid_8<20, 26, 0x00FC0>, // 64x64
            R_DrawSpanSolid_8<18, 25, 0x03F80>, // 128x128
            R_DrawSpanSolid_8<16, 24, 0x0FF00>, // 256x256
            R_DrawSpanSolid_8<14, 23, 0x3FE00>, // 512x512
            R_DrawSpanSolid_8_GEN               // General
        },
        // Translucent
        {
            R_DrawSpanTL_8<20, 26, 0x00FC0>,    // 64x64
            R_DrawSpanTL_8<18, 25, 0x03F80>,    // 128x128
            R_DrawSpanTL_8<16, 24, 0x0FF00>,    // 256x256
            R_DrawSpanTL_8<14, 23, 0x3FE00>,    // 512x512
            R_DrawSpanTL_8_GEN                  // General
        },
        // Additive
        {
            R_DrawSpanAdd_8<20, 26, 0x00FC0>,   // 64x64
            R_DrawSpanAdd_8<18, 25, 0x03F80>,   // 128x128
            R_DrawSpanAdd_8<16, 24, 0x0FF00>,   // 256x256
            R_DrawSpanAdd_8<14, 23, 0x3FE00>,   // 512x512
            R_DrawSpanAdd_8_GEN                 // General
        },
        // Solid masked
        {
            R_DrawSpanSolidMasked_8<20, 26, 0x00FC0>, // 64x64
            R_DrawSpanSolidMasked_8<18, 25, 0x03F80>, // 128x128
            R_DrawSpanSolidMasked_8<16, 24, 0x0FF00>, // 256x256
            R_DrawSpanSolidMasked_8<14, 23, 0x3FE00>, // 512x512
            R_DrawSpanSolidMasked_8_GEN               // General
        },
        // Translucent masked
        {
            R_DrawSpanTLMasked_8<20, 26, 0x00FC0>,    // 64x64
            R_DrawSpanTLMasked_8<18, 25, 0x03F80>,    // 128x128
            R_DrawSpanTLMasked_8<16, 24, 0x0FF00>,    // 256x256
            R_DrawSpanTLMasked_8<14, 23, 0x3FE00>,    // 512x512
            R_DrawSpanTLMasked_8_GEN                  // General
        },
        // Additive masked
        {
            R_DrawSpanAddMasked_8<20, 26, 0x00FC0>,   // 64x64
            R_DrawSpanAddMasked_8<18, 25, 0x03F80>,   // 128x128
            R_DrawSpanAddMasked_8<16, 24, 0x0FF00>,   // 256x256
            R_DrawSpanAddMasked_8<14, 23, 0x3FE00>,   // 512x512
            R_DrawSpanAddMasked_8_GEN                 // General
        }
    },
    
    // SoM: Sloped span drawers
    {
        // R_DrawSlope<16-n, n 1s then n 0s, n 1s>
        // NB: n 1s can be represented as (1 << n) - 1
       
        {
            R_drawSlopeSolid_8<10, 0x00FC0, 0x03F>, // 64x64
            R_drawSlopeSolid_8< 9, 0x03F80, 0x07F>, // 128x128
            R_drawSlopeSolid_8< 8, 0x0FF00, 0x0FF>, // 256x256
            R_drawSlopeSolid_8< 7, 0x3FE00, 0x1FF>, // 512x512
            R_drawSlopeSolid_8_GEN                  // General
        },
        // Translucent
        {
            R_drawSlopeTL_8<10, 0x00FC0, 0x03F>,    // 64x64
            R_drawSlopeTL_8< 9, 0x03F80, 0x07F>,    // 128x128
            R_drawSlopeTL_8< 8, 0x0FF00, 0x0FF>,    // 256x256
            R_drawSlopeTL_8< 7, 0x3FE00, 0x1FF>,    // 512x512
            R_drawSlopeTL_8_GEN                     // General
        },
        // Additive
        {
            R_drawSlopeAdd_8<10, 0x00FC0, 0x03F>,   // 64x64
            R_drawSlopeAdd_8< 9, 0x03F80, 0x07F>,   // 128x128
            R_drawSlopeAdd_8< 8, 0x0FF00, 0x0FF>,   // 256x256
            R_drawSlopeAdd_8< 7, 0x3FE00, 0x1FF>,   // 512x512
            R_drawSlopeAdd_8_GEN                    // General
        },
        // Solid masked
        {
            R_drawSlopeSolidMasked_8<10, 0x00FC0, 0x03F>, // 64x64
            R_drawSlopeSolidMasked_8< 9, 0x03F80, 0x07F>, // 128x128
            R_drawSlopeSolidMasked_8< 8, 0x0FF00, 0x0FF>, // 256x256
            R_drawSlopeSolidMasked_8< 7, 0x3FE00, 0x1FF>, // 512x512
            R_drawSlopeSolidMasked_8_GEN                  // General
        },
        // Translucent masked
        {
            R_drawSlopeTLMasked_8<10, 0x00FC0, 0x03F>,    // 64x64
            R_drawSlopeTLMasked_8< 9, 0x03F80, 0x07F>,    // 128x128
            R_drawSlopeTLMasked_8< 8, 0x0FF00, 0x0FF>,    // 256x256
            R_drawSlopeTLMasked_8< 7, 0x3FE00, 0x1FF>,    // 512x512
            R_drawSlopeTLMasked_8_GEN                     // General
        },
        // Additive masked
        {
            R_drawSlopeAddMasked_8<10, 0x00FC0, 0x03F>,   // 64x64
            R_drawSlopeAddMasked_8< 9, 0x03F80, 0x07F>,   // 128x128
            R_drawSlopeAddMasked_8< 8, 0x0FF00, 0x0FF>,   // 256x256
            R_drawSlopeAddMasked_8< 7, 0x3FE00, 0x1FF>,   // 512x512
            R_drawSlopeAddMasked_8_GEN                    // General
        }
    }
};

// clang-format on

// EOF

