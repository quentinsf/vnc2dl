/*
 *  Some portions (c) 2009 Q.Stafford-Fraser. All Rights Reserved.
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
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
 * vnc2dl.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include "rfbproto.h"
#include "caps.h"

extern int endianTest;

#define Swap16IfLE(s) \
    (*(char *)&endianTest ? ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)) : (s))

#define Swap32IfLE(l) \
    (*(char *)&endianTest ? ((((l) & 0xff000000) >> 24) | \
			     (((l) & 0x00ff0000) >> 8)  | \
			     (((l) & 0x0000ff00) << 8)  | \
			     (((l) & 0x000000ff) << 24))  : (l))

#define MAX_ENCODINGS 20

#define LISTEN_PORT_OFFSET 5500
#define TUNNEL_PORT_OFFSET 5500
#define SERVER_PORT_OFFSET 5900

#define DEFAULT_SSH_CMD "/usr/bin/ssh"
#define DEFAULT_TUNNEL_CMD  \
  (DEFAULT_SSH_CMD " -f -L %L:localhost:%R %H sleep 20")
#define DEFAULT_VIA_CMD     \
  (DEFAULT_SSH_CMD " -f -L %L:%H:%R %G sleep 20")


/* args.c */

typedef struct {
  Bool shareDesktop;
  Bool viewOnly;

  String encodingsString;

  Bool useBGR233;
  int nColours;
  Bool useSharedColours;
  Bool forceOwnCmap;
  Bool forceTrueColour;
  int requestedDepth;

  char *userLogin;

  char *passwordFile;

  int rawDelay;
  int copyRectDelay;

  Bool debug;

  int bumpScrollTime;
  int bumpScrollPixels;

  int compressLevel;
  int qualityLevel;
  Bool enableJPEG;
  Bool useRemoteCursor;
  Bool autoPass;

} AppData;

extern AppData appData;

extern char *fallback_resources[];
extern char vncServerHost[];
extern int vncServerPort;
extern Bool listenSpecified;
extern int listenPort;

extern char* cmdLineOptions[];
extern int numCmdLineOptions;

extern void removeArgs(int *argc, char** argv, int idx, int nargs);
extern void usage(void);
extern void GetArgsAndResources(int argc, char **argv);


/* cursor.c */

extern Bool HandleCursorShape(int xhot, int yhot, int width, int height,
                              CARD32 enc);
extern void SoftCursorLockArea(int x, int y, int w, int h);
extern void SoftCursorUnlockScreen(void);
extern void SoftCursorMove(int x, int y);

/* listen.c */

extern void listenForIncomingConnections();

/* rfbproto.c */

extern int rfbsock;
extern Bool canUseCoRRE;
extern Bool canUseHextile;
extern char *desktopName;
extern rfbPixelFormat myFormat;
extern rfbServerInitMsg si;
extern char *serverCutText;
extern Bool newServerCutText;

extern Bool ConnectToRFBServer(const char *hostname, int port);
extern Bool InitialiseRFBConnection();
extern Bool SetFormatAndEncodings();
extern Bool SendIncrementalFramebufferUpdateRequest();
extern Bool SendFramebufferUpdateRequest(int x, int y, int w, int h,
					 Bool incremental);
extern Bool SendPointerEvent(int x, int y, int buttonMask);
extern Bool SendKeyEvent(CARD32 key, Bool down);
extern Bool SendClientCutText(char *str, int len);
extern Bool HandleRFBServerMessage();

extern void PrintPixelFormat(rfbPixelFormat *format);


/* sockets.c */

extern Bool errorMessageOnReadFailure;

extern Bool ReadFromRFBServer(char *out, unsigned int n);
extern Bool WriteExact(int sock, char *buf, int n);
extern int FindFreeTcpPort(void);
extern int ListenAtTcpPort(int port);
extern int ConnectToTcpAddr(unsigned int host, int port);
extern int AcceptTcpConnection(int listenSock);
extern Bool SetNonBlocking(int sock);

extern int StringToIPAddr(const char *str, unsigned int *addr);
extern Bool SameMachine(int sock);

/* tunnel.c */

extern Bool tunnelSpecified;

extern Bool createTunnel(int *argc, char **argv, int tunnelArgIndex);

/* vnc2dl.c */

extern char *programName;

