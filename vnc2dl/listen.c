/*
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
 * listen.c - listen for incoming connections
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <vnc2dl.h>

/*
 * listenForIncomingConnections() - listen for incoming connections from
 * servers, and fork a new process to deal with each connection. 
 */

void
listenForIncomingConnections()
{
  int listenSocket, sock;
  fd_set fds;
  int n;
  int i;
  char *displayname = NULL;
  char *display;
  char *colonPos;
  struct utsname hostinfo;
  uname(&hostinfo);

  listenSocket = ListenAtTcpPort(appData.listenPort);

  if (listenSocket < 0) exit(1);

  fprintf(stderr,"%s Listening on port %d\n",
	  programName, appData.listenPort);
  fprintf(stderr,"%s -listen: Command line errors are not reported until "
	  "a connection comes in.\n", programName);

  while (True) {

    /* reap any zombies */
    int status, pid;
    while ((pid= wait3(&status, WNOHANG, (struct rusage *)0))>0);

    FD_ZERO(&fds); 

    FD_SET(listenSocket, &fds);

    select(FD_SETSIZE, &fds, NULL, NULL, NULL);

    if (FD_ISSET(listenSocket, &fds)) {
      rfbsock = AcceptTcpConnection(listenSocket);
      if (rfbsock < 0) exit(1);
      if (!SetNonBlocking(rfbsock)) exit(1);

      /* Now fork off a new process to deal with it... */

      switch (fork()) {

          case -1: 
          perror("fork"); 
          exit(1);

          case 0:
          /* child - return to caller */
          close(listenSocket);
          return;

          default:
          /* parent - go round and listen again */
          close(rfbsock); 
          break;
      }
  }
  }
}

