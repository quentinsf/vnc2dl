/* $XConsortium: sperr.c,v 1.6 94/04/17 20:17:47 dpw Exp $ */
/*
 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of M.I.T., Network Computing Devices,
 * or Digital not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. Network Computing Devices, or Digital
 * make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES, DIGITAL AND MIT DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, DIGITAL OR MIT BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/*

Copyright (c) 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
#include	"spint.h"

#if NeedVarargsPrototypes
#include <stdarg.h>

void
SpeedoErr(char *str, ...)
{
    va_list v;
    int a1;

    va_start(v, str);
    ErrorF("Speedo: ");
    a1 = va_arg(v, int);
    ErrorF(str, a1);
    va_end(v);
}

#else

/* VARARGS1 */
void
SpeedoErr(str, a1)
    char       *str;
    char       *a1;
{
    ErrorF("Speedo: ");
    ErrorF(str, a1);
}
#endif /* NeedVarargsPrototypes else */


/*
 * Called by Speedo character generator to report an error.
 *
 *  Since character data not available is one of those errors
 *  that happens many times, don't report it to user
 */
void
sp_report_error(n)
    fix15       n;
{
    switch (n) {
    case 1:
	SpeedoErr("Insufficient font data loaded\n");
	break;
    case 3:
	SpeedoErr("Transformation matrix out of range\n");
	break;
    case 4:
	SpeedoErr("Font format error\n");
	break;
    case 5:
	SpeedoErr("Requested specs not compatible with output module\n");
	break;
    case 7:
	SpeedoErr("Intelligent transformation requested but not supported\n");
	break;
    case 8:
	SpeedoErr("Unsupported output mode requested\n");
	break;
    case 9:
	SpeedoErr("Extended font loaded but only compact fonts supported\n");
	break;
    case 10:
	SpeedoErr("Font specs not set prior to use of font\n");
	break;
    case 12:
	break;
    case 13:
	SpeedoErr("Track kerning data not available()\n");
	break;
    case 14:
	SpeedoErr("Pair kerning data not available()\n");
	break;
    default:
	SpeedoErr("report_error(%d)\n", n);
	break;
    }
}
