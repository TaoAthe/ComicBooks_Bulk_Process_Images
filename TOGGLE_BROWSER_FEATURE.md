# Toggle Folder Browser Feature

## ? New Feature Added!

A **Show/Hide Browser** toggle button has been added to the application.

## Location

The toggle button is located at the **top-right of the main panel**, just above the "Server:" dropdown.

```
???????????????????????????????????????????????????????
?  [? Hide Browser]                                   ?  ? Toggle Button Here
?                                                       ?
?  Server: [Ollama/LM Studio ?]                       ?
?  Model: [Select model... ?]                         ?
?  ...                                                 ?
???????????????????????????????????????????????????????
```

## How to Use

### Hide the Folder Browser
1. Click the **"? Hide Browser"** button
2. The left panel (folder browser and queue) will slide out of view
3. Button text changes to **"? Show Browser"**
4. Status bar shows: "Folder browser hidden"

### Show the Folder Browser
1. Click the **"? Show Browser"** button  
2. The left panel (folder browser and queue) will reappear
3. Button text changes to **"? Hide Browser"**
4. Status bar shows: "Folder browser visible"

## Benefits

**More Screen Space**
- Hide the browser when you don't need it
- Get more room for viewing the results table
- Perfect for reviewing processed comics

**Easy Access**
- One click to show/hide
- No menu navigation needed
- Instant toggle

**Visual Feedback**
- Button text shows current state and action
- Arrow indicators (?/?) show direction
- Status bar confirms the action

## Button States

| State | Button Text | Left Panel |
|-------|-------------|------------|
| Visible | ? Hide Browser | Shown |
| Hidden | ? Show Browser | Hidden |

## Keyboard Alternative

Currently, the toggle is mouse-only. If you need keyboard access:
- The button can be activated by tabbing to it and pressing Space or Enter
- Consider requesting a keyboard shortcut (e.g., Ctrl+B) in future updates

## Technical Details

**Implementation:**
- Toggle button added to right panel header
- Connected to `onToggleFolderBrowser()` slot
- Uses `QWidget::hide()` and `QWidget::show()`
- Left panel state persists during session (not saved between sessions)

**File Changes:**
- `mainwindow.h` - Added `toggleBrowserButton` and `onToggleFolderBrowser()` slot
- `mainwindow.cpp` - Added button creation and toggle logic

## Build Information

**Last Build:** December 18, 2025
**Executable:** `comicvision-sorter-stage1\build\Release\ComicVisionSorterStage1.exe`
**Status:** ? Working

## Notes

- The toggle state is **not** saved between sessions
- When you restart the app, the folder browser will always be visible
- This is intentional to ensure users can always access the browser
- Future enhancement could add a "Remember state" option

---

**Quick Tip:** If you accidentally hide the browser and can't find the toggle button, look for the **"? Show Browser"** button at the top-right of the window!
