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
 *  vncpasswd:  A standalone program which gets and verifies a password, 
 *              encrypts it, and stores it to a file.  Always ignore anything
 *              after 8 characters, since this is what Solaris getpass() does
 *              anyway.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "vncauth.h"

static void usage(char *argv[]) {
  fprintf(stderr,"Usage: %s [file]\n",argv[0]);
  exit(1);
}

int main(int argc, char *argv[]) {
  char *home_env;
  char *passwd;
  char *passwd1;
  char passwdDir[256];
  char passwdFile[256];
  int i;

  if (argc == 1) {
    home_env = getenv("HOME");
    if (home_env == NULL) {
      fprintf(stderr,"Error: no HOME environment variable\n");
      exit(1);
    }
    if (strlen(home_env) > 240) {
      fprintf(stderr,"Error: HOME environment variable string too long\n");
      exit(1);
    }
    sprintf(passwdDir, "%s/.vnc", home_env);
    sprintf(passwdFile, "%s/passwd", passwdDir);

  } else if (argc == 2) {
    strcpy(passwdFile,argv[1]);

  } else {
    usage(argv);
  }

  while (1) {  
    passwd = getpass("Password: ");
    if (!passwd) {
      fprintf(stderr,"Can't get password: not a tty?\n");
      exit(1);
    }   
    if (strlen(passwd) < 6) {
      fprintf(stderr,"Password too short\n");
      exit(1);
    }   
    if (strlen(passwd) > 8) {
      passwd[8] = '\0';
    }

    passwd1 = strdup(passwd);

    passwd = getpass("Verify:   ");
    if (strlen(passwd) > 8) {
      passwd[8] = '\0';
    }

    if (strcmp(passwd1, passwd) == 0) {
      if (argc == 1 &&
	  mkdir(passwdDir, (S_IRWXU | S_IRGRP | S_IXGRP |
			    S_IROTH | S_IXOTH)) == -1 &&
	  errno != EEXIST) {
	perror(passwdDir);
	exit(1);
      }
      if (vncEncryptAndStorePasswd(passwd, passwdFile) != 0) {
	fprintf(stderr,"Cannot write password file %s\n",passwdFile);
	exit(1);
      }
      for (i = 0; i < strlen(passwd); i++)
	passwd[i] = passwd1[i] = '\0';
      return;
    }

    fprintf(stderr,"Passwords do not match. Please try again.\n\n");
  }
}
