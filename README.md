# MacIRC

An IRC client for classic 68k Macintosh computers, written in C against the
original Macintosh Toolbox and the MacTCP networking API.

```
┌─ #retrocomputing ─────────────────────────────────┬──────────────┐
│ [21:04] *** You have joined #retrocomputing       │ @chanop      │
│ [21:04] *** Topic for #retrocomputing: vintage... │ +helper      │
│ [21:05] <alice> anyone running System 7.5.5?      │ alice        │
│ [21:05] <MacUser> yes, on an SE/30!               │ bob          │
│ [21:06] * bob waves                               │ MacUser      │
├───────────────────────────────────────────────────┴──────────────┤
│ type here_                                                       │
└──────────────────────────────────────────────────────────────────┘
```

## Features

**Conversations**
- One window per channel and per private conversation, plus a server window
- A topic bar at the top of each channel; click it to change the topic
- Channel nick lists with the server's own status prefixes (`~ & @ % +`),
  tracking several at once (multi-prefix). Users who are away are shown in
  gray. Double-click a nick to open a private conversation
- Styled scrollback: bold nicknames, colours for joins/parts, notices,
  actions and errors (on colour Macs). Lines that mention your nickname or
  one of your highlight words are shown in red
- Channel List window (IRC ▸ List Channels…): sort by name or size,
  double-click to join
- Ignore list with nicknames or `nick!user@host` masks (`*` and `?` work)
- Set Away / I'm Back, with your saved away message

**Staying connected**
- Connects by host name (via the MacTCP resolver) or IP address
- Reconnects automatically if the connection drops, waiting 10 seconds, then
  20, 40… up to 5 minutes, and rejoins your channels
- Rejoins the channels you were in when you last quit
- Identifies you to NickServ after connecting
- Measures lag every minute (shown in the server window's title) and
  reconnects if the server stops answering
- Paces outgoing lines so pasting a lot of text doesn't get you
  disconnected for flooding; asks before sending more than two pasted lines

**Mac touches**
- Preferences: font (Monaco, Geneva or Courier) and size (9, 10 or 12),
  timestamps, a sound when mentioned, highlight words, quit and away
  messages, logging, reconnect and rejoin options
- Conversation logs as SimpleText files in a "MacIRC Logs" folder
- Command-click a link to open it with Internet Config (or, without
  Internet Config, to copy it to the Clipboard)
- Drag text out of a conversation, or drop text into the input line
  (System 7.5, or the Drag Manager extension)
- Balloon Help for the menus
- Windows menu with ⌘1–⌘9, Tile and Cascade commands. A bullet marks
  windows with new messages, and a diamond marks windows where someone
  mentioned you. The zoom box fills whatever screen you have
- Notification Manager alert when you're mentioned while MacIRC is in the
  background
- Tab completion of nicknames, ↑/↓ input history, Page Up/Down, Home/End
- UTF-8 ⇄ Mac OS Roman conversion; mIRC colour codes are removed
- Replies to CTCP VERSION, PING, TIME and CLIENTINFO; supports `/me`
- Asynchronous networking, so the interface never freezes; works in the
  background under MultiFinder and System 7

## Requirements

- A 68k Mac with 128K ROM or later (Mac Plus, SE, II series, Classic, LC,
  PowerBook, Quadra, etc.). It also runs on Power Macs in emulation
- System 6.0.4 or later (System 7.x recommended)
- MacTCP, or Open Transport (which includes MacTCP compatibility), with
  TCP/IP set up and a name server configured
- About 1 MB of free memory (640 KB minimum)

## Commands

| Command | Description |
|---|---|
| `/join #channel [key]` | Join a channel (opens a window) |
| `/part [#channel] [reason]` | Leave a channel |
| `/msg nick text` | Send a private message |
| `/query nick [text]` | Open a private conversation window |
| `/me action` | Send an action (`* nick waves`) |
| `/notice target text` | Send a notice |
| `/nick newnick` | Change your nickname |
| `/topic [#channel] [text]` | Show or set the topic |
| `/names [#channel]` | List the users in a channel |
| `/whois nick` | Look up a user |
| `/away [message]` | Set your away status (`/away` alone toggles it) |
| `/back` | Clear your away status |
| `/list [min users]` | Open the Channel List |
| `/ignore [mask]` | Ignore someone, or show the ignore list |
| `/unignore mask` | Stop ignoring someone |
| `/invite nick [#channel]` | Invite someone to a channel |
| `/kick nick [reason]` | Kick someone (channel operators only) |
| `/op` `/deop` `/voice` `/devoice nick` | Change channel status |
| `/ctcp nick command` | Send a CTCP request |
| `/quote line` or `/raw line` | Send a raw IRC line |
| `/server host [port]` | Connect to a server |
| `/quit [message]` | Disconnect |
| `/clear` | Clear the current window |
| `/close` | Leave and close the current window |
| `/help` | List the commands |

Any other `/command` is sent to the server as-is (for example, `/list`,
`/motd`, `/mode`). Start a line with `//` to send text that begins with a
slash.

## Using it

1. Launch MacIRC and choose **File ▸ Connect…** (⌘K).
2. Enter a server, port (usually 6667), nickname, and optionally a
   comma- or space-separated list of channels to join automatically and
   your NickServ password.
3. Click **Connect**. When you're registered, MacIRC joins your channels.

The connect dialog defaults to `irc.libera.chat` port 6667.

**TLS:** MacIRC connects over plain-text TCP only. A 68k Mac can't do
modern TLS at a usable speed, so use a network that still allows plain-text
connections. You can also run a bouncer (such as ZNC) or a TLS-terminating
proxy on your local network. Don't reuse an important password: the server
password and NickServ password are sent and stored in the preferences
file without encryption.

## Building

MacIRC is built with [Retro68](https://github.com/autc04/Retro68), a GCC
cross-compiler for classic Mac OS.

With the Retro68 Docker image:

```sh
docker run --rm -v "$PWD":/src -w /src ghcr.io/autc04/retro68 sh -c '
  cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake &&
  cmake --build build'
```

Or with a local Retro68 build:

```sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
cmake --build build
```

The build writes these files to `build/`:

- `MacIRC.dsk`: an 800K HFS disk image containing the application. Use it
  with emulators such as Mini vMac, Basilisk II or Snow, or write it to a
  floppy
- `MacIRC.bin`: a MacBinary file, to transfer to a real Mac and decode with
  StuffIt Expander
- `MacIRC.APPL`, `MacIRC.ad`, `%MacIRC.ad`: the application with its
  resource fork in other formats (raw, AppleDouble, and netatalk)

Every push is built by GitHub Actions (`.github/workflows/build.yml`). The
built application can be downloaded from the run's artifacts.

## Source layout

| File | Purpose |
|---|---|
| `src/main.c` | Toolbox setup, event loop, menus, notifications |
| `src/window.c` | Windows: styled TextEdit scrollback, input line, nick list |
| `src/irc.c` | IRC protocol handling and `/commands` |
| `src/tcp.c` | Non-blocking TCP on the MacTCP `.IPP` driver |
| `src/dnr.c` | Glue for the MacTCP Domain Name Resolver |
| `src/mactcp.h` | MacTCP parameter block definitions |
| `src/charset.c` | UTF-8 ⇄ Mac OS Roman, formatting-code removal |
| `src/dialogs.c` | Connect dialog, prompts, alerts |
| `src/prefs.c` | Preferences file |
| `src/chanlist.c` | Channel List window |
| `src/log.c` | Conversation logs |
| `src/url.c` | Finding and opening links (Internet Config) |
| `src/util.c` | String helpers, wildcard matching, trap checks |
| `src/MacIRC.r` | Resources: menus, dialogs, icon, `SIZE` |
| `src/Balloons.r` | Balloon Help, generated by `tools/make_balloons.py` |
