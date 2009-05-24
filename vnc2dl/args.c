/*
 *  Copyright (C) 2002-2006 Constantin Kaplinsky.  All Rights Reserved.
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
 * argsresources.c - deal with command-line args and resources.
 */

#include "vnc2dl.h"


/*
 * vncServerHost and vncServerPort are set either from the command line or
 * from a dialog box.
 */

char vncServerHost[256];
int vncServerPort = 0;


/*
 * appData is our application-specific data which can be set by the user with
 * application resource specs.  The AppData structure is defined in the header
 * file.
 */

AppData appData;

/*
static appDataResourceList[] = {
  {"shareDesktop",  bool, True},

  {"viewOnly", bool, False},

  {"fullScreen", bool, False},

  {"passwordFile", string,  (XtPointer) 0},

  {"passwordDialog",  bool,  False},

  {"encodings", string, null},

  {"useBGR233",bool, False},

  {"nColours",int, 256},

  {"useSharedColours",bool, True},

  {"forceOwnCmap",  bool, False},

  {"forceTrueColour", bool, False},

  {"debug", bool, False},

  {"rawDelay",  int, 0},

  {"copyRectDelay", int, 0},

  {"bumpScrollTime", int, 25},

  {"bumpScrollPixels", int, 20},

  {"compressLevel", int,  -1},

  {"qualityLevel", int, 6},

  {"enableJPEG", bool, True},

  {"useRemoteCursor", bool, True},

  {"autoPass", bool, False}
};

*/

/*
 * The cmdLineOptions array specifies how certain app resource specs can be set
 * with command-line options.
 */

/* char * cmdLineOptions[] = {
  {"-shared",        "*shareDesktop",       XrmoptionNoArg,  "True"},
  {"-noshared",      "*shareDesktop",       XrmoptionNoArg,  "False"},
  {"-viewonly",      "*viewOnly",           XrmoptionNoArg,  "True"},
  {"-fullscreen",    "*fullScreen",         XrmoptionNoArg,  "True"},
  {"-noraiseonbeep", "*raiseOnBeep",        XrmoptionNoArg,  "False"},
  {"-passwd",        "*passwordFile",       XrmoptionSepArg, 0},
  {"-encodings",     "*encodings",          XrmoptionSepArg, 0},
  {"-bgr233",        "*useBGR233",          XrmoptionNoArg,  "True"},
  {"-owncmap",       "*forceOwnCmap",       XrmoptionNoArg,  "True"},
  {"-truecolor",     "*forceTrueColour",    XrmoptionNoArg,  "True"},
  {"-truecolour",    "*forceTrueColour",    XrmoptionNoArg,  "True"},
  {"-depth",         "*requestedDepth",     XrmoptionSepArg, 0},
  {"-compresslevel", "*compressLevel",      XrmoptionSepArg, 0},
  {"-quality",       "*qualityLevel",       XrmoptionSepArg, 0},
  {"-nojpeg",        "*enableJPEG",         XrmoptionNoArg,  "False"},
  {"-nocursorshape", "*useRemoteCursor",    XrmoptionNoArg,  "False"},
  {"-x11cursor",     "*useX11Cursor",       XrmoptionNoArg,  "True"},
  {"-autopass",      "*autoPass",           XrmoptionNoArg,  "True"}

};
/*

/*
 * removeArgs() is used to remove some of command line arguments.
 */

void
removeArgs(int *argc, char** argv, int idx, int nargs)
{
  int i;
  if ((idx+nargs) > *argc) return;
  for (i = idx+nargs; i < *argc; i++) {
    argv[i-nargs] = argv[i];
  }
  *argc -= nargs;
}

/*
 * usage() prints out the usage message.
 */

void
usage(void)
{
  fprintf(stderr,
	  "vnc2dl\n"
	  "\n"
	  "Usage: %s [<OPTIONS>] [<HOST>][:<DISPLAY#>]\n"
	  "       %s [<OPTIONS>] [<HOST>][::<PORT#>]\n"
	  "       %s [<OPTIONS>] -listen [<DISPLAY#>]\n"
	  "       %s -help\n"
	  "\n"
	  "<OPTIONS> include:\n"
	  "        -via <GATEWAY>\n"
	  "        -shared (set by default)\n"
	  "        -noshared\n"
	  "        -passwd <PASSWD-FILENAME> (standard VNC authentication)\n"
	  "        -encodings <ENCODING-LIST> (e.g. \"tight copyrect\")\n"
	  "        -depth <DEPTH>\n"
	  "        -compresslevel <COMPRESS-VALUE> (0..9: 0-fast, 9-best)\n"
	  "        -quality <JPEG-QUALITY-VALUE> (0..9: 0-low, 9-high)\n"
	  "        -nojpeg\n"
	  "        -nocursorshape\n"
	  "        -autopass\n"
	  "\n"
	  "See the manual page for more information."
	  "\n", programName, programName, programName, programName);
  exit(1);
}


/*
 * GetArgsAndResources() deals with resources and any command-line arguments
 * not already processed by XtVaAppInitialize().  It sets vncServerHost and
 * vncServerPort and all the fields in appData.
 */

void
GetArgsAndResources(int argc, char **argv)
{
  int i;
  char *vncServerName, *colonPos;
  int len, portOffset;
  int disp;

 /* Check any remaining command-line arguments.  If -listen was specified
     there should be none.  Otherwise the only argument should be the VNC
     server name.  If not given then pop up a dialog box and wait for the
     server name to be entered. */

  if (listenSpecified) {
    if (argc != 1) {
      fprintf(stderr,"\n%s -listen: invalid command line argument: %s\n",
	      programName, argv[1]);
      usage();
    }
    return;
  }

  if (argc == 1) {
    usage();
  } else if (argc != 2) {
    usage();
  } else {
    vncServerName = argv[1];

    if (vncServerName[0] == '-')
      usage();
  }

  if (strlen(vncServerName) > 255) {
    fprintf(stderr,"VNC server name too long\n");
    exit(1);
  }

  colonPos = strchr(vncServerName, ':');
  if (colonPos == NULL) {
    /* No colon -- use default port number */
    strcpy(vncServerHost, vncServerName);
    vncServerPort = SERVER_PORT_OFFSET;
  } else {
    memcpy(vncServerHost, vncServerName, colonPos - vncServerName);
    vncServerHost[colonPos - vncServerName] = '\0';
    len = strlen(colonPos + 1);
    portOffset = SERVER_PORT_OFFSET;
    if (colonPos[1] == ':') {
      /* Two colons -- interpret as a port number */
      colonPos++;
      len--;
      portOffset = 0;
    }
    if (!len || strspn(colonPos + 1, "0123456789") != len) {
      usage();
    }
    disp = atoi(colonPos + 1);
    if (portOffset != 0 && disp >= 100)
      portOffset = 0;
    vncServerPort = disp + portOffset;
  }
}
