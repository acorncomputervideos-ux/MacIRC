/*
 * log.c - conversation logs: one SimpleText file per window, kept in a
 * "MacIRC Logs" folder next to the application.
 */
#include <string.h>
#include <stdio.h>
#include <Files.h>
#include <OSUtils.h>
#include <Events.h>
#include "MacIRC.h"

static long    gLogDirID = 0;
static short   gLogVRef = 0;
static Boolean gUnflushed = false;
static unsigned long gLastFlush = 0;

static Boolean FindLogFolder(void)
{
    CInfoPBRec cpb;
    HParamBlockRec hpb;
    Str255 name;

    if (gLogDirID) return true;
    CToPStr("MacIRC Logs", name);

    memset(&cpb, 0, sizeof(cpb));
    cpb.hFileInfo.ioNamePtr = name;
    cpb.hFileInfo.ioVRefNum = gAppVRef;
    cpb.hFileInfo.ioDirID = gAppDirID;
    if (PBGetCatInfoSync(&cpb) == noErr) {
        if (!(cpb.hFileInfo.ioFlAttrib & 0x10)) return false;   /* a file, not a folder */
        gLogDirID = cpb.hFileInfo.ioDirID;
        gLogVRef = gAppVRef;
        return true;
    }

    memset(&hpb, 0, sizeof(hpb));
    hpb.fileParam.ioNamePtr = name;
    hpb.fileParam.ioVRefNum = gAppVRef;
    hpb.fileParam.ioDirID = gAppDirID;
    if (PBDirCreateSync(&hpb) != noErr) return false;
    gLogDirID = hpb.fileParam.ioDirID;
    gLogVRef = gAppVRef;
    return true;
}

static void LogFileName(IRCWin *w, Str255 out)
{
    char name[40];
    short i, n;
    if (w->kind == kWinServer) strcpy(name, "Server");
    else StrNCopy(name, w->name, 27);
    n = strlen(name);
    for (i = 0; i < n; i++)
        if (name[i] == ':' || (unsigned char)name[i] < 0x20) name[i] = '-';
    strcat(name, ".log");
    CToPStr(name, out);
}

void Log_Init(void)
{
    /* nothing to do until the first log is opened */
}

void Log_Open(IRCWin *w)
{
    Str255 fname;
    short ref;
    OSErr err;
    char header[96];
    unsigned long secs;
    DateTimeRec dt;
    LONGINT count;

    if (!gPrefs.logging || w->logRef) return;
    if (!FindLogFolder()) return;
    LogFileName(w, fname);
    err = HCreate(gLogVRef, gLogDirID, fname, 'ttxt', 'TEXT');
    if (err != noErr && err != dupFNErr) return;
    if (HOpen(gLogVRef, gLogDirID, fname, fsRdWrPerm, &ref) != noErr) return;
    SetFPos(ref, fsFromLEOF, 0);
    w->logRef = ref;

    GetDateTime(&secs);
    SecondsToDate(secs, &dt);
    sprintf(header, "\r--- Log opened %04d-%02d-%02d %02d:%02d\r",
            dt.year, dt.month, dt.day, dt.hour, dt.minute);
    count = strlen(header);
    FSWrite(ref, &count, header);
}

void Log_Write(IRCWin *w, const char *line)
{
    LONGINT count;
    if (!w->logRef) return;
    count = strlen(line);
    if (FSWrite(w->logRef, &count, line) != noErr) {
        Log_Close(w);
        return;
    }
    count = 1;
    FSWrite(w->logRef, &count, "\r");
    gUnflushed = true;
}

/* Write buffered log data to disk every 30 seconds, so a crash or power
   cut loses at most that much. Called from the event loop. */
void Log_Idle(void)
{
    IRCWin *w;
    if (!gUnflushed || TickCount() - gLastFlush < 60L * 30) return;
    gLastFlush = TickCount();
    gUnflushed = false;
    for (w = gWinList; w; w = w->next) {
        if (w->logRef) {
            ParamBlockRec pb;
            memset(&pb, 0, sizeof(pb));
            pb.ioParam.ioRefNum = w->logRef;
            PBFlushFileSync(&pb);
        }
    }
    FlushVol(NULL, gLogVRef);
}

void Log_Close(IRCWin *w)
{
    if (!w->logRef) return;
    FSClose(w->logRef);
    w->logRef = 0;
    FlushVol(NULL, gLogVRef);
}

/* Logging was switched on or off in Preferences. */
void Log_ApplyPrefs(void)
{
    IRCWin *w;
    for (w = gWinList; w; w = w->next) {
        if (gPrefs.logging) Log_Open(w);
        else Log_Close(w);
    }
}
