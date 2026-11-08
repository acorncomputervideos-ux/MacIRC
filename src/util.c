/*
 * util.c - string helpers.
 */
#include <string.h>
#include <OSUtils.h>
#include <Gestalt.h>
#include <Traps.h>
#include "MacIRC.h"

void CToPStr(const char *c, Str255 p)
{
    long n = strlen(c);
    if (n > 255) n = 255;
    p[0] = (unsigned char)n;
    memcpy(p + 1, c, n);
}

void PToCStr(ConstStr255Param p, char *c, short max)
{
    short n = p[0];
    if (n > max - 1) n = max - 1;
    memcpy(c, p + 1, n);
    c[n] = 0;
}

void StrNCopy(char *dst, const char *src, long max)
{
    long i;
    if (max <= 0) return;
    for (i = 0; i < max - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

/* RFC 1459 case mapping: []\~ are the upper-case forms of {}|^ */
char IRCToLower(char c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    if (c == '[') return '{';
    if (c == ']') return '}';
    if (c == '\\') return '|';
    if (c == '~') return '^';
    return c;
}

int IRCStrNCmp(const char *a, const char *b, long n)
{
    while (n-- > 0) {
        char ca = IRCToLower(*a++), cb = IRCToLower(*b++);
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        if (!ca) return 0;
    }
    return 0;
}

int IRCStrCmp(const char *a, const char *b)
{
    return IRCStrNCmp(a, b, 0x7FFFFFFFL);
}

/* Case-insensitive substring test that only matches on word boundaries,
   so the nick "al" isn't highlighted by "really". */
Boolean IRCStrStr(const char *hay, const char *needle)
{
    long n = strlen(needle);
    const char *p;
    if (n == 0) return false;
    for (p = hay; *p; p++) {
        if (IRCStrNCmp(p, needle, n) == 0) {
            char before = (p == hay) ? ' ' : p[-1];
            char after = p[n];
            Boolean bOK = !((before >= 'a' && before <= 'z') || (before >= 'A' && before <= 'Z') ||
                            (before >= '0' && before <= '9') || before == '_' || before == '-');
            Boolean aOK = !((after >= 'a' && after <= 'z') || (after >= 'A' && after <= 'Z') ||
                            (after >= '0' && after <= '9') || after == '_' || after == '-');
            if (bOK && aOK) return true;
        }
    }
    return false;
}

Boolean IsChannelName(const char *s)
{
    return s[0] == '#' || s[0] == '&' || s[0] == '+' || s[0] == '!';
}

/* Is a trap implemented on this machine? (Needs 128K ROM or later.) */
Boolean TrapAvailable(unsigned short trap)
{
    TrapType type = (trap & 0x0800) ? kToolboxTrapType : kOSTrapType;
    if (type == kToolboxTrapType) {
        trap &= 0x07FF;
        if (trap >= 0x0200 &&
            NGetTrapAddress(0xA86E, kToolboxTrapType) == NGetTrapAddress(0xAA6E, kToolboxTrapType))
            trap = 0x9F;  /* small trap table: treat as unimplemented */
    }
    return NGetTrapAddress(trap, type) != NGetTrapAddress(0xA89F, kToolboxTrapType);
}

/* Gestalt that is safe to call on systems older than 6.0.4. */
OSErr SafeGestalt(OSType selector, LONGINT *response)
{
    *response = 0;
    if (!TrapAvailable(0xA1AD)) return -5551;   /* gestaltUndefSelectorErr */
    return Gestalt(selector, response);
}
