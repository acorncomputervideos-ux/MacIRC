/*
 * url.c - find URLs in text and open them with Internet Config.
 *
 * If Internet Config isn't installed, the URL is put on the clipboard
 * instead so it can be pasted into a browser.
 */
#include <string.h>
#include <stdio.h>
#include <Memory.h>
#include <Gestalt.h>
#include "MacIRC.h"

typedef long ComponentInst;

/* Component Manager and Internet Config component calls (not in the
   Multiversal headers). Selectors are from Apple's Components.h and
   InternetConfig.h. */
pascal ComponentInst MyOpenDefaultComponent(OSType type, OSType subType)
    M68K_INLINE(0x7021, 0xA82A);
pascal OSErr MyCloseComponent(ComponentInst inst)
    M68K_INLINE(0x7008, 0xA82A);
pascal long ICCStartComp(ComponentInst inst, OSType creator)
    M68K_INLINE(0x2F3C, 0x0004, 0x0000, 0x7000, 0xA82A);
pascal long ICCStopComp(ComponentInst inst)
    M68K_INLINE(0x2F3C, 0x0000, 0x0001, 0x7000, 0xA82A);
pascal long ICCFindConfigFileComp(ComponentInst inst, short count, Ptr folders)
    M68K_INLINE(0x2F3C, 0x0006, 0x0002, 0x7000, 0xA82A);
pascal long ICCLaunchURLComp(ComponentInst inst, ConstStr255Param hint, Ptr data,
                                    long len, long *selStart, long *selEnd)
    M68K_INLINE(0x2F3C, 0x0014, 0x0011, 0x7000, 0xA82A);

static Boolean IsURLChar(char c)
{
    unsigned char u = (unsigned char)c;
    if (u <= ' ' || u >= 0x7F) return false;
    return strchr("<>\"'`{}|\\^", c) == NULL;
}

static Boolean HasPrefix(const char *s, long len, const char *prefix)
{
    long n = strlen(prefix);
    return len >= n && IRCStrNCmp(s, prefix, n) == 0;
}

/* Is there a URL around text[offset]? Returns its range [start, end). */
Boolean URL_FindAt(const char *text, long len, long offset, long *start, long *end)
{
    long s = offset, e = offset;
    const char *p;
    long n;

    if (offset < 0 || offset > len) return false;
    if (offset == len && offset > 0) s = e = offset - 1;
    while (s > 0 && IsURLChar(text[s - 1])) s--;
    while (e < len && IsURLChar(text[e])) e++;
    /* trim trailing punctuation that is usually not part of the URL */
    while (e > s && strchr(".,;:!?)]", text[e - 1])) e--;
    /* trim leading punctuation */
    while (s < e && strchr("([", text[s])) s++;
    if (e - s < 5) return false;

    p = text + s;
    n = e - s;
    if (HasPrefix(p, n, "http://") || HasPrefix(p, n, "https://") ||
        HasPrefix(p, n, "ftp://") || HasPrefix(p, n, "gopher://") ||
        HasPrefix(p, n, "irc://") || HasPrefix(p, n, "news:") ||
        HasPrefix(p, n, "mailto:") || HasPrefix(p, n, "www.") ||
        HasPrefix(p, n, "ftp.")) {
        *start = s;
        *end = e;
        return true;
    }
    return false;
}

static Boolean LaunchWithIC(const char *url)
{
    LONGINT resp;
    ComponentInst inst;
    long selStart, selEnd, len, err;

    if (SafeGestalt('cpnt', &resp) != noErr) return false;   /* no Component Manager */
    inst = MyOpenDefaultComponent('PREF', 'ICAp');
    if (!inst) return false;

    err = ICCStartComp(inst, kCreator);
    if (err == noErr) {
        ICCFindConfigFileComp(inst, 0, NULL);
        len = strlen(url);
        selStart = 0;
        selEnd = len;
        err = ICCLaunchURLComp(inst, "\p", (Ptr)url, len, &selStart, &selEnd);
        ICCStopComp(inst);
    }
    MyCloseComponent(inst);
    return err == noErr;
}

void URL_Open(const char *url)
{
    char full[300];
    IRCWin *w = Win_Front();
    char msg[340];

    if (IRCStrNCmp(url, "www.", 4) == 0) sprintf(full, "http://%.280s", url);
    else if (IRCStrNCmp(url, "ftp.", 4) == 0) sprintf(full, "ftp://%.280s", url);
    else StrNCopy(full, url, sizeof(full));

    if (LaunchWithIC(full)) {
        sprintf(msg, "Opening %.300s", full);
        Win_AddLine(w ? w : Win_Server(), kLineInfo, NULL, msg);
        return;
    }

    /* Fallback: put it on the clipboard */
    ZeroScrap();
    PutScrap(strlen(full), 'TEXT', full);
    sprintf(msg, "Copied %.250s to the clipboard (install Internet Config to open links directly).", full);
    Win_AddLine(w ? w : Win_Server(), kLineInfo, NULL, msg);
}
