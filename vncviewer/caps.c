/*
 *  Copyright (C) 2003 Constantin Kaplinsky.  All Rights Reserved.
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
 * caps.c
 */

#include "vncviewer.h"

static int CapsIndex(CapsContainer *pcaps, CARD32 code);

/*
 * The constructor.
 */

CapsContainer *
CapsNewContainer(void)
{
  CapsContainer *pcaps;

  pcaps = malloc(sizeof(CapsContainer));
  if (pcaps != NULL)
    pcaps->count = 0;

  return pcaps;
}

/*
 * The destructor.
 */

void
CapsDeleteContainer(CapsContainer *pcaps)
{
  int i;

  for (i = 0; i < pcaps->count; i++) {
    if (pcaps->desc_list[i] != NULL)
      free(pcaps->desc_list[i]);
  }

  free(pcaps);
}

/*
 * Add information about a particular capability into the object. There are
 * two functions to perform this task. These functions overwrite capability
 * records with the same code.
 */

void
CapsAdd(CapsContainer *pcaps,
        CARD32 code, char *vendor, char *name, char *desc)
{
  /* Fill in an rfbCapabilityInfo structure and pass it to CapsAddInfo(). */
  rfbCapabilityInfo capinfo;
  capinfo.code = code;
  memcpy(capinfo.vendorSignature, vendor, sz_rfbCapabilityInfoVendor);
  memcpy(capinfo.nameSignature, name, sz_rfbCapabilityInfoName);
  CapsAddInfo(pcaps, &capinfo, desc);
}

void
CapsAddInfo(CapsContainer *pcaps,
            rfbCapabilityInfo *capinfo, char *desc)
{
  int i;
  char *desc_copy;

  i = CapsIndex(pcaps, capinfo->code);
  if (i == -1) {
    if (pcaps->count >= TIGHTVNC_MAX_CAPS) {
      return;                   /* container full */
    }
    i = pcaps->count++;
    pcaps->known_list[i] = capinfo->code;
    pcaps->desc_list[i] = NULL;
  }

  pcaps->info_list[i] = *capinfo;
  pcaps->enabled_list[i] = (char)False;
  if (pcaps->desc_list[i] != NULL) {
    free(pcaps->desc_list[i]);
  }

  desc_copy = NULL;
  if (desc != NULL) {
    desc_copy = strdup(desc);
  }
  pcaps->desc_list[i] = desc_copy;
}

/*
 * Check if a capability with the specified code was added earlier.
 */

static int
CapsIndex(CapsContainer *pcaps, CARD32 code)
{
  int i;

  for (i = 0; i < pcaps->count; i++) {
    if (pcaps->known_list[i] == code)
      return i;
  }

  return -1;
}

Bool
CapsIsKnown(CapsContainer *pcaps, CARD32 code)
{
  return (CapsIndex(pcaps, code) != -1);
}

/*
 * Fill in a rfbCapabilityInfo structure with contents corresponding to the
 * specified code. Returns True on success, False if the specified code is
 * not known.
 */

Bool
CapsGetInfo(CapsContainer *pcaps, CARD32 code, rfbCapabilityInfo *capinfo)
{
  int i;

  i = CapsIndex(pcaps, code);
  if (i != -1) {
    *capinfo = pcaps->info_list[i];
    return True;
  }

  return False;
}

/*
 * Get a description string for the specified capability code. Returns NULL
 * either if the code is not known, or if there is no description for this
 * capability.
 */

char *
CapsGetDescription(CapsContainer *pcaps, CARD32 code)
{
  int i;

  i = CapsIndex(pcaps, code);
  if (i != -1) {
    return pcaps->desc_list[i];
  }

  return NULL;
}

/*
 * Mark the specified capability as "enabled". This function checks "vendor"
 * and "name" signatures in the existing record and in the argument structure
 * and enables the capability only if both records are the same.
 */

Bool
CapsEnable(CapsContainer *pcaps, rfbCapabilityInfo *capinfo)
{
  int i;
  rfbCapabilityInfo *known;

  i = CapsIndex(pcaps, capinfo->code);
  if (i == -1)
    return False;

  known = &pcaps->info_list[i];
  if ( memcmp(known->vendorSignature, capinfo->vendorSignature,
              sz_rfbCapabilityInfoVendor) != 0 ||
       memcmp(known->nameSignature, capinfo->nameSignature,
              sz_rfbCapabilityInfoName) != 0 ) {
    pcaps->enabled_list[i] = (char)False;
    return False;
  }

  pcaps->enabled_list[i] = (char)True;
  return True;
}

/*
 * Check if the specified capability is known and enabled.
 */

Bool
CapsIsEnabled(CapsContainer *pcaps, CARD32 code)
{
  int i;

  i = CapsIndex(pcaps, code);
  if (i != -1) {
    return (pcaps->enabled_list[i] != (char)False);
  }

  return False;
}

