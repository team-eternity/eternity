//
// Copyright (C) 2005-2007 Free Software Foundation, Inc.
// Written by Bruno Haible <bruno@clisp.org>, 2005.
//
// Modifications for Eternity Copyright (C) 2025 James Haley et al.
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
// Purpose: GNUlib strcasestr implementation for case-insensitive substring
//  detection.
//
// Authors: Bruno Haible, James Haley
//

#include "z_zone.h"
#include "m_ctype.h"
#include "m_utils.h"
#include "i_system.h"

//
// Knuth-Morris-Pratt algorithm.
// See http://en.wikipedia.org/wiki/Knuth-Morris-Pratt_algorithm
// Return a boolean indicating success
//
static bool knuth_morris_pratt(const char *haystack, const char *needle, const char **resultp)
{
    size_t m = strlen(needle);

    // Allocate the table.
    size_t *table = ecalloc(size_t *, m, sizeof(size_t));

    // Fill the table:
    // For 0 < i < m:
    //   0 < table[i] <= i is defined such that
    //   rhaystack[0..i-1] == needle[0..i-1] and rhaystack[i] != needle[i]
    //   implies
    //   forall 0 <= x < table[i]: rhaystack[x..x+m-1] != needle[0..m-1],
    //   and table[i] is as large as possible with this property.
    // table[0] remains uninitialized.

    {
        size_t i, j = 0;

        table[1] = 1;

        for(i = 2; i < m; i++)
        {
            unsigned char b = ectype::toLower(needle[i - 1]);

            for(;;)
            {
                if(b == ectype::toLower(needle[j]))
                {
                    table[i] = i - ++j;
                    break;
                }
                if(j == 0)
                {
                    table[i] = i;
                    break;
                }
                j = j - table[j];
            }
        }
    }

    // Search, using the table to accelerate the processing.
    {
        size_t      j = 0;
        const char *rhaystack, *phaystack;

        *resultp  = nullptr;
        rhaystack = haystack;
        phaystack = haystack;

        // Invariant: phaystack = rhaystack + j
        while(*phaystack != '\0')
        {
            if(ectype::toLower(needle[j]) == ectype::toLower(*phaystack))
            {
                j++;
                phaystack++;
                if(j == m)
                {
                    // The entire needle has been found
                    *resultp = rhaystack;
                    break;
                }
            }
            else if(j > 0)
            {
                // Found a match of needle[0..j-1], mismatch at needle[j]
                rhaystack += table[j];
                j         -= table[j];
            }
            else
            {
                // Found a mismatch at needle[0] already.
                rhaystack++;
                phaystack++;
            }
        }
    }

    efree(table);
    return true;
}

//
// M_StrCaseStr
//
// Find the first occurrence of NEEDLE in HAYSTACK, using case-insensitive
// comparison.
//
const char *M_StrCaseStr(const char *haystack, const char *needle)
{
    // haleyjd: return nullptr when given nullptr argument(s)
    if(!haystack || !needle)
        return nullptr;

    // Be careful not to look at the entire extent of haystack or needle until
    // needed. This is useful because of these two cases:
    // - haystack may be very long, and a match of needle found early
    // - needle may be very long, and not even a short initial segment of needle
    //   may be found in haystack
    if(*needle != '\0')
    {
        // Minimizing the worst-case complexity:
        // Let n = strlen(haystack), m = strlen(needle). The naive algorithm is
        // O(n*m) worst-case. The Knuth-Morris-Pratt algorithm is O(n) worst-case
        // but needs a memory allocation. To achieve linear complexity and yet
        // amortize the cost of the memory allocation, we activate the K-M-P
        // algorithm only once the naive algorithm has already run for some time;
        // more precisely, when:
        // - the outer loop count is >= 10
        // - the average number of comparisons per outer loop is >= 5
        // - the total number of comparisons is >= m.

        bool        try_kmp            = true;
        size_t      outer_loop_count   = 0;
        size_t      comparison_count   = 0;
        size_t      last_ccount        = 0;      // last comparison count
        const char *needle_last_ccount = needle; // = needle + last_ccount

        // Speed up the following searches of needle by caching its first character
        unsigned char b = ectype::toLower(*needle);
        ++needle;
        for(;; haystack++)
        {
            if(!*haystack)
                return nullptr; // No match.

            // See whether it's advisable to use an asymptotically faster algorithm
            if(try_kmp && outer_loop_count >= 10 && comparison_count >= 5 * outer_loop_count)
            {
                // See if needle + comparison_count now reaches the end of the needle
                if(needle_last_ccount)
                {
                    needle_last_ccount += M_Strnlen(needle_last_ccount, comparison_count - last_ccount);
                    if(!*needle_last_ccount)
                        needle_last_ccount = nullptr;
                    last_ccount = comparison_count;
                }
                if(!needle_last_ccount)
                {
                    // Try the Knuth-Morris-Pratt algorithm.
                    const char *result;
                    bool        success = knuth_morris_pratt(haystack, needle - 1, &result);
                    if(success)
                        return result;
                    try_kmp = false;
                }
            }

            ++outer_loop_count;
            ++comparison_count;
            if(ectype::toLower(*haystack) == b)
            {
                // The first character matches
                const char *rhaystack = haystack + 1;
                const char *rneedle   = needle;
                for(;; rhaystack++, rneedle++)
                {
                    if(!*rneedle)
                        return haystack; // Found a match
                    if(!*rhaystack)
                        return nullptr; // No match.

                    ++comparison_count;
                    if(ectype::toLower(*rhaystack) != ectype::toLower(*rneedle))
                        break; // Nothing in this round.
                }
            }
        }
    }
    else
        return haystack;
}

//
// haleyjd: Overload taking non-const haystack and returning non-const pointer
//
char *M_StrCaseStr(char *haystack, const char *needle)
{
    return const_cast<char *>(M_StrCaseStr((const char *)haystack, needle));
}

// EOF

