/*
 *  Copyright (C) 1999 Const Kaplinsky.  All Rights Reserved.
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
 * tunnel.c - tunneling support (e.g. for using standalone SSH installation)
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/wait.h>
//#include <sys/time.h>
//#include <sys/utsname.h>
#include <vncviewer.h>


static char *getCmdPattern (void);
static Bool fillCmdPattern (char *result, char *pattern, char *remoteHost,
                            char *remotePort, char *localPort);
static Bool runCommand (char *cmd);

Bool
createTunnel(int *argc, char **argv, int tunnelArgIndex)
{
  char *pattern;
  char cmd[1024];
  int port;
  char localPortStr[8];
  char remotePortStr[8];

  pattern = getCmdPattern();
  if (!pattern)
    return False;

  port = FindFreeTcpPort();
  if (port == 0)
    return False;

  sprintf (localPortStr, "%hu", (unsigned short)port);
  sprintf (remotePortStr, "%hu", (unsigned short)5901);

  if (!fillCmdPattern(cmd, pattern, "ce.cctpu.edu.ru",
                      remotePortStr, localPortStr))
    return False;

  if (!runCommand(cmd))
    return False;

  return True;
}

static char *
getCmdPattern (void)
{
  struct stat st;
  char *pattern;

  pattern = getenv("VNC_TUNNEL_CMD");
  if (pattern == NULL) {
    if ( stat(DEFAULT_SSH_CMD, &st) != 0 ||
         !(S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) ) {
      fprintf(stderr, "DEBUG: missing %s binary.\n", DEFAULT_SSH_CMD);
      return NULL;
    }
    pattern = DEFAULT_TUNNEL_CMD;
  }

  return pattern;
}

/* Note: in fillCmdPattern() result points to a 1024-byte buffer */

static Bool
fillCmdPattern (char *result, char *pattern, char *remoteHost,
                char *remotePort, char *localPort)
{
  int i, j;
  Bool H_found = False, R_found = False, L_found = False;

  for (i=0, j=0; pattern[i] && j<1023; i++, j++) {
    if (pattern[i] == '%') {
      switch (pattern[++i]) {
      case 'H':
        strncpy(&result[j], remoteHost, 1024 - j);
        j += strlen(remoteHost) - 1;
        H_found = True;
        continue;
      case 'R':
        strncpy(&result[j], remotePort, 1024 - j);
        j += strlen(remotePort) - 1;
        R_found = True;
        continue;
      case 'L':
        strncpy(&result[j], localPort, 1024 - j);
        j += strlen(localPort) - 1;
        L_found = True;
        continue;
      case '\0':
        i--;
        continue;
      }
    }
    result[j] = pattern[i];
  }

  if (pattern[i]) {
    fprintf(stderr, "DEBUG: Tunneling command too long.\n");
    return False;
  }

  if (!H_found || !R_found || !L_found) {
    fprintf(stderr, "DEBUG: %H, %R or %L absent in tunneling command.\n");
    return False;
  }

  result[j] = '\0';

  fprintf(stderr, "DEBUG: Tunneling command is:\n\"%s\"\n");
  return True;
}

static Bool
runCommand (char *cmd)
{
  if (system(cmd) != 0) {
    fprintf(stderr, "DEBUG: Tunneling command failed.\n");
    return False;
  }
}

