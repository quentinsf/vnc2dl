/*
 *  Copyright (C) 2000 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/*
 * tight.c - handle ``tight'' encoding.
 *
 * This file shouldn't be compiled directly. It is included multiple
 * times by rfbproto.c, each time with a different definition of the
 * macro BPP. For each value of BPP, this file defines a function
 * which handles an zlib encoded rectangle with BPP bits per pixel.
 *
 * FIXME: Print messages to stderr on errors.
 * FIXME: Split into separate functions in a better way.
 */

#define CARDBPP CONCAT2E(CARD,BPP)
#define filterPtrBPP CONCAT2E(filterPtr,BPP)

#define HandleTightBPP CONCAT2E(HandleTight,BPP)
#define InitFilterCopyBPP CONCAT2E(InitFilterCopy,BPP)
#define FilterCopyBPP CONCAT2E(FilterCopy,BPP)

/* Type declarations */

typedef void (*filterPtrBPP)(int, CARDBPP *);

/* Prototypes */

static int InitFilterCopyBPP (int rw, int rh);
static void FilterCopyBPP (int numRows, CARDBPP *destBuffer);

/* Definitions */

static Bool
HandleTightBPP (int rx, int ry, int rw, int rh)
{
  CARDBPP fill_colour;
  XGCValues gcv;
  CARD8 comp_ctl;
  CARD8 filter_id;
  filterPtrBPP filterFn;
  z_streamp zs;
  char *buffer2;
  int err, stream_id, compressedLen, bitsPixel;
  int bufferSize, rowSize, numRows, portionLen, rowsProcessed, extraBytes;
  CARDBPP *rawData;

  if (!ReadFromRFBServer((char *)&comp_ctl, 1))
    return False;

  /* Flush zlib streams if we are told by the server to do so. */
  for (stream_id = 0; stream_id < 4; stream_id++) {
    if ((comp_ctl & 1) && zlibStreamActive[stream_id]) {
      if (inflateEnd (&zlibStream[stream_id]) != Z_OK)
        fprintf(stderr, "inflateEnd: %s\n",
                zlibStream[stream_id].msg); /* DEBUG */
      zlibStreamActive[stream_id] = False;
    }
    comp_ctl >>= 1;
  }

  /* Handle solid rectangles. */
  if (comp_ctl == rfbTightFill) {
    if (!ReadFromRFBServer((char*)&fill_colour, sizeof(fill_colour)))
      return False;
    gcv.foreground = fill_colour;
    XChangeGC(dpy, gc, GCForeground, &gcv);
    XFillRectangle(dpy, desktopWin, gc, rx, ry, rw, rh);
    return True;
  }

  /* Quit on unsupported subencoding value. */
  if (comp_ctl >=8)
    return False;

  /*
   * Here primary compression mode handling begins.
   * Data was processed with optional filter + zlib compression.
   */

  /* First, we should identify a filter to use. */
  if (comp_ctl >= 4) {
    if (!ReadFromRFBServer((char*)&filter_id, 1))
      return False;
    switch (filter_id) {
    case rfbTightFilterCopy:
      filterFn = FilterCopyBPP;
      bitsPixel = InitFilterCopyBPP(rw, rh);
      break;
/*
    case rfbTightFilterPalette:
      filterFn = FilterPaletteBPP;
      bitsPixel = InitFilterPaletteBPP(rw, rh);
      break;
    case rfbTightFilterGradient:
      filterFn = FilterGradientBPP;
      bitsPixel = InitFilterGradientBPP(rw, rh);
      break;
    case rfbTightFilterGradientPalette:
      filterFn = FilterGradientPaletteBPP;
      bitsPixel = InitFilterGradientPaletteBPP(rw, rh);
      break;
*/
    delault:
      return False;
    }
  } else {
    filterFn = FilterCopyBPP;
    bitsPixel = InitFilterCopyBPP(rw, rh);
  }
  if (!bitsPixel)
    return False;               /* Filter initialization failed. */

  /* Read the length (1..3 bytes) of compressed data following. */
  compressedLen = ReadCompactLen();
  if (compressedLen < 0)
    return False;

  /* Now let's initialize compression stream if needed. */
  stream_id = comp_ctl & 0x03;
  zs = &zlibStream[stream_id];
  if (!zlibStreamActive[stream_id]) {
    zs->zalloc = Z_NULL;
    zs->zfree = Z_NULL;
    zs->opaque = Z_NULL;
    err = inflateInit(zs);
    if (err != Z_OK)
      return False;
    zlibStreamActive[stream_id] = True;
  }

  /* Read, decode and draw actual pixel data in a loop. */

  bufferSize = BUFFER_SIZE * bitsPixel / (bitsPixel + BPP) & 0xFFFFFFFC;
  buffer2 = &buffer[bufferSize];
  rowSize = (rw * bitsPixel + 7) / 8;
  if (rowSize > bufferSize)
    return False;               /* Impossible when BUFFER_SIZE >= 16384 */

  rowsProcessed = 0;
  extraBytes = 0;

  while (compressedLen > 0) {
    if (compressedLen > ZLIB_BUFFER_SIZE)
      portionLen = ZLIB_BUFFER_SIZE;
    else
      portionLen = compressedLen;

    if (!ReadFromRFBServer((char*)zlib_buffer, portionLen))
      return False;

    compressedLen -= portionLen;

    zs->next_in = (Bytef *)zlib_buffer;
    zs->avail_in = portionLen;

    do {
      zs->next_out = (Bytef *)&buffer[extraBytes];
      zs->avail_out = bufferSize - extraBytes;

      err = inflate(zs, Z_SYNC_FLUSH);
      if (err != Z_OK && err != Z_STREAM_END) {
        fprintf(stderr, "inflate: %s\n", zs->msg);
        return False;
      }

      numRows = (bufferSize - zs->avail_out) / rowSize;

      filterFn(numRows, (CARDBPP *)buffer2);

      extraBytes = bufferSize - zs->avail_out - numRows * rowSize;
      if (extraBytes > 0)
        memcpy(buffer, &buffer[numRows * rowSize], extraBytes);

      CopyDataToScreen(buffer2, rx, ry + rowsProcessed, rw, numRows);
      rowsProcessed += numRows;
    }
    while (zs->avail_out == 0);
  }

  if (rowsProcessed != rh)
    return False;

  return True;
}

/*----------------------------------------------------------------------------
 *
 * Filter stuff.
 *
 */

/*
   The following variables are defined in rfbproto.c:
     Bool cutZeros;
     int rectWidth, rectHeight, currentLine, colorsUsed;
     char rgbPalette[256*3];
*/

static int
InitFilterCopyBPP (int rw, int rh)
{
  rectWidth = rw;

#if BPP == 32
  if (myFormat.depth == 24 && myFormat.redMax == 0xFF &&
      myFormat.greenMax == 0xFF && myFormat.blueMax == 0xFF) {
    cutZeros = True;
    return 24;
  }
  cutZeros = False;
  return BPP;
#else
  return BPP;
#endif
}

static void
FilterCopyBPP (int numRows, CARDBPP *destBuffer)
{
#if BPP == 32
  int x, y;

  if (cutZeros) {
    for (y = 0; y < numRows; y++) {
      for (x = 0; x < rectWidth; x++) {
        destBuffer[y*rectWidth+x] =
          ((CARDBPP)buffer[(y*rectWidth+x)*3] & 0xFF)
            << myFormat.redShift |
          ((CARDBPP)buffer[(y*rectWidth+x)*3+1] & 0xFF)
            << myFormat.greenShift |
          ((CARDBPP)buffer[(y*rectWidth+x)*3+2] & 0xFF)
            << myFormat.blueShift;
      }
    }
  } else {
    memcpy (destBuffer, buffer, numRows * rectWidth * (BPP / 8));
  }
#else
  memcpy (destBuffer, buffer, numRows * rectWidth * (BPP / 8));
#endif
}

