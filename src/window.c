/*
 * window.c - IRC windows: topic bar, scrollback (styled TextEdit), input
 * line, scroll bar, and a channel nick list (List Manager).
 */
#include <string.h>
#include <stdio.h>
#include <Memory.h>
#include <LowMem.h>
#include <Events.h>
#include <Fonts.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <Resources.h>
#include <Gestalt.h>
#include <Drag.h>
#include "MacIRC.h"

#define kZoomDocProc  8
#define kListW        112
#define kTrimAt       28000L
#define kTrimChunk    8000L

IRCWin  *gWinList = NULL;

static short         gLineH = 11, gAscent = 9;
static StScrpHandle  gStyleScrap = NULL;
static short         gWinCount = 0;
static char          gPrefixChars[8] = "~&@%+";
static char          gPrefixModes[8] = "qaohv";
static short         gHasDrag = -1;
static Boolean       gDragHilite = false;

static char  gHistory[kHistoryMax][kInputMax + 1];
static short gHistCount = 0;
static short gHistPos = 0;

/* Colours from the standard 16-colour palette, so they look right at
   every screen depth instead of being rounded to black. */
static const RGBColor kBlack  = { 0x0000, 0x0000, 0x0000 };
static const RGBColor kGray   = { 0x8080, 0x8080, 0x8080 };
static const RGBColor kGreen  = { 0x0000, 0x6464, 0x1111 };
static const RGBColor kBlue   = { 0x0000, 0x0000, 0xD4D4 };
static const RGBColor kRed    = { 0xDDDD, 0x0808, 0x0606 };
static const RGBColor kPurple = { 0x4646, 0x0000, 0xA5A5 };
static const RGBColor kBrown  = { 0x5656, 0x2C2C, 0x0505 };

static void SetupDrag(IRCWin *w);
static void RemoveDrag(IRCWin *w);

/* ------------------------------------------------------------------ */
/* Lookup                                                              */

IRCWin *Win_FromWindow(WindowPtr wp)
{
    IRCWin *w;
    if (!wp) return NULL;
    for (w = gWinList; w; w = w->next)
        if (w->win == wp) return w;
    return NULL;
}

IRCWin *Win_Front(void)
{
    return Win_FromWindow(FrontWindow());
}

IRCWin *Win_Find(const char *name)
{
    IRCWin *w;
    for (w = gWinList; w; w = w->next)
        if (w->kind != kWinServer && IRCStrCmp(w->name, name) == 0) return w;
    return NULL;
}

IRCWin *Win_Server(void)
{
    IRCWin *w;
    for (w = gWinList; w; w = w->next)
        if (w->kind == kWinServer) return w;
    return NULL;
}

static Boolean Win_IsValid(IRCWin *target)
{
    IRCWin *w;
    for (w = gWinList; w; w = w->next)
        if (w == target) return true;
    return false;
}

void Win_GetLineMetrics(short *lineH, short *ascent)
{
    *lineH = gLineH;
    *ascent = gAscent;
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */

typedef struct WinLayout {
    Rect  topic;    /* topic bar (channels), empty otherwise */
    Rect  out;      /* scrollback text area */
    Rect  sb;       /* scrollback scroll bar */
    Rect  list;     /* nick list rView (channels) */
    Rect  in;       /* input text area */
    short divY;     /* divider between scrollback and input */
} WinLayout;

static void ComputeMetrics(void)
{
    FontInfo fi;
    TextFont(gPrefs.fontID);
    TextSize(gPrefs.fontSize);
    GetFontInfo(&fi);
    gLineH = fi.ascent + fi.descent + fi.leading;
    gAscent = fi.ascent;
    if (!gStyleScrap) gStyleScrap = (StScrpHandle)NewHandle(sizeof(StScrpRec));
}

static void ComputeLayout(IRCWin *w, WinLayout *L)
{
    Rect r = w->win->portRect;
    short inputH = 2 * gLineH + 8;
    short listLeft = r.right;
    short top = r.top;

    if (w->kind == kWinChannel) {
        SetRect(&L->topic, r.left, r.top, r.right, r.top + gLineH + 6);
        top = L->topic.bottom;
    } else {
        SetRect(&L->topic, 0, 0, 0, 0);
    }
    L->divY = r.bottom - inputH;
    if (w->kind == kWinChannel) {
        listLeft = r.right - kListW;
        SetRect(&L->list, listLeft + 1, top, r.right - 15, L->divY);
    } else {
        SetRect(&L->list, 0, 0, 0, 0);
    }
    SetRect(&L->sb, listLeft - 15, top - 1, listLeft + 1, L->divY + 1);
    SetRect(&L->out, r.left + 4, top + 2, listLeft - 17, L->divY - 2);
    SetRect(&L->in, r.left + 4, L->divY + 4, r.right - 19, L->divY + 4 + 2 * gLineH);
}

static short VisLines(IRCWin *w)
{
    Rect *v = &(*w->out)->viewRect;
    short n = (v->bottom - v->top) / gLineH;
    return n < 1 ? 1 : n;
}

static short TopLine(IRCWin *w)
{
    TEPtr t = *w->out;
    return (t->viewRect.top - t->destRect.top) / gLineH;
}

static short MaxTop(IRCWin *w)
{
    short n = (*w->out)->nLines, vis = VisLines(w);
    if ((*w->out)->teLength == 0) n = 0;
    return n > vis ? n - vis : 0;
}

static void ScrollOutTo(IRCWin *w, short line)
{
    short cur = TopLine(w);
    short max = MaxTop(w);
    if (line > max) line = max;
    if (line < 0) line = 0;
    if (line != cur)
        TEScroll(0, (cur - line) * gLineH, w->out);
    SetControlValue(w->vscroll, line);
}

static void UpdateScroll(IRCWin *w, Boolean toBottom)
{
    short max = MaxTop(w);
    SetControlMaximum(w->vscroll, max);
    ScrollOutTo(w, toBottom ? max : TopLine(w));
}

static void DrawGrowBox(IRCWin *w)
{
    Rect r = w->win->portRect;
    RgnHandle save = NewRgn();
    GetClip(save);
    r.left = r.right - 15;
    r.top = r.bottom - 15;
    ClipRect(&r);
    DrawGrowIcon(w->win);
    SetClip(save);
    DisposeRgn(save);
}

static void DrawTopic(IRCWin *w, WinLayout *L)
{
    Rect r = L->topic;
    RgnHandle save;
    const char *label = "Topic: ";

    if (w->kind != kWinChannel) return;
    r.bottom -= 1;
    EraseRect(&r);
    save = NewRgn();
    GetClip(save);
    r.right -= 4;
    ClipRect(&r);
    MoveTo(r.left + 4, r.top + 3 + gAscent);
    TextFace(bold);
    DrawText((Ptr)label, 0, strlen(label));
    TextFace(normal);
    if (w->topic[0]) {
        DrawText(w->topic, 0, strlen(w->topic));
    } else {
        const char *none = w->joined ? "(none)" : "(not in channel)";
        if (gHasColorQD) RGBForeColor((RGBColor *)&kGray);
        DrawText((Ptr)none, 0, strlen(none));
        ForeColor(blackColor);
    }
    SetClip(save);
    DisposeRgn(save);
    MoveTo(L->topic.left, L->topic.bottom - 1);
    LineTo(L->topic.right, L->topic.bottom - 1);
}

/* ---- nick list drawing ---- */

static short PrefixRank(char c)
{
    char *p = c ? strchr(gPrefixChars, c) : NULL;
    return p ? (short)(p - gPrefixChars) : 99;
}

static pascal void NickLDEF(short msg, Boolean sel, Rect *r, Cell cell,
                            short off, short len, ListHandle lh)
{
    IRCWin *w = (IRCWin *)(*lh)->refCon;
    (void)off; (void)len;

    if (msg == lHiliteMsg) {
        InvertRect(r);
        return;
    }
    if (msg != lDrawMsg) return;
    EraseRect(r);
    if (w && w->nicks && cell.v >= 0 && cell.v < w->nNicks) {
        NickEntry e = (*w->nicks)[cell.v];
        char buf[kMaxNick + 2];
        short n = 0;
        if (e.modes[0]) buf[n++] = e.modes[0];
        StrNCopy(buf + n, e.nick, sizeof(buf) - n);
        MoveTo(r->left + 3, r->top + gAscent);
        if (e.away) {
            if (gHasColorQD) RGBForeColor((RGBColor *)&kGray);
            else TextFace(italic);
        }
        DrawText(buf, 0, strlen(buf));
        ForeColor(blackColor);
        TextFace(normal);
    }
    if (sel) InvertRect(r);
}

static void CreateList(IRCWin *w, const Rect *rView)
{
    Rect bounds;
    Point cSize;
    Handle stub;
    SetRect(&bounds, 0, 0, 1, 0);
    cSize.h = 0;
    cSize.v = 0;
    w->list = LNew(rView, &bounds, cSize, 0, w->win, true, false, false, true);
    if (w->list) {
        (*w->list)->selFlags = lOnlyOne;
        (*w->list)->refCon = (long)w;
        stub = MakeLDEFStub((ProcPtr)NickLDEF);
        if (stub) (*w->list)->listDefProc = stub;
        w->listDirty = true;
    }
}

static void Relayout(IRCWin *w)
{
    WinLayout L;
    short oldTop;
    Boolean atBottom;
    GrafPtr save;

    GetPort(&save);
    SetPort(w->win);
    atBottom = TopLine(w) >= MaxTop(w);
    oldTop = TopLine(w);

    ComputeLayout(w, &L);
    EraseRect(&w->win->portRect);
    InvalRect(&w->win->portRect);

    (*w->out)->viewRect = L.out;
    (*w->out)->destRect = L.out;
    TECalText(w->out);
    SetControlValue(w->vscroll, 0);
    SetControlMaximum(w->vscroll, MaxTop(w));
    ScrollOutTo(w, atBottom ? MaxTop(w) : oldTop);

    (*w->in)->viewRect = L.in;
    (*w->in)->destRect = L.in;
    TECalText(w->in);
    TESelView(w->in);

    MoveControl(w->vscroll, L.sb.left, L.sb.top);
    SizeControl(w->vscroll, L.sb.right - L.sb.left, L.sb.bottom - L.sb.top);

    if (w->kind == kWinChannel) {
        if (w->list) LDispose(w->list);
        CreateList(w, &L.list);
    }
    SetPort(save);
}

/* ------------------------------------------------------------------ */
/* Creation / disposal                                                 */

static void DefaultBounds(IRCWin *w, Rect *b)
{
    Rect scr = qd.screenBits.bounds;
    short width = scr.right - scr.left - 16;
    short height = scr.bottom - scr.top - 20 - 18 - 12;
    short off;

    if (width > 580) width = 580;
    if (height > 360) height = 360;

    if (w->kind == kWinServer) {
        Rect *p = &gPrefs.winBounds;
        if (p->right - p->left >= 200 && p->bottom - p->top >= 100 &&
            p->left >= scr.left && p->top >= scr.top + 20 &&
            p->right <= scr.right && p->bottom <= scr.bottom) {
            *b = *p;
            return;
        }
        off = 0;
    } else {
        off = 16 + 14 * (gWinCount % 5);
    }
    SetRect(b, 8 + off, 20 + 22 + off, 8 + off + width, 20 + 22 + off + height);
    if (b->right > scr.right - 2) OffsetRect(b, scr.right - 2 - b->right, 0);
    if (b->bottom > scr.bottom - 2) OffsetRect(b, 0, scr.bottom - 2 - b->bottom);
}

IRCWin *Win_Create(short kind, const char *name)
{
    IRCWin *w, **link;
    Rect bounds;
    Str255 title;
    WinLayout L;
    GrafPtr save;

    w = (IRCWin *)NewPtr(sizeof(IRCWin));
    if (!w) return NULL;
    memset(w, 0, sizeof(IRCWin));
    w->kind = kind;
    StrNCopy(w->name, name, sizeof(w->name));

    DefaultBounds(w, &bounds);
    CToPStr(name, title);
    /* A colour port is needed for styled text colours: on an old-style
       port, RGBForeColor snaps to the 8 basic QuickDraw colours. */
    if (gHasColorQD)
        w->win = NewCWindow(NULL, &bounds, title, false, kZoomDocProc,
                            (WindowPtr)-1L, kind != kWinServer, (long)w);
    else
        w->win = NewWindow(NULL, &bounds, title, false, kZoomDocProc,
                           (WindowPtr)-1L, kind != kWinServer, (long)w);
    if (!w->win) {
        DisposePtr((Ptr)w);
        return NULL;
    }

    GetPort(&save);
    SetPort(w->win);
    ComputeMetrics();
    ComputeLayout(w, &L);

    w->out = TEStyleNew(&L.out, &L.out);
    TEDeactivate(w->out);
    w->in = TENew(&L.in, &L.in);
    TEAutoView(true, w->in);
    w->vscroll = NewControl(w->win, &L.sb, "\p", true, 0, 0, 0, scrollBarProc, (long)w);
    if (kind == kWinChannel) {
        w->nicks = (NickEntry **)NewHandle(0);
        CreateList(w, &L.list);
    }

    /* append to the end of the window list so the menu keeps its order */
    for (link = &gWinList; *link; link = &(*link)->next) ;
    *link = w;
    gWinCount++;

    SetupDrag(w);
    Log_Open(w);
    ShowWindow(w->win);
    SelectWindow(w->win);
    SetPort(save);
    Win_RebuildMenu();
    return w;
}

void Win_Dispose(IRCWin *w)
{
    IRCWin **link;
    for (link = &gWinList; *link; link = &(*link)->next) {
        if (*link == w) {
            *link = w->next;
            break;
        }
    }
    if (w->kind == kWinServer) {
        /* remember where the server window was, in global coordinates */
        GrafPtr save;
        Point tl, br;
        GetPort(&save);
        SetPort(w->win);
        tl.h = w->win->portRect.left;  tl.v = w->win->portRect.top;
        br.h = w->win->portRect.right; br.v = w->win->portRect.bottom;
        LocalToGlobal(&tl);
        LocalToGlobal(&br);
        SetRect(&gPrefs.winBounds, tl.h, tl.v, br.h, br.v);
        SetPort(save);
    }
    Log_Close(w);
    RemoveDrag(w);
    TEDispose(w->out);
    TEDispose(w->in);
    if (w->list) LDispose(w->list);
    if (w->nicks) DisposeHandle((Handle)w->nicks);
    DisposeWindow(w->win);
    DisposePtr((Ptr)w);
    {
        /* never leave the current port pointing at a disposed window */
        WindowPtr f = FrontWindow();
        if (f) SetPort(f);
    }
    Win_RebuildMenu();
}

void Win_Rename(IRCWin *w, const char *name)
{
    Str255 title;
    StrNCopy(w->name, name, sizeof(w->name));
    CToPStr(name, title);
    SetWTitle(w->win, title);
    Win_RebuildMenu();
}

void Win_SetTitle(IRCWin *w, const char *title)
{
    Str255 t, cur;
    CToPStr(title, t);
    GetWTitle(w->win, cur);
    if (cur[0] == t[0] && memcmp(cur + 1, t + 1, t[0]) == 0) return;
    SetWTitle(w->win, t);
}

void Win_SetTopic(IRCWin *w, const char *topic)
{
    WinLayout L;
    GrafPtr save;
    if (!w || w->kind != kWinChannel) return;
    StrNCopy(w->topic, topic, sizeof(w->topic));
    GetPort(&save);
    SetPort(w->win);
    ComputeLayout(w, &L);
    InvalRect(&L.topic);
    SetPort(save);
}

/* ------------------------------------------------------------------ */
/* Output                                                              */

static void InsertStyled(TEHandle te, const char *s, long n, short face, const RGBColor *color)
{
    ScrpSTElement *e;
    if (n <= 0) return;
    (*gStyleScrap)->scrpNStyles = 1;
    e = &(*gStyleScrap)->scrpStyleTab[0];
    e->scrpStartChar = 0;
    e->scrpHeight = gLineH;
    e->scrpAscent = gAscent;
    e->scrpFont = gPrefs.fontID;
    e->scrpFace = (Style)face;
    e->scrpSize = gPrefs.fontSize;
    e->scrpColor = *color;
    TEStyleInsert((Ptr)s, n, gStyleScrap, te);
}

static void TrimScrollback(IRCWin *w, short *ss, short *se)
{
    TEHandle te = w->out;
    long len = (*te)->teLength, cut;
    char *p;

    if (len < kTrimAt) return;
    p = *(*te)->hText;
    for (cut = kTrimChunk; cut < len && p[cut] != '\r'; cut++) ;
    if (cut >= len) return;
    cut++;
    TESetSelect(0, cut, te);
    TEDelete(te);
    *ss -= cut;
    *se -= cut;
    if (*ss < 0 || *se < 0) *ss = *se = 0;
}

void Win_AddLine(IRCWin *w, short cls, const char *who, const char *text)
{
    TEHandle te;
    GrafPtr save;
    Boolean atBottom;
    short ss, se;
    long len;
    char stamp[32], logLine[700];
    const RGBColor *color = &kBlack;
    short face = normal;
    char pre[kMaxNick + 8];

    if (!w) return;
    te = w->out;
    GetPort(&save);
    SetPort(w->win);

    atBottom = TopLine(w) >= MaxTop(w);
    ss = (*te)->selStart;
    se = (*te)->selEnd;
    TrimScrollback(w, &ss, &se);

    len = (*te)->teLength;
    TESetSelect(len, len, te);

    stamp[0] = 0;
    if (gPrefs.showTimestamps || w->logRef) {
        unsigned long secs;
        DateTimeRec dt;
        GetDateTime(&secs);
        SecondsToDate(secs, &dt);
        sprintf(stamp, "[%02d:%02d] ", dt.hour, dt.minute);
    }
    if (len > 0) InsertStyled(te, "\r", 1, normal, &kGray);
    if (gPrefs.showTimestamps)
        InsertStyled(te, stamp, strlen(stamp), normal, &kGray);

    switch (cls) {
    case kLineMsg:       color = &kBlack;  break;
    case kLineSelf:      color = &kBlue;   break;
    case kLineAction:    color = &kPurple; break;
    case kLineNotice:    color = &kBrown;  break;
    case kLineEvent:     color = &kGreen;  break;
    case kLineInfo:      color = &kGray;   break;
    case kLineError:     color = &kRed;    break;
    case kLineHighlight: color = &kRed;    break;
    }

    pre[0] = 0;
    if (who) {
        switch (cls) {
        case kLineAction: sprintf(pre, "* %s ", who); break;
        case kLineNotice: sprintf(pre, "-%s- ", who); break;
        default:          sprintf(pre, "<%s> ", who); break;
        }
        InsertStyled(te, pre, strlen(pre), bold, color);
    } else if (cls == kLineError || cls == kLineInfo || cls == kLineEvent) {
        strcpy(pre, "*** ");
        InsertStyled(te, pre, 4, normal, color);
    }
    if (cls == kLineHighlight) face = bold;
    InsertStyled(te, text, strlen(text), face, color);

    if (ss != se) TESetSelect(ss, se, te);
    UpdateScroll(w, atBottom);

    if (w->logRef) {
        sprintf(logLine, "%s%s%.600s", stamp, pre, text);
        Log_Write(w, logLine);
    }

    /* Activity marks for the Windows menu */
    if ((w->win != FrontWindow() || gInBackground) &&
        (cls == kLineMsg || cls == kLineAction || cls == kLineNotice || cls == kLineHighlight)) {
        short level = (cls == kLineHighlight || w->kind == kWinQuery) ? 2 : 1;
        if (level > w->activity) {
            w->activity = level;
            Win_RebuildMenu();
        }
    }
    SetPort(save);
}

void Win_Clear(IRCWin *w)
{
    GrafPtr save;
    GetPort(&save);
    SetPort(w->win);
    TESetSelect(0, 32767, w->out);
    TEDelete(w->out);
    UpdateScroll(w, true);
    SetPort(save);
}

void Win_ClearActivity(IRCWin *w)
{
    if (w && w->activity) {
        w->activity = 0;
        Win_RebuildMenu();
    }
}

/* Font or size changed in Preferences: restyle every window. */
void Win_ApplyFont(void)
{
    IRCWin *w;
    GrafPtr save;
    TextStyle ts;

    GetPort(&save);
    for (w = gWinList; w; w = w->next) {
        short ss, se;
        SetPort(w->win);
        ComputeMetrics();
        ts.tsFont = gPrefs.fontID;
        ts.tsSize = gPrefs.fontSize;
        ss = (*w->out)->selStart;
        se = (*w->out)->selEnd;
        TESetSelect(0, 32767, w->out);
        TESetStyle(doFont + doSize, &ts, false, w->out);
        TESetSelect(ss, se, w->out);
        (*w->in)->txFont = gPrefs.fontID;
        (*w->in)->txSize = gPrefs.fontSize;
        (*w->in)->lineHeight = gLineH;
        (*w->in)->fontAscent = gAscent;
        Relayout(w);
    }
    SetPort(save);
}

/* ------------------------------------------------------------------ */
/* Events                                                              */

void Win_Update(IRCWin *w)
{
    WinLayout L;
    GrafPtr save;

    GetPort(&save);
    SetPort(w->win);
    BeginUpdate(w->win);
    ComputeLayout(w, &L);
    EraseRect(&w->win->portRect);
    DrawTopic(w, &L);
    TEUpdate(&w->win->portRect, w->out);
    TEUpdate(&w->win->portRect, w->in);
    MoveTo(w->win->portRect.left, L.divY);
    LineTo(w->win->portRect.right, L.divY);
    DrawControls(w->win);
    if (w->list) LUpdate(w->win->visRgn, w->list);
    DrawGrowBox(w);
    EndUpdate(w->win);
    SetPort(save);
}

static void SetFocus(IRCWin *w, Boolean toOut)
{
    if (w->focusOut == toOut) return;
    w->focusOut = toOut;
    if (toOut) {
        TEDeactivate(w->in);
        TEActivate(w->out);
    } else {
        TEDeactivate(w->out);
        TEActivate(w->in);
    }
}

void Win_Activate(IRCWin *w, Boolean active)
{
    GrafPtr save;
    GetPort(&save);
    SetPort(w->win);
    if (active) {
        if (w->focusOut) TEActivate(w->out); else TEActivate(w->in);
        HiliteControl(w->vscroll, 0);
        w->activity = 0;
    } else {
        TEDeactivate(w->out);
        TEDeactivate(w->in);
        HiliteControl(w->vscroll, 255);
    }
    if (w->list) LActivate(active, w->list);
    DrawGrowBox(w);
    SetPort(save);
    Win_RebuildMenu();
}

static pascal void ScrollAction(ControlHandle c, short part)
{
    IRCWin *w = (IRCWin *)(*c)->contrlRfCon;
    short delta, vis;
    if (!w) return;
    vis = VisLines(w);
    switch (part) {
    case inUpButton:   delta = -1; break;
    case inDownButton: delta = 1; break;
    case inPageUp:     delta = -(vis > 1 ? vis - 1 : 1); break;
    case inPageDown:   delta = (vis > 1 ? vis - 1 : 1); break;
    default: return;
    }
    ScrollOutTo(w, GetControlValue(c) + delta);
}

static void OpenQueryFromList(IRCWin *w)
{
    long last = LLastClick(w->list);
    short row = HiWord(last);
    char nick[kMaxNick];
    IRCWin *q;

    if (row < 0 || row >= w->nNicks) return;
    StrNCopy(nick, (*w->nicks)[row].nick, sizeof(nick));
    if (!nick[0]) return;
    q = Win_Find(nick);
    if (q) SelectWindow(q->win);
    else Win_Create(kWinQuery, nick);
}

static void EditTopic(IRCWin *w)
{
    char buf[kMaxTopic], prompt[kMaxName + 32], cmd[kMaxTopic + 16];
    if (!w->joined || !IRC_IsRegistered()) {
        SysBeep(1);
        return;
    }
    StrNCopy(buf, w->topic, 250);   /* the edit field holds a Str255 */
    sprintf(prompt, "Topic for %s:", w->name);
    if (DoPrompt(prompt, buf, 250)) {
        sprintf(cmd, "/topic %s", buf);
        IRC_Input(w, cmd);
    }
}

/* Cmd-click on a URL in the scrollback opens it. */
static Boolean ClickURL(IRCWin *w, Point pt)
{
    TEHandle te = w->out;
    short off = TEGetOffset(pt, te);
    long len = (*te)->teLength, s, e;
    char url[300];

    if (!URL_FindAt(*(*te)->hText, len, off, &s, &e)) return false;
    if (e - s > (long)sizeof(url) - 1) e = s + sizeof(url) - 1;
    BlockMove(*(*te)->hText + s, url, e - s);
    url[e - s] = 0;
    URL_Open(url);
    return true;
}

/* ---- Drag Manager: drag text out of the scrollback, drop text into the
        input line. Requires System 7.5 or the Macintosh Drag and Drop
        extension. ---- */

static Boolean HasDrag(void)
{
    if (gHasDrag < 0) {
        LONGINT resp;
        gHasDrag = (SafeGestalt('drag', &resp) == noErr && (resp & 1)) ? 1 : 0;
    }
    return gHasDrag == 1;
}

static Boolean DragHasText(DragRef drag)
{
    UInt16 n, i;
    DragItemRef item;
    Size size;
    if (CountDragItems(drag, &n) != noErr) return false;
    for (i = 1; i <= n; i++) {
        if (GetDragItemReferenceNumber(drag, i, &item) != noErr) continue;
        if (GetFlavorDataSize(drag, item, 'TEXT', &size) == noErr) return true;
    }
    return false;
}

static pascal OSErr DragTrack(DragTrackingMessage msg, WindowPtr win, void *refCon, DragRef drag)
{
    IRCWin *w = (IRCWin *)refCon;
    (void)win;
    switch (msg) {
    case kDragTrackingEnterWindow:
        gDragHilite = false;
        break;
    case kDragTrackingInWindow:
        if (!gDragHilite && DragHasText(drag)) {
            RgnHandle rgn = NewRgn();
            WinLayout L;
            GrafPtr save;
            GetPort(&save);
            SetPort(w->win);
            ComputeLayout(w, &L);
            InsetRect(&L.in, -3, -3);
            RectRgn(rgn, &L.in);
            ShowDragHilite(drag, rgn, true);
            DisposeRgn(rgn);
            SetPort(save);
            gDragHilite = true;
        }
        break;
    case kDragTrackingLeaveWindow:
        if (gDragHilite) HideDragHilite(drag);
        gDragHilite = false;
        break;
    }
    return noErr;
}

static pascal OSErr DragReceive(WindowPtr win, void *refCon, DragRef drag)
{
    IRCWin *w = (IRCWin *)refCon;
    UInt16 n, i;
    DragItemRef item;
    Size size, total = 0;
    char buf[kInputMax * 2 + 1];
    GrafPtr save;
    long room;
    (void)win;

    if (gDragHilite) HideDragHilite(drag);
    gDragHilite = false;
    if (CountDragItems(drag, &n) != noErr) return dragNotAcceptedErr;
    for (i = 1; i <= n; i++) {
        if (GetDragItemReferenceNumber(drag, i, &item) != noErr) continue;
        if (GetFlavorDataSize(drag, item, 'TEXT', &size) != noErr) continue;
        if (total + size > (Size)sizeof(buf) - 1) size = sizeof(buf) - 1 - total;
        if (size <= 0) break;
        if (GetFlavorData(drag, item, 'TEXT', buf + total, &size, 0) == noErr)
            total += size;
    }
    if (total == 0) return dragNotAcceptedErr;

    GetPort(&save);
    SetPort(w->win);
    SetFocus(w, false);
    room = kInputMax * 2 - ((*w->in)->teLength - ((*w->in)->selEnd - (*w->in)->selStart));
    if (total > room) total = room > 0 ? room : 0;
    if (total > 0) {
        TEInsert(buf, total, w->in);
        TESelView(w->in);
    }
    SetPort(save);
    return noErr;
}

static void SetupDrag(IRCWin *w)
{
    if (!HasDrag()) return;
    InstallTrackingHandler((DragTrackingHandlerUPP)DragTrack, w->win, w);
    InstallReceiveHandler((DragReceiveHandlerUPP)DragReceive, w->win, w);
}

static void RemoveDrag(IRCWin *w)
{
    if (!HasDrag()) return;
    RemoveTrackingHandler((DragTrackingHandlerUPP)DragTrack, w->win);
    RemoveReceiveHandler((DragReceiveHandlerUPP)DragReceive, w->win);
}

static short LineOf(TEHandle te, short off)
{
    short i, n = (*te)->nLines;
    for (i = 0; i < n - 1; i++)
        if ((*te)->lineStarts[i + 1] > off) return i;
    return n > 0 ? n - 1 : 0;
}

static void DragOutSelection(IRCWin *w, EventRecord *ev)
{
    TEHandle te = w->out;
    DragRef drag;
    RgnHandle rgn, inner;
    Rect r;
    Point origin;
    short ss = (*te)->selStart, se = (*te)->selEnd;
    short first, last;

    if (NewDrag(&drag) != noErr) return;
    HLock((*te)->hText);
    AddDragItemFlavor(drag, 1, 'TEXT', *(*te)->hText + ss, se - ss, 0);
    HUnlock((*te)->hText);

    first = LineOf(te, ss);
    last = LineOf(te, se > ss ? se - 1 : se);
    r = (*te)->viewRect;
    r.top = (*te)->destRect.top + first * gLineH;
    r.bottom = (*te)->destRect.top + (last + 1) * gLineH;
    if (r.top < (*te)->viewRect.top) r.top = (*te)->viewRect.top;
    if (r.bottom > (*te)->viewRect.bottom) r.bottom = (*te)->viewRect.bottom;

    rgn = NewRgn();
    inner = NewRgn();
    RectRgn(rgn, &r);
    origin.h = 0;
    origin.v = 0;
    LocalToGlobal(&origin);
    OffsetRgn(rgn, origin.h, origin.v);
    CopyRgn(rgn, inner);
    InsetRgn(inner, 1, 1);
    DiffRgn(rgn, inner, rgn);

    TrackDrag(drag, ev, rgn);
    DisposeRgn(inner);
    DisposeRgn(rgn);
    DisposeDrag(drag);
}

void Win_Click(IRCWin *w, EventRecord *ev)
{
    Point pt = ev->where;
    Boolean shift = (ev->modifiers & shiftKey) != 0;
    ControlHandle ctl;
    short part;
    GrafPtr save;
    WinLayout L;

    GetPort(&save);
    SetPort(w->win);
    GlobalToLocal(&pt);
    ComputeLayout(w, &L);

    if (w->kind == kWinChannel && PtInRect(pt, &L.topic)) {
        EditTopic(w);
    } else if (PtInRect(pt, &(*w->in)->viewRect)) {
        SetFocus(w, false);
        TEClick(pt, shift, w->in);
    } else if (PtInRect(pt, &(*w->out)->viewRect)) {
        TEHandle te = w->out;
        if ((ev->modifiers & cmdKey) && ClickURL(w, pt)) {
            /* opened */
        } else if (HasDrag() && !shift && (*te)->selStart != (*te)->selEnd &&
                   TEGetOffset(pt, te) >= (*te)->selStart &&
                   TEGetOffset(pt, te) < (*te)->selEnd &&
                   WaitMouseMoved(ev->where)) {
            DragOutSelection(w, ev);
        } else {
            SetFocus(w, true);
            TEClick(pt, shift, te);
        }
    } else if (w->list) {
        Rect lr = (*w->list)->rView;
        lr.right += 15;
        if (PtInRect(pt, &lr)) {
            if (LClick(pt, ev->modifiers, w->list))
                OpenQueryFromList(w);
            SetPort(save);
            return;
        }
    }

    part = FindControl(pt, w->win, &ctl);
    if (part && ctl == w->vscroll) {
        if (part == inThumb) {
            TrackControl(ctl, pt, NULL);
            ScrollOutTo(w, GetControlValue(ctl));
        } else {
            TrackControl(ctl, pt, (ControlActionUPP)ScrollAction);
        }
    }
    if (Win_IsValid(w)) SetPort(save);
}

static void HistoryAdd(const char *line)
{
    short i;
    if (!*line) return;
    if (gHistCount > 0 && strcmp(gHistory[gHistCount - 1], line) == 0) {
        gHistPos = gHistCount;
        return;
    }
    if (gHistCount == kHistoryMax) {
        for (i = 1; i < kHistoryMax; i++)
            strcpy(gHistory[i - 1], gHistory[i]);
        gHistCount--;
    }
    StrNCopy(gHistory[gHistCount++], line, kInputMax + 1);
    gHistPos = gHistCount;
}

static void SetInputText(IRCWin *w, const char *s)
{
    TESetSelect(0, 32767, w->in);
    TEDelete(w->in);
    TEInsert((Ptr)s, strlen(s), w->in);
    TESetSelect(32767, 32767, w->in);
    TESelView(w->in);
}

static void Submit(IRCWin *w)
{
    char buf[kInputMax * 2 + 2];
    long n = (*w->in)->teLength;
    char *line, *p;
    short lines = 0;

    if (n == 0) return;
    if (n > (long)sizeof(buf) - 1) n = sizeof(buf) - 1;
    BlockMove(*(*w->in)->hText, buf, n);
    buf[n] = 0;

    /* Pasted or dropped text may hold several lines: confirm first */
    for (p = buf; *p; ) {
        char *e = strchr(p, '\r');
        if (e != p && *p != '/') lines++;
        if (!e) break;
        p = e + 1;
    }
    if (lines > 2 && !ConfirmPaste(lines, w->kind == kWinServer ? "the server" : w->name))
        return;

    TESetSelect(0, 32767, w->in);
    TEDelete(w->in);
    HistoryAdd(buf);

    line = buf;
    while (line && *line) {
        p = strchr(line, '\r');
        if (p) *p++ = 0;
        if (*line) {
            IRC_Input(w, line);
            if (!Win_IsValid(w)) return;   /* e.g. /close */
        }
        line = p;
    }
}

static void CompleteNick(IRCWin *w)
{
    TEHandle in = w->in;
    short start, end = (*in)->selStart;
    char word[kMaxNick + 1], repl[kMaxName + 4];
    char match[kMaxName];
    char *text;
    short i, n;

    text = *(*in)->hText;
    for (start = end; start > 0 && text[start - 1] != ' '; start--) ;
    n = end - start;
    if (n <= 0 || n > kMaxNick) { SysBeep(1); return; }
    memcpy(word, text + start, n);
    word[n] = 0;

    match[0] = 0;
    if (w->kind == kWinChannel && w->nicks) {
        for (i = 0; i < w->nNicks; i++) {
            if (IRCStrNCmp((*w->nicks)[i].nick, word, n) == 0 &&
                IRCStrCmp((*w->nicks)[i].nick, IRC_MyNick()) != 0) {
                StrNCopy(match, (*w->nicks)[i].nick, sizeof(match));
                break;
            }
        }
    } else if (w->kind == kWinQuery && IRCStrNCmp(w->name, word, n) == 0) {
        StrNCopy(match, w->name, sizeof(match));
    }
    if (!match[0]) { SysBeep(1); return; }

    sprintf(repl, "%s%s", match, start == 0 ? ": " : " ");
    TESetSelect(start, end, in);
    TEDelete(in);
    TEInsert(repl, strlen(repl), in);
    TESelView(in);
}

void Win_Key(IRCWin *w, EventRecord *ev)
{
    char c = ev->message & charCodeMask;
    TEHandle in = w->in;
    GrafPtr save;

    GetPort(&save);
    SetPort(w->win);
    if (w->focusOut) SetFocus(w, false);

    switch ((unsigned char)c) {
    case '\r':
    case 0x03:          /* Enter */
        Submit(w);
        if (!Win_IsValid(w)) return;
        break;
    case '\t':
        CompleteNick(w);
        break;
    case 0x1E:          /* up arrow: history */
        if (gHistPos > 0) {
            gHistPos--;
            SetInputText(w, gHistory[gHistPos]);
        } else SysBeep(1);
        break;
    case 0x1F:          /* down arrow */
        if (gHistPos < gHistCount - 1) {
            gHistPos++;
            SetInputText(w, gHistory[gHistPos]);
        } else {
            gHistPos = gHistCount;
            SetInputText(w, "");
        }
        break;
    case 0x0B:          /* page up */
        ScrollOutTo(w, TopLine(w) - (VisLines(w) - 1));
        break;
    case 0x0C:          /* page down */
        ScrollOutTo(w, TopLine(w) + (VisLines(w) - 1));
        break;
    case 0x01:          /* home */
        ScrollOutTo(w, 0);
        break;
    case 0x04:          /* end */
        ScrollOutTo(w, MaxTop(w));
        break;
    case 0x7F:          /* forward delete */
        if ((*in)->selStart == (*in)->selEnd) {
            if ((*in)->selEnd >= (*in)->teLength) break;
            TESetSelect((*in)->selStart, (*in)->selStart + 1, in);
        }
        TEDelete(in);
        break;
    default:
        if (c == 0x08 || c == 0x1C || c == 0x1D ||
            (*in)->teLength - ((*in)->selEnd - (*in)->selStart) < kInputMax)
            TEKey(c, in);
        else
            SysBeep(1);
        break;
    }
    SetPort(save);
}

void Win_Grow(IRCWin *w, Point where)
{
    Rect limits;
    long size;
    SetRect(&limits, 240, 120, 32767, 32767);
    size = GrowWindow(w->win, where, &limits);
    if (size) {
        SizeWindow(w->win, LoWord(size), HiWord(size), true);
        Relayout(w);
    }
}

/* The zoomed ("standard") size fills the main screen below the menu bar,
   whatever its size: 512x342 on a Classic up to large colour screens. */
static void SetStdState(WindowPtr win)
{
    WStateData **ws = (WStateData **)((WindowPeek)win)->dataHandle;
    Rect s = qd.screenBits.bounds;
    if (!ws) return;
    s.top += GetMBarHeight() + 19;
    s.left += 2;
    s.right -= 3;
    s.bottom -= 3;
    (*ws)->stdState = s;
}

void Win_Zoom(IRCWin *w, Point where, short part)
{
    GrafPtr save;
    if (!TrackBox(w->win, where, part)) return;
    GetPort(&save);
    SetPort(w->win);
    SetStdState(w->win);
    EraseRect(&w->win->portRect);
    ZoomWindow(w->win, part, false);
    Relayout(w);
    SetPort(save);
}

static void PlaceWindow(IRCWin *w, short left, short top, short width, short height)
{
    if (width < 240) width = 240;
    if (height < 100) height = 100;
    MoveWindow(w->win, left, top, false);
    SizeWindow(w->win, width, height, true);
    Relayout(w);
}

void Win_Tile(void)
{
    IRCWin *w;
    short n = 0, cols, rows, i, cellW, cellH;
    Rect scr = qd.screenBits.bounds;
    short top = scr.top + GetMBarHeight();

    for (w = gWinList; w; w = w->next) n++;
    if (n == 0) return;
    if (n <= 3) { cols = 1; rows = n; }
    else { cols = 2; rows = (n + 1) / 2; }
    if (rows > 4) { cols = 3; rows = (n + 2) / 3; }
    cellW = (scr.right - scr.left - 2) / cols;
    cellH = (scr.bottom - top - 2) / rows;
    for (w = gWinList, i = 0; w; w = w->next, i++) {
        short c = i % cols, r = i / cols;
        PlaceWindow(w, scr.left + 2 + c * cellW, top + r * cellH + 20,
                    cellW - 4, cellH - 22);
    }
}

void Win_Cascade(void)
{
    IRCWin *w;
    short i = 0;
    Rect scr = qd.screenBits.bounds;
    short top0 = scr.top + GetMBarHeight() + 20;

    for (w = gWinList; w; w = w->next, i++) {
        short left = 4 + 18 * i, top = top0 + 20 * i;
        short width = scr.right - left - 6, height = scr.bottom - top - 6;
        if (width > 580) width = 580;
        if (height > 360) height = 360;
        PlaceWindow(w, left, top, width, height);
        SelectWindow(w->win);
    }
}

/* ------------------------------------------------------------------ */
/* Nick list                                                           */

void Win_SetPrefixes(const char *modes, const char *chars)
{
    if (!*modes || strlen(modes) != strlen(chars) || strlen(chars) >= sizeof(gPrefixChars)) return;
    strcpy(gPrefixModes, modes);
    strcpy(gPrefixChars, chars);
}

char Win_PrefixForMode(char m)
{
    char *p = strchr(gPrefixModes, m);
    return (p && m) ? gPrefixChars[p - gPrefixModes] : 0;
}

static Boolean IsPrefixChar(char c)
{
    return c && strchr(gPrefixChars, c) != NULL;
}

static int NickOrder(const NickEntry *a, const NickEntry *b)
{
    short r1 = PrefixRank(a->modes[0]), r2 = PrefixRank(b->modes[0]);
    if (r1 != r2) return r1 - r2;
    return IRCStrCmp(a->nick, b->nick);
}

static short FindNick(IRCWin *w, const char *nick)
{
    short i;
    if (!w->nicks) return -1;
    for (i = 0; i < w->nNicks; i++)
        if (IRCStrCmp((*w->nicks)[i].nick, nick) == 0) return i;
    return -1;
}

static void RemoveAt(IRCWin *w, short i, NickEntry *out)
{
    NickEntry *p = *w->nicks;
    if (out) *out = p[i];
    if (i < w->nNicks - 1)
        BlockMove(&p[i + 1], &p[i], (w->nNicks - i - 1) * (long)sizeof(NickEntry));
    w->nNicks--;
    w->listDirty = true;
}

static void InsertSorted(IRCWin *w, const NickEntry *e)
{
    long need = (w->nNicks + 1) * (long)sizeof(NickEntry);
    short i;
    NickEntry *p;

    if (GetHandleSize((Handle)w->nicks) < need) {
        SetHandleSize((Handle)w->nicks, need + 32 * sizeof(NickEntry));
        if (MemError() != noErr) return;
    }
    p = *w->nicks;
    for (i = 0; i < w->nNicks; i++)
        if (NickOrder(e, &p[i]) < 0) break;
    if (i < w->nNicks)
        BlockMove(&p[i], &p[i + 1], (w->nNicks - i) * (long)sizeof(NickEntry));
    p[i] = *e;
    w->nNicks++;
    w->listDirty = true;
}

/* Keep a mode set ordered from highest to lowest prefix. */
static void SortModes(char *m)
{
    short i, j, n = strlen(m);
    for (i = 1; i < n; i++) {
        char c = m[i];
        for (j = i - 1; j >= 0 && PrefixRank(m[j]) > PrefixRank(c); j--)
            m[j + 1] = m[j];
        m[j + 1] = c;
    }
}

void Win_NickAdd(IRCWin *w, const char *s)
{
    NickEntry e;
    short i, n = 0;
    if (!w || !w->nicks) return;
    memset(&e, 0, sizeof(e));
    while (IsPrefixChar(*s)) {
        if (!strchr(e.modes, *s) && n < (short)sizeof(e.modes) - 1) e.modes[n++] = *s;
        s++;
    }
    if (!*s) return;
    SortModes(e.modes);
    StrNCopy(e.nick, s, sizeof(e.nick));
    i = FindNick(w, s);
    if (i >= 0) {
        NickEntry old;
        if (strcmp((*w->nicks)[i].modes, e.modes) == 0) return;
        RemoveAt(w, i, &old);
        e.away = old.away;
    }
    InsertSorted(w, &e);
}

Boolean Win_NickRemove(IRCWin *w, const char *nick)
{
    short i;
    if (!w || !w->nicks) return false;
    i = FindNick(w, nick);
    if (i < 0) return false;
    RemoveAt(w, i, NULL);
    return true;
}

Boolean Win_HasNick(IRCWin *w, const char *nick)
{
    return w && w->nicks && FindNick(w, nick) >= 0;
}

Boolean Win_NickRename(IRCWin *w, const char *oldNick, const char *newNick)
{
    short i;
    NickEntry e;
    if (!w || !w->nicks) return false;
    i = FindNick(w, oldNick);
    if (i < 0) return false;
    RemoveAt(w, i, &e);
    StrNCopy(e.nick, newNick, sizeof(e.nick));
    InsertSorted(w, &e);
    return true;
}

void Win_NickSetMode(IRCWin *w, const char *nick, char prefix, Boolean add)
{
    short i, n;
    NickEntry e;
    char *p;
    if (!w || !w->nicks || !prefix) return;
    i = FindNick(w, nick);
    if (i < 0) return;
    e = (*w->nicks)[i];
    p = strchr(e.modes, prefix);
    if (add) {
        if (p) return;
        n = strlen(e.modes);
        if (n >= (short)sizeof(e.modes) - 1) return;
        e.modes[n] = prefix;
        e.modes[n + 1] = 0;
        SortModes(e.modes);
    } else {
        if (!p) return;
        memmove(p, p + 1, strlen(p));
    }
    RemoveAt(w, i, NULL);
    InsertSorted(w, &e);
}

void Win_NickSetAway(IRCWin *w, const char *nick, Boolean away)
{
    short i;
    if (!w || !w->nicks) return;
    i = FindNick(w, nick);
    if (i < 0 || (*w->nicks)[i].away == away) return;
    (*w->nicks)[i].away = away;
    w->listDirty = true;
}

void Win_NickClear(IRCWin *w)
{
    if (!w || !w->nicks) return;
    w->nNicks = 0;
    w->listDirty = true;
}

static void RebuildList(IRCWin *w)
{
    GrafPtr save;
    Rect r;
    short rows;

    if (!w->list) return;
    GetPort(&save);
    SetPort(w->win);
    rows = (*w->list)->dataBounds.bottom;
    LSetDrawingMode(false, w->list);
    if (rows > w->nNicks) LDelRow(rows - w->nNicks, w->nNicks, w->list);
    else if (rows < w->nNicks) LAddRow(w->nNicks - rows, rows, w->list);
    LSetDrawingMode(true, w->list);
    r = (*w->list)->rView;
    r.right += 16;
    r.top -= 1;
    InvalRect(&r);
    w->listDirty = false;
    SetPort(save);
}

/* ------------------------------------------------------------------ */
/* Idle, cursor, menus, clipboard                                      */

void Win_IdleAll(void)
{
    IRCWin *w, *front = Win_Front();
    for (w = gWinList; w; w = w->next)
        if (w->listDirty) RebuildList(w);
    if (front && !gInBackground) {
        GrafPtr save;
        GetPort(&save);
        SetPort(front->win);
        TEIdle(front->focusOut ? front->out : front->in);
        SetPort(save);
    }
}

void Win_AdjustCursor(Point pt)
{
    static short current = -1;
    IRCWin *w = Win_Front();
    short want = 0;
    if (w && !gInBackground) {
        GrafPtr save;
        GetPort(&save);
        SetPort(w->win);
        GlobalToLocal(&pt);
        if (PtInRect(pt, &(*w->in)->viewRect) || PtInRect(pt, &(*w->out)->viewRect))
            want = 1;
        SetPort(save);
    }
    if (want != current) {
        current = want;
        if (want) {
            CursHandle h = GetCursor(iBeamCursor);
            if (h) SetCursor(*h);
        } else {
            SetCursor(&qd.arrow);
        }
    }
}

void Win_RebuildMenu(void)
{
    MenuHandle m = GetMenuHandle(kWindowsMenu);
    IRCWin *w;
    short i, n;
    WindowPtr front = FrontWindow();
    Str255 title;

    if (!m) return;
    n = CountMItems(m);
    for (i = n; i >= kFirstWindowItem; i--)
        DeleteMenuItem(m, i);
    for (w = gWinList, i = 1; w; w = w->next, i++) {
        char key[16];
        Str255 item;
        short idx = kFirstWindowItem + i - 1;
        if (i <= 9) sprintf(key, "x/%d", i);
        else strcpy(key, "x");
        CToPStr(key, item);
        AppendMenu(m, item);
        if (w->kind == kWinServer) strcpy((char *)title + 1, "Server");
        else StrNCopy((char *)title + 1, w->name, 254);
        title[0] = strlen((char *)title + 1);
        SetMenuItemText(m, idx, title);
        if (w->activity == 2) SetItemMark(m, idx, 0x13);        /* diamond */
        else if (w->activity == 1) SetItemMark(m, idx, 0xA5);   /* bullet */
        else if (w->win == front) SetItemMark(m, idx, 0x12);    /* check */
        else SetItemMark(m, idx, 0);
    }
}

void Win_SelectFromMenu(short item)
{
    IRCWin *w;
    short i;
    for (w = gWinList, i = kFirstWindowItem; w; w = w->next, i++) {
        if (i == item) {
            SelectWindow(w->win);
            return;
        }
    }
}

/* TEToScrap/TEFromScrap/TEGetScrapLength are link-time glue in Apple's
   libraries rather than traps, so they're implemented here. */
static void TEScrapToDesk(void)
{
    Handle h = LMGetTEScrpHandle();
    if (!h) return;
    HLock(h);
    PutScrap(LMGetTEScrpLength(), 'TEXT', *h);
    HUnlock(h);
}

static Boolean TEScrapFromDesk(void)
{
    Handle h = LMGetTEScrpHandle();
    LONGINT offset, len;
    if (!h) return false;
    len = GetScrap(h, 'TEXT', &offset);
    if (len < 0) return false;
    if (len > 32767) len = 32767;
    LMSetTEScrpLength((short)len);
    return true;
}

void Win_Copy(IRCWin *w)
{
    GrafPtr save;
    GetPort(&save);
    SetPort(w->win);
    if (w->focusOut) {
        ZeroScrap();
        TECopy(w->out);   /* styled TE writes TEXT+styl to the desk scrap */
    } else {
        TECopy(w->in);
        ZeroScrap();
        TEScrapToDesk();
    }
    SetPort(save);
}

void Win_Cut(IRCWin *w)
{
    GrafPtr save;
    if (w->focusOut) { Win_Copy(w); return; }
    GetPort(&save);
    SetPort(w->win);
    TECut(w->in);
    ZeroScrap();
    TEScrapToDesk();
    SetPort(save);
}

void Win_Paste(IRCWin *w)
{
    GrafPtr save;
    GetPort(&save);
    SetPort(w->win);
    if (w->focusOut) SetFocus(w, false);
    if (TEScrapFromDesk()) {
        long cur = (*w->in)->teLength - ((*w->in)->selEnd - (*w->in)->selStart);
        if (cur + LMGetTEScrpLength() > kInputMax * 2)
            SysBeep(1);
        else {
            TEPaste(w->in);
            TESelView(w->in);
        }
    }
    SetPort(save);
}

void Win_ClearSel(IRCWin *w)
{
    GrafPtr save;
    if (w->focusOut) return;
    GetPort(&save);
    SetPort(w->win);
    TEDelete(w->in);
    SetPort(save);
}

void Win_SelectAll(IRCWin *w)
{
    GrafPtr save;
    GetPort(&save);
    SetPort(w->win);
    TESetSelect(0, 32767, w->focusOut ? w->out : w->in);
    SetPort(save);
}
