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
#define transPtrBPP CONCAT2E(transPtr,BPP)
#define filterPtrBPP CONCAT2E(filterPtr,BPP)

#define HandleTightBPP CONCAT2E(HandleTight,BPP)
#define HandleTightDataBPP CONCAT2E(HandleTightData,BPP)
#define FilterCopyBPP CONCAT2E(FilterCopy,BPP)
#define FilterHdiffBPP CONCAT2E(FilterHdiff,BPP)
#define TransFnBPP CONCAT2E(TransFn,BPP)

/* Type declarations */

#if BPP != 8
typedef CARDBPP (*transPtrBPP)(CARD8, CARD8, CARD8);
#endif

typedef void (*filterPtrBPP)(char *, CARDBPP *, int, int);

/* Prototypes */

static Bool HandleTightDataBPP (int compressedLen, int stream_id,
                                filterPtrBPP filterFn,
                                int rx, int ry, int rw, int rh);
static void FilterCopyBPP (char *rgb, CARDBPP *clientData, int w, int h);
static void FilterHdiffBPP (char *rgb, CARDBPP *clientData, int w, int h);

#if BPP != 8
static CARDBPP TransFnBPP (CARD8 r, CARD8 g, CARD8 b);
#endif

/* Definitions */

static Bool
HandleTightBPP (int rx, int ry, int rw, int rh)
{
  CARDBPP fill_colour;
  XGCValues gcv;
  CARD8 comp_ctl;
  CARD8 filter_id;
  filterPtrBPP filterFn;
  int stream_id;
  int compressedLen;
  int err;

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
   * Here handling of primary compression mode begins.
   * Data was processed with optional filter + zlib compression.
   */

  /* First, we should identify a filter to use. */
  if (comp_ctl >= 4) {
    if (!ReadFromRFBServer((char*)&filter_id, 1))
      return False;
    switch (filter_id) {
    case rfbTightFilterCopy:  filterFn = FilterCopyBPP;
      break;
    case rfbTightFilterHdiff: filterFn = FilterHdiffBPP;
      break;
    delault:
      return False;
    }
  } else
    filterFn = FilterCopyBPP;

  /* FIXME: Here we should read parameters for current filter */
  /*        (currently none of filters needs parameters).     */

  /* Read the length (1..3 bytes) of compressed data following. */
  compressedLen = ReadCompactLen();
  if (compressedLen < 0)
    return False;

  /* Now let's initialize compression stream if needed. */
  stream_id = comp_ctl & 0x03;
  if (!zlibStreamActive[stream_id]) {
    zlibStream[stream_id].zalloc = Z_NULL;
    zlibStream[stream_id].zfree = Z_NULL;
    zlibStream[stream_id].opaque = NULL;
    err = inflateInit(&zlibStream[stream_id]);
    if (err != Z_OK)
      return False;
  }

  /* Read compressed data, restore and draw the whole rectangle. */
  if (!HandleTightDataBPP(compressedLen, stream_id, filterFn, rx, ry, rw, rh))
    return False;

  return True;
}

static Bool
HandleTightDataBPP(int compressedLen, int stream_id, filterPtrBPP filterFn,
                   int rx, int ry, int rw, int rh)
{
  z_streamp zs;
  int rgbLen, readLen;
  int err;

  /* 
   * FIXME: get rid of such large output buffers (rgbBuffer, rawBuffer).
   */

  zs = &zlibStream[stream_id];
#if BPP == 8
  rgbLen = rw * rh;
#else
  rgbLen = rw * rh * 3;
#endif

  /* Allocate space for 8-bit colors or 24-bit RGB samples */
  if (rgbBufferSize < rgbLen) {
    if (rgbBuffer != NULL)
      free(rgbBuffer);
    rgbBufferSize = rgbLen;
    rgbBuffer = malloc(rgbBufferSize);
    if (rgbBuffer == NULL)
      return False;
  }

  /* Allocate space for raw data */
  if (rawBufferSize < rw * rh * (BPP / 8)) {
    if (rawBuffer != NULL)
      free(rawBuffer);
    rawBufferSize = rw * rh * (BPP / 8);
    rawBuffer = malloc(rawBufferSize);
    if (rawBuffer == NULL)
      return False;
  }

  /* Prepare compression stream */
  zs->next_out = (Bytef *)rgbBuffer;
  zs->avail_out = rgbBufferSize;

  /* Read compressed stream and decompress it with zlib */
  while (compressedLen > 0) {
    if (compressedLen < DEBUG_BUFFER_SIZE)
      readLen = compressedLen;
    else
      readLen = DEBUG_BUFFER_SIZE;
    if (!ReadFromRFBServer(buffer, readLen))
      return False;

    zs->next_in = (Bytef *)buffer;
    zs->avail_in = readLen;

    err = inflate (zs, Z_SYNC_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END)
      return False;
    if (err == Z_STREAM_END)    /* DEBUG */
      fprintf(stderr, "inflate: %s\n", zs->msg);

    if (zs->avail_in) {
      fprintf(stderr, "inflate: extra data in compressed stream\n");
      return False;
    }

    compressedLen -= readLen;
  }

  if (zs->avail_out != 0) {
    fprintf(stderr, "inflate: not enough data in compressed stream\n");
    return False;
  }

  filterFn(rgbBuffer, (CARDBPP *)rawBuffer, rw, rh);

  CopyDataToScreen(rawBuffer, rx, ry, rw, rh);

  return True;
}

/*----------------------------------------------------------------------------
 *
 * Filter functions.
 *
 */

#if BPP == 8

static void
FilterCopyBPP (char *rgb, CARDBPP *clientData, int w, int h)
{
  int x, y;

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++)
      clientData[y*w] = rgb[y*w+x];
  }
}

static void
FilterHdiffBPP (char *rgb, CARDBPP *clientData, int w, int h)
{
  int x, y;

  if (w && h) {
    clientData[0] = rgb[0];
    for (x = 1; x < w; x++)
      clientData[x] = rgb[x] - rgb[x-1];
    for (y = 1; y < h; y++) {
      clientData[y*w] = rgb[y*w] - rgb[y*w-1];
      for (x = 1; x < w; x++)
        clientData[y*w] = rgb[y*w+x] - rgb[y*w+x-1];
    }
  }
}

#else

static void
FilterCopyBPP (char *rgb, CARDBPP *clientData, int w, int h)
{
  int x, y;

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++)
      clientData[y*w] = TransFnBPP(rgb[(y*w+x)*3],
                                   rgb[(y*w+x)*3+1],
                                   rgb[(y*w+x)*3+2]);
  }
}

static void
FilterHdiffBPP (char *rgb, CARDBPP *clientData, int w, int h)
{
  int x, y;

  if (w && h) {
    clientData[0] = TransFnBPP(rgb[0], rgb[1], rgb[2]);
    for (x = 1; x < w; x++) {
      clientData[x] = TransFnBPP(rgb[x*3] - rgb[(x-1)*3],
                                 rgb[x*3+1] - rgb[(x-1)*3+1],
                                 rgb[x*3+2] - rgb[(x-1)*3+2]);
    }
    for (y = 1; y < h; y++) {
      clientData[y*w] = TransFnBPP(rgb[y*w*3] - rgb[(y*w-1)*3],
                                   rgb[y*w*3+1] - rgb[(y*w-1)*3+1],
                                   rgb[y*w*3+2] - rgb[(y*w-1)*3+2]);
      for (x = 1; x < w; x++) {
        clientData[y*w] = TransFnBPP(rgb[(y*w+x)*3] - rgb[(y*w+x-1)*3],
                                     rgb[(y*w+x)*3+1] - rgb[(y*w+x-1)*3+1],
                                     rgb[(y*w+x)*3+2] - rgb[(y*w+x-1)*3+2]);
      }
    }
  }
}

#endif

/*----------------------------------------------------------------------------
 *
 * Functions to translate RGB sample into local pixel format.
 *
 */

#if BPP != 8

static CARDBPP
TransFnBPP (CARD8 r, CARD8 g, CARD8 b)
{
  return (((CARDBPP)r & myFormat.redMax) << myFormat.redShift |
          ((CARDBPP)g & myFormat.greenMax) << myFormat.greenShift |
          ((CARDBPP)b & myFormat.blueMax) << myFormat.blueShift);
}

#endif

