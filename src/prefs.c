/*
 * prefs.c - load/save settings in "MacIRC Prefs" (Preferences folder on
 * System 7, System Folder on System 6).
 */
#include <string.h>
#include <Files.h>
#include <Fonts.h>
#include <OSUtils.h>
#include <Gestalt.h>
#include "MacIRC.h"

#define kPrefsName "\pMacIRC Prefs"

Prefs gPrefs;

/* The version 1 file is the first part of the current structure. */
#define kPrefsV1Size ((long)((char *)&((Prefs *)0)->nickservPass - (char *)0))

static void Defaults(void)
{
    memset(&gPrefs, 0, sizeof(gPrefs));
    gPrefs.version = kPrefsVersion;
    strcpy(gPrefs.server, "irc.libera.chat");
    gPrefs.port = 6667;
    strcpy(gPrefs.nick, "MacUser");
    strcpy(gPrefs.realname, "MacIRC User");
    strcpy(gPrefs.quitMsg, "MacIRC " kAppVersion " \xD1 classic Mac IRC");
    strcpy(gPrefs.awayMsg, "Away from my Mac");
    gPrefs.fontID = kFontIDMonaco;
    gPrefs.fontSize = 9;
    gPrefs.showTimestamps = true;
    gPrefs.beepOnMention = false;
    gPrefs.autoReconnect = true;
    gPrefs.rejoinChannels = true;
    gPrefs.logging = false;
    gPrefs.notifyBackground = true;
}

static Boolean PrefsLocation(short *vRef, LONGINT *dirID)
{
    LONGINT resp;
    if (SafeGestalt(gestaltFindFolderAttr, &resp) == noErr && (resp & 1)) {
        if (FindFolder((short)0x8000 /* kOnSystemDisk */, kPreferencesFolderType,
                       true, vRef, dirID) == noErr)
            return true;
    }
    {
        SysEnvRec env;
        if (SysEnvirons(1, &env) == noErr) {
            *vRef = env.sysVRefNum;   /* working directory for System Folder */
            *dirID = 0;
            return true;
        }
    }
    return false;
}

static void Terminate(char *s, long size)
{
    s[size - 1] = 0;
}

void Prefs_Load(void)
{
    short vRef, ref;
    LONGINT dirID, count;
    Prefs p;

    Defaults();
    if (!PrefsLocation(&vRef, &dirID)) return;
    if (HOpen(vRef, dirID, kPrefsName, fsRdPerm, &ref) != noErr) return;
    memset(&p, 0, sizeof(p));
    count = sizeof(p);
    FSRead(ref, &count, &p);
    FSClose(ref);

    if (p.version == kPrefsVersion && count == sizeof(p)) {
        gPrefs = p;
    } else if (p.version == kPrefsVersion1 && count >= kPrefsV1Size) {
        /* keep the connection settings, take defaults for the rest */
        BlockMove(&p, &gPrefs, kPrefsV1Size);
        gPrefs.version = kPrefsVersion;
    } else {
        return;
    }

    Terminate(gPrefs.server, sizeof(gPrefs.server));
    Terminate(gPrefs.nick, sizeof(gPrefs.nick));
    Terminate(gPrefs.realname, sizeof(gPrefs.realname));
    Terminate(gPrefs.channels, sizeof(gPrefs.channels));
    Terminate(gPrefs.password, sizeof(gPrefs.password));
    Terminate(gPrefs.nickservPass, sizeof(gPrefs.nickservPass));
    Terminate(gPrefs.lastChannels, sizeof(gPrefs.lastChannels));
    Terminate(gPrefs.ignoreList, sizeof(gPrefs.ignoreList));
    Terminate(gPrefs.highlightWords, sizeof(gPrefs.highlightWords));
    Terminate(gPrefs.quitMsg, sizeof(gPrefs.quitMsg));
    Terminate(gPrefs.awayMsg, sizeof(gPrefs.awayMsg));
    if (gPrefs.port <= 0) gPrefs.port = 6667;
    if (gPrefs.fontSize < 6 || gPrefs.fontSize > 24) gPrefs.fontSize = 9;
    if (gPrefs.fontID < 0) gPrefs.fontID = kFontIDMonaco;
}

void Prefs_Save(void)
{
    short vRef, ref;
    LONGINT dirID, count;
    OSErr err;

    if (!PrefsLocation(&vRef, &dirID)) return;
    err = HCreate(vRef, dirID, kPrefsName, kCreator, 'pref');
    if (err != noErr && err != dupFNErr) return;
    if (HOpen(vRef, dirID, kPrefsName, fsRdWrPerm, &ref) != noErr) return;
    count = sizeof(gPrefs);
    gPrefs.version = kPrefsVersion;
    FSWrite(ref, &count, &gPrefs);
    SetEOF(ref, sizeof(gPrefs));
    FSClose(ref);
    FlushVol(NULL, vRef);
}
