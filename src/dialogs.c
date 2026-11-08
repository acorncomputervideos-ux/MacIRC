/*
 * dialogs.c - Connect and Preferences dialogs, single-field prompt,
 * paste confirmation, About box, error alert.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <Dialogs.h>
#include <Memory.h>
#include <Events.h>
#include <Fonts.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include "MacIRC.h"

/* Connect dialog items */
enum {
    kConnOK = 1, kConnCancel = 2, kConnOutline = 3,
    kConnServer = 5, kConnPort = 7, kConnNick = 9,
    kConnReal = 11, kConnChans = 13, kConnPass = 15, kConnNickServ = 17
};

/* Prompt dialog items */
enum { kPromptOK = 1, kPromptCancel = 2, kPromptOutline = 3, kPromptText = 5 };

/* Preferences dialog items */
enum {
    kPrefOK = 1, kPrefCancel = 2, kPrefOutline = 3,
    kPrefMonaco = 5, kPrefGeneva = 6, kPrefCourier = 7,
    kPref9 = 9, kPref10 = 10, kPref12 = 11,
    kPrefStamps = 12, kPrefBeep = 13, kPrefReconnect = 14, kPrefRejoin = 15,
    kPrefLog = 16, kPrefNotify = 17,
    kPrefHighlight = 19, kPrefQuit = 21, kPrefAway = 23
};

/* Draws the thick rounded border around the default button. */
static pascal void OutlineDefault(DialogPtr d, short item)
{
    short type;
    Handle h;
    Rect r;
    GetDialogItem(d, item, &type, &h, &r);
    PenSize(3, 3);
    FrameRoundRect(&r, 16, 16);
    PenNormal();
}

static void InstallOutline(DialogPtr d, short item)
{
    short type;
    Handle h;
    Rect r;
    GetDialogItem(d, item, &type, &h, &r);
    SetDialogItem(d, item, type, (Handle)OutlineDefault, &r);
}

static void CenterWindow(WindowPtr w)
{
    Rect scr = qd.screenBits.bounds;
    Rect r = w->portRect;
    short width = r.right - r.left, height = r.bottom - r.top;
    short left = (scr.right - scr.left - width) / 2;
    short top = 20 + (scr.bottom - scr.top - 20 - height) / 3;
    if (top < 28) top = 28;
    MoveWindow(w, left, top, false);
}

static void SetItemCText(DialogPtr d, short item, const char *s)
{
    short type;
    Handle h;
    Rect r;
    Str255 p;
    GetDialogItem(d, item, &type, &h, &r);
    CToPStr(s, p);
    SetDialogItemText(h, p);
}

static void GetItemCText(DialogPtr d, short item, char *s, short max)
{
    short type;
    Handle h;
    Rect r;
    Str255 p;
    GetDialogItem(d, item, &type, &h, &r);
    GetDialogItemText(h, p);
    PToCStr(p, s, max);
}

static void SetCtl(DialogPtr d, short item, short value)
{
    short type;
    Handle h;
    Rect r;
    GetDialogItem(d, item, &type, &h, &r);
    SetControlValue((ControlHandle)h, value);
}

static short GetCtl(DialogPtr d, short item)
{
    short type;
    Handle h;
    Rect r;
    GetDialogItem(d, item, &type, &h, &r);
    return GetControlValue((ControlHandle)h);
}

/* Balloon Help for a dialog comes from the 'hdlg' resource with the
   dialog's ID. Retro68's Rez template writes DITL help items in a form
   the Dialog Manager can't read, so the help item is added at run time
   with AppendDITL (System 7 and later). */
static Boolean HasHelpMgr(void)
{
    LONGINT r;
    return SafeGestalt('help', &r) == noErr && (r & 1);
}

static void AddHelpItem(DialogPtr d, short hdlgID)
{
    Handle h = NewHandleClear(20);
    unsigned char *p;
    if (!h) return;
    p = (unsigned char *)*h;
    /* count-1 = 0; one item: reserved long, empty rect, then
       type (helpItem, disabled), data length 4, HMScanhdlg (1), resource ID */
    p[14] = 0x81;
    p[15] = 4;
    p[17] = 1;
    p[18] = (unsigned char)(hdlgID >> 8);
    p[19] = (unsigned char)hdlgID;
    AppendDITL(d, h, overlayDITL);
    DisposeHandle(h);
}

static DialogPtr OpenHelpDialog(short id)
{
    DialogPtr d = GetNewDialog(id, NULL, (WindowPtr)-1L);
    if (d && HasHelpMgr()) AddHelpItem(d, id);
    return d;
}

static void CloseHelpDialog(DialogPtr d)
{
    DisposeDialog(d);
}

static void Trim(char *s)
{
    char *p = s;
    long n;
    while (*p == ' ') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n > 0 && s[n - 1] == ' ') s[--n] = 0;
}

Boolean DoConnectDialog(void)
{
    DialogPtr d;
    short item;
    char buf[160], portStr[16];
    Boolean ok = false;
    GrafPtr save;

    GetPort(&save);
    d = OpenHelpDialog(kConnectDLOG);
    if (!d) {
        SysBeep(1);
        return false;
    }
    CenterWindow(d);
    InstallOutline(d, kConnOutline);

    sprintf(portStr, "%d", gPrefs.port);
    SetItemCText(d, kConnServer, gPrefs.server);
    SetItemCText(d, kConnPort, portStr);
    SetItemCText(d, kConnNick, gPrefs.nick);
    SetItemCText(d, kConnReal, gPrefs.realname);
    SetItemCText(d, kConnChans, gPrefs.channels);
    SetItemCText(d, kConnPass, gPrefs.password);
    SetItemCText(d, kConnNickServ, gPrefs.nickservPass);
    SelectDialogItemText(d, kConnServer, 0, 32767);
    ShowWindow(d);
    SetPort(d);

    for (;;) {
        ModalDialog(NULL, &item);
        if (item == kConnCancel) break;
        if (item == kConnOK) {
            char server[128], nick[kMaxNick];
            long port;

            GetItemCText(d, kConnServer, server, sizeof(server));
            Trim(server);
            GetItemCText(d, kConnNick, nick, sizeof(nick));
            Trim(nick);
            GetItemCText(d, kConnPort, buf, sizeof(buf));
            port = atol(buf);

            if (!server[0]) {
                SysBeep(1);
                SelectDialogItemText(d, kConnServer, 0, 32767);
                continue;
            }
            if (!nick[0] || strchr(nick, ' ')) {
                SysBeep(1);
                SelectDialogItemText(d, kConnNick, 0, 32767);
                continue;
            }
            if (port <= 0 || port > 65535) {
                SysBeep(1);
                SelectDialogItemText(d, kConnPort, 0, 32767);
                continue;
            }

            strcpy(gPrefs.server, server);
            strcpy(gPrefs.nick, nick);
            gPrefs.port = (short)port;
            GetItemCText(d, kConnReal, gPrefs.realname, sizeof(gPrefs.realname));
            GetItemCText(d, kConnChans, gPrefs.channels, sizeof(gPrefs.channels));
            GetItemCText(d, kConnPass, gPrefs.password, sizeof(gPrefs.password));
            GetItemCText(d, kConnNickServ, gPrefs.nickservPass, sizeof(gPrefs.nickservPass));
            Prefs_Save();
            ok = true;
            break;
        }
    }
    CloseHelpDialog(d);
    SetPort(save);
    return ok;
}

Boolean DoPrompt(const char *prompt, char *buf, short max)
{
    DialogPtr d;
    short item;
    Str255 p;
    Boolean ok = false;
    GrafPtr save;

    GetPort(&save);
    CToPStr(prompt, p);
    ParamText(p, "\p", "\p", "\p");
    d = GetNewDialog(kPromptDLOG, NULL, (WindowPtr)-1L);
    if (!d) {
        SysBeep(1);
        return false;
    }
    CenterWindow(d);
    InstallOutline(d, kPromptOutline);
    SetItemCText(d, kPromptText, buf);
    SelectDialogItemText(d, kPromptText, 0, 32767);
    ShowWindow(d);
    SetPort(d);

    for (;;) {
        ModalDialog(NULL, &item);
        if (item == kPromptCancel) break;
        if (item == kPromptOK) {
            GetItemCText(d, kPromptText, buf, max);
            Trim(buf);
            ok = true;
            break;
        }
    }
    DisposeDialog(d);
    SetPort(save);
    return ok;
}

static void SetFontRadios(DialogPtr d, short font)
{
    SetCtl(d, kPrefMonaco, font == kFontIDMonaco);
    SetCtl(d, kPrefGeneva, font == kFontIDGeneva);
    SetCtl(d, kPrefCourier, font == kFontIDCourier);
}

static void SetSizeRadios(DialogPtr d, short size)
{
    SetCtl(d, kPref9, size == 9);
    SetCtl(d, kPref10, size == 10);
    SetCtl(d, kPref12, size == 12);
}

Boolean DoPrefsDialog(void)
{
    DialogPtr d;
    short item, font = gPrefs.fontID, size = gPrefs.fontSize;
    Boolean ok = false;
    GrafPtr save;

    GetPort(&save);
    d = OpenHelpDialog(kPrefsDLOG);
    if (!d) {
        SysBeep(1);
        return false;
    }
    CenterWindow(d);
    InstallOutline(d, kPrefOutline);
    if (font != kFontIDMonaco && font != kFontIDGeneva && font != kFontIDCourier) font = kFontIDMonaco;
    if (size != 9 && size != 10 && size != 12) size = 9;
    SetFontRadios(d, font);
    SetSizeRadios(d, size);
    SetCtl(d, kPrefStamps, gPrefs.showTimestamps);
    SetCtl(d, kPrefBeep, gPrefs.beepOnMention);
    SetCtl(d, kPrefReconnect, gPrefs.autoReconnect);
    SetCtl(d, kPrefRejoin, gPrefs.rejoinChannels);
    SetCtl(d, kPrefLog, gPrefs.logging);
    SetCtl(d, kPrefNotify, gPrefs.notifyBackground);
    SetItemCText(d, kPrefHighlight, gPrefs.highlightWords);
    SetItemCText(d, kPrefQuit, gPrefs.quitMsg);
    SetItemCText(d, kPrefAway, gPrefs.awayMsg);
    SelectDialogItemText(d, kPrefHighlight, 0, 32767);
    ShowWindow(d);
    SetPort(d);

    for (;;) {
        ModalDialog(NULL, &item);
        switch (item) {
        case kPrefMonaco:  font = kFontIDMonaco;  SetFontRadios(d, font); break;
        case kPrefGeneva:  font = kFontIDGeneva;  SetFontRadios(d, font); break;
        case kPrefCourier: font = kFontIDCourier; SetFontRadios(d, font); break;
        case kPref9:  size = 9;  SetSizeRadios(d, size); break;
        case kPref10: size = 10; SetSizeRadios(d, size); break;
        case kPref12: size = 12; SetSizeRadios(d, size); break;
        case kPrefStamps: case kPrefBeep: case kPrefReconnect:
        case kPrefRejoin: case kPrefLog: case kPrefNotify:
            SetCtl(d, item, !GetCtl(d, item));
            break;
        }
        if (item == kPrefCancel) break;
        if (item == kPrefOK) {
            Boolean fontChanged = font != gPrefs.fontID || size != gPrefs.fontSize;
            Boolean logChanged = (GetCtl(d, kPrefLog) != 0) != (gPrefs.logging != 0);
            gPrefs.fontID = font;
            gPrefs.fontSize = size;
            gPrefs.showTimestamps = GetCtl(d, kPrefStamps);
            gPrefs.beepOnMention = GetCtl(d, kPrefBeep);
            gPrefs.autoReconnect = GetCtl(d, kPrefReconnect);
            gPrefs.rejoinChannels = GetCtl(d, kPrefRejoin);
            gPrefs.logging = GetCtl(d, kPrefLog);
            gPrefs.notifyBackground = GetCtl(d, kPrefNotify);
            GetItemCText(d, kPrefHighlight, gPrefs.highlightWords, sizeof(gPrefs.highlightWords));
            GetItemCText(d, kPrefQuit, gPrefs.quitMsg, sizeof(gPrefs.quitMsg));
            GetItemCText(d, kPrefAway, gPrefs.awayMsg, sizeof(gPrefs.awayMsg));
            Prefs_Save();
            CloseHelpDialog(d);
            SetPort(save);
            if (fontChanged) Win_ApplyFont();
            if (logChanged) Log_ApplyPrefs();
            return true;
        }
    }
    CloseHelpDialog(d);
    SetPort(save);
    return ok;
}

Boolean ConfirmPaste(short lines, const char *target)
{
    Str255 n, t;
    char num[8];
    sprintf(num, "%d", lines);
    CToPStr(num, n);
    CToPStr(target, t);
    ParamText(n, t, "\p", "\p");
    return CautionAlert(kPasteALRT, NULL) == 1;
}

void DoAbout(void)
{
    Alert(kAboutALRT, NULL);
}

void ErrorAlert(const char *msg)
{
    Str255 p;
    CToPStr(msg, p);
    ParamText(p, "\p", "\p", "\p");
    StopAlert(kErrorALRT, NULL);
}
