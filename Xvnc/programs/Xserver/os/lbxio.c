/***********************************************************
Copyright 1987, 1989 by Digital Equipment Corporation, Maynard, Massachusetts,
and the Massachusetts Institute of Technology, Cambridge, Massachusetts.
Copyright 1996 X Consortium, Inc.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Digital or MIT not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XConsortium: lbxio.c /main/10 1996/12/16 23:03:30 rws $ */




/* $XFree86: xc/programs/Xserver/os/lbxio.c,v 3.8 1997/01/18 07:18:31 dawes Exp $ */

#include <stdio.h>
#include <X11/Xtrans.h>
#ifdef X_NOT_STDC_ENV
extern int errno;
#endif
#include "Xmd.h"
#include <errno.h>
#ifndef Lynx
#include <sys/param.h>
#ifndef __EMX__
#include <sys/uio.h>
#endif
#else
#include <uio.h>
#endif
#include "X.h"
#include "Xproto.h"
#include "os.h"
#include "Xpoll.h"
#include "osdep.h"
#include "opaque.h"
#include "dixstruct.h"
#include "misc.h"
#include "lbxserve.h"

/* check for both EAGAIN and EWOULDBLOCK, because some supposedly POSIX
 * systems are broken and return EWOULDBLOCK when they should return EAGAIN
 */
#if defined(EAGAIN) && defined(EWOULDBLOCK)
#define ETEST(err) (err == EAGAIN || err == EWOULDBLOCK)
#else
#ifdef EAGAIN
#define ETEST(err) (err == EAGAIN)
#else
#define ETEST(err) (err == EWOULDBLOCK)
#endif
#endif

extern fd_set ClientsWithInput, IgnoredClientsWithInput;
extern fd_set AllClients, AllSockets;
extern fd_set ClientsWriteBlocked;
extern fd_set OutputPending;
extern int ConnectionTranslation[];
extern Bool NewOutputPending;
extern Bool AnyClientsWriteBlocked;
extern Bool CriticalOutputPending;
extern int timesThisConnection;
extern ConnectionInputPtr FreeInputs;
extern ConnectionOutputPtr FreeOutputs;
extern OsCommPtr AvailableInput;

#define get_req_len(req,cli) ((cli)->swapped ? \
			      lswaps((req)->length) : (req)->length)

#define YieldControl()				\
        { isItTimeToYield = TRUE;		\
	  timesThisConnection = 0; }
#define YieldControlNoInput()			\
        { YieldControl();			\
	  FD_CLR(fd, &ClientsWithInput); }

void
SwitchClientInput (client, pending)
    ClientPtr client;
    Bool pending;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    
    ConnectionTranslation[oc->fd] = client->index;
    if (pending)
	FD_SET(oc->fd, &ClientsWithInput);
    else
	YieldControl();
}

void
LbxPrimeInput(client, proxy)
    ClientPtr	client;
    LbxProxyPtr proxy;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    ConnectionInputPtr oci = oc->input;

    if (oci && proxy->compHandle) {
	char *extra = oci->bufptr + oci->lenLastReq;
	int left = oci->bufcnt + oci->buffer - extra;

	(*proxy->streamOpts.streamCompStuffInput)(oc->fd,
						  (unsigned char *)extra,
						  left);
	oci->bufcnt -= left;
	AvailableInput = oc;
    }
}

void
AvailableClientInput (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;

    if (FD_ISSET(oc->fd, &AllSockets))
	FD_SET(oc->fd, &ClientsWithInput);
}

/*****************************************************************
 * AppendFakeRequest
 *    Append a (possibly partial) request in as the last request.
 *
 **********************/
 
Bool
AppendFakeRequest (client, data, count)
    ClientPtr client;
    char *data;
    int count;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    int fd = oc->fd;
    register int gotnow;

    if (!oci)
    {
	if (oci = FreeInputs)
	    FreeInputs = oci->next;
	else if (!(oci = AllocateInputBuffer()))
	    return FALSE;
	oc->input = oci;
    } else if (AvailableInput == oc)
	AvailableInput = (OsCommPtr)NULL;
    /* do not free AvailableInput here, it could be proxy's */
    oci->bufptr += oci->lenLastReq;
    oci->lenLastReq = 0;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    if ((gotnow + count) > oci->size)
    {
	char *ibuf;

	ibuf = (char *)xrealloc(oci->buffer, gotnow + count);
	if (!ibuf)
	    return(FALSE);
	oci->size = gotnow + count;
	oci->buffer = ibuf;
	oci->bufptr = ibuf + oci->bufcnt - gotnow;
    }
    if (oci->bufcnt + count > oci->size) {
	memmove(oci->buffer, oci->bufptr, gotnow);
	oci->bufcnt = gotnow;
	oci->bufptr = oci->buffer;
    }
    memmove(oci->bufptr + gotnow, data, count);
    oci->bufcnt += count;
    gotnow += count;
    if ((gotnow >= sizeof(xReq)) &&
	(gotnow >= (int)(get_req_len((xReq *)oci->bufptr, client) << 2)))
	FD_SET(fd, &ClientsWithInput);
    else
	YieldControlNoInput();
    return(TRUE);
}

static int
LbxWrite(trans_conn, proxy, buf, len)
    XtransConnInfo trans_conn;
    LbxProxyPtr proxy;
    char *buf;
    int len;
{
    struct iovec iov;
    int n;
    int notWritten;

    notWritten = len;
    iov.iov_base = buf;
    iov.iov_len = len;
    while (notWritten) {
	errno = 0;
	if (proxy->compHandle)
	    n = (*proxy->streamOpts.streamCompWriteV)(proxy->fd, &iov, 1);
	else
	    n = _XSERVTransWritev(trans_conn, &iov, 1);
	if (n >= 0) {
	    iov.iov_base = (char *)iov.iov_base + n;
	    notWritten -= n;
	    iov.iov_len = notWritten;
	}
	else if (ETEST(errno)
#ifdef SUNSYSV /* check for another brain-damaged OS bug */
		 || (errno == 0)
#endif
#ifdef EMSGSIZE /* check for another brain-damaged OS bug */
		 || ((errno == EMSGSIZE) && (iov.iov_len == 1))
#endif
		)
	    break;
#ifdef EMSGSIZE /* check for another brain-damaged OS bug */
	else if (errno == EMSGSIZE)
	    iov.iov_len >>= 1;
#endif
	else
	    return -1;
    }
    return len - notWritten;
}

static Bool
LbxAppendOutput(proxy, client, oco)
    LbxProxyPtr proxy;
    ClientPtr client;
    ConnectionOutputPtr oco;
{
    ConnectionOutputPtr noco = proxy->olast;
    LbxClientPtr lbxClient = LbxClient(client);

    if (!lbxClient) {
	xfree(oco->buf);
	xfree(oco);
	return TRUE;
    }
    if (noco)
	LbxReencodeOutput(client,
			  (char *)noco->buf, &noco->count,
			  (char *)oco->buf, &oco->count);
    else
	LbxReencodeOutput(client,
			  (char *)NULL, (int *)NULL,
			  (char *)oco->buf, &oco->count);
    if (!oco->count) {
	if (oco->size > BUFWATERMARK)
	{
	    xfree(oco->buf);
	    xfree(oco);
	}
	else
	{
	    oco->next = FreeOutputs;
	    FreeOutputs = oco;
	}
	return TRUE;
    }
    if ((lbxClient->id != proxy->cur_send_id) && proxy->lbxClients[0]) {
	xLbxSwitchEvent *ev;
	int n;

	if (!noco || (noco->size - noco->count) < sz_xLbxSwitchEvent) {
	    if (noco = FreeOutputs)
		FreeOutputs = noco->next;
	    else
		noco = AllocateOutputBuffer();
	    if (!noco) {
		MarkClientException(client);
		return FALSE;
	    }
	    noco->next = NULL;
	    if (proxy->olast)
		proxy->olast->next = noco;
	    else
		proxy->ofirst = noco;
	    proxy->olast = noco;
	}
	ev = (xLbxSwitchEvent *) (noco->buf + noco->count);
	noco->count += sz_xLbxSwitchEvent;
	proxy->cur_send_id = lbxClient->id;
	ev->type = LbxEventCode;
	ev->lbxType = LbxSwitchEvent;
	ev->pad = 0;
	ev->client = proxy->cur_send_id;
	if (LbxProxyClient(proxy)->swapped) {
	    swapl(&ev->client, n);
	}
    }
    oco->next = NULL;
    if (proxy->olast)
	proxy->olast->next = oco;
    else
	proxy->ofirst = oco;
    proxy->olast = oco;
    return TRUE;
}

static int
LbxClientOutput(client, oc, extraBuf, extraCount, nocompress)
    ClientPtr client;
    OsCommPtr oc;
    char *extraBuf;
    int extraCount;
    Bool nocompress;
{
    ConnectionOutputPtr oco;
    int len;

    if (oco = oc->output) {
	oc->output = NULL;
	if (!LbxAppendOutput(oc->proxy, client, oco))
	    return -1;
    }

    if (extraCount) {
	NewOutputPending = TRUE;
	FD_SET(oc->fd, &OutputPending);
	len = (extraCount + 3) & ~3;
	if ((oco = FreeOutputs) && (oco->size >= len))
	    FreeOutputs = oco->next;
	else {
	    oco = (ConnectionOutputPtr)xalloc(sizeof(ConnectionOutput));
	    if (!oco) {
		MarkClientException(client);
		return -1;
	    }
	    oco->size = len;
	    if (oco->size < BUFSIZE)
		oco->size = BUFSIZE;
	    oco->buf = (unsigned char *) xalloc(oco->size);
	    if (!oco->buf) {
		xfree(oco);
		MarkClientException(client);
		return -1;
	    }
	}
	oco->count = len;
	oco->nocompress = nocompress;
	memmove((char *)oco->buf, extraBuf, extraCount);
	if (!nocompress && oco->count < oco->size)
	    oc->output = oco;
	else if (!LbxAppendOutput(oc->proxy, client, oco))
	    return -1;
    }
    return extraCount;
}

void
LbxForceOutput(proxy)
    LbxProxyPtr proxy;
{
    int i;
    LbxClientPtr lbxClient;
    OsCommPtr coc;
    ConnectionOutputPtr oco;

    for (i = proxy->maxIndex; i >= 0; i--) { /* proxy must be last */
	lbxClient = proxy->lbxClients[i];
	if (!lbxClient)
	    continue;
	coc = (OsCommPtr)lbxClient->client->osPrivate;
	if (oco = coc->output) {
	    coc->output = NULL;
	    LbxAppendOutput(proxy, lbxClient->client, oco);
	}
    }
}

int
LbxFlushClient(who, oc, extraBuf, extraCount)
    ClientPtr who;
    OsCommPtr oc;
    char *extraBuf;
    int extraCount;
{
    LbxProxyPtr proxy;
    ConnectionOutputPtr oco;
    int n;
    XtransConnInfo trans_conn;

    if (extraBuf)
	return LbxClientOutput(who, oc, extraBuf, extraCount, FALSE);
    proxy = oc->proxy;
    if (!proxy->lbxClients[0])
	return 0;
    LbxForceOutput(proxy);
    if (!proxy->compHandle)
	trans_conn = ((OsCommPtr)LbxProxyClient(proxy)->osPrivate)->trans_conn;
    while (oco = proxy->ofirst) {
	/* XXX bundle up into writev someday */
	if (proxy->compHandle) {
	    if (oco->nocompress)
		(*proxy->streamOpts.streamCompOff)(proxy->fd);
	    n = LbxWrite(NULL, proxy, (char *)oco->buf, oco->count);
	    if (oco->nocompress)
		(*proxy->streamOpts.streamCompOn)(proxy->fd);
	} else
	    n = LbxWrite(trans_conn, proxy, (char *)oco->buf, oco->count);
	if (n < 0) {
	    ClientPtr pclient = LbxProxyClient(proxy);
	    if (proxy->compHandle)
		trans_conn = ((OsCommPtr)pclient->osPrivate)->trans_conn;
	    _XSERVTransDisconnect(trans_conn);
	    _XSERVTransClose(trans_conn);
	    ((OsCommPtr)pclient->osPrivate)->trans_conn = NULL;
	    MarkClientException(pclient);
	    return 0;
	} else if (n == oco->count) {
	    proxy->ofirst = oco->next;
	    if (!proxy->ofirst)
		proxy->olast = NULL;
	    if (oco->size > BUFWATERMARK)
	    {
		xfree(oco->buf);
		xfree(oco);
	    }
	    else
	    {
		oco->next = FreeOutputs;
		oco->count = 0;
		FreeOutputs = oco;
	    }
	} else {
	    if (n) {
		oco->count -= n;
		memmove((char *)oco->buf, (char *)oco->buf + n, oco->count);
	    }
	    break;
	}
    }
    if ((proxy->compHandle &&
	 (*proxy->streamOpts.streamCompFlush)(proxy->fd)) ||
	proxy->ofirst) {
	FD_SET(proxy->fd, &ClientsWriteBlocked);
	AnyClientsWriteBlocked = TRUE;
    }
    return 0;
}

int
UncompressedWriteToClient (who, count, buf)
    ClientPtr who;
    char *buf;
    int count;
{
    return LbxClientOutput(who, (OsCommPtr)who->osPrivate, buf, count, TRUE);
}

LbxFreeOsBuffers(proxy)
    LbxProxyPtr proxy;
{
    ConnectionOutputPtr oco;

    while (oco = proxy->ofirst) {
	proxy->ofirst = oco->next;
	xfree(oco->buf);
	xfree(oco);
    }
}

Bool
AllocateLargeReqBuffer(client, size)
    ClientPtr client;
    int size;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci;

    if (!(oci = oc->largereq)) {
	if (oci = FreeInputs)
	    FreeInputs = oci->next;
	else {
	    oci = (ConnectionInputPtr)xalloc(sizeof(ConnectionInput));
	    if (!oci)
		return FALSE;
	    oci->buffer = NULL;
	    oci->size = 0;
	}
    }
    if (oci->size < size) {
	char *ibuf;

	oci->size = size;
	if (size < BUFSIZE)
	    oci->size = BUFSIZE;
	if (!(ibuf = (char *)xrealloc(oci->buffer, oci->size)))
	{
	    xfree(oci->buffer);
	    xfree(oci);
	    oc->largereq = NULL;
	    return FALSE;
	}
	oci->buffer = ibuf;
    }
    oci->bufptr = oci->buffer;
    oci->bufcnt = 0;
    oci->lenLastReq = size;
    oc->largereq = oci;
    return TRUE;
}

Bool
AddToLargeReqBuffer(client, data, size)
    ClientPtr client;
    char *data;
    int size;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->largereq;

    if (!oci || (oci->bufcnt + size > oci->lenLastReq))
	return FALSE;
    memcpy(oci->buffer + oci->bufcnt, data, size);
    oci->bufcnt += size;
    return TRUE;
}

static OsCommRec lbxAvailableInput;

int
PrepareLargeReqBuffer(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->largereq;

    if (!oci)
	return client->req_len << 2;
    oc->largereq = NULL;
    if (oci->bufcnt != oci->lenLastReq) {
	xfree(oci->buffer);
	xfree(oci);
	return client->req_len << 2;
    }
    client->requestBuffer = oci->buffer;
    client->req_len = oci->lenLastReq >> 2;
    oci->bufcnt = 0;
    oci->lenLastReq = 0;
    if (AvailableInput)
    {
	register ConnectionInputPtr aci = AvailableInput->input;
	if (aci->size > BUFWATERMARK)
	{
	    xfree(aci->buffer);
	    xfree(aci);
	}
	else
	{
	    aci->next = FreeInputs;
	    FreeInputs = aci;
	}
	AvailableInput->input = (ConnectionInputPtr)NULL;
    }
    lbxAvailableInput.input = oci;
    AvailableInput = &lbxAvailableInput;
    return client->req_len << 2;
}
