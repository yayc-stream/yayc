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
