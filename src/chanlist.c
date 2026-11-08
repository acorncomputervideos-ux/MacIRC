/*
 * chanlist.c - the Channel List window, filled from the server's LIST reply.
 * Click a column heading to sort; double-click a channel to join it.
 */
#include <string.h>
#include <stdio.h>
#include <Memory.h>
#include <Events.h>
#include <Fonts.h>
#include <ToolUtils.h>
#include "MacIRC.h"

#define kMaxChans   1000
#define kHeaderH    34
#define kUsersX     150
#define kTopicX     196

typedef struct ChanEntry {
    char name[48];
    long users;
    char topic[76];
} ChanEntry;

static WindowPtr    gCLWin = NULL;
static ListHandle   gCLList = NULL;
static ChanEntry  **gEntries = NULL;
static short        gCount = 0;
static long         gMinUsers = 0;
static long         gSeen = 0;
static Boolean      gReceiving = false;
static Boolean      gSortByName = false;
static short        gLineH = 11, gAscent = 9;

Boolean ChanList_IsWindow(WindowPtr w)
{
    return w && w == gCLWin;
}

static void ListRect(Rect *r)
{
    *r = gCLWin->portRect;
    r->top += kHeaderH;
    r->right -= 15;
    r->bottom -= 15;
}

static pascal void ChanLDEF(short msg, Boolean sel, Rect *r, Cell cell,
                            short off, short len, ListHandle lh)
{
    ChanEntry *e;
    char num[16];
    Rect clip;
    RgnHandle save;

    (void)off; (void)len; (void)lh;
    if (msg == lHiliteMsg) {
        InvertRect(r);
        return;
    }
    if (msg != lDrawMsg) return;
    EraseRect(r);
    if (!gEntries || cell.v < 0 || cell.v >= gCount) return;
    e = &(*gEntries)[cell.v];

    save = NewRgn();
    GetClip(save);
    clip = *r;
    clip.right = r->left + kUsersX - 6;
    ClipRect(&clip);
    MoveTo(r->left + 4, r->top + gAscent);
    TextFace(bold);
    DrawText(e->name, 0, strlen(e->name));
    TextFace(normal);
    SetClip(save);

    sprintf(num, "%ld", e->users);
    MoveTo(r->left + kUsersX + 30 - TextWidth(num, 0, strlen(num)), r->top + gAscent);
    DrawText(num, 0, strlen(num));

    clip = *r;
    clip.left = r->left + kTopicX;
    ClipRect(&clip);
    MoveTo(r->left + kTopicX, r->top + gAscent);
    DrawText(e->topic, 0, strlen(e->topic));
    SetClip(save);
    DisposeRgn(save);

    if (sel) InvertRect(r);
}

static void CreateList(void)
{
    Rect r, bounds;
    Point cSize;
    Handle stub;

    ListRect(&r);
    SetRect(&bounds, 0, 0, 1, 0);
    cSize.h = 0;
    cSize.v = 0;
    gCLList = LNew(&r, &bounds, cSize, 0, gCLWin, true, false, false, true);
    if (!gCLList) return;
    (*gCLList)->selFlags = lOnlyOne;
    stub = MakeLDEFStub((ProcPtr)ChanLDEF);
    if (stub) (*gCLList)->listDefProc = stub;
}

static void OpenWindow(void)
{
    Rect b, scr = qd.screenBits.bounds;
    FontInfo fi;
    short width = 520, height = 300;

    if (gCLWin) {
        SelectWindow(gCLWin);
        return;
    }
    if (width > scr.right - 20) width = scr.right - 20;
    if (height > scr.bottom - 70) height = scr.bottom - 70;
    SetRect(&b, 30, 60, 30 + width, 60 + height);
    if (gHasColorQD)
        gCLWin = NewCWindow(NULL, &b, "\pChannel List", true, 8, (WindowPtr)-1L, true, 0);
    else
        gCLWin = NewWindow(NULL, &b, "\pChannel List", true, 8, (WindowPtr)-1L, true, 0);
    if (!gCLWin) return;
    SetPort(gCLWin);
    TextFont(gPrefs.fontID);
    TextSize(gPrefs.fontSize);
    GetFontInfo(&fi);
    gLineH = fi.ascent + fi.descent + fi.leading;
    gAscent = fi.ascent;
    CreateList();
}

static void DrawHeader(void)
{
    Rect r = gCLWin->portRect;
    char status[96];
    r.bottom = r.top + kHeaderH;
    EraseRect(&r);

    if (gReceiving)
        sprintf(status, "Receiving the channel list\xC9 %ld so far", gSeen);
    else if (gMinUsers > 1)
        sprintf(status, "%d channels with %ld or more users. Double-click a channel to join it.",
                gCount, gMinUsers);
    else
        sprintf(status, "%d channels. Double-click a channel to join it.", gCount);
    MoveTo(6, 4 + gAscent);
    DrawText(status, 0, strlen(status));

    TextFace(gSortByName ? bold + underline : bold);
    MoveTo(8, kHeaderH - 6);
    DrawString("\pChannel");
    TextFace(gSortByName ? bold : bold + underline);
    MoveTo(kUsersX - 4, kHeaderH - 6);
    DrawString("\pUsers");
    TextFace(bold);
    MoveTo(kTopicX + 4, kHeaderH - 6);
    DrawString("\pTopic");
    TextFace(normal);
    MoveTo(r.left, r.bottom - 1);
    LineTo(r.right, r.bottom - 1);
}

static void Invalidate(void)
{
    GrafPtr save;
    GetPort(&save);
    SetPort(gCLWin);
    InvalRect(&gCLWin->portRect);
    SetPort(save);
}

void ChanList_Begin(long minUsers)
{
    OpenWindow();
    if (!gCLWin) return;
    if (!gEntries) gEntries = (ChanEntry **)NewHandle(0);
    gCount = 0;
    gSeen = 0;
    gMinUsers = minUsers;
    gReceiving = true;
    if (gCLList) {
        GrafPtr save;
        GetPort(&save);
        SetPort(gCLWin);
        LDelRow(0, 0, gCLList);
        SetPort(save);
    }
    Invalidate();
}

void ChanList_Add(const char *name, long users, const char *topic)
{
    ChanEntry *e;
    long need;

    if (!gEntries) return;
    if (!gReceiving) ChanList_Begin(0);
    gSeen++;
    if (users < gMinUsers || gCount >= kMaxChans) return;
    need = (gCount + 1) * (long)sizeof(ChanEntry);
    if (GetHandleSize((Handle)gEntries) < need) {
        SetHandleSize((Handle)gEntries, need + 64 * sizeof(ChanEntry));
        if (MemError() != noErr) return;
    }
    e = &(*gEntries)[gCount++];
    StrNCopy(e->name, name, sizeof(e->name));
    e->users = users;
    StrNCopy(e->topic, topic, sizeof(e->topic));
    StripFormatting(e->topic);
    /* show progress now and then */
    if ((gSeen & 63) == 0 && gCLWin) {
        GrafPtr save;
        GetPort(&save);
        SetPort(gCLWin);
        DrawHeader();
        SetPort(save);
    }
}

static int Compare(const ChanEntry *a, const ChanEntry *b)
{
    if (gSortByName) return IRCStrCmp(a->name, b->name);
    if (a->users != b->users) return a->users > b->users ? -1 : 1;
    return IRCStrCmp(a->name, b->name);
}

static void Sort(void)
{
    /* insertion sort: fine for a thousand entries and needs no extra memory */
    short i, j;
    ChanEntry tmp, *p;
    HLock((Handle)gEntries);
    p = *gEntries;
    for (i = 1; i < gCount; i++) {
        tmp = p[i];
        for (j = i - 1; j >= 0 && Compare(&p[j], &tmp) > 0; j--)
            p[j + 1] = p[j];
        p[j + 1] = tmp;
    }
    HUnlock((Handle)gEntries);
}

static void Fill(void)
{
    GrafPtr save;
    if (!gCLList) return;
    GetPort(&save);
    SetPort(gCLWin);
    LSetDrawingMode(false, gCLList);
    LDelRow(0, 0, gCLList);
    if (gCount) LAddRow(gCount, 0, gCLList);
    LSetDrawingMode(true, gCLList);
    InvalRect(&gCLWin->portRect);
    SetPort(save);
}

void ChanList_End(void)
{
    if (!gCLWin) return;
    gReceiving = false;
    Sort();
    Fill();
}

void ChanList_Update(void)
{
    GrafPtr save;
    Rect g;
    GetPort(&save);
    SetPort(gCLWin);
    BeginUpdate(gCLWin);
    EraseRect(&gCLWin->portRect);
    DrawHeader();
    if (gCLList) LUpdate(gCLWin->visRgn, gCLList);
    g = gCLWin->portRect;
    g.left = g.right - 15;
    g.top = g.bottom - 15;
    {
        RgnHandle r = NewRgn();
        GetClip(r);
        ClipRect(&g);
        DrawGrowIcon(gCLWin);
        SetClip(r);
        DisposeRgn(r);
    }
    EndUpdate(gCLWin);
    SetPort(save);
}

void ChanList_Activate(Boolean active)
{
    GrafPtr save;
    GetPort(&save);
    SetPort(gCLWin);
    if (gCLList) LActivate(active, gCLList);
    InvalRect(&gCLWin->portRect);
    SetPort(save);
}

void ChanList_Click(EventRecord *ev)
{
    Point pt = ev->where;
    GrafPtr save;
    Rect lr;

    GetPort(&save);
    SetPort(gCLWin);
    GlobalToLocal(&pt);
    if (pt.v < kHeaderH && pt.v > kHeaderH - 18) {
        Boolean byName = pt.h < kUsersX - 8;
        if (pt.h < kTopicX && byName != gSortByName) {
            gSortByName = byName;
            Sort();
            Fill();
        }
    } else if (gCLList) {
        ListRect(&lr);
        lr.right += 15;
        if (PtInRect(pt, &lr) && LClick(pt, ev->modifiers, gCLList)) {
            Cell c;
            long last = LLastClick(gCLList);
            c.v = HiWord(last);
            if (c.v >= 0 && c.v < gCount) {
                char cmd[64];
                sprintf(cmd, "/join %s", (*gEntries)[c.v].name);
                IRC_Input(Win_Server(), cmd);
            }
        }
    }
    SetPort(save);
}

void ChanList_Grow(Point where)
{
    Rect limits;
    long size;
    GrafPtr save;
    SetRect(&limits, 300, 120, 32767, 32767);
    size = GrowWindow(gCLWin, where, &limits);
    if (!size) return;
    GetPort(&save);
    SetPort(gCLWin);
    SizeWindow(gCLWin, LoWord(size), HiWord(size), true);
    if (gCLList) LDispose(gCLList);
    gCLList = NULL;
    CreateList();
    Fill();
    SetPort(save);
}

void ChanList_Close(void)
{
    if (!gCLWin) return;
    if (gCLList) LDispose(gCLList);
    gCLList = NULL;
    DisposeWindow(gCLWin);
    gCLWin = NULL;
    gCount = 0;
    if (gEntries) SetHandleSize((Handle)gEntries, 0);
    {
        WindowPtr f = FrontWindow();
        if (f) SetPort(f);
    }
}
