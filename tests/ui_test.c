/* Native presentation contract: exact time strings, path rules and seek
 * increments, including simultaneous controls and fractional frame rates. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../include/aplayer_ui.h"

int main(void)
{
   char text[32];
   const double rates[] = { 1, 50, 60, 59.94 };
   const unsigned bits[] = { RETRO_DEVICE_ID_JOYPAD_LEFT,
      RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_L2,
      RETRO_DEVICE_ID_JOYPAD_R2 };
   const int seconds[] = { -15, 15, 180, -180, -300, 300 };
   aplayer_format_time(-1, text, sizeof text);
   assert(!strcmp(text, "00:00:00"));
   aplayer_format_time(3661.9, text, sizeof text);
   assert(!strcmp(text, "01:01:01"));
   aplayer_format_time(360000, text, sizeof text);
   assert(!strcmp(text, "100:00:00"));
   aplayer_format_time(0, NULL, 0);
   text[0] = 'x';
   aplayer_format_time(0, text, 0);
   assert(text[0] == 'x');
   aplayer_format_time(3661, text, 4);
   assert(!strcmp(text, "01:"));
   assert(aplayer_filename_from_path(NULL) == NULL);
   assert(aplayer_filename_from_path("") == NULL);
   assert(!strcmp(aplayer_filename_from_path("song.mp3"), "song.mp3"));
   assert(!strcmp(aplayer_filename_from_path("sd:/music/song.wav64"), "song.wav64"));
   assert(!strcmp(aplayer_filename_from_path("C:\\music/song.mp3"), "song.mp3"));
   assert(!strcmp(aplayer_filename_from_path("music/"), ""));
   for (unsigned r = 0; r < sizeof rates / sizeof *rates; r++)
      for (unsigned mask = 0; mask <= UINT16_MAX; mask++) {
         int expected = 0;
         for (unsigned b = 0; b < sizeof bits / sizeof *bits; b++)
            if (mask & (1u << bits[b]))
               expected = (int)(expected + seconds[b] * rates[r]);
         assert(aplayer_seek_frames(mask, rates[r]) == expected);
      }
   puts("PASS native UI: time, filenames, 262144 seek combinations");
   return 0;
}
