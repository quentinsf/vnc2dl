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

#ifndef _TIGHT_C_INCLUDED_
#define _TIGHT_C_INCLUDED_

/*****************************************************************************
 *
 * The following code is independent of BPP value and thus should be defined
 * only once.
 *
 *****************************************************************************/

/* Compression streams for zlib library */
static z_stream zlibStream[4];
static Bool zlibStreamActive[4] = {
  False, False, False, False
};

/* DEBUG; ``buffer'' variable should be used instead */
#define DEBUG_BUFFER_SIZE 1024
static char debug_buffer[DEBUG_BUFFER_SIZE];

static char *rgbBuffer = NULL;
static int rgbBufferSize = 0;
static char *rawBuffer = NULL;
static int rawBufferSize = 0;

#endif /* not defined _TIGHT_C_INCLUDED_ */


/*****************************************************************************
 *
 * BPP-dependent code.  Included several times for 8, 16, 32 bits per pixel.
 *
 *****************************************************************************/

#define CARDBPP CONCAT2E(CARD,BPP)
#define transPtrBPP CONCAT2E(transPtr,BPP)
#define filterPtrBPP CONCAT2E(filterPtr,BPP)

#define HandleTightBPP CONCAT2E(HandleTight,BPP)
#define HandleTightBPP CONCAT2E(HandleTightData,BPP)
#define FilterCopyBPP CONCAT2E(FilterCopy,BPP)
#define FilterHdiffBPP CONCAT2E(FilterHdiff,BPP)

/* Type declarations */

#if BPP == 8
typedef CARDBPP (*transPtrBPP)(CARD8);
#else
typedef CARDBPP (*transPtrBPP)(CARD8, CARD8, CARD8);
#endif

typedef void (*filterPtrBPP)(char *, CARDBPP *, int, int);

/* Prototypes */

static Bool HandleTightBPP (int rx, int ry, int rw, int rh);
static Bool HandleTightDataBPP (int compressedLen, int stream_id,
                                filterPtrBPP filterFn,
                                int rx, int ry, int rw, int rh);
static void FilterCopyBPP (char *rgb, CARDBPP *clientData, int w, int h);
static void FilterHdiffBPP (char *rgb, CARDBPP *clientData, int w, int h);

/* Definitions */

static Bool
HandleTightBPP (int rx, int ry, int rw, int rh)
{
  CARDBPP fill_colour;
  CARD8 comp_ctl;
  CARD8 filter_id;
  filterPtrBPP filterFn;
  int stream_id;
  int compressedLen;
  CARD8 b1 = 0, b2 = 0, b3 = 0;

  if (!ReadFromRFBServer((char *)&comp_ctl, 1))
    return False;

  /* Flush zlib streams if we are told by the server to do so. */
  for (stream_id = 0; stream_id < 4; stream_id++) {
    if ((comp_ctl & 1) && zlibStreamActive[stream_id]) {
      if (inflateEnd (zlibStream[stream_id]) != OK)
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
    XFillRectangle(dpy, desktopWin, gc, x, y, w, h);
    return True;
  }

  /* Quit on unsupported subencoding value. */
  if (comp_ctl >=8)
    return False;

  /* Primary mode: optional filter + zlib compression: */

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
    filterFn = filterCopyBPP;

  /* FIXME: Here we should read parameters for current filter */
  /*        (currently none of filters needs parameters).     */

  /* Read the length (1..3 bytes) of compressed data following. */
  if (!ReadFromRFBServer((char *)&b1, 1))
    return False;
  compressedLen = (int)b1 & 0x7F;
  if (b1 & 0x80) {
    if (!ReadFromRFBServer((char *)&b2, 1))
      return False;
    compressedLen |= ((int)b2 & 0x7F) << 7;
    if (b2 & 0x80) {
      if (!ReadFromRFBServer((char *)&b3, 1))
        return False;
      compressedLen |= ((int)b3 & 0xFF) << 14;
    }
  }

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
  if (!handleTightDataBPP(compressedLen, stream_id, filterFn, rx, ry, rw, rh))
    return False;

  return True;
}

static Bool
handleTightDataBPP(int compressedLen, int stream_id, filterPtrBPP filterFn,
                   int rx, int ry, int rw, int rh)
{
  z_streamp zs;
  int rgbLen, readLen;
  int err;

  /* 
   * FIXME: get rid of such large output buffers (rgbBuffer, rawBuffer).
   */

  zs = &zlibStream[stream_id];
  rgbLen = rw * rh * 3;

  /* Allocate space for 24-bit RGB samples */
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
    if (!ReadFromRFBServer(debug_buffer, readLen))
      return False;

    zs->next_in = (Bytef *)debug_buffer;
    zs->avail_in = readLen;

    err = inflate (zs, Z_SYNC_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END)
      return False;
    if (err == Z_STREAM_END)    /* DEBUG */
      fprintf(stderr, "inflate: %s\n", zs->msg);

    if (zs->avail_in) {
      fprintf(stderr, "inflate: extra data in the compressed stream\n");
      return False;
    }

    compressedLen -= readLen;
  }

  if (zs->avail_out != 0) {
    fprintf(stderr, "inflate: not enough data in the compressed stream\n");
    return False;
  }

  filterFnBPP(rgbBuffer, rawBuffer, rw, rh);

  CopyDataToScreen(rawBuffer, rx, ry, rw, rh);

  return True;
}

/*----------------------------------------------------------------------------
 *
 * Filter functions.
 *
 */

static void
filterCopyBPP (char *rgb, CARDBPP *clientData, int w, int h)
{
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      clientData[y*w] = transFn(rgb[(y*w+x)*3],
                                rgb[(y*w+x)*3+1],
                                rgb[(y*w+x)*3+2]);
    }
  }
}

static void
filterHdiffBPP (char *rgb, CARDBPP *clientData, int w, int h)
{
  int x, y;

  if (w && h) {
    clientData[0] = translateColor(rgb[0], rgb[1], rgb[2]);
    for (x = 1; x < w; x++) {
      clientData[x] = transFn(rgb[x*3] - rgb[(x-1)*3],
                              rgb[x*3+1] - rgb[(x-1)*3+1],
                              rgb[x*3+2] - rgb[(x-1)*3+2]);
    }
    for (y = 1; y < h; y++) {
      clientData[y*w] = transFn(rgb[y*w*3] - rgb[(y*w-1)*3],
                                rgb[y*w*3+1] - rgb[(y*w-1)*3+1],
                                rgb[y*w*3+2] - rgb[(y*w-1)*3+2]);
      for (x = 1; x < w; x++) {
        clientData[y*w] = transFn(rgb[(y*w+x)*3] - rgb[(y*w+x-1)*3],
                                  rgb[(y*w+x)*3+1] - rgb[(y*w+x-1)*3+1],
                                  rgb[(y*w+x)*3+2] - rgb[(y*w+x-1)*3+2]);
      }
    }
  }
}

