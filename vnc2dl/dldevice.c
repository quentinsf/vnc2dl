/*
 *  Talk to the DisplayLink device
 *  (c) Copyright 2009 Quentin Stafford-Fraser. All Rights Reserved. 
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

#include "vnc2dl.h"
#include <stdio.h> 
#include "libdlo.h" 

dlo_dev_t dl_uid; 

Bool InitialiseDevice() {
    dlo_init_t    ini_flags = { 0 };
    dlo_claim_t   cnf_flags = { 0 }; 
    dlo_retcode_t err; 
    dlo_mode_t desc; 
    dlo_mode_t *info; 

    /* Initialise libdlo */ 
    ERR_GOTO(dlo_init(ini_flags)); 
  /* Look for a DisplayLink device to connect to */ 
    dl_uid = dlo_claim_first_device(cnf_flags, 0); 

    /* Select a mode */ 
    desc.view.base    = 0;     /* Base address in device memory for this screen display */ 
    desc.view.width   = 1280; 
    desc.view.height  = 1024;  /* We can use zero as a wildcard here */ 
    desc.view.bpp     = 24;    /* Can be a wildcard, meaning we don't mind what colour depth */ 
    desc.refresh      = 0;     /* Refresh rate in Hz. Can be a wildcard; any refresh rate */ 
    ERR(dlo_set_mode(dl_uid, &desc)); 
    
    /* Read current mode information */ 
    info = dlo_get_mode(dl_uid); 
    NERR(info); 
    printf("DL device mode %ux%u @ %u Hz %u bpp base &%X\n", 
        info->view.width, 
        info->view.height, 
        info->refresh, 
        info->view.bpp, 
        (int)info->view.base); 


    /* Clear the screen */ 
    srandom(time(NULL));
    ERR(dlo_fill_rect(dl_uid, NULL, NULL, DLO_RGB(random() & 0xff, random() & 0xff, random() & 0xff))); 
    
    // We want the VNC server to use the device's pixel format
    myFormat.bitsPerPixel = 32;
    myFormat.depth = 24;
    myFormat.trueColour = 1;
    myFormat.bigEndian = 0;
    myFormat.redMax = 255;
    myFormat.greenMax = 255;
    myFormat.blueMax = 255;
    myFormat.redShift = 0;
    myFormat.greenShift = 8;
    myFormat.blueShift = 16;

    return True; 

  error: 
  /* The ERR_GOTO() macro jumps here if there was an error */ 
    printf("Error %u '%s'\n", (int)err, dlo_strerror(err)); 
    return False; 

}

/*
 * CopyDataToScreen.
 */

void
CopyDataToScreen(char *buf, int x, int y, int width, int height)
{
    dlo_fbuf_t    fbuf;
    dlo_retcode_t err; 
    dlo_dot_t     dot;
    dlo_bmpflags_t bflags = {0};
    
    // if (appData.rawDelay != 0) {
    //     // XXX Draw a coloured rectangle and then...
    //         usleep(appData.rawDelay * 1000);
    //     }

    // int widthInBytes = width * myFormat.bitsPerPixel / 8;
    // int scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;

    // for (h = 0; h < height; h++) {
    //   memcpy(scr, buf, widthInBytes);
    //   buf += widthInBytes;
    //   scr += scrWidthInBytes;
    // }

    dlo_rect_t r;
    
    dot.x = x;
    dot.y = y;
    fbuf.width = width;
    fbuf.height = height;
    fbuf.base = buf;
    fbuf.stride = width;
    fbuf.fmt = dlo_pixfmt_abgr8888;
    // r.origin = dot;
    // r.width = width;
    // r.height = height;
    // ERR(dlo_fill_rect(dl_uid, NULL, &r, DLO_RGB(random() & 0xff, random() & 0xff, random() & 0xff))); 
    ERR_GOTO(dlo_copy_host_bmp(dl_uid, bflags, &fbuf, NULL, &dot));
    return;
    
    error:
    // Not much we can do here
        printf("dlo_copy_host_bmp error %u '%s'\n", (int)err, dlo_strerror(err));
}


void ReleaseDevice() {
    dlo_final_t   fin_flags = { 0 }; 
    dlo_retcode_t err; 

    if (dl_uid) 
    { 
        /* we claimed a device */ 
        /* Release the device when we're ﬁnished with it */ 
        ERR_GOTO(dlo_release_device(dl_uid)); 
    } 
  /* Finalise libdlo */ 
    ERR_GOTO(dlo_final(fin_flags)); 
    return;

  error: 
      /* The ERR_GOTO() macro jumps here if there was an error */ 
    printf("Error %u '%s'\n", (int)err, dlo_strerror(err)); 
}

