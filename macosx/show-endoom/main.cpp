//
// Eternity ENDOOM display
// Copyright (C) 2019 Ioan Chera
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
// Purpose: External tool to show ENDOOM, to work around a macOS crash.
// Authors: Ioan Chera
//

#include <stdint.h>
#include <stdio.h>
#include "txt_main.h"

// https://stackoverflow.com/a/6782480/11738219

static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
   'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
   'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
   'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
   'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
   'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
   'w', 'x', 'y', 'z', '0', '1', '2', '3',
   '4', '5', '6', '7', '8', '9', '+', '/'};
static char *decoding_table = NULL;

static void build_decoding_table() {

   decoding_table = (char*)malloc(256);

   for (int i = 0; i < 64; i++)
      decoding_table[(unsigned char) encoding_table[i]] = i;
}

static unsigned char *base64_decode(const char *data,
                             size_t input_length,
                             size_t *output_length) {

   if (decoding_table == NULL) build_decoding_table();

   if (input_length % 4 != 0) return NULL;

   *output_length = input_length / 4 * 3;
   if (data[input_length - 1] == '=') (*output_length)--;
   if (data[input_length - 2] == '=') (*output_length)--;

   unsigned char *decoded_data = (unsigned char *)malloc(*output_length);
   if (decoded_data == NULL) return NULL;

   for (int i = 0, j = 0; i < input_length;) {

      uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
      uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
      uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
      uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

      uint32_t triple = (sextet_a << 3 * 6)
      + (sextet_b << 2 * 6)
      + (sextet_c << 1 * 6)
      + (sextet_d << 0 * 6);

      if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
      if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
      if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
   }

   return decoded_data;
}

enum
{
   ENDOOM_SIZE = 4000
};

namespace strings
{
   static const char title[] = "Thanks for using the Eternity Engine!";
}

//
// Returns current time in nanoseconds
//
static int64_t getTime_ns()
{
   timespec ts;
   if(clock_gettime(CLOCK_MONOTONIC, &ts))
      return -1;
   return ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;
}



//
// Entry point
// Expects the path to a 4000-long file
//
int main(int argc, const char * argv[])
{
   // insert code here...
   size_t len = 0;
   unsigned char *endoom = base64_decode(argv[1], strlen(argv[1]), &len);
   int delay_tics = atoi(argv[2]);

   int64_t delay_ns = delay_tics * 1'000'000'000LL / 35;

   puts(argv[1]);
   puts(argv[2]);

   if(!TXT_Init())
      return EXIT_FAILURE;

   TXT_SetWindowTitle(strings::title);

   unsigned char *screendata = TXT_GetScreenData();
   memcpy(screendata, endoom, len < ENDOOM_SIZE ? len : ENDOOM_SIZE);

   int64_t start_ns = getTime_ns();
   if(start_ns == -1)
      start_ns = INT64_MIN;   // ensure no lockup on error

   while(getTime_ns() < start_ns + delay_ns)
   {
      TXT_UpdateScreen();
      if(TXT_GetChar() > 0)
         break;
      TXT_Sleep(1);
   }

   TXT_Shutdown();
   return 0;
}
