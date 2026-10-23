# Changelog

## 0.4.1 (October 2026)

- **Taskbar buttons:** hovering over Tonal in the taskbar shows previous, play/pause and next buttons under the
  window thumbnail, like other music players.

## 0.4.0 (October 2026)

- **Lyric translations:** a **Translation** button in the lyrics view adds a subtitle under each line in the app
  language (Brazilian Portuguese or English). Lines already in that language get none, so Brazilian songs stay as
  they are. Translations come from Google Translate with no sign-up, or from DeepL with your own free API key, and
  are cached on disk; the ⋯ menu translates a song again (with either service) or deletes its translation or all
  saved ones.

## 0.3.0 (October 2026)

- **English interface:** Tonal is now available in English and Brazilian Portuguese. It follows the Windows
  language on first run, and you can switch it in the account menu (Tonal restarts to apply it). Dates and
  durations follow the chosen language too.
- **Setlists:** a new page (button next to Home) that searches a band's shows on setlist.fm and turns the checked
  shows into a private playlist. Shows stay checked when you search another band, so one playlist can mix bands,
  and songs repeated across shows go in once. It needs your own free setlist.fm API key, stored in Windows
  Credential Manager.

## 0.2.0 (October 2026)

- **Synced lyrics:** a microphone button in the player bar. The current line is highlighted, the lyrics follow
  the song and clicking a line seeks to it. Lyrics come from [LRCLIB](https://lrclib.net), and Tonal only sends
  it the track title while the lyrics view is open.
- **Playlists:** create one with the **+** in Your Library, rename it and edit its description (click the
  title), delete it, add tracks to a playlist and remove them from any track's menu, and drag tracks to reorder
  them. On your first edit, Tonal asks you to authorize it again in Spotify, now with access to your private
  playlists.
- **Media keys and the Windows media flyout:** play, pause, next and previous from the keyboard, even with Tonal
  minimized, and the current track with its cover in the Windows media flyout.
- **Better protected login:** the Spotify tokens move from the registry to Windows Credential Manager. This
  happens automatically the first time the new version opens.

If you pinned Tonal to the taskbar, pin it again after updating: the old pin may open as a separate button.

## 0.1.0 (October 2026)

First release: home screen, search, Your Library, playlist, album, Liked Songs and artist pages, a player with
queue, Now Playing and device switching, and an audio engine (librespot) that recovers by itself when the
connection drops.
