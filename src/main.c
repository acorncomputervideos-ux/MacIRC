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
#include <Files.h>
#include <Fonts.h>
#include <ToolUtils.h>
#include <Resources.h>
#include <OSUtils.h>
#include <Processes.h>
#include "MacIRC.h"

Boolean        gInBackground = false;
Boolean        gHasColorQD = false;
short          gAppVRef = 0;
long           gAppDirID = 0;
static Boolean gQuit = false;
static Boolean gHasWNE = false;
static NMRec   gNotify;
static Boolean gNotifyPosted = false;

/* ------------------------------------------------------------------ */
/* Notification Manager: flag the application menu when someone talks
   to us while we're in the background.                                */

void Notify_Attention(void)
{
    if (!gInBackground || gNotifyPosted || !gPrefs.notifyBackground) return;
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
    Boolean isList = ChanList_IsWindow(front);
    Boolean online = IRC_IsRegistered();
    MenuHandle m;

    m = GetMenuHandle(kFileMenu);
    SetEnabled(m, iDisconnect, IRC_IsConnected() || IRC_ReconnectPending());
    SetEnabled(m, iClose, isDA || isList || (w && w->kind != kWinServer));

    m = GetMenuHandle(kEditMenu);
    if (isDA) {
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
    EnableItem(m, iPrefs);

    m = GetMenuHandle(kIRCMenu);
    SetEnabled(m, iJoin, online);
    SetEnabled(m, iPart, online && w && w->kind == kWinChannel && w->joined);
    SetEnabled(m, iNick, true);
    SetEnabled(m, iQuery, true);
    SetEnabled(m, iList, online);
    SetEnabled(m, iTopic, online && w && w->kind == kWinChannel && w->joined);
    SetEnabled(m, iAway, online);
    SetMenuItemText(m, iAway, IRC_IsAway() ? "\pI'm Back" : "\pSet Away\xC9");
    SetEnabled(m, iIgnore, true);
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
    if (ChanList_IsWindow(front)) {
        ChanList_Close();
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
    char buf[260], cmd[300];

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
        if (item == iPrefs) {
            DoPrefsDialog();
            break;
        }
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
            if (DoPrompt("Join which channel?", buf, kMaxName) && buf[0]) {
                sprintf(cmd, "/join %s", buf);
                IRC_Input(target, cmd);
            }
            break;
        case iPart:
            if (w && w->kind == kWinChannel) IRC_Input(w, "/part");
            break;
        case iNick:
            StrNCopy(buf, IRC_IsConnected() ? IRC_MyNick() : gPrefs.nick, kMaxNick);
            if (DoPrompt("New nickname:", buf, kMaxNick) && buf[0]) {
                if (IRC_IsRegistered()) {
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
            if (DoPrompt("Start a private conversation with:", buf, kMaxNick) && buf[0]) {
                sprintf(cmd, "/query %s", buf);
                IRC_Input(target, cmd);
            }
            break;
        case iList:
            strcpy(buf, "10");
            if (DoPrompt("List channels with at least this many users:", buf, 8)) {
                sprintf(cmd, "/list %s", buf[0] ? buf : "0");
                IRC_Input(Win_Server(), cmd);
            }
            break;
        case iTopic:
            if (w && w->kind == kWinChannel) {
                char prompt[kMaxName + 16];
                StrNCopy(buf, w->topic, 250);
                sprintf(prompt, "Topic for %s:", w->name);
                if (DoPrompt(prompt, buf, 250)) {
                    sprintf(cmd, "/topic %.250s", buf);
                    IRC_Input(w, cmd);
                }
            }
            break;
        case iAway:
            if (IRC_IsAway()) {
                IRC_Input(target, "/back");
            } else {
                StrNCopy(buf, gPrefs.awayMsg, 200);
                if (DoPrompt("Away message:", buf, 200)) {
                    sprintf(cmd, "/away %s", buf[0] ? buf : "Away");
                    IRC_Input(target, cmd);
                }
            }
            break;
        case iIgnore:
            StrNCopy(buf, gPrefs.ignoreList, 250);
            if (DoPrompt("Ignore these nicknames or nick!user@host masks (separate with spaces):",
                         buf, 250)) {
                StrNCopy(gPrefs.ignoreList, buf, sizeof(gPrefs.ignoreList));
                Prefs_Save();
            }
            break;
        case iClearWin:
            if (w) Win_Clear(w);
            break;
        }
        break;

    case kWindowsMenu:
        if (item == iTile) Win_Tile();
        else if (item == iCascade) Win_Cascade();
        else Win_SelectFromMenu(item);
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
        else if (ChanList_IsWindow(wp)) ChanList_Click(ev);
        else if ((w = Win_FromWindow(wp)) != NULL) Win_Click(w, ev);
        break;
    case inGrow:
        if (ChanList_IsWindow(wp)) ChanList_Grow(ev->where);
        else if ((w = Win_FromWindow(wp)) != NULL) Win_Grow(w, ev->where);
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
        if (ChanList_IsWindow((WindowPtr)ev->message))
            ChanList_Update();
        else if ((w = Win_FromWindow((WindowPtr)ev->message)) != NULL)
            Win_Update(w);
        break;

    case activateEvt:
        if (ChanList_IsWindow((WindowPtr)ev->message))
            ChanList_Activate((ev->modifiers & activeFlag) != 0);
        else if ((w = Win_FromWindow((WindowPtr)ev->message)) != NULL)
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
            else if (ChanList_IsWindow(FrontWindow()))
                ChanList_Activate(!gInBackground);
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
    if (err == -23004 || err == -23001 || err == -23002) {
        Win_AddLine(s, kLineError, NULL,
                    "TCP/IP has no IP address yet. Set one up in the MacTCP (or TCP/IP) control panel, then restart.");
    } else if (err != noErr) {
        char msg[128];
        sprintf(msg, "MacTCP is not available (%s). Install MacTCP or Open Transport to connect.",
                Net_ErrString(err));
        Win_AddLine(s, kLineError, NULL, msg);
    }
}

static void FindAppFolder(void)
{
    WDPBRec pb;
    memset(&pb, 0, sizeof(pb));
    if (PBHGetVolSync(&pb) == noErr) {
        gAppVRef = pb.ioWDVRefNum;
        gAppDirID = pb.ioWDDirID;
    }
}

int main(void)
{
    IRCWin *s;
    short i;
    SysEnvRec env;

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
    if (SysEnvirons(1, &env) == noErr) gHasColorQD = env.hasColorQD;
    FindAppFolder();

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

        if (IRC_IsActive()) sleep = gInBackground ? 4 : 1;
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
        Log_Idle();
    }

    IRC_RememberChannels();
    IRC_Disconnect(NULL);
    ChanList_Close();
    while (gWinList) {
        IRCWin *last = gWinList;
        while (last->next) last = last->next;
        Win_Dispose(last);    /* the server window, disposed last, saves its bounds */
    }
    Prefs_Save();
    Net_Shutdown();
    return 0;
}
