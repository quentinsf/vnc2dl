/*
 *  Copyright (C) 2002 Constantin Kaplinsky.  All Rights Reserved.
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
#include <errno.h>
#include "vncauth.h"

static void usage(char *argv[]);
static char *getenv_safe(char *name, size_t maxlen);
static void mkdir_and_check(char *dirname, int be_strict);

int main(int argc, char *argv[]) {
  int make_directory = 0;
  int check_strictly = 0;
  char *passwd;
  char *passwd1;
  char passwdDir[256];
  char passwdFile[256];
  int i;

  if (argc == 1) {

    sprintf(passwdDir, "%s/.vnc", getenv_safe("HOME", 240));
    sprintf(passwdFile, "%s/passwd", passwdDir);
    make_directory = 1;
    check_strictly = 0;

  } else if (argc == 2) {

    if (strcmp(argv[1], "-t") == 0) {
      sprintf(passwdDir, "/tmp/%s-vnc", getenv_safe("USER", 32));
      sprintf(passwdFile, "%s/passwd", passwdDir);
      make_directory = 1;
      check_strictly = 1;
    } else {
      strcpy(passwdFile, argv[1]);
      make_directory = 0;
      check_strictly = 0;
    }

  } else {
    usage(argv);
  }

  if (make_directory)
    fprintf(stderr, "Using password file %s\n", passwdFile);

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
      if (make_directory) {
        mkdir_and_check(passwdDir, check_strictly);
      }
      if (vncEncryptAndStorePasswd(passwd, passwdFile) != 0) {
	fprintf(stderr,"Cannot write password file %s\n",passwdFile);
	exit(1);
      }
      for (i = 0; i < strlen(passwd); i++) {
	passwd[i] = passwd1[i] = '\0';
      }
      return 0;
    }

    fprintf(stderr,"Passwords do not match. Please try again.\n\n");
  }

  return 2;                     /* never executed */
}

static void usage(char *argv[]) {
  fprintf(stderr,
          "Usage: %s [FILE]\n"
          "       %s -t\n",
          argv[0], argv[0], argv[0]);
  exit(1);
}

static char *getenv_safe(char *name, size_t maxlen) {
  char *result;

  result = getenv(name);
  if (result == NULL) {
    fprintf(stderr, "Error: no %s environment variable\n", name);
    exit(1);
  }
  if (strlen(result) > maxlen) {
    fprintf(stderr, "Error: %s environment variable string too long\n", name);
    exit(1);
  }
  return result;
}

/*
 * Check if the specified vnc directory exists, create it if
 * necessary, and perform a number of sanity checks.
 */

static void mkdir_and_check(char *dirname, int be_strict) {
  struct stat stbuf;

  if (lstat(dirname, &stbuf) != 0) {
    if (errno != ENOENT) {
      fprintf(stderr, "lstat() failed for %s: %s\n", dirname, strerror(errno));
      exit(1);
    }
    fprintf(stderr, "VNC directory %s does not exist, creating.\n", dirname);
    if (mkdir(dirname, S_IRWXU) == -1) {
      fprintf(stderr, "Error creating directory %s: %s\n",
              dirname, strerror(errno));
      exit(1);
    }
  }

  if (lstat(dirname, &stbuf) != 0) {
    fprintf(stderr, "Error in lstat() for %s: %s\n", dirname, strerror(errno));
    exit(1);
  }
  if (!S_ISDIR(stbuf.st_mode)) {
    fprintf(stderr, "Error: %s is not a directory\n", dirname);
    exit(1);
  }
  if (stbuf.st_uid != getuid()) {
    fprintf(stderr, "Error: bad ownership on %s\n", dirname);
    exit(1);
  }
  if (be_strict && ((S_IRWXG|S_IRWXO) & stbuf.st_mode)){
    fprintf(stderr, "Error: bad access modes on %s\n", dirname);
    exit(1);
  }
}
