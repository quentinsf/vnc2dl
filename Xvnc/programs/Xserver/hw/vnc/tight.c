/*
 * tight.c
 *
 * Routines to implement Tight Encoding
 */

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

#include <stdio.h>
#include "rfb.h"


typedef struct COLOR_LIST_s {
    struct COLOR_LIST_s *next;
    int idx;
    CARD32 rgb;
} COLOR_LIST;

typedef struct PALETTE_ENTRY_s {
    COLOR_LIST *listNode;
    int numPixels;
} PALETTE_ENTRY;

typedef struct PALETTE_s {
    PALETTE_ENTRY entry[256];
    COLOR_LIST *hash[256];
    COLOR_LIST list[256];
} PALETTE;

typedef struct PALETTE8_s {
    CARD8 pixelValue[256];
    int numPixels[256];
    CARD8 colorIdx[256];
} PALETTE8;


static int paletteNumColors;
static PALETTE palette;
static PALETTE8 palette8;

static int tightBeforeBufSize = 0;
static char *tightBeforeBuf = NULL;

static int tightAfterBufSize = 0;
static char *tightAfterBuf = NULL;


static int SendSubrect(rfbClientPtr cl, int x, int y, int w, int h);
static BOOL SendSolidRect(rfbClientPtr cl, int w, int h);
static BOOL SendIndexedRect(rfbClientPtr cl, int w, int h);
static BOOL SendFullColorRect(rfbClientPtr cl, int w, int h);
static BOOL CompressData(rfbClientPtr cl, int streamId, int dataLen);
static void FillPalette(rfbClientPtr cl, int w, int h);
static void PaletteReset(void);
static int PaletteFind(CARD32 rgb);
static int PaletteInsert(CARD32 rgb);
static void PaletteReset8(void);
static int PaletteInsert8(CARD8 value);


/*
 * Tight encoding implementation.
 */

Bool
rfbSendRectEncodingTight(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    int maxBeforeSize, maxAfterSize;
    int dx, dy;
    int rw, rh;

    if ( cl->format.bitsPerPixel != 8 &&
         cl->format.bitsPerPixel != 16 &&
         cl->format.bitsPerPixel != 32 ) {
        rfbLog("rfbSendRectEncodingTight: bpp %d?\n", cl->format.bitsPerPixel);
        return FALSE;
    }

    maxBeforeSize = (TIGHT_MAX_RECT_HEIGHT * TIGHT_MAX_RECT_WIDTH *
                     (cl->format.bitsPerPixel / 8));
    maxAfterSize = maxBeforeSize + (maxBeforeSize + 99) / 100 + 12;

    if (tightBeforeBufSize < maxBeforeSize) {
	tightBeforeBufSize = maxBeforeSize;
	if (tightBeforeBuf == NULL)
	    tightBeforeBuf = (char *)xalloc(tightBeforeBufSize);
	else
	    tightBeforeBuf = (char *)xrealloc(tightBeforeBuf,
                                              tightBeforeBufSize);
    }

    if (tightAfterBufSize < maxAfterSize) {
	tightAfterBufSize = maxAfterSize;
	if (tightAfterBuf == NULL)
	    tightAfterBuf = (char *)xalloc(tightAfterBufSize);
	else
	    tightAfterBuf = (char *)xrealloc(tightAfterBuf,
                                             tightAfterBufSize);
    }

    if ((w > 2 && h > 2 && w * h > 1024) || w > 2048 || h > 2048) {
        for (dy = 0; dy < h; dy += TIGHT_MAX_RECT_HEIGHT) {
            for (dx = 0; dx < w; dx += TIGHT_MAX_RECT_WIDTH) {
                rw = (dx + TIGHT_MAX_RECT_WIDTH < w) ?
                    TIGHT_MAX_RECT_WIDTH : w - dx;
                rh = (dy + TIGHT_MAX_RECT_WIDTH < h) ?
                    TIGHT_MAX_RECT_WIDTH : w - dx;
                if (!SendSubrect(cl, x+dx, y+dy, rw, rh))
                    return FALSE;
            }
        }
    } else {
        if (!SendSubrect(cl, x, y, w, h))
            return FALSE;
    }

    return TRUE;
}

static int
SendSubrect(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    rfbFramebufferUpdateRectHeader rect;
    char *fbptr;
    int success = 0;

    if (ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingTight);

    memcpy(&updateBuf[ublen], (char *)&rect,
	   sz_rfbFramebufferUpdateRectHeader);
    ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbRectanglesSent[rfbEncodingTight]++;
    cl->rfbBytesSent[rfbEncodingTight] += sz_rfbFramebufferUpdateRectHeader;

    fbptr = (rfbScreen.pfbMemory + (rfbScreen.paddedWidthInBytes * y)
             + (x * (rfbScreen.bitsPerPixel / 8)));

    (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
                       &cl->format, fbptr, tightBeforeBuf,
                       rfbScreen.paddedWidthInBytes, w, h);

    FillPalette(cl, w, h);

    switch (paletteNumColors) {
    case 1:
        /* Solid rectangle */
        success = SendSolidRect(cl, w, h);
        break;
    case 0:
        /* Truecolor image */
        success = SendFullColorRect(cl, w, h);
        break;
    default:
        /* Up to 256 different colors */
/*
        if (w * h >= paletteNumColors * 2)
            success = SendIndexedRect(cl, w, h);
        else
*/
            success = SendFullColorRect(cl, w, h);
    }
    return success;
}


/*
 * Subencoding implementations.
 */

static BOOL
SendSolidRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    if (ublen + 1 + cl->format.bitsPerPixel / 8 > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    updateBuf[ublen++] = 8;     /* Solid rectangle, color follows */
    memcpy (&updateBuf[ublen], tightBeforeBuf, cl->format.bitsPerPixel / 8);
    ublen += cl->format.bitsPerPixel / 8;

    return TRUE;
}

static BOOL
SendIndexedRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int i, streamId, dataLen;

    if ( ublen + 5 + paletteNumColors * cl->format.bitsPerPixel / 8 >
         UPDATE_BUF_SIZE ) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    /* Prepare tight encoding header. */
    if (paletteNumColors == 2) {
        streamId = 1;
        dataLen = (w + 7) / 8;
        dataLen *= h;
    } else if (paletteNumColors <= 16) {
        streamId = 2;
        dataLen = w * h;
    } else {
        streamId = 3;
        dataLen = w * h;
    }
    updateBuf[ublen++] = streamId << 4 | 0x40; /* 0x40: filter id follows */
    updateBuf[ublen++] = rfbTightFilterPalette;

    /* Prepare palette, convert image. */
    switch (cl->format.bitsPerPixel) {
    case 32:
        for (i = 0; i < paletteNumColors; i++) {
            ((CARD32 *)tightAfterBuf)[i] =
                palette.entry[i].listNode->rgb;
        }
        memcpy(&updateBuf[ublen], tightAfterBuf, paletteNumColors * 4);
        ublen += paletteNumColors * 4;

        for (i = 0; i < w * h; i++)
            tightBeforeBuf[i] =
                (char)PaletteFind(((CARD32 *)tightBeforeBuf)[i]);
        break;

    case 16:
        for (i = 0; i < paletteNumColors; i++) {
            ((CARD16 *)tightAfterBuf)[i] =
                (CARD16)palette.entry[i].listNode->rgb;
        }
        memcpy(&updateBuf[ublen], tightAfterBuf, paletteNumColors * 2);
        ublen += paletteNumColors * 2;

        for (i = 0; i < w * h; i++)
            tightBeforeBuf[i] =
                (char)PaletteFind(((CARD16 *)tightBeforeBuf)[i]);
        break;

    default:
        memcpy (&updateBuf[ublen], palette8.pixelValue, paletteNumColors);
        ublen += paletteNumColors;

        for (i = 0; i < w * h; i++)
            tightBeforeBuf[i] = palette8.colorIdx[tightBeforeBuf[i & 0xFF]];
    }

    return CompressData(cl, streamId, dataLen);
}

static BOOL
SendFullColorRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    if ( ublen + 4 > UPDATE_BUF_SIZE ) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    updateBuf[ublen++] = 0x00;  /* stream id = 0, no flushing, no filter */

    return CompressData(cl, 0, w * h * (cl->format.bitsPerPixel / 8));
}

static BOOL
CompressData(cl, streamId, dataLen)
    rfbClientPtr cl;
    int streamId, dataLen;
{
    z_streamp pz;
    int compressedLen, portionLen;
    int i;

    pz = &cl->zsStruct[streamId];

    /* Initialize compression stream. */
    if (!cl->zsActive[streamId]) {
        pz->zalloc = Z_NULL;
        pz->zfree = Z_NULL;
        pz->opaque = Z_NULL;

        if (deflateInit2 (pz, 8, Z_DEFLATED, MAX_WBITS,
                          MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK) {
            return FALSE;
        }
        cl->zsActive[streamId] = 1;
    }

    /* Actual compression. */
    pz->next_in = (Bytef *)tightBeforeBuf;
    pz->avail_in = dataLen;
    pz->next_out = (Bytef *)tightAfterBuf;
    pz->avail_out = tightAfterBufSize;

    if ( deflate (pz, Z_SYNC_FLUSH) != Z_OK ||
         pz->avail_in != 0 || pz->avail_out == 0 ) {
        return FALSE;
    }

    compressedLen = (size_t)(tightAfterBufSize - pz->avail_out);

    /* Prepare compressed data size for sending. */
    updateBuf[ublen++] = compressedLen & 0x7F;
    if (compressedLen > 0x7F) {
        updateBuf[ublen-1] |= 0x80;
        updateBuf[ublen++] = compressedLen >> 7 & 0x7F;
        if (compressedLen > 0x3FFF) {
            updateBuf[ublen-1] |= 0x80;
            updateBuf[ublen++] = compressedLen >> 14 & 0xFF;
        }
    }

    /* Send update. */
    for (i = 0; i < compressedLen; ) {
        portionLen = compressedLen - i;
        if (portionLen > UPDATE_BUF_SIZE - ublen)
            portionLen = UPDATE_BUF_SIZE - ublen;

        memcpy(&updateBuf[ublen], &tightAfterBuf[i], portionLen);

        ublen += portionLen;
        i += portionLen;

        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    return TRUE;
}


static void
FillPalette(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int i;
    CARD8 *data8 = (CARD8 *)tightBeforeBuf;
    CARD16 *data16 = (CARD16 *)tightBeforeBuf;
    CARD32 *data32 = (CARD32 *)tightBeforeBuf;

    /* Note: this function assumes that fb pointer is 4-byte aligned. */

    switch (cl->format.bitsPerPixel) {
    case 32:
        PaletteReset();
        for (i = 0; i <= w * h; i++) {
            if (!PaletteInsert (data32[i]))
                return;
        }
        break;
    case 16:
        PaletteReset();
        for (i = 0; i <= w * h; i++) {
            if (!PaletteInsert ((CARD32)data16[i]))
                return;
        }
        break;
    default:                    /* bpp == 8 */
        PaletteReset8();
        for (i = 0; i <= w * h; i++) {
            if (!PaletteInsert8 (data8[i]))
                return;
        }
        break;
    }
}


/*
 * Functions to operate with palette structures.
 */

static void
PaletteReset(void)
{
    int i;

    paletteNumColors = 0;
    for (i = 0; i < 256; i++)
        palette.hash[i] = NULL;
}

static int
PaletteFind(rgb)
    CARD32 rgb;
{
    COLOR_LIST *pnode;
    CARD8 *crgb = (CARD8 *)&rgb;

    pnode = palette.hash[(int)(crgb[0] + crgb[1] + crgb[2]) & 0xFF];

    while (pnode != NULL) {
        if (pnode->rgb == rgb)
            return pnode->idx;
        pnode = pnode->next;
    }
    return -1;
}

static int
PaletteInsert(rgb)
    CARD32 rgb;
{
    COLOR_LIST *pnode, *new_pnode;
    COLOR_LIST *prev_pnode = NULL;
    int hash_key, idx, new_idx, count;
    CARD8 *crgb = (CARD8 *)&rgb;

    hash_key = (int)(crgb[0] + crgb[1] + crgb[2]) & 0xFF;
    pnode = palette.hash[hash_key];

    while (pnode != NULL) {
        if (pnode->rgb == rgb) {
            /* Such palette entry already exists. */
            new_idx = idx = pnode->idx;
            count = palette.entry[idx].numPixels;
            if (new_idx && count == palette.entry[new_idx-1].numPixels) {
                do {
                    new_idx--;
                }
                while (new_idx &&
                       count == palette.entry[new_idx-1].numPixels);
                /* Preserve sort order */
                new_pnode = palette.entry[new_idx].listNode;
                palette.entry[idx].listNode = new_pnode;
                palette.entry[new_idx].listNode = pnode;
                pnode->idx = new_idx;
                new_pnode->idx = idx;
            }
            palette.entry[new_idx].numPixels++;
            return paletteNumColors;
        }
        prev_pnode = pnode;
        pnode = pnode->next;
    }

    if (paletteNumColors == 256) {
        paletteNumColors = 0;
        return 0;
    }

    /* Add new palette entry. */

    idx = paletteNumColors;
    pnode = &palette.list[idx];
    if (prev_pnode != NULL) {
        prev_pnode->next = pnode;
    } else {
        palette.hash[hash_key] = pnode;
    }
    pnode->next = NULL;
    pnode->idx = idx;
    pnode->rgb = rgb;
    palette.entry[idx].listNode = pnode;
    palette.entry[idx].numPixels = 1;

    return (++paletteNumColors);
}

static void
PaletteReset8(void)
{
    int i;

    paletteNumColors = 0;
    for (i = 0; i < 256; i++)
        palette8.colorIdx[i] = -1;
}

static int
PaletteInsert8(CARD8 value)
{
    int idx, new_idx, count;
    CARD8 other_value;

    idx = palette8.colorIdx[value];
    if (idx != -1) {
        /* Such palette entry already exists. */
        new_idx = idx;
        count = palette8.numPixels[idx];
        if (new_idx && count == palette8.numPixels[new_idx-1]) {
            do {
                new_idx--;
            } while (new_idx && count == palette8.numPixels[new_idx-1]);
            /* Preserve sort order */
            other_value = palette8.pixelValue[new_idx];
            palette8.colorIdx[value] = new_idx;
            palette8.colorIdx[other_value] = idx;
            palette8.pixelValue[new_idx] = value;
            palette8.pixelValue[idx] = other_value;
        }
        palette8.numPixels[new_idx]++;
        return paletteNumColors;
    }

    if (paletteNumColors == 256) {
        paletteNumColors = 0;
        return 0;
    }

    /* Add new palette entry. */

    idx = paletteNumColors;
    palette8.colorIdx[value] = idx;
    palette8.pixelValue[idx] = value;
    palette8.numPixels[idx] = 1;

    return (++paletteNumColors);
}

