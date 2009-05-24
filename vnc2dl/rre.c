/*
 *  Copyright (c) 2009 Cambridge Visual Networks Ltd.  All Rights Reserved.
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
 * rre.c - handle RRE encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles an RRE
 * encoded rectangle with BPP bits per pixel.
 */

#define HandleRREBPP CONCAT2E(HandleRRE,BPP)
#define CARDBPP CONCAT2E(CARD,BPP)

static Bool
HandleRREBPP (int rx, int ry, int rw, int rh)
{
    rfbRREHeader hdr;
    int i;
    CARDBPP pix;
    rfbRectangle subrect;
    dlo_rect_t rec;

    if (!ReadFromRFBServer((char *)&hdr, sz_rfbRREHeader))
        return False;

    hdr.nSubrects = Swap32IfLE(hdr.nSubrects);

    if (!ReadFromRFBServer((char *)&pix, sizeof(pix)))
        return False;

    rec.origin.x = rx;
    rec.origin.y = ry;
    rec.width = rw;
    rec.height = rh;
    ERR(dlo_fill_rect(dl_uid, NULL, &rec, pix)); 
    // ERR(dlo_fill_rect(dl_uid, NULL, &rec, DLO_RGB(0, 0, 0))); 

    for (i = 0; i < hdr.nSubrects; i++) {
        if (!ReadFromRFBServer((char *)&pix, sizeof(pix)))
            return False;

        if (!ReadFromRFBServer((char *)&subrect, sz_rfbRectangle))
            return False;

        rec.origin.x = rx + Swap16IfLE(subrect.x);
        rec.origin.y = ry + Swap16IfLE(subrect.y);
        rec.width  = Swap16IfLE(subrect.w);
        rec.height = Swap16IfLE(subrect.h);

        ERR(dlo_fill_rect(dl_uid, NULL, &rec, pix));
    }

    return True;

    error:
        return False;
}
