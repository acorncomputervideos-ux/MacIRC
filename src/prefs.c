/*
 * prefs.c - load/save settings in "MacIRC Prefs" (Preferences folder on
 * System 7, System Folder on System 6).
 */
#include <string.h>
#include <Files.h>
#include <OSUtils.h>
#include <Gestalt.h>
#include "MacIRC.h"

#define kPrefsName "\pMacIRC Prefs"

Prefs gPrefs;

static void Defaults(void)
{
    memset(&gPrefs, 0, sizeof(gPrefs));
    gPrefs.version = kPrefsVersion;
    strcpy(gPrefs.server, "irc.libera.chat");
    gPrefs.port = 6667;
    strcpy(gPrefs.nick, "MacUser");
    strcpy(gPrefs.realname, "MacIRC User");
    strcpy(gPrefs.channels, "");
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

void Prefs_Load(void)
{
    short vRef, ref;
    LONGINT dirID, count;
    Prefs p;

    Defaults();
    if (!PrefsLocation(&vRef, &dirID)) return;
    if (HOpen(vRef, dirID, kPrefsName, fsRdPerm, &ref) != noErr) return;
    count = sizeof(p);
    if (FSRead(ref, &count, &p) == noErr && count == sizeof(p) && p.version == kPrefsVersion) {
        /* make sure every string is terminated */
        p.server[sizeof(p.server) - 1] = 0;
        p.nick[sizeof(p.nick) - 1] = 0;
        p.realname[sizeof(p.realname) - 1] = 0;
        p.channels[sizeof(p.channels) - 1] = 0;
        p.password[sizeof(p.password) - 1] = 0;
        if (p.port <= 0) p.port = 6667;
        gPrefs = p;
    }
    FSClose(ref);
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
