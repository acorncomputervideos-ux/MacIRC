An IRC client for classic 68k Macintosh computers.

## Downloads

- **MacIRC-1.1.dsk**: an 800K disk image containing the application. Use it with
  emulators such as Basilisk II, Mini vMac or SheepShaver, or write it to an
  800K floppy.
- **MacIRC-1.1.bin**: MacBinary, for copying to a real Mac. Decode it with
  StuffIt Expander.

## Requirements

- A 68k Mac with 128K ROM or later (Mac Plus, SE, II series, Classic, LC,
  PowerBook, Quadra and others). Also runs on Power Macs in emulation.
- System 6.0.4 or later; System 7 recommended.
- MacTCP, or Open Transport, with an IP address and name server set up.
- About 1 MB of free memory (640 KB minimum).

MacIRC connects without encryption (no TLS), so use a network that allows
plain-text connections, or a bouncer or TLS proxy on your local network.

## What's new in 1.1

- Reconnects automatically if the connection drops, and rejoins your channels
- Rejoins the channels you were in when you last quit
- Identifies you to NickServ after connecting
- Shows lag in the server window's title and reconnects if the server stops answering
- Channel List window: sort by name or size, double-click to join
- Topic bar at the top of each channel; click it to change the topic
- Nick lists use the server's own status prefixes, track several at once,
  and show away users in gray
- Ignore list, with nicknames or `nick!user@host` masks
- Set Away / I'm Back
- Conversation logs as SimpleText files
- Preferences: font and size, timestamps, sound when mentioned, highlight
  words, quit and away messages
- Command-click a link to open it with Internet Config (or copy it to the
  Clipboard)
- Drag text out of conversations or into the input line
- Asks before sending several pasted lines, and sends them at a pace
  servers accept
- Balloon Help for the menus
- Tile and Cascade window commands; the zoom box fits your screen

## Known issues

- Balloon Help doesn't appear in the Connect and Preferences dialogs.
- Tested on an emulated Quadra 630 with System 7.5.5 and MacTCP 2.0.6.
  Not yet tested on System 6, black-and-white Macs, Open Transport or real
  hardware.
