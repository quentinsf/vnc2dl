/*
 *  Copyright (c) 2009 Q.Stafford-Fraser, Camvine. All Rights Reserved.
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
#include <getopt.h>

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

AppData appData = {
   0,       // Bool shareDesktop;
   0,       // Bool viewOnly;
   0,       // Bool listen;
   LISTEN_PORT_OFFSET, // Bool listenPort;
   NULL,   // char encodingsString[256];
   0,       // Bool useBGR233;
   24,       // int nColours;
   0,       // Bool useSharedColours;
   0,       // Bool forceOwnCmap;
   1,       // Bool forceTrueColour;
   24,       // int requestedDepth;
   0,       // char *userLogin;
   0,       // char passwordFile[256];
   0,       // int rawDelay;
   0,       // int copyRectDelay;
   0,       // Bool debug;
   0,       // int compressLevel;
   0,       // int qualityLevel;
   0,       // Bool enableJPEG;
   0,       // Bool autoPass;
};


/*
 * The command-line options.
 */

static struct option long_options[] = {
  {"shared",       no_argument,          &appData.shareDesktop,   1},
  {"noshared",     no_argument,          &appData.shareDesktop,   0},
  {"viewonly",     no_argument,          &appData.viewOnly,       1},
  {"passwd",       required_argument,    NULL,                    'p'},
  {"encodings",    required_argument,    NULL,                    'e'},
  {"bgr233",       no_argument,          &appData.useBGR233,      1},
  {"depth",        required_argument,    NULL,                    'd'},
  {"compresslevel",required_argument,    &appData.compressLevel,  'c'},
  {"quality",      required_argument,    &appData.qualityLevel,   'q'},
  {"nojpeg",       no_argument,          &appData.enableJPEG,     0},
  {"autopass",     no_argument,          &appData.autoPass,       1},
  {"listen",       no_argument,          &appData.listen,         1},
  {"listenPort",   required_argument,    NULL,                    'L'},
  {0,              0,                      0,                     0}
};


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
	  "       %s [<OPTIONS>] -listen [-listenPort <PORT#>]\n"
	  "       %s -help\n"
	  "\n"
	  "<OPTIONS> include:\n"
	  "        -via <GATEWAY>\n"
	  "        -shared\n"
	  "        -noshared\n"
	  "        -passwd <PASSWD-FILENAME> (standard VNC authentication)\n"
	  "        -encodings <ENCODING-LIST> (e.g. \"tight copyrect\")\n"
	  "        -depth <DEPTH>\n"
	  "        -compresslevel <COMPRESS-VALUE> (0..9: 0-fast, 9-best)\n"
	  "        -quality <JPEG-QUALITY-VALUE> (0..9: 0-low, 9-high)\n"
	  "        -nojpeg\n"
	  "        -autopass\n"
	  "        -listen\n"
  	  "        -listenPort <PORTNUM>\n"
	  "\n"
	  "See the manual page for more information."
	  "\n", programName, programName, programName, programName);
  exit(1);
}


/*
 * ProcessArgs() deals with any command-line arguments.  It sets vncServerHost and
 * vncServerPort and all the fields in appData.
 */

void
ProcessArgs(int argc, char **argv)
{
  int i;
  char *vncServerName, *colonPos;
  int len, portOffset;
  int disp;
  int option_index = 0;

  while (1) {
      int c = getopt_long_only(argc, argv, "p:e:d:c:q:L:", long_options, &option_index);
   
      if (c == -1)  break; /* end of options */
      
      switch (c) {
          case 0:
          /* If this option set a flag, do nothing else now. */
          if (long_options[option_index].flag != 0)
              break;
          printf ("option %s", long_options[option_index].name);
          if (optarg)
              printf (" with arg %s", optarg);
          printf ("\n");
          break;

          case 'p':
          appData.passwordFile = strdup(optarg);
          printf ("Password file set to `%s'\n", optarg);
          break;
          
          case 'e':
          appData.encodingsString = strdup(optarg);
          printf ("Encodings set to `%s'\n", optarg);
          break;

          case 'd':
          appData.requestedDepth = atoi(optarg);
          printf ("Depth set to %d\n", appData.requestedDepth);
          break;

          case 'c':
          appData.compressLevel = atoi(optarg);
          printf ("Compression level set to `%s'\n", optarg);
          break;

          case 'q':
          appData.qualityLevel = atoi(optarg);
          printf ("Quality level set to `%s'\n", optarg);
          break;
          
          case 'L':
          appData.listenPort = atoi(optarg);
          printf ("Listening port set to %d\n", appData.listenPort);
          break;
          
          default:
          usage();
          break;
      }
  }
  
  if (appData.listen) {
      vncServerName = "Incoming";
      if (argc != optind) {
          usage();
      }
  } else {
      if (argc == 1) {
        usage();
      } else if (argc != optind+1) {
        usage();
      } else {
        vncServerName = argv[optind];

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
}
