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
    130, textMenuProc, 0x7FFFFEBD, enabled, "Edit",
    {
        "Undo", noIcon, "Z", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Cut", noIcon, "X", noMark, plain;
        "Copy", noIcon, "C", noMark, plain;
        "Paste", noIcon, "V", noMark, plain;
        "Clear", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Select All", noIcon, "A", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Preferences\0xC9", noIcon, noKey, noMark, plain
    }
};

resource 'MENU' (131) {
    131, textMenuProc, 0x7FFFFDEF, enabled, "IRC",
    {
        "Join Channel\0xC9", noIcon, "J", noMark, plain;
        "Leave Channel", noIcon, noKey, noMark, plain;
        "Change Nickname\0xC9", noIcon, "N", noMark, plain;
        "Private Message\0xC9", noIcon, "M", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "List Channels\0xC9", noIcon, "L", noMark, plain;
        "Set Topic\0xC9", noIcon, "T", noMark, plain;
        "Set Away\0xC9", noIcon, "Y", noMark, plain;
        "Ignore List\0xC9", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Clear Window", noIcon, noKey, noMark, plain
    }
};

resource 'MENU' (132) {
    132, textMenuProc, 0x7FFFFFFB, enabled, "Windows",
    {
        "Tile Windows", noIcon, noKey, noMark, plain;
        "Cascade Windows", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain
    }
};

/* -------------------------------------------------------------- Dialogs */

/* Connect */
resource 'DLOG' (128) {
    {0, 0, 236, 300}, dBoxProc, invisible, noGoAway, 0x0, 128, "Connect", noAutoCenter
};

resource 'DITL' (128) {
    {
        {202, 214, 222, 284}, Button { enabled, "Connect" };
        {202, 130, 222, 200}, Button { enabled, "Cancel" };
        {198, 210, 226, 288}, UserItem { disabled };
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
        {142, 110, 158, 284}, EditText { enabled, "" };
        {168, 12, 184, 104},  StaticText { disabled, "NickServ:" };
        {168, 110, 184, 284}, EditText { enabled, "" }
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

/* Preferences */
resource 'DLOG' (133) {
    {0, 0, 310, 420}, dBoxProc, invisible, noGoAway, 0x0, 133, "Preferences", noAutoCenter
};

resource 'DITL' (133) {
    {
        {278, 330, 298, 400}, Button { enabled, "OK" };
        {278, 248, 298, 318}, Button { enabled, "Cancel" };
        {274, 326, 302, 404}, UserItem { disabled };
        {11, 12, 27, 56},     StaticText { disabled, "Font:" };
        {10, 60, 26, 160},    RadioButton { enabled, "Monaco" };
        {28, 60, 44, 160},    RadioButton { enabled, "Geneva" };
        {46, 60, 62, 160},    RadioButton { enabled, "Courier" };
        {11, 190, 27, 226},   StaticText { disabled, "Size:" };
        {10, 230, 26, 330},   RadioButton { enabled, "9 point" };
        {28, 230, 44, 330},   RadioButton { enabled, "10 point" };
        {46, 230, 62, 330},   RadioButton { enabled, "12 point" };
        {74, 12, 90, 408},    CheckBox { enabled, "Show timestamps" };
        {92, 12, 108, 408},   CheckBox { enabled, "Beep when someone mentions me" };
        {110, 12, 126, 408},  CheckBox { enabled, "Reconnect automatically if the connection drops" };
        {128, 12, 144, 408},  CheckBox { enabled, "Rejoin my channels when MacIRC starts" };
        {146, 12, 162, 408},  CheckBox { enabled, "Log conversations to the \0xD2MacIRC Logs\0xD3 folder" };
        {164, 12, 180, 408},  CheckBox { enabled, "Flag the Application menu when I'm mentioned" };
        {191, 12, 207, 130},  StaticText { disabled, "Highlight words:" };
        {190, 136, 206, 404}, EditText { enabled, "" };
        {217, 12, 233, 130},  StaticText { disabled, "Quit message:" };
        {216, 136, 232, 404}, EditText { enabled, "" };
        {243, 12, 259, 130},  StaticText { disabled, "Away message:" };
        {242, 136, 258, 404}, EditText { enabled, "" }
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
            "MacIRC 1.1\0x0D\0x0D"
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

/* Confirm sending several pasted lines */
resource 'ALRT' (132) {
    {60, 60, 200, 400}, 132,
    {
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1
    },
    alertPositionMainScreen
};

resource 'DITL' (132) {
    {
        {108, 254, 128, 324}, Button { enabled, "Send" };
        {108, 170, 128, 240}, Button { enabled, "Cancel" };
        {10, 64, 100, 330}, StaticText { disabled,
            "Send ^0 lines to ^1?\0x0D\0x0D"
            "MacIRC sends them slowly so the server doesn't disconnect you for flooding." }
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
    $"0A" "MacIRC 1.1"
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
    0x01, 0x10, release, 0x00, verUS,
    "1.1",
    "1.1, an IRC client for classic Macintosh"
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
