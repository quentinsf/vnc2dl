/*
 *  Some portions (c) 2009 Q.Stafford-Fraser. All Rights Reserved.
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
 * vnc2dl.c - View VNC sessions on DisplayLink devices.
 */

#include "vnc2dl.h"

char *programName;

int
main(int argc, char **argv)
{
  int i;
  programName = argv[0];

  /* Process any command-line arguments (eg. the VNC server name).  */
  ProcessArgs(argc, argv);


  /* Connect to the first DisplayLink device */ 
  if (!InitialiseDevice()) exit(1);
  

  /* The -listen option is used to make us a daemon process which listens for
     incoming connections from servers, rather than actively connecting to a
     given server. For -listen option, when a successful incoming connection has been accepted,
     listenForIncomingConnections() returns, setting the appData.listen
     flag. */

  if (appData.listen) {
    listenForIncomingConnections();
  } else {
    /* Otherwise, we make an outgoing connection */
    if (!ConnectToRFBServer(vncServerHost, vncServerPort)) exit(1);
  }

  /* Initialise the VNC connection, including reading the password */

  if (!InitialiseRFBConnection()) exit(1);

  /* Tell the VNC server which pixel format and encodings we want to use */

  SetFormatAndEncodings();

  
  /* And kick things off */
  SendIncrementalFramebufferUpdateRequest();
  
  /* Now enter the main loop, processing VNC messages. */
  
  while (1) {
    if (!HandleRFBServerMessage())
      break;
  }

  // Cleanup();
  ReleaseDevice();

  return 0;
}
