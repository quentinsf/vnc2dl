/* 
/*
 *  Copyright(c) 2009 Q.Stafford-Fraser. All Rights Reserved.
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
 * Some types that would be defined in Xmd.h on a machine with X.
 * These are used in RFB definitions.
 */

#ifndef XTYPES_H
#define XTYPES_H 1
#include <inttypes.h>

/* It would be better to use the uint8_t types etc defined in stdint.h
   but the definitions in the JPEG library headers conflict with some of these.
*/

typedef char           INT8;
typedef short          INT16;
typedef long           INT32;
typedef long long      INT64;

typedef unsigned char       CARD8;
typedef unsigned short      CARD16;
typedef unsigned long       CARD32;
typedef unsigned long long  CARD64;

typedef int             Bool;
typedef char *          String;
#define True  1
#define False 0

#endif /* XTYPES_H */
