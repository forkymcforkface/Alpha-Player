/* Alpha-Player presentation primitives shared by native and libretro hosts.
 * The host owns decoding, clocks, input polling, and rendered text storage.
 * Time inputs are finite seconds within the player's validated duration range.
 */
#ifndef APLAYER_UI_H
#define APLAYER_UI_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../libretro-common/include/libretro.h"

static inline void aplayer_format_time(double seconds, char *out, size_t out_size)
{
   if (!out || out_size == 0)
      return;

   if (seconds < 0.0)
      seconds = 0.0;

   unsigned total = (unsigned)seconds;
   unsigned hours = total / 3600;
   unsigned minutes = (total % 3600) / 60;
   unsigned secs = total % 60;

   snprintf(out, out_size, "%02u:%02u:%02u", hours, minutes, secs);
}

static inline const char *aplayer_filename_from_path(const char *path)
{
   const char *filename_unix = NULL;
   const char *filename_windows = NULL;

   if (!path || !path[0])
      return NULL;

   filename_unix = strrchr(path, '/');
   filename_windows = strrchr(path, '\\');

   if (filename_unix && filename_windows)
      return (filename_unix > filename_windows ? filename_unix : filename_windows) + 1;
   if (filename_unix)
      return filename_unix + 1;
   if (filename_windows)
      return filename_windows + 1;

   return path;
}

/* Newly pressed buttons only; opposing inputs retain libretro's operation
 * order, including its integer rounding at fractional frame rates. */
static inline int aplayer_seek_frames(uint16_t pressed, double fps)
{
   int frames = 0;
   if (pressed & (1u << RETRO_DEVICE_ID_JOYPAD_LEFT)) frames -= 15 * fps;
   if (pressed & (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT)) frames += 15 * fps;
   if (pressed & (1u << RETRO_DEVICE_ID_JOYPAD_UP)) frames += 180 * fps;
   if (pressed & (1u << RETRO_DEVICE_ID_JOYPAD_DOWN)) frames -= 180 * fps;
   if (pressed & (1u << RETRO_DEVICE_ID_JOYPAD_L2)) frames -= 300 * fps;
   if (pressed & (1u << RETRO_DEVICE_ID_JOYPAD_R2)) frames += 300 * fps;
   return frames;
}

#endif
