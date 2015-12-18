//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// psnprintf
//
// Portable snprintf
//
// Amongst other things, tries to always guarantee the buffer will be null-
// terminated, which some implementations do not do.
//
//-----------------------------------------------------------------------------

#include <string.h> /* for memset */
#include <stdarg.h> /* for va_list */
#include <stdlib.h> /* for fcvt */
#include <inttypes.h> /* haleyjd */

#include "psnprntf.h"

/* Windows stdlib defines fcvt differently <sigh> */

/* haleyjd: not all platforms have fcvt. As a guy on comp.os.msdos.djgpp
 * so eloquently put it, this is driving out Baal by invoking
 * Beelzebub. So, I've copied DJGPP's (barely) portable fcvt, and it
 * can be made to compile by defining NO_FCVT. This is not good, but
 * it'll do.
 */

#ifndef NO_FCVT
   #ifdef __FreeBSD__ // [Kate] Update as necessary
   #define NO_FCVT
   #endif
   #ifdef WIN32
      #ifndef CYGWIN
      #define FCVT _fcvt
      #else
      #define FCVT fcvt
      #endif
   #else
      #define FCVT fcvt
   #endif
#endif

#ifdef NO_FCVT
#include "m_fcvt.h"
#define FCVT M_Fcvt
#endif

int psnprintf(char *str, size_t n, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = pvsnprintf(str, n, format, args);
    va_end(args);
    return ret;
}

#define STATE_NONE 0
#define STATE_OPERATOR 1 /* Just received % */
#define STATE_FLAG 2     /* Just received a flag or prefix or width */
#define STATE_WIDTH 3
#define STATE_BEFORE_PRECISION 4 /* just got dot */
#define STATE_PRECISION 5 /* got at least one number after dot */
#define STATE_PREFIX 6   /* just received prefix (h, l or L) */

#define UNKNOWN_WIDTH 0
#define VARIABLE_WIDTH -2
#define UNKNOWN_PRECISION -1
#define VARIABLE_PRECISION -2

/* Following macros give reusable switch cases, used in combination
 * depending on current state. Sucks to do these as macros, but should
 * give the compiler lots of freedom to optimize.
 */
#define CHECK_FLAG \
    case '-':  \
        info.flags |= FLAG_LEFT_ALIGN; \
        state = STATE_FLAG; \
        break; \
    case '+': \
        info.flags |= FLAG_SIGNED; \
        state = STATE_FLAG; \
        break; \
    case '0': \
        info.flags |= FLAG_ZERO_PAD; \
        state = STATE_FLAG; \
        break; \
    case ' ': \
        info.flags |= FLAG_SIGN_PAD; \
        state = STATE_FLAG; \
        break; \
    case '#': \
        info.flags |= FLAG_HASH; \
        state = STATE_FLAG; \
        break;

#define CHECK_WIDTH \
    case '1': \
    case '2': \
    case '3': \
    case '4': \
    case '5': \
    case '6': \
    case '7': \
    case '8': \
    case '9': \
        info.width = *info.fmt - '0'; /* convert to integer */ \
        state = STATE_WIDTH; \
        break; \
    case '*': \
        info.width = VARIABLE_WIDTH; \
        state = STATE_WIDTH; \
        break;

#define CHECK_PRECISION \
    case '.': \
        info.precision = 0; \
        state = STATE_BEFORE_PRECISION; \
        break;

#define CHECK_PREFIX \
    case 'h': \
    case 'l': \
    case 'L': \
        info.prefix = *info.fmt; \
        state = STATE_PREFIX; \
        break;

#define GET_VARS \
    if (info.width == VARIABLE_WIDTH) \
        info.width = va_arg(ap, int); \
    if (info.precision == VARIABLE_PRECISION) \
        info.precision = va_arg(ap, int);

#define CHECK_TYPE \
    case 'd': \
    case 'i': \
    case 'u': \
    case 'o': \
    case 'x': \
    case 'X': \
    case 'p': \
        GET_VARS \
        if(*info.fmt == 'p') \
           ip.p = va_arg(ap, void *); \
        else \
           ip.i = va_arg(ap, int); \
        ncount += pvsnfmt_int(&info, &ip); \
        state = STATE_NONE; \
        break; \
    case 'e': \
    case 'E': \
    case 'f': \
    case 'g': \
    case 'G': \
        GET_VARS \
        ncount += pvsnfmt_double(&info, (double)(va_arg(ap, double))); \
        state = STATE_NONE; \
        break; \
    case 'c': \
        GET_VARS \
        ncount += pvsnfmt_char(&info, (char)(va_arg(ap, int))); \
        state = STATE_NONE; \
        break; \
    case 's': \
        GET_VARS \
        ncount += pvsnfmt_str(&info, (const char *)(va_arg(ap, const char *))); \
        state = STATE_NONE; \
        break; \
    case 'n': \
        *(va_arg(ap, int *)) = ncount; \
        state = STATE_NONE; \
        break;

#define PUTCHAR(ch) \
    if (nmax > 1) \
    { \
        *info.pinsertion++ = ch;  \
        nmax--; \
    } \
    ncount++;

int pvsnprintf(char *str, size_t nmax, const char *format, va_list ap)
{
    /* nmax gives total size of buffer including null
     * null is ALWAYS added, even if buffer too small for format
     * (contrary to C99)
     */

    int ncount = 0;     /* number of characters printed so far */
    int state = STATE_NONE;

    /* haleyjd 08/01/09: rewrite to use structure */
    pvsnfmt_vars info;
    pvsnfmt_intparm_t ip;

    info.pinsertion = str;
    info.nmax       = nmax;
    info.fmt        = format;
    info.flags      = 0;
    info.width      = 0;
    info.precision  = 0;
    info.prefix     = 0;

    while(*info.fmt)
    {
        switch (state)
        {
        case STATE_NONE:
            switch (*(info.fmt))
            {
            case '%':
                state = STATE_OPERATOR;
                info.flags = FLAG_DEFAULT;
                info.width = UNKNOWN_WIDTH;
                info.precision = UNKNOWN_PRECISION;
                info.prefix = '\0';
                break;

            default:
                PUTCHAR(*(info.fmt))
            }
            break;

        case STATE_OPERATOR:
            switch (*(info.fmt))
            {
                CHECK_FLAG
                CHECK_WIDTH
                CHECK_PRECISION
                CHECK_PREFIX
                CHECK_TYPE
            default:
                PUTCHAR(*info.fmt) /* Unknown format, just print it (e.g. "%%") */
                state = STATE_NONE;
            }
            break;

        case STATE_FLAG:
            switch (*info.fmt)
            {
                CHECK_FLAG
                CHECK_WIDTH
                CHECK_PRECISION
                CHECK_PREFIX
                CHECK_TYPE
            }
            break;

        case STATE_WIDTH:
            if (*info.fmt >= '0' && *info.fmt <= '9' && info.width != -1)
            {
                info.width = info.width * 10 + (*info.fmt - '0');
                break;
            }
            switch (*info.fmt)
            {
                CHECK_PRECISION
                CHECK_PREFIX
                CHECK_TYPE
            }
            break;

        case STATE_BEFORE_PRECISION:
            if (*info.fmt >= '0' && *info.fmt <= '9')
            {
                info.precision = *info.fmt - '0';
                state = STATE_PRECISION;
            }
            else if (*info.fmt == '*')
            {
                info.precision = VARIABLE_PRECISION;
                state = STATE_PRECISION;
            }
            switch (*info.fmt)
            {
                CHECK_PREFIX
                CHECK_TYPE
            }
            break;

        case STATE_PRECISION:
            if (*info.fmt >= '0' && *info.fmt <= '9' && info.precision != -1)
            {
                info.precision = info.precision * 10 + (*info.fmt - '0');
                break;
            }
            switch (*info.fmt)
            {
                CHECK_PREFIX
                CHECK_TYPE
            }
            break;

        case STATE_PREFIX:
            switch (*info.fmt)
            {
                CHECK_TYPE
            }


        } /* switch state */
        info.fmt++;

    } /* while *pfmt */

    /* Add null if there is room
     * NOTE there is always room even if str doesn't fit unless
     * nmax initially passed in as 0.  fmt functions take care to
     * always leave at least one free byte at end.
     */
    if (nmax > 0)
        *info.pinsertion = '\0';

    return ncount;
}

int pvsnfmt_char(pvsnfmt_vars *info, char c)
{
    if(info->nmax > 1)
    {
        *(info->pinsertion) = c;
        info->pinsertion += 1;
        info->nmax -= 1;
    }
    return 1;
}

/* strnlen not available on all platforms.. maybe autoconf it? */
size_t pstrnlen(const char *s, size_t count)
{
    const char *p = s;
    while (*p && count-- > 0)
        p++;

    return p - s;
}

/* Format a string into the buffer.  Parameters:
 *   *&pinsertion   Reference to pointer to buffer (can be reference to NULL)
 *   &nmax          Reference to size of buffer.  This is may be modified
 *   fmt            Format character ('s')
 *   flags          0 or combination of flags (see .h file for #defines)
 *   width          Width of string, as defined in printf
 *   precision      Precision of string, as defined in printf
 *   ap             Argument list
 */

int pvsnfmt_str(pvsnfmt_vars *info, const char *s)
{
    const char *str = s;
    int nprinted;
    int len;
    int pad = 0;
    int width, flags;

    width = info->width;
    flags = info->flags;

    /* Get width magnitude, set aligment flag */
    if(width < 0)
    {
        width = -width;
        flags |= FLAG_LEFT_ALIGN;
    }

    /* Truncate due to precision */
    if (info->precision < 0)
        len = static_cast<int>(strlen(str));
    else
        len = static_cast<int>(pstrnlen(str, info->precision));

    /* Determine padding length */
    if (width > len)
        pad = width - len;

    /* Exit if just counting (not printing) */
    if (info->nmax <= 1)
        return len + pad;

    /* If right-aligned, print pad */
    if ( !(flags & FLAG_LEFT_ALIGN) )
    {
        char padchar;
        if (flags & FLAG_ZERO_PAD)
            padchar = '0';
        else
            padchar = ' ';

        if ((int) info->nmax - 1 < pad)
            nprinted = static_cast<int>(info->nmax - 1);
        else
            nprinted = pad;

        memset(info->pinsertion, padchar, nprinted);
        info->pinsertion += nprinted;
        info->nmax -= nprinted;
    }

    /* Output string */
    if (info->nmax <= 1)
        nprinted = 0;
    else if ((int) info->nmax - 1 < len)
        nprinted = static_cast<int>(info->nmax - 1);
    else
        nprinted = len;

    memcpy(info->pinsertion, str, nprinted);
    info->pinsertion += nprinted;
    info->nmax -= nprinted;

    /* If left aligned, add pad */
    if (flags & FLAG_LEFT_ALIGN)
    {
        if (info->nmax <= 1)
            nprinted = 0;
        else if ((int)info->nmax - 1 < pad)
            nprinted = static_cast<int>(info->nmax - 1);
        else
            nprinted = pad;

        if(nprinted)
           memset(info->pinsertion, ' ', nprinted);
        info->pinsertion += nprinted;
        info->nmax -= nprinted;
    }

    return len + pad; /* Return total length of pad + string even if some
                       * was truncated
                       */
}

/* Format an integer into the buffer.  Parameters:
 *   *&pinsertion   Reference to pointer to buffer (can be reference to NULL)
 *   &nmax          Reference to size of buffer.  This is may be modified
 *   fmt            Format character (one of "diuoxX")
 *   flags          0 or combination of flags (see .h file for #defines)
 *   width          Width of integer, as defined in printf
 *   precision      Precision of integer, as defined in printf
 *   ap             Argument list
 */

int pvsnfmt_int(pvsnfmt_vars *info, pvsnfmt_intparm_t *ip)
{
    int number = 0;
    unsigned int unumber = 0;
    char numbersigned = 1;
    char iszero = 0; /* bool */
    int base = 10;   /* haleyjd: default to something valid */
    int len = 0; /* length of number component (no sign or padding) */
    char char10 = 0;
    char sign = 0;
    int widthpad = 0;
    int addprefix = 0; /* optional "0x" = 2 */
    int totallen;
    int temp; /* haleyjd 07/19/03: bug fix */

    /* Stack used to hold digits, which are generated backwards
     * and need to be popped off in the correct order
     */
    char numstack[22];    /* largest 64 bit number has 22 octal digits */
    char *stackpos = numstack;
    
    char fmt       = *info->fmt;
    int flags      = info->flags;
    int precision  = info->precision;

#define PUSH(x) \
    *stackpos++ = (char)(x)

#define POP() \
    *(--stackpos)

    // haleyjd: init this array for safety
    memset(numstack, 0, sizeof(numstack));

    /* Retrieve value */
    switch (info->prefix)
    {
    case 'h':
        switch (fmt)
        {
            case 'd':
            case 'i':
                number = (signed short int) ip->i;
                break;
            case 'u':
            case 'o':
            case 'x':
            case 'X':
                unumber = (unsigned short int) ip->i;
                numbersigned = 0;
                break;
             case 'p':
                unumber = (unsigned int)((size_t)ip->p); // FIXME: Not x64 friendly
                numbersigned = 0;
                break;
        }
        break;
    case 'l':
        switch (fmt)
        {
            case 'd':
            case 'i':
                number = ip->i;
                break;
            case 'u':
            case 'o':
            case 'x':
            case 'X':
                unumber = (unsigned int) ip->i;
                numbersigned = 0;
                break;
             case 'p':
                unumber = (unsigned int)((size_t)ip->p); // FIXME: Not x64 friendly
                numbersigned = 0;
                break;                
        }
        break;
    default:
        switch (fmt)
        {
            case 'd':
            case 'i':
                number = ip->i;
                break;
            case 'u':
            case 'o':
            case 'x':
            case 'X':
                unumber = (unsigned int) ip->i;
                numbersigned = 0;
                break;
             case 'p':
                unumber = (unsigned int)((size_t)ip->p); // FIXME: Not x64 friendly
                numbersigned = 0;
                break;
         }
    } /* switch fmt to retrieve number */

    if (fmt == 'p')
    {
        fmt = 'x';
        flags |= FLAG_HASH;
    }

    /* Discover base */
    switch (fmt)
    {
        case 'd':
        case 'i':
        case 'u':
            base = 10;
            break;
        case 'o':
            base = 8;
            break;
        case 'X':
            base = 16;
            char10 = 'A';
            break;
        case 'x':
            base = 16;
            char10 = 'a';
    }

    if (numbersigned)
    {
        if (number < 0)
        {
            /* Deal with negativity */
            sign = '-';
            number = -number;
        }
        else if (flags & FLAG_SIGNED)
        {
            sign = '+';
        }
        else if (flags & FLAG_SIGN_PAD)
        {
            sign = ' ';
        }
    }

    /* Create number */
    if (numbersigned)
    {
        if (number == 0)
            iszero = 1;
        do
        {
            PUSH(number % base);
            number /= base;
            len++;
        } while (number != 0);
    }
    else
    {
        if (unumber == 0)
            iszero = 1;
        do
        {
            PUSH(unumber % base);
            unumber /= base;
            len++;
        } while (unumber != 0);
    }

    /* Octal hash character (alternate form) */
    if (fmt == 'o' && (flags & FLAG_HASH) && precision <= len &&
        precision != 0 && !iszero )
    {
        precision = len + 1;
    }

    /* Determine width of sign, if any. */
    if ( (fmt == 'x' || fmt == 'X') && (flags & FLAG_HASH) && !iszero )
        addprefix = 2;
    else if (sign != 0)
        addprefix = 1;

    /* Make up precision (zero pad on left) */
    while (len < precision)
    {
        PUSH(0);
        len++;
    }


    if (len + addprefix < info->width)
    {
        totallen = info->width;
        widthpad = info->width - (len + addprefix);
    }
    else
        totallen = len + addprefix;

    if (info->nmax <= 1)
        return totallen;

    /* Write sign or "0x" */
    if (flags & FLAG_ZERO_PAD)
    {
        if (addprefix == 2) /* 0x */
        {
            if (info->nmax > 1)
            {
                *(info->pinsertion) = '0';
                info->pinsertion += 1;
                info->nmax -= 1;
            }
            if (info->nmax > 1)
            {
                *(info->pinsertion) = fmt;
                info->pinsertion += 1;
                info->nmax -= 1;
            }
        }
        else if (addprefix == 1) /* sign */
        {
            if (info->nmax > 1)
            {
                *(info->pinsertion) = sign;
                info->pinsertion += 1;
                info->nmax -= 1;
            }
        }
    }

    /* Width pad */
    if ( !(flags & FLAG_LEFT_ALIGN) )
    {
        /* haleyjd 07/19/03: bug fix: nmax + 1 => nmax - 1 */
        if (info->nmax <= 1)
            widthpad = 0;
        else if ((int) info->nmax - 1 < widthpad)
            widthpad = static_cast<int>(info->nmax - 1);

        if (flags & FLAG_ZERO_PAD)
            memset(info->pinsertion, '0', widthpad);
        else
            memset(info->pinsertion, ' ', widthpad);

        info->pinsertion += widthpad;
        info->nmax -= widthpad;
    }

    /* Write sign or "0x" */
    if ( !(flags & FLAG_ZERO_PAD) )
    {
        if (addprefix == 2) /* 0x */
        {
            if (info->nmax > 1)
            {
                *(info->pinsertion) = '0';
                info->pinsertion += 1;
                info->nmax -= 1;
            }
            if (info->nmax > 1)
            {
                *(info->pinsertion) = fmt;
                info->pinsertion += 1;
                info->nmax -= 1;
            }
        }
        else if (addprefix == 1) /* sign */
        {
            if (info->nmax > 1)
            {
                *(info->pinsertion) = sign;
                info->pinsertion += 1;
                info->nmax -= 1;
            }
        }
    }

    /* haleyjd 07/19/03: bug fix: nmax + 1 => nmax - 1 */
    /* Write number */
    if (info->nmax <= 1)
        len = 0;
    else if ((int) info->nmax - 1 < len)
        len = static_cast<int>(info->nmax - 1);

    /* haleyjd 07/19/03: bug fix: Do NOT use len as the counter
     * variable for this loop. This messes up the length calculations
     * afterward, and allows writing off the end of the string buffer.
     * Special thanks to schepe for nailing this down.
     */
    temp = len;
    for (; temp > 0; temp--)
    {
        char n = POP();
        if (n <= 9)
        {
            *(info->pinsertion) = n + '0';
            info->pinsertion += 1;
        }
        else
        {
            *(info->pinsertion) = n - 10 + char10;
            info->pinsertion += 1;
        }
    }
    info->nmax -= len;

    if (flags & FLAG_LEFT_ALIGN)
    {
        /* haleyjd 07/19/03: bug fix: nmax + 1 => nmax - 1 */
        if (info->nmax <= 1)
            widthpad = 0;
        else if ((int) info->nmax - 1 < widthpad)
            widthpad = static_cast<int>(info->nmax - 1);

        if(widthpad)
           memset(info->pinsertion, ' ', widthpad);
        info->pinsertion += widthpad;
        info->nmax -= widthpad;
    }

    return totallen;
}

/*
 * WARNING: Assumes 64 bit doubles
 */
typedef union DBLBITS_u 
{
   double D;
   struct word_s 
   {
      uint32_t W0;
      uint32_t W1;
   } WORDS; /* haleyjd: this must be named */
} DBLBITS;

#define EXP_MASK  0x7FF00000
#define SIGN_MASK 0x80000000
#define MANTISSA_MASK ~(SIGN_MASK | EXP_MASK)
#define QNAN_MASK 0x00080000 /* first bit of mantissa */

/* These functions not defined on all platforms, so
 * do the checks manually
 * WARNING: Assumes 64-bit IEEE representation
 *    (so it won't run on your Cray :)
 */

#define ISNAN(x) \
    ( (((DBLBITS *) &x)->WORDS.W1 & EXP_MASK) == EXP_MASK \
  && ((((DBLBITS *) &x)->WORDS.W1 & MANTISSA_MASK) != 0 || ((DBLBITS *) &x)->WORDS.W0 != 0) )

#define ISQNAN(x) \
    ( ISNAN(x) && (((DBLBITS *) &x)->WORDS.W1 & QNAN_MASK) )

#define ISSNAN(x) \
    ( ISNAN(x) && !ISQNAN(x) )

#define ISINF(x) \
   ( (((DBLBITS *) &x)->WORDS.W1 & EXP_MASK) == EXP_MASK \
  && ((((DBLBITS *) &x)->WORDS.W1 & MANTISSA_MASK) == 0 && ((DBLBITS *) &x)->WORDS.W0 == 0) )

/* Format a double-precision float into the buffer.  Parameters:
 *   *&pinsertion   Reference to pointer to buffer (can be reference to NULL)
 *   &nmax          Reference to size of buffer.  This is may be modified
 *   fmt            Format character (one of "eEfgG")
 *   flags          0 or combination of flags (see .h file for #defines)
 *   width          Width of double, as defined in printf
 *   precision      Precision of double, as defined in printf
 *   ap             Argument list
 */

int pvsnfmt_double(pvsnfmt_vars *info, double d)
{
    char *digits;
    int sign = 0;
    int dec;
    double value = d;

    int len;
    int pad = 0;
    //int signwidth = 0;
    int totallen;
    char signchar = 0;
    int leadingzeros = 0;

    int printdigits; /* temporary var used in different contexts */

    int flags = info->flags;
    int width = info->width;
    const char fmt = *(info->fmt);
    int precision = info->precision;

    /* Check for special values first */
    const char *special = 0;
    if (ISSNAN(value))
        special = "NaN";
    else if (ISQNAN(value))
        special = "NaN";
    else if ( ISINF(value) )
    {
        if (value < 0)
            sign = 1;
        special = "Inf";
    }

    if (special)
    {
        totallen = len = static_cast<int>(strlen(special));

        /* Sign (this is silly for NaN but conforming to printf */
        if (flags & (FLAG_SIGNED | FLAG_SIGN_PAD) || sign)
        {
            if (sign)
                signchar = '-';
            else if (flags & FLAG_SIGN_PAD)
                signchar = ' ';
            else
                signchar = '+';
            totallen++;
        }

        /* Padding */
        if (totallen < width)
            pad = width - totallen;
        else
            pad = 0;

        totallen += pad ;

        // haleyjd 05/07/08: this was forgotten!
        if (info->nmax <= 1)
           return totallen;


        /* Sign now if zeropad */
        if (flags & FLAG_ZERO_PAD && signchar)
        {
            if (info->nmax > 1)
            {
                *(info->pinsertion) = signchar;
                info->pinsertion += 1;
                info->nmax -= 1;
            }
        }

        /* Right align */
        if ( !(flags & FLAG_LEFT_ALIGN) )
        {
            if (info->nmax <= 1)
                pad  = 0;
            else if ((int) info->nmax - 1 < pad )
                pad  = static_cast<int>(info->nmax - 1);

            if(pad)
            {
               if (flags & FLAG_ZERO_PAD)
                  memset(info->pinsertion, '0', pad );
               else
                  memset(info->pinsertion, ' ', pad );
            }
            info->pinsertion += pad ;
            info->nmax -= pad ;
        }

        /* Sign now if not zeropad */
        if (!(flags & FLAG_ZERO_PAD) && signchar)
        {
            if (info->nmax > 1)
            {
                *(info->pinsertion) = signchar;
                info->pinsertion += 1;
                info->nmax -= 1;
            }
        }

        if (info->nmax <= 0)
            len = 0;
        else if ((int) info->nmax - 1 < len)
            len = static_cast<int>(info->nmax - 1);
        if(len)
           memcpy(info->pinsertion, special, len);
        info->pinsertion += len;
        info->nmax -= len;

        /* Left align */
        if (flags & FLAG_LEFT_ALIGN)
        {
            if (info->nmax <= 1)
                pad  = 0;
            else if ((int) info->nmax - 1 < pad )
                pad  = static_cast<int>(info->nmax - 1);

            if(pad)
               memset(info->pinsertion, ' ', pad );
            info->pinsertion += pad ;
            info->nmax -= pad ;
        }

        return totallen;
    }

    if (fmt == 'f')
    {
        if (precision == UNKNOWN_PRECISION)
            precision = 6;

        digits = FCVT(value, precision, &dec, &sign);
        len = static_cast<int>(strlen(digits));

        if (dec > 0)
            totallen = dec;
        else
            totallen = 0;

        /* plus 1 for decimal place */
        if (dec <= 0)
            totallen += 2; /* and trailing ".0" */
        else if (precision > 0 || flags & FLAG_HASH)
            totallen += 1;


        /* Determine sign width (0 or 1) */
        if (flags & (FLAG_SIGNED | FLAG_SIGN_PAD) || sign)
        {
            if (sign)
                signchar = '-';
            else if (flags & FLAG_SIGN_PAD)
                signchar = ' ';
            else
                signchar = '+';
            totallen++;
        }

        /* Determine if leading zeros required */
        if (dec <= 0)
        {
            leadingzeros = 1 - dec; /* add one for zero before decimal point (0.) */
        }

        if (leadingzeros - 1 > precision)
            totallen += precision;
        else if (len - dec > 0)
            totallen += precision;
        else
            totallen += leadingzeros;

        /* Determine padding width */
        if (totallen < width)
            pad = width - totallen;

        totallen += pad;
        if (info->nmax <= 1)
            return totallen;

        /* Now that the length has been calculated, print as much of it
         * as possible into the buffer
         */

        /* Print sign now if padding with zeros */
        if (flags & FLAG_ZERO_PAD && signchar != 0)
        {
            if (info->nmax > 1)
            {
                *(info->pinsertion) = signchar;
                info->pinsertion += 1;
                info->nmax -= 1;
            }
        }

        /* Print width padding if right-aligned */
        if ( !(flags & FLAG_LEFT_ALIGN) )
        {
            if (info->nmax <= 1)
                pad = 0;
            else if ((int) info->nmax - 1 < pad)
                pad = static_cast<int>(info->nmax - 1);

            if(pad)
            {
               if (flags & FLAG_ZERO_PAD)
                  memset(info->pinsertion, '0', pad);
               else
                  memset(info->pinsertion, ' ', pad);
            }

            info->pinsertion += pad;
            info->nmax -= pad;
        }

        /* Print sign now if padding was spaces */
        if ( !(flags & FLAG_ZERO_PAD) && signchar != 0 )
        {
            *(info->pinsertion) = signchar;
            info->pinsertion += 1;
            info->nmax -= 1;
        }

        /* Print leading zeros */
        if (leadingzeros)
        {
            /* Print "0.", then leadingzeros - 1 */
            if (info->nmax > 1)
            {
                *(info->pinsertion) = '0';
                info->pinsertion += 1;
                info->nmax -= 1;
            }

            if (precision > 0 || flags & FLAG_HASH)
            {
                if (info->nmax > 1)
                {
                    *(info->pinsertion) = '.';
                    info->pinsertion += 1;
                    info->nmax -= 1;
                }
            }

            /* WARNING not rounding here!
             * i.e. printf(".3f", 0.0007) gives "0.000" not "0.001"
             *
             * This whole function could do with a rewrite...
             */
            if (leadingzeros - 1 > precision)
            {
                leadingzeros = precision + 1;
                len = 0;
            }
            /* END WARNING */

            precision -= leadingzeros - 1;

            if (info->nmax <= 1)
                leadingzeros = 0;
            else if ((int) info->nmax /* - 1 */ < leadingzeros /* -1 */)
                leadingzeros = static_cast<int>(info->nmax); /* -1 */

            leadingzeros--;
            if(leadingzeros > 0)
               memset(info->pinsertion, '0', leadingzeros);
            info->pinsertion += leadingzeros;
            info->nmax -= leadingzeros;
        }

        /* Print digits before decimal place */
        if (dec > 0)
        {
            if (info->nmax <= 1)
                printdigits = 0;
            else if ((int) info->nmax - 1 < dec)
                printdigits = static_cast<int>(info->nmax - 1);
            else
                printdigits = dec;

            if(printdigits)
               memcpy(info->pinsertion, digits, printdigits);
            info->pinsertion += printdigits;
            info->nmax -= printdigits;

            if (precision > 0 || flags & FLAG_HASH)
            {
                /* Print decimal place */
                if (info->nmax > 1)
                {
                    *(info->pinsertion) = '.';
                    info->pinsertion += 1;
                    info->nmax -= 1;
                }

                /* Print trailing zero if no precision but hash given */
                if (precision == 0 && info->nmax > 1)
                {
                    *(info->pinsertion) = '0';
                    info->pinsertion += 1;
                    info->nmax -= 1;
                }
            }

            /* Bypass the digits we've already printed */
            len -= dec;
            digits += dec;
        }

        /* Print digits after decimal place */
        if (len > precision)
            len = precision;

        if (info->nmax <= 1)
            printdigits = 0;
        else if ((int) info->nmax - 1 < len)
            printdigits = static_cast<int>(info->nmax - 1);
        else
            printdigits = len;

        if(printdigits)
           memcpy(info->pinsertion, digits, printdigits);
        info->pinsertion += printdigits;
        info->nmax -= printdigits;

        /* Print left-aligned pad */
        if (flags & FLAG_LEFT_ALIGN)
        {
            if (info->nmax <= 1)
                pad = 0;
            else if ((int) info->nmax - 1 < pad)
                pad = static_cast<int>(info->nmax - 1);

            if(pad)
               memset(info->pinsertion, ' ', pad);
            info->pinsertion += pad;
            info->nmax -= pad;
        }
        return totallen;
    }

    return 0;
}

// EOF

