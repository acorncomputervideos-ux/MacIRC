/*
 * MacIRC.r - resources for MacIRC.
 */
#include "Processes.r"
#include "Menus.r"
#include "Windows.r"
#include "Dialogs.r"
#include "Icons.r"
#include "Finder.r"

/* ---------------------------------------------------------------- Menus */

resource 'MBAR' (128) {
    { 128, 129, 130, 131, 132 }
};

resource 'MENU' (128) {
    128, textMenuProc, 0x7FFFFFFD, enabled, apple,
    {
        "About MacIRC\0xC9", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain
    }
};

resource 'MENU' (129) {
    129, textMenuProc, 0x7FFFFFEB, enabled, "File",
    {
        "Connect\0xC9", noIcon, "K", noMark, plain;
        "Disconnect", noIcon, "D", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Close", noIcon, "W", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Quit", noIcon, "Q", noMark, plain
    }
};

resource 'MENU' (130) {
    130, textMenuProc, 0x7FFFFFBD, enabled, "Edit",
    {
        "Undo", noIcon, "Z", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Cut", noIcon, "X", noMark, plain;
        "Copy", noIcon, "C", noMark, plain;
        "Paste", noIcon, "V", noMark, plain;
        "Clear", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Select All", noIcon, "A", noMark, plain
    }
};

resource 'MENU' (131) {
    131, textMenuProc, 0x7FFFFFEF, enabled, "IRC",
    {
        "Join Channel\0xC9", noIcon, "J", noMark, plain;
        "Leave Channel", noIcon, noKey, noMark, plain;
        "Change Nickname\0xC9", noIcon, "N", noMark, plain;
        "Private Message\0xC9", noIcon, "M", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Clear Window", noIcon, noKey, noMark, plain
    }
};

resource 'MENU' (132) {
    132, textMenuProc, allEnabled, enabled, "Windows",
    {
    }
};

/* -------------------------------------------------------------- Dialogs */

/* Connect */
resource 'DLOG' (128) {
    {0, 0, 210, 300}, dBoxProc, invisible, noGoAway, 0x0, 128, "Connect", noAutoCenter
};

resource 'DITL' (128) {
    {
        {176, 214, 196, 284}, Button { enabled, "Connect" };
        {176, 130, 196, 200}, Button { enabled, "Cancel" };
        {172, 210, 200, 288}, UserItem { disabled };
        {12, 12, 28, 104},    StaticText { disabled, "Server:" };
        {12, 110, 28, 284},   EditText { enabled, "" };
        {38, 12, 54, 104},    StaticText { disabled, "Port:" };
        {38, 110, 54, 170},   EditText { enabled, "" };
        {64, 12, 80, 104},    StaticText { disabled, "Nickname:" };
        {64, 110, 80, 284},   EditText { enabled, "" };
        {90, 12, 106, 104},   StaticText { disabled, "Real name:" };
        {90, 110, 106, 284},  EditText { enabled, "" };
        {116, 12, 132, 104},  StaticText { disabled, "Channels:" };
        {116, 110, 132, 284}, EditText { enabled, "" };
        {142, 12, 158, 104},  StaticText { disabled, "Password:" };
        {142, 110, 158, 284}, EditText { enabled, "" }
    }
};

/* One-line prompt */
resource 'DLOG' (129) {
    {0, 0, 110, 300}, dBoxProc, invisible, noGoAway, 0x0, 129, "", noAutoCenter
};

resource 'DITL' (129) {
    {
        {80, 214, 100, 284}, Button { enabled, "OK" };
        {80, 130, 100, 200}, Button { enabled, "Cancel" };
        {76, 210, 104, 288}, UserItem { disabled };
        {10, 12, 42, 288},   StaticText { disabled, "^0" };
        {48, 15, 64, 285},   EditText { enabled, "" }
    }
};

/* About */
resource 'ALRT' (130) {
    {60, 60, 200, 400}, 130,
    {
        OK, visible, silent;
        OK, visible, silent;
        OK, visible, silent;
        OK, visible, silent
    },
    alertPositionMainScreen
};

resource 'DITL' (130) {
    {
        {106, 254, 126, 324}, Button { enabled, "OK" };
        {10, 64, 98, 330}, StaticText { disabled,
            "MacIRC 1.0\0x0D\0x0D"
            "An IRC client for classic 68k Macintosh computers.\0x0D\0x0D"
            "Requires System 6.0.4 or later with MacTCP or Open Transport." };
        {10, 16, 42, 48}, Icon { disabled, 128 }
    }
};

/* Error */
resource 'ALRT' (131) {
    {60, 60, 170, 400}, 131,
    {
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1
    },
    alertPositionMainScreen
};

resource 'DITL' (131) {
    {
        {76, 254, 96, 324}, Button { enabled, "OK" };
        {10, 64, 66, 330}, StaticText { disabled, "^0" }
    }
};

/* --------------------------------------------------------- Finder bits */

resource 'ICN#' (128) {
    {
        $"00000000 3FFFFC00 40000200 80000100"
        $"9BC70100 9B6D8100 9B6C0100 9BCC0100"
        $"9B6D8100 9B670100 80000100 800001F8"
        $"40000204 3E7FFC02 02800002 028FFF82"
        $"03000002 020FFF82 02000002 020FF802"
        $"02000002 01000004 00FFFF38 00000120"
        $"000000A0 00000060 00000020 00000000"
        $"00000000 00000000 00000000 00000000",
        $"00000000 3FFFFC00 7FFFFE00 FFFFFF00"
        $"FFFFFF00 FFFFFF00 FFFFFF00 FFFFFF00"
        $"FFFFFF00 FFFFFF00 FFFFFF00 FFFFFFF8"
        $"7FFFFFFC 3FFFFFFE 03FFFFFE 03FFFFFE"
        $"03FFFFFE 03FFFFFE 03FFFFFE 03FFFFFE"
        $"03FFFFFE 01FFFFFC 00FFFFF8 000001E0"
        $"000000E0 00000060 00000020 00000000"
        $"00000000 00000000 00000000 00000000"
    }
};

data 'ICON' (128) {
    $"00000000 3FFFFC00 40000200 80000100"
    $"9BC70100 9B6D8100 9B6C0100 9BCC0100"
    $"9B6D8100 9B670100 80000100 800001F8"
    $"40000204 3E7FFC02 02800002 028FFF82"
    $"03000002 020FFF82 02000002 020FF802"
    $"02000002 01000004 00FFFF38 00000120"
    $"000000A0 00000060 00000020 00000000"
    $"00000000 00000000 00000000 00000000"
};

data 'MIRC' (0) {
    $"0A" "MacIRC 1.0"
};

resource 'FREF' (128) {
    'APPL', 0, ""
};

resource 'BNDL' (128) {
    'MIRC', 0,
    {
        'ICN#', { 0, 128 };
        'FREF', { 0, 128 }
    }
};

resource 'vers' (1) {
    0x01, 0x00, release, 0x00, verUS,
    "1.0",
    "1.0, an IRC client for classic Macintosh"
};

/* ----------------------------------------------------------------- SIZE */

resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    notHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    1024 * 1024,
    640 * 1024
};
