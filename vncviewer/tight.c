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
 * FIXME: Print messages to stderr on errors.
 * FIXME: Split to separate functions in a better way.
 */

/* Types */

typedef void (*filterPtr)(char *, CARDBPP *, int, int);


/* Static data */

static z_stream zlibStream[4];
static Bool zlibStreamActive[4] = { False, False, False, False };

#define DEBUG_BUFFER_SIZE 1024
char debug_buffer[DEBUG_BUFFER_SIZE]; /* DEBUG */


/* Prototypes */

static Bool initStream (int stream_id);
static Bool handleTightDataBPP (int compressedLen, int stream_id,
                                filterPtr filterFn,
                                int rx, int ry, int rw, int rh);

static filterPtr filterById (char id);
static int getCompactLen (void);

static void filterCopy (char *rgb, CARDBPP *clientData, int w, int h)
static void filterHdiff (char *rgb, CARDBPP *clientData, int w, int h);


/* Definitions */

static Bool
HandleTightBPP (int rx, int ry, int rw, int rh)
{
  CARDBPP fill_colour;
  CARD8 comp_ctl;
  CARD8 filter_id = 0;
  filterPtr filterFn == NULL;
  int stream_id;
  int compressedLen;

  if (!ReadFromRFBServer((char *)&comp_ctl, 1))
    return False;

  for (stream_id = 0; stream_id < 4; stream_id++) {
    if ((comp_ctl & 1) && zlibStreamActive[stream_id]) {
      /* Flush zlib streams if we are told by the server to do so. */
      if (deflateEnd (zlibStream[stream_id]) != OK)
        fprintf(stderr, "deflateEnd: %s\n",
                zlibStream[stream_id].msg); /* DEBUG */
      zlibStreamActive[stream_id] = False;
    }
    comp_ctl >>= 1;
  }

  if (comp_ctl == rfbTightFill) {
    /* Solid rectangle */
    if (!ReadFromRFBServer((char*)&fill_colour, sizeof(fill_colour)))
      return False;
    gcv.foreground = fill_colour;
    XChangeGC(dpy, gc, GCForeground, &gcv);
    XFillRectangle(dpy, desktopWin, gc, x, y, w, h);
  }
  else if (comp_ctl < 8) {
    /* Optional filter + zlib compression */
    /* First, we should identify a filter to use */
    if (comp_ctl >= 4) {
      if (!ReadFromRFBServer((char*)&filter_id, 1))
        return False;
      if (filter_id) {
        filterFn = filterById (filter_id);
        if (filterFn == NULL)
          return False;
      }
    }
    /* Now let's initialize compression stream if needed */
    stream_id = comp_ctl & 0x03;
    if (!zlibStreamActive[stream_id] &&
        !initStream(stream_id))
      return False;

    /* FIXME: Here we should read parameters for current filter... */

    /* Read data lenght */
    compressedLen = getCompactLen();
    if (compressedLen < 0)
      return False;

    /* Read compressed data, restore and draw the whole rectangle */
    if (!handleTightDataBPP(compressedLen, filterFn, rx, ry, rw, rh))
      return False;
  }
  else
    /* Unsupported subencoding value */
    return False;

  return True;
}

static Bool
initStream(int stream_id)
{
  zlibStream[stream_id].zalloc = Z_NULL;
  zlibStream[stream_id].zfree = Z_NULL;
  zlibStream[stream_id].opaque = NULL;
  err = inflateInit(&zlibStream[stream_id]);
  if (err != Z_OK)
    return False;

  return True;
}

static Bool
handleTightDataBPP(int compressedLen, int stream_id, filterPtr filterFn,
                   int rx, int ry, int rw, int rh)
{
  static char *rgbBuffer = NULL;
  static int rgbBufferSize = 0;
  static char *rawBuffer = NULL;
  static int rawBufferSize = 0;

  z_streamp zs;
  int rgbLen, readLen;

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

    /* ... */

    compressedLen -= readLen;
  }

  return True;
}

static filterPtr
filterById (char id)
{
  switch (id) {
  case rfbTightFilterCopy:  return filterCopy;
    break;
  case rfbTightFilterHdiff: return filterHdiff;
    break;
  delault:
    return NULL;
  }
}

static int
getCompactLen (void)            /* Won't work if sizeof(int) == 2 */
{
  int len;
  CARD8 b1 = 0, b2 = 0, b3 = 0;

  if (!ReadFromRFBServer((char *)&b1, 1))
    return -1;
  len = (int)b1 & 0x7F;
  if (b1 & 0x80) {
    if (!ReadFromRFBServer((char *)&b2, 1))
      return -1;
    len = ((int)b2 & 0x7F) << 7;
    if (b2 & 0x80) {
      if (!ReadFromRFBServer((char *)&b3, 1))
        return -1;
      len = ((int)b2 & 0xFF) << 14;
    }
  }
  return len;
}

/*
 * Filter functions
 */

static void
filterCopy (char *rgb, CARDBPP *clientData, int w, int h)
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
filterHdiff (char *rgb, CARDBPP *clientData, int w, int h)
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

