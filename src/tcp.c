/*
 * tcp.c - a small non-blocking TCP client built on the MacTCP .IPP driver.
 *
 * Opening a connection and sending data use asynchronous driver calls whose
 * ioResult is polled from the event loop, so the UI never freezes waiting
 * for the network. Incoming data is pulled with TCPStatus + TCPRcv only
 * when MacTCP reports unread bytes, so those calls return immediately.
 */
#include <string.h>
#include <stdio.h>
#include <Memory.h>
#include <Devices.h>
#include <Events.h>
#include <OSUtils.h>
#include "MacIRC.h"
#include "mactcp.h"

#define kRcvBufSize   16384L
#define kSendQSize    8192L

static short          gIPPRef = 0;
static Ptr            gStream = NULL;
static Ptr            gRcvBuf = NULL;
static short          gState = kNetIdle;
static unsigned long  gRemoteAddr;
static unsigned short gRemotePort;

static MTTCPiopb      gOpenPB;
static MTTCPiopb      gSendPB;
static MTTCPiopb      gClosePB;
static MTTCPiopb      gCtlPB;
static Boolean        gSendBusy;

static char          *gSendQ = NULL;       /* waiting to be sent */
static long           gSendQLen = 0;
static char          *gSendFly = NULL;     /* being sent right now */
static MTWDSEntry     gWDS[2];

OSErr Net_Init(void)
{
    OSErr err;
    if (gIPPRef != 0) return noErr;
    err = OpenDriver("\p.IPP", &gIPPRef);
    if (err != noErr) {
        gIPPRef = 0;
        return err;
    }
    if (!gSendQ) gSendQ = NewPtr(kSendQSize);
    if (!gSendFly) gSendFly = NewPtr(kSendQSize);
    if (!gSendQ || !gSendFly) return memFullErr;
    return noErr;
}

static OSErr SyncCall(short csCode)
{
    gCtlPB.ioCompletion = NULL;
    gCtlPB.ioCRefNum = gIPPRef;
    gCtlPB.tcpStream = gStream;
    gCtlPB.csCode = csCode;
    return PBControlSync((ParmBlkPtr)&gCtlPB);
}

static void ReleaseStream(void)
{
    if (gStream) {
        memset(&gCtlPB, 0, sizeof(gCtlPB));
        SyncCall(kMTTCPRelease);   /* aborts the connection if still open */
        gStream = NULL;
    }
    if (gRcvBuf) {
        DisposePtr(gRcvBuf);
        gRcvBuf = NULL;
    }
    gSendBusy = false;
    gSendQLen = 0;
    gState = kNetIdle;
}

void Net_Shutdown(void)
{
    Net_Close(false);
    DNR_Close();
    /* The .IPP driver is shared by the whole system; never close it. */
}

short Net_State(void)
{
    return gState;
}

unsigned long Net_RemoteAddr(void)
{
    return gRemoteAddr;
}

void Net_AddrToStr(unsigned long a, char *out)
{
    sprintf(out, "%lu.%lu.%lu.%lu", (a >> 24) & 0xFF, (a >> 16) & 0xFF,
            (a >> 8) & 0xFF, a & 0xFF);
}

static Boolean ParseDottedQuad(const char *s, unsigned long *addr)
{
    unsigned long a = 0, part;
    short dots = 0;
    while (1) {
        if (*s < '0' || *s > '9') return false;
        part = 0;
        while (*s >= '0' && *s <= '9') {
            part = part * 10 + (*s - '0');
            if (part > 255) return false;
            s++;
        }
        a = (a << 8) | part;
        if (*s == 0) break;
        if (*s != '.' || ++dots > 3) return false;
        s++;
    }
    if (dots != 3) return false;
    *addr = a;
    return true;
}

static OSErr StartOpen(void)
{
    OSErr err;

    gRcvBuf = NewPtr(kRcvBufSize);
    if (!gRcvBuf) return memFullErr;

    memset(&gCtlPB, 0, sizeof(gCtlPB));
    gCtlPB.csParam.create.rcvBuff = gRcvBuf;
    gCtlPB.csParam.create.rcvBuffLen = kRcvBufSize;
    gCtlPB.csParam.create.notifyProc = NULL;
    gStream = NULL;
    err = SyncCall(kMTTCPCreate);
    if (err != noErr) {
        DisposePtr(gRcvBuf);
        gRcvBuf = NULL;
        return err;
    }
    gStream = gCtlPB.tcpStream;

    memset(&gOpenPB, 0, sizeof(gOpenPB));
    gOpenPB.ioCompletion = NULL;
    gOpenPB.ioCRefNum = gIPPRef;
    gOpenPB.tcpStream = gStream;
    gOpenPB.csCode = kMTTCPActiveOpen;
    gOpenPB.csParam.open.commandTimeoutValue = 30;
    gOpenPB.csParam.open.remoteHost = gRemoteAddr;
    gOpenPB.csParam.open.remotePort = gRemotePort;
    gOpenPB.csParam.open.localHost = 0;
    gOpenPB.csParam.open.localPort = 0;
    err = PBControlAsync((ParmBlkPtr)&gOpenPB);
    if (err != noErr) {
        ReleaseStream();
        return err;
    }
    gState = kNetOpening;
    return noErr;
}

OSErr Net_Connect(const char *host, unsigned short port)
{
    OSErr err;

    if (gState != kNetIdle) Net_Close(false);
    err = Net_Init();
    if (err != noErr) return err;

    gRemotePort = port;
    gSendQLen = 0;
    gSendBusy = false;

    if (ParseDottedQuad(host, &gRemoteAddr))
        return StartOpen();

    err = DNR_Lookup(host);
    if (err != noErr) return err;
    gState = kNetResolving;
    return noErr;
}

static void StartSend(void)
{
    OSErr err;
    if (gSendBusy || gSendQLen == 0 || gState != kNetConnected) return;

    BlockMove(gSendQ, gSendFly, gSendQLen);
    gWDS[0].length = (unsigned short)gSendQLen;
    gWDS[0].ptr = gSendFly;
    gWDS[1].length = 0;
    gWDS[1].ptr = NULL;
    gSendQLen = 0;

    memset(&gSendPB, 0, sizeof(gSendPB));
    gSendPB.ioCompletion = NULL;
    gSendPB.ioCRefNum = gIPPRef;
    gSendPB.tcpStream = gStream;
    gSendPB.csCode = kMTTCPSend;
    gSendPB.csParam.send.pushFlag = 1;
    gSendPB.csParam.send.wdsPtr = (Ptr)gWDS;
    err = PBControlAsync((ParmBlkPtr)&gSendPB);
    if (err == noErr)
        gSendBusy = true;
}

/* Wait (briefly) for an in-flight send so its buffer can be reused. */
static Boolean WaitForSend(long ticks)
{
    unsigned long until = TickCount() + ticks;
    while (gSendBusy && gSendPB.ioResult > 0 && TickCount() < until)
        SystemTask();
    if (gSendBusy && gSendPB.ioResult <= 0) gSendBusy = false;
    return !gSendBusy;
}

OSErr Net_Send(const char *data, long len)
{
    if (gState != kNetConnected) return kMTConnectionDoesntExist;
    if (len > kSendQSize) return kMTInvalidLength;
    if (gSendQLen + len > kSendQSize) {
        /* Queue is full: wait for the current send, then push the queue */
        if (!WaitForSend(60 * 10)) return kMTCommandTimeout;
        StartSend();
        if (gSendQLen + len > kSendQSize) {
            if (!WaitForSend(60 * 10)) return kMTCommandTimeout;
            StartSend();
        }
    }
    BlockMove(data, gSendQ + gSendQLen, len);
    gSendQLen += len;
    StartSend();
    return noErr;
}

short Net_Poll(OSErr *errOut)
{
    OSErr err;
    unsigned long addr;

    *errOut = noErr;
    switch (gState) {
    case kNetResolving:
        if (DNR_Done(&err, &addr)) {
            if (err != noErr) {
                gState = kNetIdle;
                *errOut = err;
                return kNetEvtFailed;
            }
            gRemoteAddr = addr;
            err = StartOpen();
            if (err != noErr) {
                *errOut = err;
                return kNetEvtFailed;
            }
            return kNetEvtResolved;
        }
        break;

    case kNetOpening:
        if (gOpenPB.ioResult <= 0) {
            err = gOpenPB.ioResult;
            if (err != noErr) {
                ReleaseStream();
                *errOut = err;
                return kNetEvtFailed;
            }
            gState = kNetConnected;
            return kNetEvtConnected;
        }
        break;

    case kNetConnected:
        if (gSendBusy && gSendPB.ioResult <= 0) {
            err = gSendPB.ioResult;
            gSendBusy = false;
            if (err != noErr && err != kMTConnectionClosing) {
                ReleaseStream();
                *errOut = err;
                return kNetEvtClosed;
            }
        }
        StartSend();
        break;
    }
    return kNetEvtNone;
}

/* Returns bytes read (0 if nothing waiting), or -1 if the connection
   has closed and no data remains. */
long Net_Read(char *buf, long max)
{
    OSErr err;
    unsigned short unread;
    unsigned char state;

    if (gState != kNetConnected) return -1;

    memset(&gCtlPB, 0, sizeof(gCtlPB));
    err = SyncCall(kMTTCPStatus);
    if (err != noErr) return -1;
    unread = gCtlPB.csParam.status.amtUnreadData;
    state = gCtlPB.csParam.status.connectionState;

    if (unread == 0) {
        if (state != kMTStateEstablished && state != kMTStateFinWait1 &&
            state != kMTStateFinWait2)
            return -1;      /* remote side closed */
        return 0;
    }

    if (unread > max) unread = (unsigned short)max;
    memset(&gCtlPB, 0, sizeof(gCtlPB));
    gCtlPB.csParam.receive.commandTimeoutValue = 1;
    gCtlPB.csParam.receive.rcvBuff = buf;
    gCtlPB.csParam.receive.rcvBuffLen = unread;
    err = SyncCall(kMTTCPRcv);
    if (err == kMTCommandTimeout) return 0;
    if (err != noErr) return -1;
    return gCtlPB.csParam.receive.rcvBuffLen;
}

void Net_Close(Boolean graceful)
{
    if (gState == kNetResolving) {
        /* The resolver can't be cancelled; DNR_Done() will clean up */
        gState = kNetIdle;
        return;
    }
    if (!gStream) {
        gState = kNetIdle;
        return;
    }
    if (graceful && gState == kNetConnected) {
        unsigned long until;
        StartSend();
        WaitForSend(60 * 3);
        memset(&gClosePB, 0, sizeof(gClosePB));
        gClosePB.ioCRefNum = gIPPRef;
        gClosePB.tcpStream = gStream;
        gClosePB.csCode = kMTTCPClose;
        gClosePB.csParam.close.ulpTimeoutValue = 5;
        gClosePB.csParam.close.validityFlags = 0xC0;
        if (PBControlAsync((ParmBlkPtr)&gClosePB) == noErr) {
            until = TickCount() + 60 * 2;
            while (gClosePB.ioResult > 0 && TickCount() < until)
                SystemTask();
        }
    }
    ReleaseStream();
}

const char *Net_ErrString(OSErr err)
{
    static char buf[64];
    switch (err) {
    case kMTIPBadLapErr:           return "network hardware error";
    case kMTIPBadCnfgErr:
    case kMTIPNoCnfgErr:           return "TCP/IP is not configured";
    case kMTIPLoadErr:             return "TCP/IP could not be loaded";
    case kMTIPBadAddr:             return "bad IP address";
    case kMTConnectionClosing:     return "connection is closing";
    case kMTConnectionExists:      return "connection already exists";
    case kMTConnectionDoesntExist: return "not connected";
    case kMTInsufficientResources: return "out of network resources";
    case kMTConnectionTerminated:  return "connection terminated by remote host";
    case kMTOpenFailed:            return "connection refused";
    case kMTCommandTimeout:        return "timed out";
    case kMTNameSyntaxErr:         return "invalid host name";
    case kMTNoNameServer:          return "no name server configured";
    case kMTAuthNameErr:           return "host not found";
    case kMTNoAnsErr:              return "name server did not answer";
    case kMTDNRErr:                return "name lookup failed";
    case kMTOutOfMemory:           return "resolver out of memory";
    case resNotFound:              return "MacTCP resolver (DNR) not found";
    case memFullErr:               return "out of memory";
    case -43:                      return "MacTCP is not installed";
    }
    sprintf(buf, "error %d", (int)err);
    return buf;
}
