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

#ifndef PSNPRINTF_H
#define PSNPRINTF_H

int psnprintf(char *str, size_t n, const char *format, ...);
int pvsnprintf(char *str, size_t n, const char *format, va_list ap);

/* haleyjd 08/01/09: rewritten to use a structure */
typedef struct psvnfmt_vars_s
{
   char       *pinsertion;
   size_t      nmax;
   const char *fmt;
   int         flags;
   int         width;
   int         precision;
   char        prefix;
} pvsnfmt_vars;

/* Use these directly if you want to avoid overhead of psnprintf
 * Return value is number of characters printed (or number printed
 * if there had been enough room).
 */
int pvsnfmt_char(pvsnfmt_vars *info, char c);

typedef union pvsnfmt_intparm_u
{
   int   i;
   void *p;
} pvsnfmt_intparm_t;

int pvsnfmt_int(pvsnfmt_vars *info, pvsnfmt_intparm_t *ip);

int pvsnfmt_str(pvsnfmt_vars *info, const char *s);

int pvsnfmt_double(pvsnfmt_vars *info, double d);

/* These are the flags you need (use logical OR) for the flags parameter of
 * fmt functions above.
 */
#define FLAG_DEFAULT         0x00
#define FLAG_LEFT_ALIGN      0x01 // -
#define FLAG_SIGNED          0x02 // +
#define FLAG_ZERO_PAD        0x04 // 0
#define FLAG_SIGN_PAD        0x08 // ' '
#define FLAG_HASH            0x10 // #

/* Portable strnlen function (doesn't exist on all systems!) */
size_t pstrnlen(const char *s, size_t count);


#endif /* ifdef PSNPRINTF_H */


