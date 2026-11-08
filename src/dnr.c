/*
 * dnr.c - MacTCP Domain Name Resolver glue.
 *
 * The resolver lives in a 'dnrp' code resource inside the MacTCP control
 * panel (or the "MacTCP DNR" file that Open Transport installs). We find it,
 * load it, and call into it directly, which is what Apple's dnr.c glue did.
 */
#include <string.h>
#include <Memory.h>
#include <Resources.h>
#include <Files.h>
#include <OSUtils.h>
#include <Gestalt.h>
#include "MacIRC.h"
#include "mactcp.h"

#define kOpenResolver   1L
#define kCloseResolver  2L
#define kStrToAddr      3L

typedef pascal void (*DNRResultProc)(MTHostInfo *info, char *userData);

static Handle            gDNRCode = NULL;
static MTHostInfo        gHostInfo;
static volatile Boolean  gLookupDone;
static Boolean           gLookupBusy;
static char              gLookupName[256];

/*
 * Call the resolver. Its code was built with MPW C, which treats D0-D2 and
 * A0-A1 as scratch registers; gcc expects D2 to survive a call. So the call
 * is made from inline assembly with the right clobber list. Every selector
 * takes at most four arguments after the selector; extras are harmless
 * because the caller pops them.
 */
static OSErr CallDNR(long selector, long a, long b, long c, long d)
{
    register long result __asm__("d0");
    Ptr code = *gDNRCode;
    __asm__ volatile (
        "move.l %5,-(%%sp)\n\t"
        "move.l %4,-(%%sp)\n\t"
        "move.l %3,-(%%sp)\n\t"
        "move.l %2,-(%%sp)\n\t"
        "move.l %1,-(%%sp)\n\t"
        "jsr (%6)\n\t"
        "lea 20(%%sp),%%sp"
        : "=d"(result)
        : "r"(selector), "r"(a), "r"(b), "r"(c), "r"(d), "a"(code)
        : "d1", "d2", "a0", "a1", "memory", "cc");
    return (OSErr)result;
}

static pascal void DNRResult(MTHostInfo *info, char *userData)
{
    (void)info;
    *(volatile Boolean *)userData = true;
}

/* Look through one folder for a file containing the resolver. */
static short SearchFolder(short vRefNum, long dirID, Boolean useHFS)
{
    CInfoPBRec pb;
    Str255 name;
    short index;

    for (index = 1; ; index++) {
        OSType type, creator;
        memset(&pb, 0, sizeof(pb));
        pb.hFileInfo.ioNamePtr = name;
        pb.hFileInfo.ioVRefNum = vRefNum;
        pb.hFileInfo.ioFDirIndex = index;
        pb.hFileInfo.ioDirID = dirID;
        if (PBGetCatInfoSync(&pb) != noErr) break;
        if (pb.hFileInfo.ioFlAttrib & 0x10) continue;     /* directory */
        type = pb.hFileInfo.ioFlFndrInfo.fdType;
        creator = pb.hFileInfo.ioFlFndrInfo.fdCreator;
        if ((type == 'cdev' && (creator == 'ztcp' || creator == 'mtcp')) ||
            (name[0] == 10 && memcmp(name + 1, "MacTCP DNR", 10) == 0)) {
            short ref = useHFS ? HOpenResFile(vRefNum, dirID, name, fsRdPerm)
                               : OpenRFPerm(name, vRefNum, fsRdPerm);
            if (ref != -1) {
                if (Get1IndResource('dnrp', 1) != NULL)
                    return ref;
                CloseResFile(ref);
            }
        }
    }
    return -1;
}

static short OpenResolverFile(void)
{
    LONGINT response;
    short ref, vRef;
    LONGINT dirID;

    if (SafeGestalt(gestaltFindFolderAttr, &response) == noErr &&
        (response & 1L /* gestaltFindFolderPresent */)) {
        if (FindFolder((short)0x8000, kSystemFolderType, false, &vRef, &dirID) == noErr) {
            ref = SearchFolder(vRef, dirID, true);
            if (ref != -1) return ref;
        }
        if (FindFolder((short)0x8000, kControlPanelFolderType, false, &vRef, &dirID) == noErr) {
            ref = SearchFolder(vRef, dirID, true);
            if (ref != -1) return ref;
        }
        if (FindFolder((short)0x8000, kExtensionFolderType, false, &vRef, &dirID) == noErr) {
            ref = SearchFolder(vRef, dirID, true);
            if (ref != -1) return ref;
        }
    } else {
        /* System 6: the System Folder is a working directory */
        SysEnvRec env;
        if (SysEnvirons(1, &env) == noErr) {
            ref = SearchFolder(env.sysVRefNum, 0, false);
            if (ref != -1) return ref;
        }
    }
    return -1;
}

OSErr DNR_Open(void)
{
    short saveRes, ref;
    OSErr err;

    if (gDNRCode) return noErr;

    saveRes = CurResFile();
    ref = OpenResolverFile();
    if (ref != -1) {
        UseResFile(ref);
        gDNRCode = Get1IndResource('dnrp', 1);
    } else {
        /* Some configurations keep it in the System file */
        gDNRCode = GetIndResource('dnrp', 1);
    }
    if (gDNRCode) DetachResource(gDNRCode);
    if (ref != -1) CloseResFile(ref);
    UseResFile(saveRes);

    if (!gDNRCode) return resNotFound;

    MoveHHi(gDNRCode);
    HLock(gDNRCode);
    err = CallDNR(kOpenResolver, 0, 0, 0, 0);
    if (err != noErr) {
        DisposeHandle(gDNRCode);
        gDNRCode = NULL;
    }
    return err;
}

void DNR_Close(void)
{
    if (!gDNRCode) return;
    /* Never unload while a lookup could still call back into us */
    if (gLookupBusy && !gLookupDone) return;
    CallDNR(kCloseResolver, 0, 0, 0, 0);
    DisposeHandle(gDNRCode);
    gDNRCode = NULL;
}

Boolean DNR_Busy(void)
{
    return gLookupBusy && !gLookupDone;
}

OSErr DNR_Lookup(const char *host)
{
    OSErr err;

    if (DNR_Busy()) return kMTDNRErr;
    err = DNR_Open();
    if (err != noErr) return err;

    StrNCopy(gLookupName, host, sizeof(gLookupName));
    memset(&gHostInfo, 0, sizeof(gHostInfo));
    gLookupDone = false;
    gLookupBusy = true;
    err = CallDNR(kStrToAddr, (long)gLookupName, (long)&gHostInfo,
                  (long)(DNRResultProc)DNRResult, (long)&gLookupDone);
    if (err == kMTCacheFault) {
        return noErr;   /* result arrives later via DNRResult */
    }
    /* Answered immediately (from the cache) or failed */
    gLookupDone = true;
    gHostInfo.rtnCode = err;
    return noErr;
}

/* Returns true once the lookup has finished. */
Boolean DNR_Done(OSErr *err, unsigned long *addr)
{
    if (!gLookupBusy) { *err = kMTDNRErr; return true; }
    if (!gLookupDone) return false;
    gLookupBusy = false;
    *err = (OSErr)gHostInfo.rtnCode;
    *addr = gHostInfo.addr[0];
    if (*err == noErr && *addr == 0) *err = kMTNoAnsErr;
    return true;
}
