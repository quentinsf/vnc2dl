/*
 * auth.c - deal with authentication.
 *
 * This file implements authentication when setting up an RFB connection.
 */

/*
 *  Copyright (C) 2003 Constantin Kaplinsky.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include "windowstr.h"
#include "rfb.h"


char *rfbAuthPasswdFile = NULL;

static void rfbAuthSendCaps(rfbClientPtr cl, Bool authRequired);
static void rfbAuthSendChallenge(rfbClientPtr cl);


/*
 * rfbAuthNewClient is called when we reach the point of authenticating
 * a new client.  If authentication isn't being used then we simply send
 * rfbNoAuth.  Otherwise we send rfbVncAuth plus the challenge.
 *
 * NOTE: In the protocol version 3.130, the things are a little bit more
 * complicated.
 */

void
rfbAuthNewClient(cl)
    rfbClientPtr cl;
{
    Bool authRequired;
    CARD32 scheme;

    if (rfbAuthPasswdFile && !cl->reverseConnection) {
	if (rfbAuthIsBlocked()) {
	    rfbLog("Too many authentication failures - client rejected\n");
	    rfbClientConnFailed(cl, "Too many authentication failures");
	    return;
	}
	authRequired = TRUE;
	scheme = Swap32IfLE(rfbVncAuth);
    } else {
	authRequired = FALSE;
	scheme = Swap32IfLE(rfbNoAuth);
    }

    if (cl->protocol_minor_ver >= 130) {
	rfbAuthSendCaps(cl, authRequired);
    } else {
	if (WriteExact(cl->sock, (char *)&scheme, sizeof(scheme)) < 0) {
	    rfbLogPerror("rfbAuthNewClient: write");
	    rfbCloseSock(cl->sock);
	    return;
	}
	if (authRequired) {
	    rfbAuthSendChallenge(cl);
	} else {
	    /* Dispatch client input to rfbProcessClientInitMessage. */
	    cl->state = RFB_INITIALISATION;
	}
    }
}


/*
 * In rfbAuthSendCaps, we send the list of our authentication capabilities
 * to the client (protocol 3.130).
 */

static void
rfbAuthSendCaps(cl, authRequired)
    rfbClientPtr cl;
    Bool authRequired;
{
    rfbAuthenticationCapsMsg caps;
    rfbCapabilityInfo cap;

    caps.connFailed = FALSE;
    caps.nAuthTypes = Swap16IfLE(!!authRequired);
    if (WriteExact(cl->sock, (char *)&caps, sz_rfbAuthenticationCapsMsg) < 0) {
	rfbLogPerror("rfbAuthSendCaps: write");
	rfbCloseSock(cl->sock);
	return;
    }

    if (authRequired) {
	/* Inform the client that we support standard VNC authentication. */
	SetCapInfo(&cap, rfbVncAuth, rfbStandardVendor);
	if (WriteExact(cl->sock, (char *)&cap, sz_rfbCapabilityInfo) < 0) {
	    rfbLogPerror("rfbAuthSendCaps: write");
	    rfbCloseSock(cl->sock);
	    return;
	}
	/* Dispatch client input to rfbProcessClientAuthType. */
	cl->state = RFB_AUTH_TYPE;
    } else {
	/* Dispatch client input to rfbProcessClientInitMessage. */
	cl->state = RFB_INITIALISATION;
    }
}


/*
 * Read client's preferred authentication type (protocol 3.130).
 */

void
rfbAuthProcessType(cl)
    rfbClientPtr cl;
{
    CARD32 auth_type;
    int n;

    n = ReadExact(cl->sock, (char *)&auth_type, sizeof(auth_type));
    if (n <= 0) {
	if (n == 0)
	    rfbLog("rfbAuthProcessType: client gone\n");
	else
	    rfbLogPerror("rfbAuthProcessType: read");
	rfbCloseSock(cl->sock);
	return;
    }

    auth_type = Swap32IfLE(auth_type);

    /* Currently, we support only the standard VNC authentication. */
    if (auth_type != rfbVncAuth) {
	rfbLog("rfbAuthProcessType: unknown authentication scheme\n");
	rfbCloseSock(cl->sock);
	return;
    }

    rfbAuthSendChallenge(cl);
}


/*
 * Send the authentication challenge.
 */

static void
rfbAuthSendChallenge(cl)
    rfbClientPtr cl;
{
    vncRandomBytes(cl->authChallenge);
    if (WriteExact(cl->sock, (char *)cl->authChallenge, CHALLENGESIZE) < 0) {
	rfbLogPerror("rfbAuthSendChallenge: write");
	rfbCloseSock(cl->sock);
	return;
    }

    /* Dispatch client input to rfbAuthProcessResponse. */
    cl->state = RFB_AUTHENTICATION;
}


/*
 * rfbAuthProcessResponse is called when the client sends its
 * authentication response.
 */

void
rfbAuthProcessResponse(cl)
    rfbClientPtr cl;
{
    char passwdFullControl[9];
    char passwdViewOnly[9];
    int numPasswords;
    Bool ok;
    int n;
    CARD8 encryptedChallenge1[CHALLENGESIZE];
    CARD8 encryptedChallenge2[CHALLENGESIZE];
    CARD8 response[CHALLENGESIZE];
    CARD32 authResult;

    n = ReadExact(cl->sock, (char *)response, CHALLENGESIZE);
    if (n <= 0) {
	if (n != 0)
	    rfbLogPerror("rfbAuthProcessResponse: read");
	rfbCloseSock(cl->sock);
	return;
    }

    numPasswords = vncDecryptPasswdFromFile2(rfbAuthPasswdFile,
					     passwdFullControl,
					     passwdViewOnly);
    if (numPasswords == 0) {
	rfbLog("rfbAuthProcessResponse: could not get password from %s\n",
	       rfbAuthPasswdFile);

	authResult = Swap32IfLE(rfbVncAuthFailed);

	if (WriteExact(cl->sock, (char *)&authResult, 4) < 0) {
	    rfbLogPerror("rfbAuthProcessResponse: write");
	}
	rfbCloseSock(cl->sock);
	return;
    }

    memcpy(encryptedChallenge1, cl->authChallenge, CHALLENGESIZE);
    vncEncryptBytes(encryptedChallenge1, passwdFullControl);
    memcpy(encryptedChallenge2, cl->authChallenge, CHALLENGESIZE);
    vncEncryptBytes(encryptedChallenge2,
		    (numPasswords == 2) ? passwdViewOnly : passwdFullControl);

    /* Lose the passwords from memory */
    memset(passwdFullControl, 0, 9);
    memset(passwdViewOnly, 0, 9);

    ok = FALSE;
    if (memcmp(encryptedChallenge1, response, CHALLENGESIZE) == 0) {
	rfbLog("Full-control authentication passed by %s\n", cl->host);
	ok = TRUE;
	cl->viewOnly = FALSE;
    } else if (memcmp(encryptedChallenge2, response, CHALLENGESIZE) == 0) {
	rfbLog("View-only authentication passed by %s\n", cl->host);
	ok = TRUE;
	cl->viewOnly = TRUE;
    }

    if (!ok) {
	rfbLog("rfbAuthProcessResponse: authentication failed from %s\n",
	       cl->host);

	if (rfbAuthConsiderBlocking()) {
	    authResult = Swap32IfLE(rfbVncAuthTooMany);
	} else {
	    authResult = Swap32IfLE(rfbVncAuthFailed);
	}

	if (WriteExact(cl->sock, (char *)&authResult, 4) < 0) {
	    rfbLogPerror("rfbAuthProcessResponse: write");
	}
	rfbCloseSock(cl->sock);
	return;
    }

    rfbAuthUnblock();

    authResult = Swap32IfLE(rfbVncAuthOK);

    if (WriteExact(cl->sock, (char *)&authResult, 4) < 0) {
	rfbLogPerror("rfbAuthProcessResponse: write");
	rfbCloseSock(cl->sock);
	return;
    }

    /* Dispatch client input to rfbProcessClientInitMessage(). */
    cl->state = RFB_INITIALISATION;
}


/*
 * Functions to prevent too many successive authentication failures.
 * FIXME: This should be performed separately per each client IP.
 */

/* Maximum authentication failures before blocking connections */
#define MAX_AUTH_TRIES 5

/* Delay in ms, doubles for each failure over MAX_AUTH_TRIES */
#define AUTH_TOO_MANY_BASE_DELAY 10 * 1000

static int rfbAuthTries = 0;
static Bool rfbAuthTooManyTries = FALSE;
static OsTimerPtr timer = NULL;

/*
 * This function should not be called directly, it is called by
 * setting a timer in rfbAuthConsiderBlocking().
 */

static CARD32
rfbAuthReenable(OsTimerPtr timer, CARD32 now, pointer arg)
{
    rfbAuthTooManyTries = FALSE;
    return 0;
}

/*
 * This function should be called after each authentication failure.
 * The return value will be true if there was too many failures.
 */

Bool
rfbAuthConsiderBlocking(void)
{
    int i;

    rfbAuthTries++;

    if (rfbAuthTries >= MAX_AUTH_TRIES) {
	CARD32 delay = AUTH_TOO_MANY_BASE_DELAY;
	for (i = MAX_AUTH_TRIES; i < rfbAuthTries; i++)
	    delay *= 2;
	timer = TimerSet(timer, 0, delay, rfbAuthReenable, NULL);
	rfbAuthTooManyTries = TRUE;
	return TRUE;
    }

    return FALSE;
}

/*
 * This function should be called after successful authentication.
 * It resets the counter of authentication failures. Note that it's
 * not necessary to clear the rfbAuthTooManyTries flag as it will be
 * reset by the timer function.
 */

void
rfbAuthUnblock(void)
{
    rfbAuthTries = 0;
}

/*
 * This function should be called before authentication process.
 * The return value will be true if there was too many authentication
 * failures, and the server should not allow another try.
 */

Bool
rfbAuthIsBlocked(void)
{
    return rfbAuthTooManyTries;
}

