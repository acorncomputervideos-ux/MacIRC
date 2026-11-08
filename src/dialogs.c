/*
 * dialogs.c - Connect dialog, single-field prompt, About box, error alert.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <Dialogs.h>
#include <Events.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include "MacIRC.h"

/* Connect dialog items */
enum {
    kConnOK = 1, kConnCancel = 2, kConnOutline = 3,
    kConnServer = 5, kConnPort = 7, kConnNick = 9,
    kConnReal = 11, kConnChans = 13, kConnPass = 15
};

/* Prompt dialog items */
enum { kPromptOK = 1, kPromptCancel = 2, kPromptOutline = 3, kPromptText = 5 };

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
    if (top < 40) top = 40;
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
    d = GetNewDialog(kConnectDLOG, NULL, (WindowPtr)-1L);
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
            Prefs_Save();
            ok = true;
            break;
        }
    }
    DisposeDialog(d);
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
            ok = buf[0] != 0;
            break;
        }
    }
    DisposeDialog(d);
    SetPort(save);
    return ok;
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
