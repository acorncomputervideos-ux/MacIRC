/*
 * charset.c - conversion between UTF-8 (what modern IRC networks speak)
 * and Mac OS Roman (what TextEdit and QuickDraw draw).
 */
#include <string.h>
#include "MacIRC.h"

/* Unicode code points for Mac OS Roman bytes 0x80..0xFF */
static const unsigned short kMacRomanHigh[128] = {
    0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1,
    0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
    0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,
    0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,
    0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
    0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
    0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211,
    0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,
    0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,
    0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
    0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
    0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1,
    0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
    0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,
    0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7
};

static char UnicodeToMac(unsigned long u)
{
    short i;
    if (u < 0x80) return (char)u;
    for (i = 0; i < 128; i++)
        if (kMacRomanHigh[i] == u) return (char)(0x80 + i);
    /* A few useful approximations */
    switch (u) {
    case 0x00A4: return (char)0xDB;   /* currency sign -> euro slot */
    case 0x00A6: return '|';
    case 0x00AD: return '-';
    case 0x00D7: return 'x';
    case 0x2032: return '\'';
    case 0x2033: return '"';
    case 0x2212: return '-';
    case 0x2192: return '>';
    case 0x2190: return '<';
    }
    return '?';
}

/* Returns the number of bytes in a valid UTF-8 sequence starting at s,
   or 0 if it isn't valid. */
static short UTF8SeqLen(const unsigned char *s, unsigned long *cp)
{
    unsigned char c = s[0];
    short n, i;
    unsigned long u;
    if (c < 0x80) { *cp = c; return 1; }
    else if ((c & 0xE0) == 0xC0) { n = 2; u = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 3; u = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 4; u = c & 0x07; }
    else return 0;
    for (i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) return 0;
        u = (u << 6) | (s[i] & 0x3F);
    }
    if (n == 2 && u < 0x80) return 0;   /* overlong */
    *cp = u;
    return n;
}

void UTF8ToMac(const char *in, char *out, long outMax)
{
    const unsigned char *s = (const unsigned char *)in;
    long o = 0;
    while (*s && o < outMax - 1) {
        unsigned long cp;
        short n = UTF8SeqLen(s, &cp);
        if (n == 0) {
            /* Not UTF-8: assume ISO 8859-1, which many older clients send */
            cp = *s;
            n = 1;
        }
        s += n;
        if (cp == 0xFE0F || cp == 0x200B || cp == 0x200D || cp == 0xFEFF)
            continue;   /* invisible: variation selectors, zero-width chars */
        out[o++] = UnicodeToMac(cp);
    }
    out[o] = 0;
}

void MacToUTF8(const char *in, char *out, long outMax)
{
    const unsigned char *s = (const unsigned char *)in;
    long o = 0;
    while (*s) {
        unsigned long u = (*s < 0x80) ? *s : kMacRomanHigh[*s - 0x80];
        if (u < 0x80) {
            if (o + 1 >= outMax) break;
            out[o++] = (char)u;
        } else if (u < 0x800) {
            if (o + 2 >= outMax) break;
            out[o++] = (char)(0xC0 | (u >> 6));
            out[o++] = (char)(0x80 | (u & 0x3F));
        } else {
            if (o + 3 >= outMax) break;
            out[o++] = (char)(0xE0 | (u >> 12));
            out[o++] = (char)(0x80 | ((u >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (u & 0x3F));
        }
        s++;
    }
    out[o] = 0;
}

/* Remove mIRC-style formatting codes: bold, colour, italics, etc. */
void StripFormatting(char *s)
{
    char *r = s, *w = s;
    while (*r) {
        unsigned char c = (unsigned char)*r;
        if (c == 0x03) {             /* colour: ^C[fg[,bg]] */
            r++;
            if (*r >= '0' && *r <= '9') { r++; if (*r >= '0' && *r <= '9') r++; }
            if (*r == ',' && r[1] >= '0' && r[1] <= '9') {
                r += 2;
                if (*r >= '0' && *r <= '9') r++;
            }
        } else if (c == 0x04) {      /* hex colour: ^D[RRGGBB[,RRGGBB]] */
            short i;
            r++;
            for (i = 0; i < 6 && ((*r >= '0' && *r <= '9') || (*r >= 'a' && *r <= 'f') ||
                                  (*r >= 'A' && *r <= 'F')); i++) r++;
        } else if (c == 0x02 || c == 0x0F || c == 0x16 || c == 0x1D ||
                   c == 0x1F || c == 0x11 || c == 0x1E) {
            r++;
        } else if (c < 0x20 && c != '\t') {
            r++;                     /* other control characters */
        } else {
            *w++ = *r++;
        }
    }
    *w = 0;
}
