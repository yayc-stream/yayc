## YAYC v1.3.0 (2024.06) -- Limes

### ✨ New Features

- Added button to disable/enable custom JS javascript
- Yayc can be configured to switch homepage to about:blank when in bg to save CPU

### 🐞 Bug fixes

- Fixed handling presses to the window X button
- Various internal fixes
- Fixed a bug causing doubled startup time
- Fixed bookmark sorting by date

## YAYC v1.2.0 (2023.08) -- Chase

### ✨ New Features

- Browsing history support.
- Search support -- both in history and bookmarks.
- Some video controls now wrapped: playback speed, play/pause, audio volume, and
  the toggle for the Guide pane.
- Menu item to delete storage data for a video, if present.
- Add video button in toolbar now also provides access to context menu for the video, if added.
- The scrollbar on Bookmarks and History views is now touch friendly and works on tablets and without a physical keyboard attached.
- various minor UI improvements, including a splash screen to hide possibly long loading time.

### 🐞 Bug fixes

- Videos added via context menu on links now correctly fetch video titles without
  the need for updating the tooltip first
- Last opened video now saved/restored keeping track of progress
- Progress of shorts videos now also tracked


## YAYC v1.1.0 (2023.06) -- Luna

### ✨ New Features

- License upgrade: now CC-BY-NC-SA
- Easylist support
- Option to define custom javascript to be ran at every page load
- Option to enable/disable dark theme (bright theme not really maintained at the present, though)
- Switched to dense UI theme
- Support to launch video URLs in external applications
- Star button now also in the main toolbar

### 🐞 Bug fixes

- Fixed directory selection on Windows
- Last opened video now saved/restored keeping track of progress


## YAYC v1.0.0 (2023.03) -- Neo

### ✨ New Features

- Bookmark support
- Video progress support
- Star/Unstar bookmark support
- Categories/subcategories support
- Viewed status support
- Google profile support (enabling also to comment with own account)

### ⚠️ Known Issues

- Currently the used QtQuick.Controls1 TreeView appear to be fairly buggy, and might glitch easily.
  More specifically, categories into which a video has been recently moved might show only that video.
  Sometimes icons also mix up in categories, showing them as videos.
  The plan is to either use the new QtQuick.Controls2 TreeView, if that proves to work OK in particular with partial model updates, or to copy the currently used one into a separate plugin, and try to fix it.
- View history, that is recorded if a directory for it has been provided, is currently not exposed to the UI.  
- Drag and drop is currently not able to pan the TreeView or to automatically expand categories.
- Adding a video through the context menu -> Add video button currently only captures the correct video title if the related tooltip has been shown.
- Ctrl+q shortcut not working on Windows
