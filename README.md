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

- One window per channel and per private conversation, plus a server window
- Channel nick lists (ops and voices sorted first); double-click a nick to
  open a private conversation with them
- Styled scrollback: bold nicknames, colours for joins/parts, notices,
  actions and errors (on colour Macs); lines that mention you are
  highlighted
- A **Windows** menu with ⌘1–⌘9 shortcuts. A bullet marks windows with new
  messages, and a diamond marks windows where someone mentioned you
- Notification Manager alert when someone mentions you while MacIRC is in
  the background
- Tab completion of nicknames, ↑/↓ input history, Page Up/Down, Home/End
- UTF-8 ⇄ Mac OS Roman conversion, so accented letters and typographic
  punctuation show up correctly. mIRC colour/formatting codes are removed
- Replies to CTCP VERSION, PING, TIME and CLIENTINFO; supports `/me`
- Asynchronous, non-blocking networking: the interface stays responsive
  while it connects and transfers data
- Runs under MultiFinder / System 7 cooperative multitasking (works in the
  background)
- Settings saved in *MacIRC Prefs* (Preferences folder on System 7,
  System Folder on System 6)

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
| `/away [message]` | Set or clear your away status |
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
   comma- or space-separated list of channels to join automatically.
3. Click **Connect**. When you're registered, MacIRC joins your channels.

The connect dialog defaults to `irc.libera.chat` port 6667.

**TLS:** MacIRC connects over plain-text TCP only. A 68k Mac can't do
modern TLS at a usable speed, so use a network that still allows plain-text
connections. You can also run a bouncer (such as ZNC) or a TLS-terminating
proxy on your local network. Don't reuse an important password: the server
password is sent and stored in the preferences file without encryption.

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
| `src/util.c` | String helpers and trap checks |
| `src/MacIRC.r` | Resources: menus, dialogs, icon, `SIZE` |
