# Tonal

A lightweight **Spotify Premium** player for Windows, built with **C++ and Qt Quick**, with audio played by
**[librespot](https://github.com/librespot-org/librespot)**. No Electron, no embedded Chromium.

The interface is in English or Brazilian Portuguese: it follows the Windows language, and you can switch it in
the account menu.

![Tonal playing an album](docs/tonal.png)

## Actually lightweight

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/memory-dark.svg">
  <img alt="Memory chart: Tonal uses 105 MB idle and 109 MB while playing; the official Spotify app uses 745 MB and 726 MB" src="docs/memory-light.svg">
</picture>

| | Idle on the home screen | Playing music | Processes |
|---|---:|---:|---:|
| **Tonal 0.1.0** (Tonal + librespot) | **105 MB** | **109 MB** | 2 |
| Spotify 1.303 for Windows (Microsoft Store) | 745 MB | 726 MB | 8 |

How it was measured: private memory (the "Memory" column in Task Manager), summed over all of each app's
processes; median of 10 samples, 1 minute after opening and 45 seconds after starting the same playlist.
Windows 11, October 2026. Numbers vary between machines and with use.

## Features

- **Home:** shortcuts, recently played, top artists and tracks, your playlists and saved albums
- **Search** with top result, tracks, artists, albums and playlists, filters, "load more" and recent searches
- **Pages** for playlists, albums, Liked Songs (with infinite scrolling) and artists (popular tracks and
  discography)
- **Your Library** with filters and search, resizable and collapsible
- **Playlists:** create, rename, edit the description, delete, add and remove tracks (from any track's menu) and
  drag tracks to reorder them
- **Setlists:** search a band's shows on [setlist.fm](https://www.setlist.fm), check the ones you want (marked
  shows stay marked when you search another band, so you can mix bands) and turn them into a private playlist;
  songs repeated across shows go in once
- **Player:** play/pause, previous/next, shuffle, repeat, seek bar, volume and like
- **Queue** and **Now Playing** view in the right panel
- **Synced lyrics:** the current line is highlighted, the lyrics follow the song and clicking a line seeks to it
- **Lyric translations:** with **Translation** on, each line gets a subtitle in the app language (Brazilian
  Portuguese or English); lines already in that language stay as they are
- **Media keys** (even with Tonal minimized) and the current track with its cover in the **Windows media flyout**
- **Spotify Connect:** Tonal shows up as a device and can also send playback to your phone or another computer
- **Quality** 96/160/320 kbps (account menu), with volume normalization
- **English or Portuguese** interface (account menu → Language; Tonal restarts to switch)
- **Shortcuts:** `Space` play/pause · `Ctrl+→/←` next/previous · `Ctrl+↑/↓` volume · `Ctrl+L` search ·
  `Alt+←/→` back/forward · mouse side buttons
- **Self-healing audio engine:** if the connection to Spotify drops, Tonal restarts librespot and resumes the
  song where it was

## Install

Download the latest version from **[Releases](https://github.com/natheus/tonal-player/releases)**:

- `Tonal-<version>-setup.exe`: installer. It needs no administrator rights and adds a Start menu shortcut.
- `Tonal-<version>-win64.zip`: portable version. Unzip it and run `Tonal.exe`. Without the Start menu shortcut
  the installer creates, the Windows media flyout shows "Unknown app" instead of the name.

Both include everything the app needs (Qt, the Visual C++ runtime and librespot).

**Requirements:** Windows 10 (1809) or 11, 64-bit, and a **Spotify Premium** account.

The executable is not code-signed: if SmartScreen warns you, click **More info → Run anyway**.

## First run

Tonal uses the official Spotify Web API, so each user creates a free app in the Spotify developer dashboard.
The login screen walks you through it, with buttons to open the form and copy each field:

1. Open the **Create app** form at <https://developer.spotify.com/dashboard/create>.
2. Fill in a name and description, add exactly `http://127.0.0.1:8898/callback` under **Redirect URIs**
   (click **Add**), check **Web API**, accept the terms and click **Save**.
3. Copy the app's **Client ID**. When you switch back to Tonal, it is pasted automatically and checked with
   Spotify before the browser opens. The Client Secret is not used.

The first time, **two** authorizations open in the browser: Tonal's (Web API) and librespot's (the audio
engine). After that everything is saved and the app opens straight away.

Tonal's login is kept in **Windows Credential Manager** as "Tonal/Spotify", encrypted by Windows for your
account (version 0.1.0 kept it in the registry, and Tonal moves it automatically). Signing out (**Sair** in the
account menu) deletes the credential.

The **Setlists** page needs your own free [setlist.fm API key](https://www.setlist.fm/settings/api)
(non-commercial use). The page explains how to get one; it is checked with setlist.fm and stored in Credential
Manager as "Tonal/setlist.fm".

## Limitations

These come from Spotify's rules for apps in development mode (since February 2026), not from Tonal:

- **Other people's playlists** don't list their tracks through the API. Tonal says so and still lets you play
  them.
- **Playlists generated by Spotify** (Daily Mix and editorial playlists) can't be opened through the API.
- **Search** returns at most 10 items per type ("load more" pages through them). An artist's **popular
  tracks** come from a search and show up to 5 tracks.
- There are no "New releases" or categories: Spotify removed them from the API.

**Lyrics** don't come from Spotify, whose API has none, but from [LRCLIB](https://lrclib.net), an open database
maintained by volunteers, so not every song is there. Tonal sends LRCLIB the track title, artist, album and
duration only while the lyrics view is open.

**Lyric translations** come from Google Translate by default, with no sign-up, through the address used by
Google's own Chrome extension. It is not an official API: Google may block or change it without notice, as it did
in September 2026 with the address most similar tools used. You can instead paste your own free
[DeepL API](https://www.deepl.com/pro-api) key (500,000 characters a month) in the lyrics view's ⋯ menu; if its
monthly quota runs out, Tonal falls back to Google. Only songs opened with Translation on are sent, each line once,
and translations are cached on disk; the ⋯ menu translates a song again or deletes the saved translations.

**Setlists** come from setlist.fm, and not every song in them is on Spotify (covers and unreleased songs often
are not): Tonal shows how many it found. Each song becomes one or two Spotify searches.

A playlist's **visibility** (public or private) can only be changed in the official app: the API makes a
playlist public but ignores requests to make it private again. Playlists created by Tonal start private.

Tonal has only been built and tested on Windows so far.

## How it works

```
┌──────────────────────── Tonal.exe (Qt Quick) ────────────────────────┐
│  QML (interface)  ──►  C++ (App, Player, SpotifyApi, AuthManager)    │
│                              │ Web API (HTTPS)                       │
│                              ▼                                       │
│                     api.spotify.com  ◄── commands: play, pause...    │
└──────────────────────────────┬───────────────────────────────────────┘
                               │ starts and watches the process
                     librespot.exe  ──►  audio (device "Tonal @ PC")
```

Tonal talks to the official Web API (library, search, playback control). librespot shows up as a Spotify
Connect device and is what downloads and plays the music; Tonal tells Spotify to play on it.

Why it uses so little memory:

- Qt Quick UI drawn by the GPU, no browser engine
- Qt Quick Controls "Basic" style and vector icons with `QtQuick.Shapes` (no PNGs or icon fonts)
- Covers decoded at the size they are shown and cached on disk
- Pages created and destroyed on demand; long lists reuse their rows
- Player state is polled every 5 s and the progress bar moves on its own; librespot's log tells Tonal right
  away when the track changes

## Building from source

### 1. Prerequisites (once)

| What | Where | Notes |
|---|---|---|
| Visual Studio 2022 or 2026 | [visualstudio.microsoft.com](https://visualstudio.microsoft.com) | **Desktop development with C++** workload (includes CMake and Ninja) |
| Qt 6.8 or newer | [Qt Online Installer](https://www.qt.io/download-qt-installer-oss) | select **MSVC 2022 64-bit** and, under *Additional Libraries*, **Qt Image Formats** (playlist covers are WebP) |
| Rust | [rustup.rs](https://rustup.rs) | only to build librespot |

After installing Qt, create a `QTDIR` environment variable pointing to the kit, for example:

```powershell
setx QTDIR "C:\Qt\6.8.3\msvc2022_64"
```

Close and reopen Visual Studio (or the terminal) after `setx`.

### 2. Build librespot

```powershell
cargo install librespot --locked
```

The executable ends up in `%USERPROFILE%\.cargo\bin\librespot.exe`. Tonal looks for it in this order: next to
`Tonal.exe`, on the `PATH` and in `.cargo\bin`. There is no need to copy it anywhere.

### 3. Build Tonal

**In Visual Studio:** *File → Open → Folder...* and pick this folder. Choose the **Release (MSVC + Qt)** preset
in the toolbar, then *Build → Build All* and run `Tonal.exe`. `windeployqt` runs after the build and copies the
Qt DLLs next to the executable.

**From the command line** (Visual Studio Developer PowerShell):

```powershell
cmake --preset release
cmake --build --preset release
.\out\build\release\Tonal.exe
```

### 4. Make a release

With Inno Setup 6 installed (`winget install JRSoftware.InnoSetup`), in a regular PowerShell:

```powershell
.\packaging\release.ps1             # builds the ZIP, installer and SHA256SUMS.txt in out\release\
.\packaging\release.ps1 -Publish    # also publishes release v<version> on GitHub (needs a logged-in gh)
```

The version comes from `project(Tonal VERSION ...)` in `CMakeLists.txt`. Before each new release, bump the
version and write its changes in [CHANGELOG.md](CHANGELOG.md): `-Publish` copies that section to the release
page.

### Layout

Code comments and the source UI strings are in Brazilian Portuguese; the English interface comes from
`i18n/tonal_en.ts`. Commits follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/).

```
src/
  main.cpp              sets up the app, style, font and QML engine
  App.*                 singleton exposed to QML (App.auth, App.api, App.player, App.librespot, App.lyrics)
  AuthManager.*         OAuth PKCE + callback server on 127.0.0.1:8898
  TokenStore.*          Spotify tokens and the setlist.fm API key in Windows Credential Manager
  SpotifyApi.*          Web API HTTP client (401 → refresh, 429 → Retry-After)
  Player.*              playback state, commands, queue, devices
  LibrespotProcess.*    starts/restarts librespot, reads its log, Job Object on Windows
  MediaControls.*       media keys and the Windows media flyout (SMTC)
  Lyrics.*              LRCLIB lyrics: lookup, picking the right version and parsing LRC; translation cache
  DeepL.*               lyric translation with your own DeepL API key
  GoogleTranslate.*     lyric translation through Google Translate, no key
  SetlistFm.*           setlist.fm API: band shows, API key check and storage
  NetworkFactory.*      disk cache for covers
qml/
  Main.qml              window, layout, navigation, shortcuts, track menu, toast
  Theme.qml             colors (blue, black and white), sizes and formatting helpers
  Icons.qml             SVG icon paths
  Nav.qml               navigation history and shared state
  Playlists.qml         creating and editing playlists (and the permission it needs from Spotify)
  Setlists.qml          setlist.fm shows to playlist: search, selection, track matching
  components/           Icon, Cover, MediaCard, TrackRow, SeekBar, Chip...
  panels/               TopBar, LibraryPanel, PlayerBar, RightPanel, DevicePopup
  pages/                Login, Home, Search, Collection (playlist/album/liked), Artist, Lyrics, Setlists
resources/
  tonal.ico             app icon (generated by make-icon.ps1, linked to the .exe by tonal.rc)
packaging/
  release.ps1           builds the portable ZIP and the installer (and publishes the release with -Publish)
  tonal.iss             installer (Inno Setup 6)
  licenses/             Qt and librespot licenses shipped with the release
i18n/
  tonal_en.ts           English translation (compiled into the executable)
docs/                   images for this README
```

### Translations

UI text is written in Portuguese with `qsTr()` (QML) or `tr()` (C++). After adding or changing text, extract it
and translate the new entries in `i18n/tonal_en.ts` (with Qt Linguist or any text editor); untranslated entries
show the Portuguese text:

```powershell
cmake --build --preset debug --target update_translations
```

## Roadmap

- Custom playlist covers and dragging tracks straight onto a playlist in Your Library
- Code signing, and testing on Linux and macOS

## License

Tonal's code is under the [MIT license](LICENSE).

Release packages also include [Qt 6](https://www.qt.io) (LGPLv3, as dynamic libraries) and
[librespot](https://github.com/librespot-org/librespot) (MIT). Their licenses are in the package's `licenses`
folder.

Tonal is a personal, unofficial client. "Spotify" is a trademark of Spotify AB; this project is not affiliated
with the company. librespot is an open, unofficial implementation of the Spotify protocol.
