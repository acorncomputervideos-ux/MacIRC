/*
 * irc.c - the IRC protocol: registration, message parsing, and the
 * /commands typed by the user.
 */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <OSUtils.h>
#include "MacIRC.h"

#define kMaxParams   15
#define kChunk       300     /* characters per outgoing PRIVMSG */

typedef struct IRCMsg {
    char *prefix;
    char  nick[kMaxNick * 2];
    char *userhost;
    char *cmd;
    char *params[kMaxParams];
    short nparams;
} IRCMsg;

static char    gNick[kMaxNick];
static Boolean gRegistered = false;
static short   gNickTries = 0;
static char    gLineBuf[2048];
static long    gLineLen = 0;
static char    gRecvBuf[4096];

static void HandleClosed(OSErr err);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */

const char *IRC_MyNick(void)
{
    return gNick;
}

Boolean IRC_IsConnected(void)
{
    return Net_State() != kNetIdle;
}

Boolean IRC_IsActive(void)
{
    return Net_State() != kNetIdle;
}

static IRCWin *TargetWin(void)
{
    IRCWin *w = Win_Front();
    return w ? w : Win_Server();
}

static void Info(IRCWin *w, const char *fmt, ...)
{
    char buf[600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Win_AddLine(w ? w : Win_Server(), kLineInfo, NULL, buf);
}

static void Event(IRCWin *w, const char *fmt, ...)
{
    char buf[600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Win_AddLine(w ? w : Win_Server(), kLineEvent, NULL, buf);
}

static void Error(IRCWin *w, const char *fmt, ...)
{
    char buf[600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Win_AddLine(w ? w : Win_Server(), kLineError, NULL, buf);
}

void IRC_SendLine(const char *line)
{
    char buf[1100];
    long n;
    if (Net_State() != kNetConnected) return;
    MacToUTF8(line, buf, sizeof(buf) - 2);
    n = strlen(buf);
    if (n > 510) n = 510;         /* RFC 1459 line limit */
    buf[n++] = '\r';
    buf[n++] = '\n';
    Net_Send(buf, n);
}

static void SendRaw(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    IRC_SendLine(buf);
}

static Boolean IsMe(const char *nick)
{
    return IRCStrCmp(nick, gNick) == 0;
}

static Boolean RequireConnection(IRCWin *w)
{
    if (Net_State() == kNetConnected && gRegistered) return true;
    Error(w, "Not connected to a server. Choose Connect\xC9 from the File menu.");
    return false;
}

/* ------------------------------------------------------------------ */
/* Connection                                                          */

void IRC_Connect(void)
{
    IRCWin *s = Win_Server();
    OSErr err;
    char title[160];

    if (Net_State() != kNetIdle) IRC_Disconnect("Reconnecting");
    if (DNR_Busy()) {
        Error(s, "Still waiting for a previous name lookup to finish; try again in a moment.");
        return;
    }

    StrNCopy(gNick, gPrefs.nick, sizeof(gNick));
    gRegistered = false;
    gNickTries = 0;
    gLineLen = 0;

    sprintf(title, "%s", gPrefs.server);
    Win_Rename(s, title);
    Info(s, "Connecting to %s port %d\xC9", gPrefs.server, gPrefs.port);

    err = Net_Connect(gPrefs.server, gPrefs.port);
    if (err != noErr)
        Error(s, "Could not connect: %s.", Net_ErrString(err));
}

static void MarkAllDisconnected(void)
{
    IRCWin *w;
    for (w = gWinList; w; w = w->next) {
        if (w->kind == kWinChannel) {
            w->joined = false;
            Win_NickClear(w);
        }
        if (w->kind != kWinServer)
            Error(w, "Disconnected.");
    }
}

void IRC_Disconnect(const char *quitMsg)
{
    if (Net_State() == kNetIdle) return;
    if (Net_State() == kNetConnected) {
        SendRaw("QUIT :%s", quitMsg ? quitMsg : "MacIRC " kAppVersion);
        Net_Close(true);
    } else {
        Net_Close(false);
    }
    gRegistered = false;
    MarkAllDisconnected();
    Error(Win_Server(), "Disconnected.");
}

static void HandleClosed(OSErr err)
{
    Net_Close(false);
    gRegistered = false;
    MarkAllDisconnected();
    if (err != noErr)
        Error(Win_Server(), "Connection lost: %s.", Net_ErrString(err));
    else
        Error(Win_Server(), "Connection closed by server.");
}

static void MakeUserName(char *out)
{
    const char *p;
    short n = 0;
    for (p = gPrefs.nick; *p && n < 9; p++) {
        char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[n++] = c;
        else if (c >= 'A' && c <= 'Z') out[n++] = c + 32;
    }
    out[n] = 0;
    if (n == 0) strcpy(out, "macirc");
}

static void Register(void)
{
    char user[16];
    MakeUserName(user);
    if (gPrefs.password[0])
        SendRaw("PASS %s", gPrefs.password);
    SendRaw("NICK %s", gNick);
    SendRaw("USER %s 0 * :%s", user, gPrefs.realname[0] ? gPrefs.realname : gNick);
}

static void AutoJoin(void)
{
    char list[256], tok[kMaxName];
    const char *p = gPrefs.channels;
    short n = 0;

    list[0] = 0;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        n = 0;
        if (!IsChannelName(p)) tok[n++] = '#';
        while (*p && *p != ' ' && *p != ',' && n < (short)sizeof(tok) - 1)
            tok[n++] = *p++;
        tok[n] = 0;
        if (strlen(list) + strlen(tok) + 2 < sizeof(list)) {
            if (list[0]) strcat(list, ",");
            strcat(list, tok);
        }
    }
    if (list[0]) SendRaw("JOIN %s", list);
}

/* ------------------------------------------------------------------ */
/* Parsing                                                             */

static void ParseMessage(char *p, IRCMsg *m)
{
    memset(m, 0, sizeof(*m));

    if (*p == '@') {                 /* IRCv3 message tags: ignore */
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
    }
    if (*p == ':') {
        char *bang;
        m->prefix = ++p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
        StrNCopy(m->nick, m->prefix, sizeof(m->nick));
        bang = strchr(m->nick, '!');
        if (bang) {
            *bang = 0;
            m->userhost = m->prefix + (bang - m->nick) + 1;
        }
    }
    while (*p == ' ') p++;
    m->cmd = p;
    while (*p && *p != ' ') p++;
    if (*p) *p++ = 0;

    while (*p && m->nparams < kMaxParams) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == ':') {
            m->params[m->nparams++] = p + 1;
            break;
        }
        m->params[m->nparams++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
}

static const char *Param(IRCMsg *m, short i)
{
    return (i < m->nparams && m->params[i]) ? m->params[i] : "";
}

static void JoinParams(IRCMsg *m, short from, char *out, long max)
{
    short i;
    out[0] = 0;
    for (i = from; i < m->nparams; i++) {
        if (strlen(out) + strlen(m->params[i]) + 2 >= (size_t)max) break;
        if (out[0]) strcat(out, " ");
        strcat(out, m->params[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Incoming messages                                                   */

static IRCWin *QueryWin(const char *nick, Boolean create)
{
    IRCWin *w = Win_Find(nick);
    if (!w && create) {
        WindowPtr front = FrontWindow();
        w = Win_Create(kWinQuery, nick);
        /* don't steal focus from whatever the user is typing in */
        if (w && front) SelectWindow(front);
    }
    return w;
}

static void HandleCTCP(IRCMsg *m, const char *target, char *body, Boolean isNotice)
{
    char *arg, *end;
    IRCWin *w;

    end = strchr(body, 0x01);
    if (end) *end = 0;
    arg = strchr(body, ' ');
    if (arg) *arg++ = 0; else { static char empty[1]; empty[0] = 0; arg = empty; }

    if (!isNotice && strcmp(body, "ACTION") == 0) {
        Boolean hl;
        StripFormatting(arg);
        hl = !IsMe(m->nick) && IRCStrStr(arg, gNick);
        if (IsChannelName(target)) w = Win_Find(target);
        else w = QueryWin(m->nick, true);
        Win_AddLine(w ? w : Win_Server(), hl ? kLineHighlight : kLineAction, m->nick, arg);
        if (hl || !IsChannelName(target)) Notify_Attention();
        return;
    }

    if (isNotice) {
        Info(TargetWin(), "CTCP %s reply from %s: %s", body, m->nick, arg);
        return;
    }

    Info(TargetWin(), "CTCP %s request from %s", body, m->nick);
    if (strcmp(body, "VERSION") == 0)
        SendRaw("NOTICE %s :\001VERSION MacIRC " kAppVersion " for classic Macintosh (68k)\001", m->nick);
    else if (strcmp(body, "PING") == 0)
        SendRaw("NOTICE %s :\001PING %s\001", m->nick, arg);
    else if (strcmp(body, "CLIENTINFO") == 0)
        SendRaw("NOTICE %s :\001CLIENTINFO ACTION CLIENTINFO PING TIME VERSION\001", m->nick);
    else if (strcmp(body, "TIME") == 0) {
        unsigned long secs;
        DateTimeRec dt;
        GetDateTime(&secs);
        SecondsToDate(secs, &dt);
        SendRaw("NOTICE %s :\001TIME %04d-%02d-%02d %02d:%02d:%02d\001", m->nick,
                dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    }
}

static void HandlePrivmsg(IRCMsg *m)
{
    const char *target = Param(m, 0);
    char *text = m->params[1];
    IRCWin *w;
    Boolean hl, isChan = IsChannelName(target);

    if (!text) return;
    if (text[0] == 0x01) {
        HandleCTCP(m, target, text + 1, false);
        return;
    }
    StripFormatting(text);

    if (isChan) w = Win_Find(target);
    else if (!m->userhost) w = Win_Server();       /* from a server */
    else w = QueryWin(m->nick, true);
    if (!w) w = Win_Server();

    hl = !IsMe(m->nick) && (IRCStrStr(text, gNick) || !isChan);
    Win_AddLine(w, (isChan && hl) ? kLineHighlight : kLineMsg, m->nick, text);
    if (hl) Notify_Attention();
}

static void HandleNotice(IRCMsg *m)
{
    const char *target = Param(m, 0);
    char *text = m->params[1];
    IRCWin *w = NULL;

    if (!text) return;
    if (text[0] == 0x01) {
        HandleCTCP(m, target, text + 1, true);
        return;
    }
    StripFormatting(text);

    if (IsChannelName(target)) w = Win_Find(target);
    else if (!m->userhost || !gRegistered) w = Win_Server();
    else {
        w = Win_Find(m->nick);
        if (!w) w = TargetWin();
    }
    Win_AddLine(w ? w : Win_Server(), kLineNotice, m->nick[0] ? m->nick : "server", text);
}

static void HandleJoin(IRCMsg *m)
{
    const char *chan = Param(m, 0);
    IRCWin *w = Win_Find(chan);

    if (IsMe(m->nick)) {
        if (!w) w = Win_Create(kWinChannel, chan);
        if (!w) return;
        w->joined = true;
        w->namesPending = false;
        Win_NickClear(w);
        Event(w, "You have joined %s", chan);
    } else if (w) {
        Win_NickAdd(w, m->nick);
        Event(w, "%s (%s) has joined %s", m->nick, m->userhost ? m->userhost : "", chan);
    }
}

static void HandlePart(IRCMsg *m)
{
    const char *chan = Param(m, 0);
    const char *reason = Param(m, 1);
    IRCWin *w = Win_Find(chan);
    if (!w) return;

    if (IsMe(m->nick)) {
        w->joined = false;
        Win_NickClear(w);
        Event(w, "You have left %s", chan);
    } else {
        Win_NickRemove(w, m->nick);
        if (*reason) Event(w, "%s has left %s (%s)", m->nick, chan, reason);
        else Event(w, "%s has left %s", m->nick, chan);
    }
}

static void HandleQuit(IRCMsg *m)
{
    const char *reason = Param(m, 0);
    IRCWin *w;
    for (w = gWinList; w; w = w->next) {
        Boolean show = false;
        if (w->kind == kWinChannel) show = Win_NickRemove(w, m->nick);
        else if (w->kind == kWinQuery) show = IRCStrCmp(w->name, m->nick) == 0;
        if (show) Event(w, "%s has quit (%s)", m->nick, reason);
    }
}

static void HandleNick(IRCMsg *m)
{
    const char *newNick = Param(m, 0);
    IRCWin *w;
    Boolean me = IsMe(m->nick);

    for (w = gWinList; w; w = w->next) {
        if (w->kind == kWinChannel) {
            if (Win_NickRename(w, m->nick, newNick) || me) {
                if (me) Event(w, "You are now known as %s", newNick);
                else Event(w, "%s is now known as %s", m->nick, newNick);
            }
        } else if (w->kind == kWinQuery && IRCStrCmp(w->name, m->nick) == 0) {
            Win_Rename(w, newNick);
            Event(w, "%s is now known as %s", m->nick, newNick);
        }
    }
    if (me) {
        StrNCopy(gNick, newNick, sizeof(gNick));
        Event(Win_Server(), "You are now known as %s", newNick);
    }
}

static void HandleKick(IRCMsg *m)
{
    const char *chan = Param(m, 0);
    const char *victim = Param(m, 1);
    const char *reason = Param(m, 2);
    IRCWin *w = Win_Find(chan);
    if (!w) return;

    if (IsMe(victim)) {
        w->joined = false;
        Win_NickClear(w);
        Error(w, "You have been kicked from %s by %s (%s)", chan, m->nick, reason);
        Notify_Attention();
    } else {
        Win_NickRemove(w, victim);
        Event(w, "%s was kicked by %s (%s)", victim, m->nick, reason);
    }
}

static void HandleMode(IRCMsg *m)
{
    const char *target = Param(m, 0);
    char all[512];
    IRCWin *w;

    JoinParams(m, 1, all, sizeof(all));
    if (!IsChannelName(target)) {
        Info(Win_Server(), "%s sets mode %s on %s", m->nick, all, target);
        return;
    }
    w = Win_Find(target);
    if (!w) return;
    Event(w, "%s sets mode %s", m->nick, all);

    /* Track op/voice/etc. in the nick list */
    {
        const char *modes = Param(m, 1);
        short arg = 2;
        Boolean add = true;
        for (; *modes; modes++) {
            char c = *modes, prefix = 0;
            switch (c) {
            case '+': add = true; continue;
            case '-': add = false; continue;
            case 'q': prefix = '~'; break;
            case 'a': prefix = '&'; break;
            case 'o': prefix = '@'; break;
            case 'h': prefix = '%'; break;
            case 'v': prefix = '+'; break;
            case 'b': case 'e': case 'I': case 'k':
                arg++;
                continue;
            case 'l': case 'f': case 'j':
                if (add) arg++;
                continue;
            default:
                continue;
            }
            if (arg < m->nparams)
                Win_NickSetMode(w, m->params[arg], prefix, add);
            arg++;
        }
    }
}

static void HandleNames(IRCMsg *m)
{
    const char *chan = Param(m, m->nparams - 2);
    char *names = m->params[m->nparams - 1];
    IRCWin *w = Win_Find(chan);
    char *p, *tok;

    if (m->nparams < 3 || !names) return;
    if (!w || w->kind != kWinChannel) {
        Info(TargetWin(), "Users on %s: %s", chan, names);
        return;
    }
    if (!w->namesPending) {
        Win_NickClear(w);
        w->namesPending = true;
    }
    p = names;
    while (*p) {
        char *bang;
        while (*p == ' ') p++;
        if (!*p) break;
        tok = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
        bang = strchr(tok, '!');     /* userhost-in-names */
        if (bang) *bang = 0;
        Win_NickAdd(w, tok);
    }
}

static void HandleNumeric(IRCMsg *m, short num)
{
    char all[600];
    IRCWin *w;

    switch (num) {
    case 1:
        gRegistered = true;
        StrNCopy(gNick, Param(m, 0), sizeof(gNick));
        Info(Win_Server(), "%s", Param(m, 1));
        AutoJoin();
        return;

    case 5:         /* ISUPPORT: not interesting to read */
        return;

    case 331:       /* no topic */
        w = Win_Find(Param(m, 1));
        Info(w, "No topic is set for %s", Param(m, 1));
        return;

    case 332:       /* topic */
        w = Win_Find(Param(m, 1));
        if (w) {
            char topic[512];
            StrNCopy(topic, Param(m, 2), sizeof(topic));
            StripFormatting(topic);
            Info(w, "Topic for %s: %s", Param(m, 1), topic);
        }
        return;

    case 333:       /* topic set by */
        w = Win_Find(Param(m, 1));
        if (w) {
            char who[kMaxNick * 2], *bang;
            StrNCopy(who, Param(m, 2), sizeof(who));
            bang = strchr(who, '!');
            if (bang) *bang = 0;
            Info(w, "Topic set by %s", who);
        }
        return;

    case 353:
        HandleNames(m);
        return;

    case 366:       /* end of names */
        w = Win_Find(Param(m, 1));
        if (w && w->kind == kWinChannel) {
            if (w->namesPending)
                Info(w, "%d %s on %s", w->nNicks, w->nNicks == 1 ? "user" : "users", Param(m, 1));
            w->namesPending = false;
        }
        return;

    case 324:       /* channel modes */
        w = Win_Find(Param(m, 1));
        JoinParams(m, 2, all, sizeof(all));
        Info(w, "Mode for %s: %s", Param(m, 1), all);
        return;

    case 329:       /* channel creation time */
        return;

    case 432: case 433: case 436: case 437:
        if (!gRegistered) {
            if (++gNickTries > 4) {
                Error(Win_Server(), "Nickname %s is unavailable. Use /nick to choose another.", gNick);
                return;
            }
            Error(Win_Server(), "Nickname %s is unavailable, trying another\xC9", gNick);
            if (strlen(gNick) < 15) strcat(gNick, "_");
            else gNick[strlen(gNick) - 1] = '0' + gNickTries;
            SendRaw("NICK %s", gNick);
        } else {
            Error(TargetWin(), "%s: %s", Param(m, 1), Param(m, 2));
        }
        return;
    }

    JoinParams(m, 1, all, sizeof(all));
    StripFormatting(all);
    if (num >= 400) {
        Error(TargetWin(), "%s", all);
    } else if ((num >= 301 && num <= 320) || num == 330 || num == 338 ||
               num == 378 || num == 671 || num == 276) {
        Info(TargetWin(), "%s", all);   /* WHOIS / WHOWAS replies */
    } else {
        Info(Win_Server(), "%s", all);
    }
}

static void HandleLine(const char *raw)
{
    char line[1024];
    IRCMsg m;
    const char *cmd;

    UTF8ToMac(raw, line, sizeof(line));
    ParseMessage(line, &m);
    cmd = m.cmd;
    if (!cmd || !*cmd) return;

    if (cmd[0] >= '0' && cmd[0] <= '9') {
        HandleNumeric(&m, (short)atoi(cmd));
    } else if (strcmp(cmd, "PING") == 0) {
        SendRaw("PONG :%s", Param(&m, 0));
    } else if (strcmp(cmd, "PRIVMSG") == 0) {
        HandlePrivmsg(&m);
    } else if (strcmp(cmd, "NOTICE") == 0) {
        HandleNotice(&m);
    } else if (strcmp(cmd, "JOIN") == 0) {
        HandleJoin(&m);
    } else if (strcmp(cmd, "PART") == 0) {
        HandlePart(&m);
    } else if (strcmp(cmd, "QUIT") == 0) {
        HandleQuit(&m);
    } else if (strcmp(cmd, "NICK") == 0) {
        HandleNick(&m);
    } else if (strcmp(cmd, "KICK") == 0) {
        HandleKick(&m);
    } else if (strcmp(cmd, "MODE") == 0) {
        HandleMode(&m);
    } else if (strcmp(cmd, "TOPIC") == 0) {
        IRCWin *w = Win_Find(Param(&m, 0));
        char topic[512];
        StrNCopy(topic, Param(&m, 1), sizeof(topic));
        StripFormatting(topic);
        Event(w, "%s changed the topic to: %s", m.nick, topic);
    } else if (strcmp(cmd, "INVITE") == 0) {
        Info(TargetWin(), "%s invites you to %s (type /join %s)", m.nick, Param(&m, 1), Param(&m, 1));
        Notify_Attention();
    } else if (strcmp(cmd, "ERROR") == 0) {
        Error(Win_Server(), "Server says: %s", Param(&m, 0));
    } else if (strcmp(cmd, "PONG") == 0 || strcmp(cmd, "CAP") == 0) {
        /* nothing */
    } else {
        char all[600];
        JoinParams(&m, 0, all, sizeof(all));
        Info(Win_Server(), "%s %s", cmd, all);
    }
}

static void ProcessBytes(const char *data, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        char c = data[i];
        if (c == '\n') {
            if (gLineLen > 0 && gLineBuf[gLineLen - 1] == '\r') gLineLen--;
            gLineBuf[gLineLen] = 0;
            if (gLineLen > 0) HandleLine(gLineBuf);
            gLineLen = 0;
            if (Net_State() != kNetConnected) return;
        } else if (c != 0 && gLineLen < (long)sizeof(gLineBuf) - 1) {
            gLineBuf[gLineLen++] = c;
        }
    }
}

void IRC_Poll(void)
{
    OSErr err;
    short ev = Net_Poll(&err);
    IRCWin *s = Win_Server();

    switch (ev) {
    case kNetEvtResolved: {
        char addr[20];
        Net_AddrToStr(Net_RemoteAddr(), addr);
        Info(s, "Found %s, connecting\xC9", addr);
        break;
    }
    case kNetEvtConnected:
        Info(s, "Connected. Logging in as %s\xC9", gNick);
        Register();
        break;
    case kNetEvtFailed:
        Error(s, "Connection failed: %s.", Net_ErrString(err));
        break;
    case kNetEvtClosed:
        HandleClosed(err);
        return;
    }

    if (Net_State() == kNetConnected) {
        long n;
        short loops = 0;
        while (loops++ < 4 && (n = Net_Read(gRecvBuf, sizeof(gRecvBuf))) != 0) {
            if (n < 0) {
                HandleClosed(noErr);
                return;
            }
            ProcessBytes(gRecvBuf, n);
            if (Net_State() != kNetConnected) return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Outgoing messages and commands                                      */

/* Send text to a channel or nick, splitting long lines at word breaks,
   and echo it locally. */
static void SendText(IRCWin *echo, const char *target, const char *text, Boolean action)
{
    char chunk[kChunk + 1];
    long len = strlen(text);

    while (len > 0) {
        long n = len;
        if (n > kChunk) {
            long i;
            n = kChunk;
            for (i = kChunk; i > kChunk / 2; i--)
                if (text[i] == ' ') { n = i; break; }
        }
        memcpy(chunk, text, n);
        chunk[n] = 0;
        if (action) {
            SendRaw("PRIVMSG %s :\001ACTION %s\001", target, chunk);
            if (echo) Win_AddLine(echo, kLineAction, gNick, chunk);
        } else {
            SendRaw("PRIVMSG %s :%s", target, chunk);
            if (echo) Win_AddLine(echo, kLineSelf, gNick, chunk);
        }
        text += n;
        len -= n;
        while (*text == ' ') { text++; len--; }
    }
}

void IRC_PartWindow(IRCWin *w, const char *reason)
{
    if (w->kind == kWinChannel && w->joined && Net_State() == kNetConnected)
        SendRaw("PART %s :%s", w->name, reason ? reason : "Leaving");
}

/* Split off the first word of s; returns the rest (never NULL). */
static char *NextWord(char *s, char **word)
{
    while (*s == ' ') s++;
    *word = s;
    while (*s && *s != ' ') s++;
    if (*s) *s++ = 0;
    while (*s == ' ') s++;
    return s;
}

static void ShowHelp(IRCWin *w)
{
    static const char *lines[] = {
        "Commands:",
        "  /join #channel [key]     /part [#channel] [reason]",
        "  /msg nick text           /query nick [text]",
        "  /me action               /notice target text",
        "  /nick newnick            /topic [new topic]",
        "  /whois nick              /names [#channel]",
        "  /away [message]          /invite nick [#channel]",
        "  /kick nick [reason]      /mode target modes",
        "  /ctcp nick command       /quote raw IRC line",
        "  /server host [port]      /quit [message]",
        "  /clear                   /close",
        "Tab completes nicknames. Up/down arrows recall previous lines.",
        NULL
    };
    short i;
    for (i = 0; lines[i]; i++)
        Win_AddLine(w, kLineInfo, NULL, lines[i]);
}

static void DoCommand(IRCWin *w, char *line)
{
    char *cmd, *rest, *arg;
    char lc[32];
    short i;

    rest = NextWord(line, &cmd);
    for (i = 0; cmd[i] && i < (short)sizeof(lc) - 1; i++)
        lc[i] = (cmd[i] >= 'A' && cmd[i] <= 'Z') ? cmd[i] + 32 : cmd[i];
    lc[i] = 0;

    /* Commands that work without a connection */
    if (strcmp(lc, "help") == 0) { ShowHelp(w); return; }
    if (strcmp(lc, "clear") == 0) { Win_Clear(w); return; }
    if (strcmp(lc, "server") == 0 || strcmp(lc, "connect") == 0) {
        char *port;
        rest = NextWord(rest, &arg);
        if (!*arg) {
            if (DoConnectDialog()) IRC_Connect();
            return;
        }
        StrNCopy(gPrefs.server, arg, sizeof(gPrefs.server));
        NextWord(rest, &port);
        if (*port) gPrefs.port = (short)atoi(port);
        if (gPrefs.port <= 0) gPrefs.port = 6667;
        IRC_Connect();
        return;
    }
    if (strcmp(lc, "disconnect") == 0 || strcmp(lc, "quit") == 0) {
        IRC_Disconnect(*rest ? rest : NULL);
        return;
    }
    if (strcmp(lc, "close") == 0) {
        if (w->kind == kWinServer) { SysBeep(1); return; }
        IRC_PartWindow(w, *rest ? rest : NULL);
        Win_Dispose(w);
        return;
    }
    if (strcmp(lc, "query") == 0) {
        IRCWin *q;
        rest = NextWord(rest, &arg);
        if (!*arg) { Error(w, "Usage: /query nick [message]"); return; }
        q = Win_Find(arg);
        if (!q) q = Win_Create(kWinQuery, arg);
        else SelectWindow(q->win);
        if (q && *rest && RequireConnection(q)) SendText(q, arg, rest, false);
        return;
    }

    if (!RequireConnection(w)) return;

    if (strcmp(lc, "join") == 0 || strcmp(lc, "j") == 0) {
        char *key;
        rest = NextWord(rest, &arg);
        NextWord(rest, &key);
        if (!*arg) { Error(w, "Usage: /join #channel [key]"); return; }
        SendRaw("JOIN %s%s%s%s", IsChannelName(arg) ? "" : "#", arg, *key ? " " : "", key);
    } else if (strcmp(lc, "part") == 0 || strcmp(lc, "leave") == 0) {
        const char *chan = (w->kind == kWinChannel) ? w->name : "";
        if (IsChannelName(rest)) rest = NextWord(rest, &arg), chan = arg;
        if (!*chan) { Error(w, "Usage: /part #channel [reason]"); return; }
        SendRaw("PART %s :%s", chan, *rest ? rest : "Leaving");
    } else if (strcmp(lc, "msg") == 0 || strcmp(lc, "m") == 0) {
        IRCWin *q;
        rest = NextWord(rest, &arg);
        if (!*arg || !*rest) { Error(w, "Usage: /msg nick message"); return; }
        q = Win_Find(arg);
        if (q) SendText(q, arg, rest, false);
        else {
            char echo[kInputMax + kMaxName + 8];
            SendText(NULL, arg, rest, false);
            sprintf(echo, "-> *%s* %s", arg, rest);
            Win_AddLine(w, kLineSelf, NULL, echo);
        }
    } else if (strcmp(lc, "me") == 0) {
        if (w->kind == kWinServer) { Error(w, "/me only works in a channel or query window."); return; }
        if (!*rest) return;
        SendText(w, w->name, rest, true);
    } else if (strcmp(lc, "notice") == 0) {
        char echo[kInputMax + kMaxName + 8];
        rest = NextWord(rest, &arg);
        if (!*arg || !*rest) { Error(w, "Usage: /notice target message"); return; }
        SendRaw("NOTICE %s :%s", arg, rest);
        sprintf(echo, "-> -%s- %s", arg, rest);
        Win_AddLine(w, kLineSelf, NULL, echo);
    } else if (strcmp(lc, "nick") == 0) {
        NextWord(rest, &arg);
        if (!*arg) { Error(w, "Usage: /nick newnick"); return; }
        SendRaw("NICK %s", arg);
    } else if (strcmp(lc, "topic") == 0) {
        if (IsChannelName(rest)) {
            rest = NextWord(rest, &arg);
            if (*rest) SendRaw("TOPIC %s :%s", arg, rest);
            else SendRaw("TOPIC %s", arg);
        } else if (w->kind == kWinChannel) {
            if (*rest) SendRaw("TOPIC %s :%s", w->name, rest);
            else SendRaw("TOPIC %s", w->name);
        } else Error(w, "Usage: /topic [#channel] [new topic]");
    } else if (strcmp(lc, "names") == 0) {
        NextWord(rest, &arg);
        if (*arg) SendRaw("NAMES %s", arg);
        else if (w->kind == kWinChannel) SendRaw("NAMES %s", w->name);
        else Error(w, "Usage: /names #channel");
    } else if (strcmp(lc, "whois") == 0 || strcmp(lc, "wi") == 0) {
        NextWord(rest, &arg);
        if (!*arg && w->kind == kWinQuery) arg = w->name;
        if (!*arg) { Error(w, "Usage: /whois nick"); return; }
        SendRaw("WHOIS %s", arg);
    } else if (strcmp(lc, "away") == 0) {
        if (*rest) SendRaw("AWAY :%s", rest);
        else SendRaw("AWAY");
    } else if (strcmp(lc, "invite") == 0) {
        char *chan;
        rest = NextWord(rest, &arg);
        NextWord(rest, &chan);
        if (!*chan && w->kind == kWinChannel) chan = w->name;
        if (!*arg || !*chan) { Error(w, "Usage: /invite nick #channel"); return; }
        SendRaw("INVITE %s %s", arg, chan);
    } else if (strcmp(lc, "kick") == 0) {
        rest = NextWord(rest, &arg);
        if (w->kind != kWinChannel || !*arg) { Error(w, "Usage (in a channel): /kick nick [reason]"); return; }
        SendRaw("KICK %s %s :%s", w->name, arg, *rest ? rest : gNick);
    } else if (strcmp(lc, "ctcp") == 0) {
        char *what;
        rest = NextWord(rest, &arg);
        rest = NextWord(rest, &what);
        if (!*arg || !*what) { Error(w, "Usage: /ctcp nick command"); return; }
        for (i = 0; what[i]; i++)
            if (what[i] >= 'a' && what[i] <= 'z') what[i] -= 32;
        if (*rest) SendRaw("PRIVMSG %s :\001%s %s\001", arg, what, rest);
        else SendRaw("PRIVMSG %s :\001%s\001", arg, what);
    } else if (strcmp(lc, "quote") == 0 || strcmp(lc, "raw") == 0) {
        if (*rest) IRC_SendLine(rest);
    } else if (strcmp(lc, "op") == 0 || strcmp(lc, "deop") == 0 ||
               strcmp(lc, "voice") == 0 || strcmp(lc, "devoice") == 0) {
        const char *mode = lc[0] == 'o' ? "+o" : lc[0] == 'v' ? "+v" : lc[2] == 'o' ? "-o" : "-v";
        NextWord(rest, &arg);
        if (w->kind != kWinChannel || !*arg) { Error(w, "Usage (in a channel): /%s nick", lc); return; }
        SendRaw("MODE %s %s %s", w->name, mode, arg);
    } else {
        /* Unknown: pass through to the server, e.g. /mode, /list, /motd */
        for (i = 0; cmd[i]; i++)
            if (cmd[i] >= 'a' && cmd[i] <= 'z') cmd[i] -= 32;
        if (*rest) SendRaw("%s %s", cmd, rest);
        else SendRaw("%s", cmd);
    }
}

void IRC_Input(IRCWin *w, const char *text)
{
    char line[kInputMax * 2 + 2];
    StrNCopy(line, text, sizeof(line));

    if (line[0] == '/' && line[1] != '/') {
        if (line[1]) DoCommand(w, line + 1);
        return;
    }
    if (w->kind == kWinServer) {
        Error(w, "This is the server window. Join a channel with /join #channel, or type /help.");
        return;
    }
    if (!RequireConnection(w)) return;
    if (w->kind == kWinChannel && !w->joined) {
        Error(w, "You are not in %s. Type /join %s to rejoin.", w->name, w->name);
        return;
    }
    SendText(w, w->name, line[0] == '/' ? line + 1 : line, false);
}
