/*
 * mactcp.h - Minimal MacTCP driver interface definitions.
 *
 * Retro68's Multiversal Interfaces don't ship MacTCP.h, so the parameter
 * block layouts for the .IPP driver are declared here directly. Names carry
 * an "MT" prefix so they can't collide with Apple's headers if those are
 * ever used instead. The layouts follow the MacTCP Programmer's Guide and use
 * classic 68k (2-byte) alignment.
 *
 * Open Transport provides the same .IPP driver interface for compatibility,
 * so this code works with both MacTCP and Open Transport.
 */
#ifndef MACIRC_MACTCP_H
#define MACIRC_MACTCP_H

#include <MacTypes.h>

#pragma pack(push, 2)

typedef unsigned long  MTIPAddr;
typedef unsigned short MTPort;

/* TCP control codes (csCode) */
enum {
    kMTTCPCreate       = 30,
    kMTTCPPassiveOpen  = 31,
    kMTTCPActiveOpen   = 32,
    kMTTCPSend         = 34,
    kMTTCPNoCopyRcv    = 35,
    kMTTCPRcvBfrReturn = 36,
    kMTTCPRcv          = 37,
    kMTTCPClose        = 38,
    kMTTCPAbort        = 39,
    kMTTCPStatus       = 40,
    kMTTCPExtendedStat = 41,
    kMTTCPRelease      = 42,
    kMTTCPGlobalInfo   = 43
};

/* TCP connection states reported by TCPStatus */
enum {
    kMTStateClosed      = 0,
    kMTStateListen      = 2,
    kMTStateSynRcvd     = 4,
    kMTStateSynSent     = 6,
    kMTStateEstablished = 8,
    kMTStateFinWait1    = 10,
    kMTStateFinWait2    = 12,
    kMTStateCloseWait   = 14,
    kMTStateClosing     = 16,
    kMTStateLastAck     = 18,
    kMTStateTimeWait    = 20
};

/* MacTCP result codes */
enum {
    kMTInProgress            = 1,
    kMTIPBadLapErr           = -23000,
    kMTIPBadCnfgErr          = -23001,
    kMTIPNoCnfgErr           = -23002,
    kMTIPLoadErr             = -23003,
    kMTIPBadAddr             = -23004,
    kMTConnectionClosing     = -23005,
    kMTInvalidLength         = -23006,
    kMTConnectionExists      = -23007,
    kMTConnectionDoesntExist = -23008,
    kMTInsufficientResources = -23009,
    kMTInvalidStreamPtr      = -23010,
    kMTStreamAlreadyOpen     = -23011,
    kMTConnectionTerminated  = -23012,
    kMTInvalidBufPtr         = -23013,
    kMTInvalidRDS            = -23014,
    kMTOpenFailed            = -23015,
    kMTCommandTimeout        = -23016,
    kMTDuplicateSocket       = -23017,
    kMTNameSyntaxErr         = -23041,
    kMTCacheFault            = -23042,
    kMTNoResultProc          = -23043,
    kMTNoNameServer          = -23044,
    kMTAuthNameErr           = -23045,
    kMTNoAnsErr              = -23046,
    kMTDNRErr                = -23047,
    kMTOutOfMemory           = -23048
};

/* Write data structure entry; an array is terminated by length == 0. */
typedef struct MTWDSEntry {
    unsigned short length;
    Ptr            ptr;
} MTWDSEntry;

typedef struct MTTCPCreatePB {
    Ptr           rcvBuff;
    unsigned long rcvBuffLen;
    Ptr           notifyProc;
    Ptr           userDataPtr;
} MTTCPCreatePB;

typedef struct MTTCPOpenPB {
    unsigned char ulpTimeoutValue;
    unsigned char ulpTimeoutAction;
    unsigned char validityFlags;
    unsigned char commandTimeoutValue;
    MTIPAddr      remoteHost;
    MTPort        remotePort;
    MTIPAddr      localHost;
    MTPort        localPort;
    unsigned char tosFlags;
    unsigned char precedence;
    unsigned char dontFrag;
    unsigned char timeToLive;
    unsigned char security;
    unsigned char optionCnt;
    unsigned char options[40];
    Ptr           userDataPtr;
} MTTCPOpenPB;

typedef struct MTTCPSendPB {
    unsigned char  ulpTimeoutValue;
    unsigned char  ulpTimeoutAction;
    unsigned char  validityFlags;
    unsigned char  pushFlag;
    unsigned char  urgentFlag;
    Ptr            wdsPtr;
    unsigned long  sendFree;
    unsigned short sendLength;
    Ptr            userDataPtr;
} MTTCPSendPB;

typedef struct MTTCPReceivePB {
    unsigned char  commandTimeoutValue;
    unsigned char  filler;
    unsigned char  markFlag;
    unsigned char  urgentFlag;
    Ptr            rcvBuff;
    unsigned short rcvBuffLen;
    Ptr            rdsPtr;
    unsigned short rdsLength;
    unsigned short secondTimeStamp;
    Ptr            userDataPtr;
} MTTCPReceivePB;

typedef struct MTTCPClosePB {
    unsigned char ulpTimeoutValue;
    unsigned char ulpTimeoutAction;
    unsigned char validityFlags;
    Ptr           userDataPtr;
} MTTCPClosePB;

typedef struct MTTCPStatusPB {
    unsigned char  ulpTimeoutValue;
    unsigned char  ulpTimeoutAction;
    long           unused;
    MTIPAddr       remoteHost;
    MTPort         remotePort;
    MTIPAddr       localHost;
    MTPort         localPort;
    unsigned char  tosFlags;
    unsigned char  precedence;
    unsigned char  connectionState;
    unsigned short sendWindow;
    unsigned short rcvWindow;
    unsigned short amtUnackedData;
    unsigned short amtUnreadData;
    Ptr            securityLevelPtr;
    unsigned long  sendUnacked;
    unsigned long  sendNext;
    unsigned long  congestionWindow;
    unsigned long  rcvNext;
    unsigned long  srtt;
    unsigned long  lastRTT;
    unsigned long  sendMaxSegSize;
    Ptr            connStatPtr;
    Ptr            userDataPtr;
} MTTCPStatusPB;

typedef struct MTTCPiopb {
    char           fill12[12];
    Ptr            ioCompletion;
    volatile short ioResult;
    Ptr            ioNamePtr;
    short          ioVRefNum;
    short          ioCRefNum;
    short          csCode;
    Ptr            tcpStream;
    union {
        MTTCPCreatePB  create;
        MTTCPOpenPB    open;
        MTTCPSendPB    send;
        MTTCPReceivePB receive;
        MTTCPClosePB   close;
        MTTCPStatusPB  status;
        Ptr            abortUserData;
    } csParam;
} MTTCPiopb;

/* Domain Name Resolver result (AddressXlation.h "hostInfo") */
typedef struct MTHostInfo {
    long          rtnCode;
    char          cname[255];
    char          filler;
    unsigned long addr[4];
} MTHostInfo;

#pragma pack(pop)

#endif
