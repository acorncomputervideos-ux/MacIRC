/*
 * MacIRC.h - shared declarations for MacIRC, an IRC client for 68k Macs.
 */
#ifndef MACIRC_H
#define MACIRC_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>
#include <TextEdit.h>
#include <Menus.h>
#include <Dialogs.h>

#define kAppName      "MacIRC"
#define kAppVersion   "1.0"
#define kCreator      'MIRC'

/* ---- Resource IDs ---- */
enum {
    kMenuBarID   = 128,
    kAppleMenu   = 128,
    kFileMenu    = 129,
    kEditMenu    = 130,
    kIRCMenu     = 131,
    kWindowsMenu = 132
};

enum { iAbout = 1 };
enum { iConnect = 1, iDisconnect = 2, iClose = 4, iQuit = 6 };
enum { iUndo = 1, iCut = 3, iCopy = 4, iPaste = 5, iClear = 6, iSelectAll = 8 };
enum { iJoin = 1, iPart = 2, iNick = 3, iQuery = 4, iClearWin = 6 };

enum {
    kConnectDLOG = 128,
    kPromptDLOG  = 129,
    kAboutALRT   = 130,
    kErrorALRT   = 131
};

/* ---- Limits ---- */
#define kMaxNick      32
#define kMaxName      64
#define kInputMax     400
#define kHistoryMax   20

/* ---- Preferences ---- */
#define kPrefsVersion 0x4D490001L

typedef struct Prefs {
    long  version;
    char  server[128];
    short port;
    char  nick[kMaxNick];
    char  realname[64];
    char  channels[128];
    char  password[64];
    Rect  winBounds;       /* last server window position/size */
} Prefs;

extern Prefs gPrefs;

void Prefs_Load(void);
void Prefs_Save(void);

/* ---- Windows ---- */
enum { kWinServer, kWinChannel, kWinQuery };

/* Line classes (control colour/style and activity marking) */
enum {
    kLineMsg,        /* <nick> text            */
    kLineSelf,       /* our own messages       */
    kLineAction,     /* * nick does something  */
    kLineNotice,     /* -nick- text            */
    kLineEvent,      /* joins, parts, quits    */
    kLineInfo,       /* server info, topics    */
    kLineError,      /* errors                 */
    kLineHighlight   /* message mentioning us  */
};

typedef struct NickEntry {
    char mode;              /* '~','&','@','%','+' or ' ' */
    char nick[kMaxNick - 1];
} NickEntry;

typedef struct IRCWin {
    WindowPtr       win;
    short           kind;
    char            name[kMaxName];
    TEHandle        out;       /* styled, read-only scrollback */
    TEHandle        in;        /* input line */
    ControlHandle   vscroll;
    ListHandle      list;      /* channel nick list, NULL otherwise */
    NickEntry     **nicks;
    short           nNicks;
    Boolean         listDirty;
    Boolean         namesPending;
    Boolean         joined;
    Boolean         focusOut;  /* output TE has keyboard focus (for copy) */
    short           activity;  /* 0 none, 1 new messages, 2 mentioned */
    struct IRCWin  *next;
} IRCWin;

extern IRCWin *gWinList;
extern Boolean gInBackground;

IRCWin *Win_Create(short kind, const char *name);
void    Win_Dispose(IRCWin *w);
IRCWin *Win_FromWindow(WindowPtr w);
IRCWin *Win_Find(const char *name);
IRCWin *Win_Server(void);
IRCWin *Win_Front(void);
void    Win_Rename(IRCWin *w, const char *name);
void    Win_AddLine(IRCWin *w, short cls, const char *who, const char *text);
void    Win_Clear(IRCWin *w);
void    Win_Update(IRCWin *w);
void    Win_Activate(IRCWin *w, Boolean active);
void    Win_Click(IRCWin *w, EventRecord *ev);
void    Win_Key(IRCWin *w, EventRecord *ev);
void    Win_Grow(IRCWin *w, Point where);
void    Win_Zoom(IRCWin *w, Point where, short part);
void    Win_IdleAll(void);
void    Win_AdjustCursor(Point globalPt);
void    Win_Copy(IRCWin *w);
void    Win_Cut(IRCWin *w);
void    Win_Paste(IRCWin *w);
void    Win_ClearSel(IRCWin *w);
void    Win_SelectAll(IRCWin *w);
void    Win_RebuildMenu(void);
void    Win_SelectFromMenu(short item);
void    Win_ClearActivity(IRCWin *w);

/* Nick list */
void    Win_NickAdd(IRCWin *w, const char *nickWithPrefix);
Boolean Win_NickRemove(IRCWin *w, const char *nick);
Boolean Win_NickRename(IRCWin *w, const char *oldNick, const char *newNick);
Boolean Win_HasNick(IRCWin *w, const char *nick);
void    Win_NickSetMode(IRCWin *w, const char *nick, char modeChar, Boolean add);
void    Win_NickClear(IRCWin *w);

/* ---- IRC ---- */
void    IRC_Connect(void);
void    IRC_Disconnect(const char *quitMsg);
void    IRC_Poll(void);
void    IRC_Input(IRCWin *w, const char *line);
void    IRC_SendLine(const char *line);
Boolean IRC_IsConnected(void);    /* TCP connected or connecting */
Boolean IRC_IsActive(void);       /* anything network-related in flight */
const char *IRC_MyNick(void);
void    IRC_PartWindow(IRCWin *w, const char *reason);

/* ---- Networking (tcp.c) ---- */
enum { kNetIdle, kNetResolving, kNetOpening, kNetConnected };
enum { kNetEvtNone, kNetEvtResolved, kNetEvtConnected, kNetEvtFailed, kNetEvtClosed };

OSErr   Net_Init(void);
void    Net_Shutdown(void);
OSErr   Net_Connect(const char *host, unsigned short port);
short   Net_Poll(OSErr *errOut);
long    Net_Read(char *buf, long max);
OSErr   Net_Send(const char *data, long len);
void    Net_Close(Boolean graceful);
short   Net_State(void);
unsigned long Net_RemoteAddr(void);
const char *Net_ErrString(OSErr err);
void    Net_AddrToStr(unsigned long addr, char *out);

/* ---- DNR (dnr.c) ---- */
OSErr   DNR_Open(void);
void    DNR_Close(void);
OSErr   DNR_Lookup(const char *host);    /* starts async lookup */
Boolean DNR_Done(OSErr *err, unsigned long *addr);
Boolean DNR_Busy(void);

/* ---- Character sets (charset.c) ---- */
void    UTF8ToMac(const char *in, char *out, long outMax);
void    MacToUTF8(const char *in, char *out, long outMax);
void    StripFormatting(char *s);

/* ---- Dialogs (dialogs.c) ---- */
Boolean DoConnectDialog(void);
Boolean DoPrompt(const char *prompt, char *buf, short max);
void    DoAbout(void);
void    ErrorAlert(const char *msg);

/* ---- Utilities (util.c) ---- */
void    CToPStr(const char *c, Str255 p);
void    PToCStr(ConstStr255Param p, char *c, short max);
void    StrNCopy(char *dst, const char *src, long max);
int     IRCStrCmp(const char *a, const char *b);
int     IRCStrNCmp(const char *a, const char *b, long n);
Boolean IRCStrStr(const char *hay, const char *needle);
char    IRCToLower(char c);
Boolean IsChannelName(const char *s);
Boolean TrapAvailable(unsigned short trap);
OSErr   SafeGestalt(OSType selector, LONGINT *response);

/* ---- main.c ---- */
void    Notify_Attention(void);
void    Main_AdjustMenus(void);

#endif
