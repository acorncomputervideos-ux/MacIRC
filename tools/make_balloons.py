#!/usr/bin/env python3
"""Generate src/Balloons.r: Balloon Help ('hmnu' and 'hdlg') resources.

The resources are written as raw data so they don't depend on Rez
templates, following the layout in Apple's BalloonTypes.r:
each component is [size][type][...] with size counting the whole component.
"""
import struct, os

HM_STRING, HM_SKIP = 1, 256

def pstr(s):
    b = s.encode("mac_roman")
    assert len(b) < 256, s
    return bytes([len(b)]) + b

def align(b):
    return b + (b"\0" if len(b) % 2 else b"")

def string_item(enabled, disabled="", checked="", other=""):
    body = struct.pack(">h", HM_STRING) + pstr(enabled) + pstr(disabled) + pstr(checked) + pstr(other)
    body = align(body)
    return struct.pack(">h", len(body) + 2) + body

def skip_item():
    return struct.pack(">hh", 4, HM_SKIP)

def dlg_string_item(enabled, disabled=""):
    body = (struct.pack(">h", HM_STRING) + struct.pack(">hh", 0, 0) + struct.pack(">hhhh", 0, 0, 0, 0)
            + pstr(enabled) + pstr(disabled) + pstr("") + pstr(""))
    body = align(body)
    return struct.pack(">h", len(body) + 2) + body

def hmnu(title, items, missing=None):
    comps = [string_item(*title)] + [skip_item() if i is None else string_item(*i) for i in items]
    head = struct.pack(">hlhhh", 2, 0, 0, 0, len(comps))
    miss = string_item(*missing) if missing else skip_item()
    return head + miss + b"".join(comps)

def hdlg(items):
    comps = [skip_item() if i is None else dlg_string_item(*i) for i in items]
    head = struct.pack(">hhlhhh", 2, 0, 0, 0, 0, len(comps))
    return head + skip_item() + b"".join(comps)

def rez_data(rtype, rid, data, name):
    lines = [f"/* {name} */", f"data '{rtype}' ({rid}) {{"]
    for i in range(0, len(data), 16):
        lines.append('    $"' + data[i:i+16].hex().upper() + '"')
    lines.append("};\n")
    return "\n".join(lines)

DIS = "Not available right now."

menus = {
    128: ("Apple menu", ("Use this menu to learn about MacIRC or open desk accessories and other items.",), [
        ("Shows the MacIRC version and requirements.",),
        None,
    ], None),
    129: ("File menu", ("Use this menu to connect to a server, close windows, or quit.",), [
        ("Connects to an IRC server. You can change the server, nickname and channels first.",),
        ("Disconnects from the server, or stops an automatic reconnection.", "Not available because you aren't connected."),
        None,
        ("Closes the front window. Closing a channel window leaves that channel.", "The server window can't be closed; use Quit instead."),
        None,
        ("Disconnects and quits MacIRC. The channels you're in are remembered for next time.",),
    ], None),
    130: ("Edit menu", ("Use this menu to copy text from a conversation or edit what you're typing.",), [
        ("Undo isn't available in MacIRC.", "Undo isn't available in MacIRC."),
        None,
        ("Removes the selected text from the input line and puts it on the Clipboard.", "Select text in the input line to cut it."),
        ("Copies the selected text to the Clipboard.", "Select some text first."),
        ("Inserts the Clipboard's text into the input line.", DIS),
        ("Deletes the selected text from the input line.", "Select text in the input line first."),
        None,
        ("Selects all the text in the input line or conversation, whichever you last clicked.", DIS),
        None,
        ("Sets the font, timestamps, sounds, logging, reconnection and away message.",),
    ], None),
    131: ("IRC menu", ("Use this menu to join channels, change your nickname, list channels and more.",), [
        ("Joins a channel. Its conversation opens in a new window.", "Not available because you aren't connected."),
        ("Leaves the channel in the front window but keeps the window open.", "Bring a channel window to the front first."),
        ("Changes your nickname.",),
        ("Opens a private conversation with someone.",),
        None,
        ("Shows the server's channels in a list. Double-click one to join it.", "Not available because you aren't connected."),
        ("Changes the topic of the channel in the front window. You can also click the topic bar.", "Bring a channel window to the front first."),
        ("Marks you as away, or back if you're already away.", "Not available because you aren't connected."),
        ("Edits the list of people whose messages are hidden.",),
        None,
        ("Erases the conversation in the front window.", DIS),
    ], None),
    132: ("Windows menu", ("Use this menu to switch between conversations and arrange their windows.",), [
        ("Arranges all the conversation windows side by side so they fill the screen.",),
        ("Stacks all the conversation windows, each slightly below the last.",),
        None,
    ], ("Brings this conversation to the front. A bullet means new messages; a diamond means someone mentioned you.",)),
}

connect = [
    ("Connects to the server with these settings.",),
    ("Closes this dialog without connecting.",),
    None, None,
    ("The IRC server's host name, such as irc.libera.chat, or its IP address.",),
    None,
    ("The server's port. Most servers use 6667 for unencrypted connections.",),
    None,
    ("The nickname other people will see. If it's taken, MacIRC adds an underscore.",),
    None,
    ("Your real name or a short description. Other people can see it.",),
    None,
    ("Channels to join after connecting, separated by spaces or commas, such as #mac68k.",),
    None,
    ("The server's password, if it has one. Most servers don't.",),
    None,
    ("Your NickServ password. MacIRC identifies you to NickServ after connecting.",),
]

prefs = [
    ("Saves these settings.",),
    ("Closes this dialog without changing any settings.",),
    None, None,
    ("Shows conversations in Monaco, a fixed-width font.",),
    ("Shows conversations in Geneva, which fits more text in a line.",),
    ("Shows conversations in Courier.",),
    None,
    ("Uses 9-point text.",),
    ("Uses 10-point text.",),
    ("Uses 12-point text, easier to read on large screens.",),
    ("Shows the time before each line.",),
    ("Plays the alert sound when someone mentions you or sends you a private message.",),
    ("Reconnects automatically if the connection drops, waiting longer after each try.",),
    ("Joins the channels you were in when you last quit.",),
    ("Saves conversations as SimpleText files in a MacIRC Logs folder next to MacIRC.",),
    ("Flags the Application menu when someone mentions you while MacIRC is in the background.",),
    None,
    ("Words that highlight a message, in addition to your nickname. Separate them with spaces.",),
    None,
    ("The message other people see when you quit.",),
    None,
    ("The message other people see when you set yourself away.",),
]

out = ["/*", " * Balloons.r - Balloon Help for menus and dialogs.",
       " * Generated by tools/make_balloons.py; edit that script, not this file.", " */", ""]
for rid, (name, title, items, missing) in menus.items():
    out.append(rez_data("hmnu", rid, hmnu(title, items, missing), name))
out.append(rez_data("hdlg", 128, hdlg(connect), "Connect dialog"))
out.append(rez_data("hdlg", 133, hdlg(prefs), "Preferences dialog"))

here = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(here, "..", "src", "Balloons.r"), "w") as f:
    f.write("\n".join(out))
print("wrote src/Balloons.r")
