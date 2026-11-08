/*
 * window.c - IRC windows: scrollback (styled TextEdit), input line,
 * scroll bar, and a channel nick list (List Manager).
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
#include "MacIRC.h"

#define kZoomDocProc  8
#define kListW        112
#define kTrimAt       28000L
#define kTrimChunk    8000L
#define kFontSize     9

IRCWin  *gWinList = NULL;

static short         gLineH = 11, gAscent = 9;
static Boolean       gMetricsDone = false;
static StScrpHandle  gStyleScrap = NULL;
static short         gWinCount = 0;

static char  gHistory[kHistoryMax][kInputMax + 1];
static short gHistCount = 0;
static short gHistPos = 0;

static const RGBColor kBlack  = { 0x0000, 0x0000, 0x0000 };
static const RGBColor kGray   = { 0x7000, 0x7000, 0x7000 };
static const RGBColor kGreen  = { 0x0000, 0x7000, 0x0000 };
static const RGBColor kBlue   = { 0x0000, 0x0000, 0xB000 };
static const RGBColor kRed    = { 0xC000, 0x0000, 0x0000 };
static const RGBColor kPurple = { 0x7000, 0x0000, 0x9000 };
static const RGBColor kBrown  = { 0x8000, 0x4000, 0x0000 };

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

/* ------------------------------------------------------------------ */
/* Layout                                                              */

typedef struct WinLayout {
    Rect  out;      /* scrollback text area */
    Rect  sb;       /* scrollback scroll bar */
    Rect  list;     /* nick list rView (channels) */
    Rect  in;       /* input text area */
    short divY;     /* divider between scrollback and input */
} WinLayout;

static void ComputeLayout(IRCWin *w, WinLayout *L)
{
    Rect r = w->win->portRect;
    short inputH = 2 * gLineH + 8;
    short listLeft = r.right;

    L->divY = r.bottom - inputH;
    if (w->kind == kWinChannel) {
        listLeft = r.right - kListW;
        SetRect(&L->list, listLeft + 1, r.top, r.right - 15, L->divY);
    } else {
        SetRect(&L->list, 0, 0, 0, 0);
    }
    SetRect(&L->sb, listLeft - 15, r.top - 1, listLeft + 1, L->divY + 1);
    SetRect(&L->out, r.left + 4, r.top + 2, listLeft - 17, L->divY - 2);
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

static void CreateList(IRCWin *w, const Rect *rView)
{
    Rect bounds;
    Point cSize;
    SetRect(&bounds, 0, 0, 1, 0);
    cSize.h = 0;
    cSize.v = 0;
    w->list = LNew(rView, &bounds, cSize, 0, w->win, true, false, false, true);
    if (w->list) {
        (*w->list)->selFlags = lOnlyOne;
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

static void InitMetrics(void)
{
    FontInfo fi;
    if (gMetricsDone) return;
    GetFontInfo(&fi);
    gLineH = fi.ascent + fi.descent + fi.leading;
    gAscent = fi.ascent;
    gStyleScrap = (StScrpHandle)NewHandle(sizeof(StScrpRec));
    gMetricsDone = true;
}

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
    w->win = NewWindow(NULL, &bounds, title, false, kZoomDocProc,
                       (WindowPtr)-1L, kind != kWinServer, (long)w);
    if (!w->win) {
        DisposePtr((Ptr)w);
        return NULL;
    }

    GetPort(&save);
    SetPort(w->win);
    TextFont(kFontIDMonaco);
    TextSize(kFontSize);
    InitMetrics();
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
    e->scrpFont = kFontIDMonaco;
    e->scrpFace = (Style)face;
    e->scrpSize = kFontSize;
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
    char stamp[32];
    unsigned long secs;
    DateTimeRec dt;
    const RGBColor *color = &kBlack;
    short face = normal;
    char pre[kMaxNick + 8], post[4];

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

    GetDateTime(&secs);
    SecondsToDate(secs, &dt);
    sprintf(stamp, "%s[%02d:%02d] ", len > 0 ? "\r" : "", dt.hour, dt.minute);
    InsertStyled(te, stamp, strlen(stamp), normal, &kGray);

    pre[0] = post[0] = 0;
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

    if (who) {
        switch (cls) {
        case kLineAction: sprintf(pre, "* %s ", who); break;
        case kLineNotice: sprintf(pre, "-%s- ", who); break;
        default:          sprintf(pre, "<%s> ", who); break;
        }
        InsertStyled(te, pre, strlen(pre), bold, color);
    } else if (cls == kLineError || cls == kLineInfo || cls == kLineEvent) {
        InsertStyled(te, "*** ", 4, normal, color);
    }
    if (cls == kLineHighlight) face = bold;
    InsertStyled(te, text, strlen(text), face, color);

    if (ss != se) TESetSelect(ss, se, te);
    UpdateScroll(w, atBottom);

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
    Cell cell;
    long last = LLastClick(w->list);
    char buf[kMaxNick + 2];
    short len = kMaxNick;
    IRCWin *q;
    char *nick;

    cell.v = HiWord(last);
    cell.h = LoWord(last);
    if (cell.v < 0 || cell.v >= w->nNicks) return;
    LGetCell(buf, &len, cell, w->list);
    if (len <= 0) return;
    buf[len] = 0;
    nick = buf;
    while (*nick && strchr("~&@%+", *nick)) nick++;
    if (!*nick) return;
    q = Win_Find(nick);
    if (q) SelectWindow(q->win);
    else Win_Create(kWinQuery, nick);
}

void Win_Click(IRCWin *w, EventRecord *ev)
{
    Point pt = ev->where;
    Boolean shift = (ev->modifiers & shiftKey) != 0;
    ControlHandle ctl;
    short part;
    GrafPtr save;

    GetPort(&save);
    SetPort(w->win);
    GlobalToLocal(&pt);

    if (PtInRect(pt, &(*w->in)->viewRect)) {
        SetFocus(w, false);
        TEClick(pt, shift, w->in);
    } else if (PtInRect(pt, &(*w->out)->viewRect)) {
        SetFocus(w, true);
        TEClick(pt, shift, w->out);
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
    SetPort(save);
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

    if (n == 0) return;
    if (n > (long)sizeof(buf) - 1) n = sizeof(buf) - 1;
    BlockMove(*(*w->in)->hText, buf, n);
    buf[n] = 0;
    TESetSelect(0, 32767, w->in);
    TEDelete(w->in);
    HistoryAdd(buf);

    /* Pasted text may contain several lines: send each one */
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
    const char *match = NULL;
    char *text;
    short i, n;

    text = *(*in)->hText;
    for (start = end; start > 0 && text[start - 1] != ' '; start--) ;
    n = end - start;
    if (n <= 0 || n > kMaxNick) { SysBeep(1); return; }
    memcpy(word, text + start, n);
    word[n] = 0;

    if (w->kind == kWinChannel && w->nicks) {
        for (i = 0; i < w->nNicks; i++) {
            if (IRCStrNCmp((*w->nicks)[i].nick, word, n) == 0) {
                match = (*w->nicks)[i].nick;
                break;
            }
        }
    } else if (w->kind == kWinQuery && IRCStrNCmp(w->name, word, n) == 0) {
        match = w->name;
    }
    if (!match) { SysBeep(1); return; }

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

void Win_Zoom(IRCWin *w, Point where, short part)
{
    GrafPtr save;
    if (!TrackBox(w->win, where, part)) return;
    GetPort(&save);
    SetPort(w->win);
    EraseRect(&w->win->portRect);
    ZoomWindow(w->win, part, false);
    Relayout(w);
    SetPort(save);
}

/* ------------------------------------------------------------------ */
/* Nick list                                                           */

static short ModeRank(char m)
{
    switch (m) {
    case '~': return 0;
    case '&': return 1;
    case '@': return 2;
    case '%': return 3;
    case '+': return 4;
    }
    return 5;
}

static int NickOrder(char m1, const char *n1, char m2, const char *n2)
{
    short r1 = ModeRank(m1), r2 = ModeRank(m2);
    if (r1 != r2) return r1 - r2;
    return IRCStrCmp(n1, n2);
}

static short FindNick(IRCWin *w, const char *nick)
{
    short i;
    if (!w->nicks) return -1;
    for (i = 0; i < w->nNicks; i++)
        if (IRCStrCmp((*w->nicks)[i].nick, nick) == 0) return i;
    return -1;
}

static void RemoveAt(IRCWin *w, short i)
{
    NickEntry *p = *w->nicks;
    if (i < w->nNicks - 1)
        BlockMove(&p[i + 1], &p[i], (w->nNicks - i - 1) * (long)sizeof(NickEntry));
    w->nNicks--;
    w->listDirty = true;
}

static void InsertSorted(IRCWin *w, char mode, const char *nick)
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
        if (NickOrder(mode, nick, p[i].mode, p[i].nick) < 0) break;
    if (i < w->nNicks)
        BlockMove(&p[i], &p[i + 1], (w->nNicks - i) * (long)sizeof(NickEntry));
    p[i].mode = mode;
    StrNCopy(p[i].nick, nick, sizeof(p[i].nick));
    w->nNicks++;
    w->listDirty = true;
}

void Win_NickAdd(IRCWin *w, const char *s)
{
    char mode = ' ';
    short i;
    if (!w || !w->nicks) return;
    while (*s && strchr("~&@%+", *s)) {
        if (mode == ' ') mode = *s;   /* first prefix is the highest */
        s++;
    }
    if (!*s) return;
    i = FindNick(w, s);
    if (i >= 0) {
        if ((*w->nicks)[i].mode == mode) return;
        RemoveAt(w, i);
    }
    InsertSorted(w, mode, s);
}

Boolean Win_NickRemove(IRCWin *w, const char *nick)
{
    short i;
    if (!w || !w->nicks) return false;
    i = FindNick(w, nick);
    if (i < 0) return false;
    RemoveAt(w, i);
    return true;
}

Boolean Win_HasNick(IRCWin *w, const char *nick)
{
    return w && w->nicks && FindNick(w, nick) >= 0;
}

Boolean Win_NickRename(IRCWin *w, const char *oldNick, const char *newNick)
{
    short i;
    char mode;
    if (!w || !w->nicks) return false;
    i = FindNick(w, oldNick);
    if (i < 0) return false;
    mode = (*w->nicks)[i].mode;
    RemoveAt(w, i);
    InsertSorted(w, mode, newNick);
    return true;
}

void Win_NickSetMode(IRCWin *w, const char *nick, char modeChar, Boolean add)
{
    short i;
    char cur, mode;
    if (!w || !w->nicks) return;
    i = FindNick(w, nick);
    if (i < 0) return;
    cur = (*w->nicks)[i].mode;
    if (add) {
        mode = ModeRank(modeChar) < ModeRank(cur) ? modeChar : cur;
    } else {
        mode = (cur == modeChar) ? ' ' : cur;
    }
    if (mode == cur) return;
    {
        char name[kMaxNick];
        StrNCopy(name, (*w->nicks)[i].nick, sizeof(name));
        RemoveAt(w, i);
        InsertSorted(w, mode, name);
    }
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
    short i;
    Cell cell;
    char buf[kMaxNick + 2];
    Rect r;

    if (!w->list) return;
    GetPort(&save);
    SetPort(w->win);
    LSetDrawingMode(false, w->list);
    LDelRow(0, 0, w->list);
    if (w->nNicks > 0)
        LAddRow(w->nNicks, 0, w->list);
    for (i = 0; i < w->nNicks; i++) {
        NickEntry *e = &(*w->nicks)[i];
        short len;
        if (e->mode != ' ') {
            buf[0] = e->mode;
            StrNCopy(buf + 1, e->nick, sizeof(buf) - 1);
        } else {
            StrNCopy(buf, e->nick, sizeof(buf));
        }
        len = strlen(buf);
        cell.h = 0;
        cell.v = i;
        LSetCell(buf, len, cell, w->list);
    }
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
    for (i = n; i >= 1; i--)
        DeleteMenuItem(m, i);
    for (w = gWinList, i = 1; w; w = w->next, i++) {
        char key[16];
        Str255 item;
        if (i <= 9) sprintf(key, "x/%d", i);
        else strcpy(key, "x");
        CToPStr(key, item);
        AppendMenu(m, item);
        if (w->kind == kWinServer) strcpy((char *)title + 1, "Server");
        else StrNCopy((char *)title + 1, w->name, 254);
        title[0] = strlen((char *)title + 1);
        SetMenuItemText(m, i, title);
        if (w->activity == 2) SetItemMark(m, i, 0x13);        /* diamond */
        else if (w->activity == 1) SetItemMark(m, i, 0xA5);   /* bullet */
        else if (w->win == front) SetItemMark(m, i, 0x12);    /* check */
        else SetItemMark(m, i, 0);
    }
}

void Win_SelectFromMenu(short item)
{
    IRCWin *w;
    short i;
    for (w = gWinList, i = 1; w; w = w->next, i++) {
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
