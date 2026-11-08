/*
 * main.c - MacIRC: an IRC client for classic 68k Macintosh computers.
 *
 * Toolbox initialisation, the event loop, and menu dispatch.
 */
#include <string.h>
#include <stdio.h>
#include <Memory.h>
#include <Events.h>
#include <Menus.h>
#include <Devices.h>
#include <Fonts.h>
#include <ToolUtils.h>
#include <Resources.h>
#include <OSUtils.h>
#include <Processes.h>
#include "MacIRC.h"

Boolean        gInBackground = false;
static Boolean gQuit = false;
static Boolean gHasWNE = false;
static NMRec   gNotify;
static Boolean gNotifyPosted = false;

/* ------------------------------------------------------------------ */
/* Notification Manager: flag the application menu when someone talks
   to us while we're in the background.                                */

void Notify_Attention(void)
{
    if (!gInBackground || gNotifyPosted) return;
    if (!TrapAvailable(0xA05E)) return;          /* _NMInstall */
    memset(&gNotify, 0, sizeof(gNotify));
    gNotify.qType = 8;                           /* nmType */
    gNotify.nmMark = 1;
    gNotify.nmIcon = NULL;
    gNotify.nmSound = (Handle)-1L;               /* system alert sound */
    gNotify.nmStr = NULL;
    gNotify.nmResp = NULL;
    if (NMInstall(&gNotify) == noErr) gNotifyPosted = true;
}

static void CancelNotify(void)
{
    if (gNotifyPosted) {
        NMRemove(&gNotify);
        gNotifyPosted = false;
    }
}

/* ------------------------------------------------------------------ */
/* Menus                                                               */

static void SetupMenus(void)
{
    Handle mbar = GetNewMBar(kMenuBarID);
    SetMenuBar(mbar);
    DisposeHandle(mbar);
    AppendResMenu(GetMenuHandle(kAppleMenu), 'DRVR');
    DrawMenuBar();
}

static void SetEnabled(MenuHandle m, short item, Boolean on)
{
    if (on) EnableItem(m, item);
    else DisableItem(m, item);
}

void Main_AdjustMenus(void)
{
    WindowPtr front = FrontWindow();
    IRCWin *w = Win_FromWindow(front);
    Boolean isDA = front && ((WindowPeek)front)->windowKind < 0;
    Boolean online = Net_State() == kNetConnected;
    MenuHandle m;

    m = GetMenuHandle(kFileMenu);
    SetEnabled(m, iDisconnect, IRC_IsConnected());
    SetEnabled(m, iClose, isDA || (w && w->kind != kWinServer));

    m = GetMenuHandle(kEditMenu);
    if (isDA) {
        EnableItem(m, 0);
        EnableItem(m, iUndo);
        EnableItem(m, iCut);
        EnableItem(m, iCopy);
        EnableItem(m, iPaste);
        EnableItem(m, iClear);
        DisableItem(m, iSelectAll);
    } else {
        Boolean sel = false, inSel = false;
        if (w) {
            TEHandle te = w->focusOut ? w->out : w->in;
            sel = (*te)->selStart != (*te)->selEnd;
            inSel = !w->focusOut && sel;
        }
        DisableItem(m, iUndo);
        SetEnabled(m, iCut, inSel);
        SetEnabled(m, iCopy, sel);
        SetEnabled(m, iPaste, w != NULL);
        SetEnabled(m, iClear, inSel);
        SetEnabled(m, iSelectAll, w != NULL);
    }

    m = GetMenuHandle(kIRCMenu);
    SetEnabled(m, iJoin, online);
    SetEnabled(m, iPart, online && w && w->kind == kWinChannel && w->joined);
    SetEnabled(m, iNick, true);
    SetEnabled(m, iQuery, true);
    SetEnabled(m, iClearWin, w != NULL);
}

static void CloseFrontWindow(void)
{
    WindowPtr front = FrontWindow();
    IRCWin *w;
    if (!front) return;
    if (((WindowPeek)front)->windowKind < 0) {
        CloseDeskAcc(((WindowPeek)front)->windowKind);
        return;
    }
    w = Win_FromWindow(front);
    if (!w) return;
    if (w->kind == kWinServer) {
        SysBeep(1);   /* the server window stays open; use Quit */
        return;
    }
    IRC_PartWindow(w, NULL);
    Win_Dispose(w);
}

static void DoMenu(long result)
{
    short menu = HiWord(result), item = LoWord(result);
    IRCWin *w = Win_Front();
    IRCWin *target = w ? w : Win_Server();
    char buf[kMaxName + 16], cmd[kMaxName + 24];

    switch (menu) {
    case kAppleMenu:
        if (item == iAbout) {
            DoAbout();
        } else {
            Str255 name;
            GetMenuItemText(GetMenuHandle(kAppleMenu), item, name);
            OpenDeskAcc(name);
        }
        break;

    case kFileMenu:
        switch (item) {
        case iConnect:
            if (DoConnectDialog()) IRC_Connect();
            break;
        case iDisconnect:
            IRC_Disconnect(NULL);
            break;
        case iClose:
            CloseFrontWindow();
            break;
        case iQuit:
            gQuit = true;
            break;
        }
        break;

    case kEditMenu:
        if (SystemEdit(item - 1)) break;
        if (!w) break;
        switch (item) {
        case iCut:       Win_Cut(w); break;
        case iCopy:      Win_Copy(w); break;
        case iPaste:     Win_Paste(w); break;
        case iClear:     Win_ClearSel(w); break;
        case iSelectAll: Win_SelectAll(w); break;
        }
        break;

    case kIRCMenu:
        switch (item) {
        case iJoin:
            buf[0] = 0;
            if (DoPrompt("Join which channel?", buf, sizeof(buf))) {
                sprintf(cmd, "/join %s", buf);
                IRC_Input(target, cmd);
            }
            break;
        case iPart:
            if (w && w->kind == kWinChannel) IRC_Input(w, "/part");
            break;
        case iNick:
            StrNCopy(buf, IRC_IsConnected() ? IRC_MyNick() : gPrefs.nick, kMaxNick);
            if (DoPrompt("New nickname:", buf, kMaxNick)) {
                if (Net_State() == kNetConnected) {
                    sprintf(cmd, "/nick %s", buf);
                    IRC_Input(target, cmd);
                } else {
                    StrNCopy(gPrefs.nick, buf, sizeof(gPrefs.nick));
                    Prefs_Save();
                }
            }
            break;
        case iQuery:
            buf[0] = 0;
            if (DoPrompt("Start a private conversation with:", buf, kMaxNick)) {
                sprintf(cmd, "/query %s", buf);
                IRC_Input(target, cmd);
            }
            break;
        case iClearWin:
            if (w) Win_Clear(w);
            break;
        }
        break;

    case kWindowsMenu:
        Win_SelectFromMenu(item);
        break;
    }
    HiliteMenu(0);
}

/* ------------------------------------------------------------------ */
/* Events                                                              */

static void DoMouseDown(EventRecord *ev)
{
    WindowPtr wp;
    IRCWin *w;
    short part = FindWindow(ev->where, &wp);

    switch (part) {
    case inMenuBar:
        Main_AdjustMenus();
        DoMenu(MenuSelect(ev->where));
        break;
    case inSysWindow:
        SystemClick(ev, wp);
        break;
    case inDrag: {
        Rect limit = qd.screenBits.bounds;
        InsetRect(&limit, 4, 4);
        DragWindow(wp, ev->where, &limit);
        break;
    }
    case inContent:
        if (wp != FrontWindow()) SelectWindow(wp);
        else if ((w = Win_FromWindow(wp)) != NULL) Win_Click(w, ev);
        break;
    case inGrow:
        if ((w = Win_FromWindow(wp)) != NULL) Win_Grow(w, ev->where);
        break;
    case inGoAway:
        if (TrackGoAway(wp, ev->where)) {
            if (wp != FrontWindow()) SelectWindow(wp);
            CloseFrontWindow();
        }
        break;
    case inZoomIn:
    case inZoomOut:
        if ((w = Win_FromWindow(wp)) != NULL) Win_Zoom(w, ev->where, part);
        break;
    }
}

static void DoEvent(EventRecord *ev)
{
    IRCWin *w;
    char c;

    switch (ev->what) {
    case mouseDown:
        DoMouseDown(ev);
        break;

    case keyDown:
    case autoKey:
        c = ev->message & charCodeMask;
        if (ev->modifiers & cmdKey) {
            if (ev->what == keyDown) {
                Main_AdjustMenus();
                DoMenu(MenuKey(c));
            }
        } else if ((w = Win_Front()) != NULL) {
            Win_Key(w, ev);
        }
        break;

    case updateEvt:
        if ((w = Win_FromWindow((WindowPtr)ev->message)) != NULL)
            Win_Update(w);
        break;

    case activateEvt:
        if ((w = Win_FromWindow((WindowPtr)ev->message)) != NULL)
            Win_Activate(w, (ev->modifiers & activeFlag) != 0);
        break;

    case diskEvt:
        if (HiWord(ev->message) != noErr) {
            Point pt;
            pt.h = 100;
            pt.v = 120;
            DIBadMount(pt, ev->message);
        }
        break;

    case osEvt:
        if (((ev->message >> 24) & 0xFF) == 1) {          /* suspend/resume */
            gInBackground = (ev->message & 1) == 0;
            if ((w = Win_Front()) != NULL)
                Win_Activate(w, !gInBackground);
            if (!gInBackground) CancelNotify();
        }
        break;
    }
}

static void Welcome(IRCWin *s)
{
    OSErr err;
    Win_AddLine(s, kLineInfo, NULL, "MacIRC " kAppVersion " \xD1 an IRC client for classic Macintosh.");
    Win_AddLine(s, kLineInfo, NULL, "Choose Connect\xC9 from the File menu (Command-K) to get started. Type /help for commands.");
    err = Net_Init();
    if (err != noErr) {
        char msg[128];
        sprintf(msg, "MacTCP is not available (%s). Install MacTCP or Open Transport to connect.",
                Net_ErrString(err));
        Win_AddLine(s, kLineError, NULL, msg);
    }
}

int main(void)
{
    IRCWin *s;
    short i;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);
    MaxApplZone();
    for (i = 0; i < 8; i++) MoreMasters();

    gHasWNE = TrapAvailable(0xA860);   /* _WaitNextEvent */

    Prefs_Load();
    SetupMenus();
    s = Win_Create(kWinServer, "MacIRC");
    if (!s) {
        SysBeep(1);
        return 1;
    }
    Welcome(s);

    while (!gQuit) {
        EventRecord ev;
        Boolean got;
        long sleep;

        if (Net_State() != kNetIdle) sleep = gInBackground ? 4 : 1;
        else sleep = 20;

        if (gHasWNE) {
            got = WaitNextEvent(everyEvent, &ev, sleep, NULL);
        } else {
            SystemTask();
            got = GetNextEvent(everyEvent, &ev);
        }
        if (got) DoEvent(&ev);
        if (!got || ev.what == nullEvent) Win_AdjustCursor(ev.where);

        IRC_Poll();
        Win_IdleAll();
    }

    IRC_Disconnect("MacIRC " kAppVersion " \xD1 classic Mac IRC");
    while (gWinList) {
        IRCWin *last = gWinList;
        while (last->next) last = last->next;
        Win_Dispose(last);    /* the server window, disposed last, saves its bounds */
    }
    Prefs_Save();
    Net_Shutdown();
    return 0;
}
