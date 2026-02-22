## YAYC v1.4.0 (2026.02) -- Paquito

### ✨ New Features

- Ported to Qt6 (uses Qt 6.10)
- Added context menu option to move a video where the last one was moved
- Added a home button to load youtube home
- Added a toggle button to remove youtube category bar on the main page
- Added a toggle button to switch PIP on/off
- Added a menu to change the video quality
- Added a slider to configure the number of columns in youtube home's miniature grid
- Run external command now runs also for categories, recursively

### 🐞 Bug fixes

- Fixed QtQuickControls1 TreeView glitches by porting YAYC to Qt6
- Fixed scrollbar gliching on/after searching/filtering
- Changing the youtube page scaling will now correctly show the new scale in an ephemeral overlay
- Ctrl+Q fixed for Windows
- Fixed settings clearance through the UI dialog
- Fixed Light/Dark theme
- Fixed command line options
- Fixed macOS support
- Fixed moving or deleting videos or categories
- Various internal fixes

### ⚠️ Known Issues

- Drag and drop is currently not able to pan the TreeView or to automatically expand categories.
- Building YAYC from sources using Qt6 binaries from the Qt online installer will cause YAYC to
  miss extra codecs.  This will cause some videos, specifically live streams, to not play.

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
