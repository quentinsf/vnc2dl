/*
 *  Copyright (C) 2001 Constantin Kaplinsky.  All Rights Reserved.
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
 * cursor.c - code to support cursor shape updates (XCursor, RichCursor).
 */

#include <vncviewer.h>


/* Copied from Xvnc/lib/font/util/utilbitmap.c */
static unsigned char _reverse_byte[0x100] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/* Data kept for HandleXCursor() function. */
static Bool prevXCursorSet = False;
static Cursor prevXCursor;

/* Data kept for RichCursor encoding support. */
static Bool prevRichCursorSet = False;
static Pixmap rcSavedArea;
static CARD8 *rcSource, *rcMask;
static int rcHotX, rcHotY, rcWidth, rcHeight;
static int rcCursorX, rcCursorY;
static int rcHideCounter;

static void SoftCursorCopyArea(Bool toScreen);
static void SoftCursorDraw(void);
static void FreeCursors(Bool setDotCursor);

/*
 * XCursor encoding support. XCursor shape updates are translated
 * directly into X cursors and saved in the prevXCursor variable.
 */

Bool HandleXCursor(int xhot, int yhot, int width, int height)
{
  rfbXCursorColors colors;
  size_t bytesPerRow, bytesData;
  char *buf = NULL;
  XColor bg, fg;
  Drawable dr;
  unsigned int wret = 0, hret = 0;
  Pixmap source, mask;
  Cursor cursor;
  int i;

  bytesPerRow = (width + 7) / 8;
  bytesData = bytesPerRow * height;
  dr = DefaultRootWindow(dpy);

  if (width * height) {
    if (!ReadFromRFBServer((char *)&colors, sz_rfbXCursorColors))
      return False;

    buf = malloc(bytesData * 2);
    if (buf == NULL)
      return False;

    if (!ReadFromRFBServer(buf, bytesData * 2)) {
      free(buf);
      return False;
    }

    XQueryBestCursor(dpy, dr, width, height, &wret, &hret);
  }

  if (width * height == 0 || wret < width || hret < height) {
    /* Free resources, restore dot cursor. */
    if (buf != NULL)
      free(buf);
    FreeCursors(True);
    return True;
  }

  bg.red   = (unsigned short)colors.backRed   << 8 | colors.backRed;
  bg.green = (unsigned short)colors.backGreen << 8 | colors.backGreen;
  bg.blue  = (unsigned short)colors.backBlue  << 8 | colors.backBlue;
  fg.red   = (unsigned short)colors.foreRed   << 8 | colors.foreRed;
  fg.green = (unsigned short)colors.foreGreen << 8 | colors.foreGreen;
  fg.blue  = (unsigned short)colors.foreBlue  << 8 | colors.foreBlue;

  for (i = 0; i < bytesData * 2; i++)
    buf[i] = (char)_reverse_byte[(int)buf[i] & 0xFF];

  source = XCreateBitmapFromData(dpy, dr, buf, width, height);
  mask = XCreateBitmapFromData(dpy, dr, &buf[bytesData], width, height);
  cursor = XCreatePixmapCursor(dpy, source, mask, &fg, &bg, xhot, yhot);
  XFreePixmap(dpy, source);
  XFreePixmap(dpy, mask);
  free(buf);

  FreeCursors(False);
  XDefineCursor(dpy, desktopWin, cursor);
  prevXCursor = cursor;
  prevXCursorSet = True;

  return True;
}

Bool HandleRichCursor(int xhot, int yhot, int width, int height)
{
  size_t bytesPerRow, bytesMaskData;
  Drawable dr;
  char *buf;
  CARD8 *ptr;
  int x, y, b;

  bytesPerRow = (width + 7) / 8;
  bytesMaskData = bytesPerRow * height;
  dr = DefaultRootWindow(dpy);

  FreeCursors(True);

  if (width * height == 0)
    return True;

  /* Read cursor pixel data. */

  rcSource = malloc(width * height * (myFormat.bitsPerPixel / 8));
  if (rcSource == NULL)
    return False;

  if (!ReadFromRFBServer((char *)rcSource,
                         width * height * (myFormat.bitsPerPixel / 8))) {
    free(rcSource);
    return False;
  }

  /* Read and decode mask data. */

  buf = malloc(bytesMaskData);
  if (buf == NULL) {
    free(rcSource);
    return False;
  }

  if (!ReadFromRFBServer(buf, bytesMaskData)) {
    free(rcSource);
    free(buf);
    return False;
  }

  rcMask = malloc(width * height);
  if (rcMask == NULL) {
    free(rcSource);
    free(buf);
    return False;
  }

  ptr = rcMask;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width / 8; x++) {
      for (b = 7; b >= 0; b--) {
        *ptr++ = buf[y * bytesPerRow + x] >> b & 1;
      }
    }
    for (b = 7; b > 7 - width % 8; b--) {
      *ptr++ = buf[y * bytesPerRow + x] >> b & 1;
    }
  }

  free(buf);

  /* Set remaining data associated with cursor. */

  dr = DefaultRootWindow(dpy);
  rcSavedArea = XCreatePixmap(dpy, dr, width, height, visdepth);
  rcHotX = xhot;
  rcHotY = yhot;
  rcWidth = width;
  rcHeight = height;

  SoftCursorCopyArea(False);
  SoftCursorDraw();

  rcHideCounter = 0;
  prevRichCursorSet = True;
  return True;
}

Bool SoftCursorInArea(int x, int y, int w, int h)
{
  if (!prevRichCursorSet)
    return False;

  return (x < rcCursorX - rcHotX + rcWidth &&
          y < rcCursorY - rcHotY + rcHeight &&
          x + w >= rcCursorX - rcHotX &&
          y + h >= rcCursorY - rcHotY);
}

void SoftCursorHide(void)
{
  if (!prevRichCursorSet)
    return;

  if (!rcHideCounter++)
    SoftCursorCopyArea(True);
}

void SoftCursorShow(void)
{
  if (!prevRichCursorSet)
    return;

  if (!--rcHideCounter) {
    SoftCursorCopyArea(False);
    SoftCursorDraw();
  }
}

void SoftCursorMove(int x, int y)
{
  if (prevRichCursorSet && !rcHideCounter) {
    SoftCursorCopyArea(True);
  }

  rcCursorX = x;
  rcCursorY = y;

  if (prevRichCursorSet && !rcHideCounter) {
    SoftCursorCopyArea(False);
    SoftCursorDraw();
  }
}

/*
 * Internal low-level functions.
 */

static void SoftCursorCopyArea(Bool toScreen)
{
  int x, y, w, h;

  x = rcCursorX - rcHotX;
  y = rcCursorY - rcHotY;
  if (x >= si.framebufferWidth || y >= si.framebufferHeight)
    return;

  w = rcWidth;
  h = rcHeight;
  if (x < 0) {
    w += x;
    x = 0;
  } else if (x + w > si.framebufferWidth) {
    w = si.framebufferWidth - x;
  }
  if (y < 0) {
    h += y;
    y = 0;
  } else if (y + h > si.framebufferHeight) {
    h = si.framebufferHeight - y;
  }

  if (!toScreen) {
    /* Save screen area in memory. */
#ifdef MITSHM
    if (appData.useShm)
      XSync(dpy, False);
#endif
    XCopyArea(dpy, desktopWin, rcSavedArea, gc, x, y, w, h, 0, 0);
  } else {
    /* Restore screen area. */
    XCopyArea(dpy, rcSavedArea, desktopWin, gc, 0, 0, w, h, x, y);
  }
}

static void SoftCursorDraw(void)
{
  int x, y, x0, y0;
  int offset, bytesPerPixel;
  char *pos;

  bytesPerPixel = myFormat.bitsPerPixel / 8;

  /* FIXME: Implementation may be inefficient. */
  for (y = 0; y < rcHeight; y++) {
    y0 = rcCursorY - rcHotY + y;
    if (y0 >= 0 && y0 < si.framebufferHeight) {
      for (x = 0; x < rcWidth; x++) {
        x0 = rcCursorX - rcHotX + x;
        if (x0 >= 0 && x0 < si.framebufferWidth) {
          offset = y * rcWidth + x;
          if (rcMask[offset]) {
            pos = (char *)&rcSource[offset * bytesPerPixel];
            CopyDataToScreen(pos, x0, y0, 1, 1);
          }
        }
      }
    }
  }
}

static void FreeCursors(Bool setDotCursor)
{
  if (setDotCursor)
    XDefineCursor(dpy, desktopWin, dotCursor);

  if (prevXCursorSet) {
    XFreeCursor(dpy, prevXCursor);
    prevXCursorSet = False;
  }

  if (prevRichCursorSet) {
    SoftCursorCopyArea(True);
    XFreePixmap(dpy, rcSavedArea);
    free(rcSource);
    free(rcMask);
    prevRichCursorSet = False;
  }
}

