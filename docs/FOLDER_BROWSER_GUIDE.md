# Folder Browser & Queue Feature Guide

## Overview

The ComicVision Sorter includes a built-in **Folder Browser** with a **Processing Queue** system that allows you to easily select and process multiple comic book images.

---

## Features

### ?? Folder Browser (Left Panel - Top Section)

The folder browser allows you to navigate through your computer's folders to find comic book images.

**Components:**
- **Folder Path Field**: Shows the current folder being browsed
- **Browse Button**: Opens a dialog to select a different folder
- **Tree View**: Displays folders and image files in a hierarchical view
- **Instruction Label**: Reminds you to "Double-click files to add to queue"

**Supported Image Formats:**
- JPG/JPEG
- PNG
- GIF
- BMP
- WebP
- TIFF/TIF

---

## How to Use

### 1. Browse to Your Comic Images

**Method A: Use the Browse Button**
1. Click the **"Browse..."** button in the Folder Browser section
2. Navigate to the folder containing your comic book images
3. Click **"Select Folder"**
4. The tree view will now show all image files in that folder

**Method B: Start with Default Folder**
- By default, the browser opens the `images/` folder in the application directory
- You can place images there and they'll be visible immediately

### 2. Add Images to Queue

**Double-Click Method:**
1. In the folder tree view, find the image file you want to process
2. **Double-click** on the file name
3. The file will be added to the Processing Queue below

**What Happens When You Double-Click:**
- ? File validation checks are performed:
  - Verifies the file exists
  - Checks if the file is readable
  - Validates it's a supported image format
- ? If validation passes, the file is added to the queue
- ? A status message confirms the addition
- ?? If the file is already in the queue, you'll get a notification
- ? If validation fails, you'll see an error message explaining why

### 3. Manage Your Queue

The **Processing Queue** section (below the folder browser) shows all files ready to be processed.

**Queue Information:**
- **Item Count**: Displays "Items in queue: X" at the top
- **File List**: Shows the filename of each queued item
- **Full Path Tooltip**: Hover over a filename to see its complete path

**Queue Actions:**

#### Remove Selected Items
1. Click on one or more items in the queue to select them
   - Hold `Ctrl` to select multiple individual items
   - Hold `Shift` to select a range of items
2. Click **"Remove Selected"** button
3. Selected items will be removed from the queue

#### Clear All Items
1. Click the **"Clear All"** button
2. Confirm when prompted
3. The entire queue will be emptied

### 4. Process the Queue

Once you have images in the queue:

1. **Select an AI Model** (from the right panel):
   - Choose a server (Ollama or LM Studio)
   - Select a vision-capable model
   - Wait for "Model Status: Ready"

2. **Click "Process Queue"**:
   - All files in the queue will be processed sequentially
   - The queue will be cleared as processing begins
   - Progress updates appear in the status bar

3. **Monitor Processing**:
   - Use the **"Pause"** button to pause processing
   - Use the **"Resume"** button to continue
   - Processing details appear in the status bar at the bottom

---

## Visual Layout

```
???????????????????????????????????????????????????????????????
?                    ComicVision Sorter                       ?
???????????????????????????????????????????????????????????????
?  FOLDER BROWSER   ?         MAIN CONTENT AREA               ?
?  ???????????????  ?  ?????????????????????????????????????  ?
?  ? Path: [...]  ?  ?  ? Server: [Ollama/LM Studio]       ?  ?
?  ? [Browse...]  ?  ?  ? Model: [Select Model]            ?  ?
?  ???????????????  ?  ? Model Status: Ready               ?  ?
?  ???????????????  ?  ?????????????????????????????????????  ?
?  ? Tree View    ?  ?  ?????????????????????????????????????  ?
?  ? ?? Folder1   ?  ?  ?      Processed Comics Table       ?  ?
?  ?   ?? img1.jpg???????  (Shows processed results)        ?  ?
?  ?   ?? img2.png?  ?  ?                                   ?  ?
?  ? ?? Folder2   ?  ?  ?????????????????????????????????????  ?
?  ???????????????  ?  [Process Images Folder] [Pause]        ?
?  "Double-click    ?                                           ?
?   to add to queue"?                                           ?
?                   ?                                           ?
?  PROCESSING QUEUE ?                                           ?
?  ???????????????  ?                                           ?
?  ?Items: 2      ?  ?                                           ?
?  ? img1.jpg     ?  ?                                           ?
?  ? img2.png     ?  ?                                           ?
?  ???????????????  ?                                           ?
?  [Remove] [Clear] ?                                           ?
?  [Process Queue]  ?                                           ?
???????????????????????????????????????????????????????????????
```

---

## Tips & Tricks

### Multi-Select Files
While you cannot currently multi-select files in the tree view to add multiple at once, you can:
1. Double-click files one at a time to add them quickly
2. The queue will accumulate all your selections
3. Process them all at once using "Process Queue"

### Organize Your Comics
**Recommended Folder Structure:**
```
My Comics/
??? Marvel/
?   ??? Spider-Man/
?   ??? X-Men/
??? DC/
?   ??? Batman/
?   ??? Superman/
??? Independent/
    ??? Image Comics/
```

- Browse to the root folder (e.g., "My Comics")
- The tree view shows all subfolders
- Double-click any image in any subfolder to add it to the queue

### Batch Processing Workflow
1. Browse to your comics folder
2. Double-click 10-20 images to add to queue
3. Click "Process Queue"
4. While those process, continue adding more images
5. Process the next batch when ready

### Error Handling
If you encounter errors when double-clicking:

**"File Not Found"**
- The file may have been moved or deleted
- Refresh the browser by re-selecting the folder

**"File Not Readable"**
- Check file permissions
- Make sure the file isn't open in another program

**"Invalid Image"**
- File may be corrupted
- Ensure it's a supported format
- Try opening in an image viewer to verify

**"Already in Queue"**
- This file has already been added
- Look in the Processing Queue section below
- Remove it and re-add if needed

---

## Keyboard Shortcuts

While browsing the folder tree:
- **?/? Arrow Keys**: Navigate up/down
- **Enter**: Select folder (expands/collapses)
- **Double-Click**: Add file to queue
- **Right-Click**: (Currently no context menu - future feature)

While in the queue list:
- **Ctrl + Click**: Select multiple individual items
- **Shift + Click**: Select range of items
- **Delete**: (Currently no shortcut - use "Remove Selected" button)

---

## Advanced Features

### File Validation
Every file added to the queue goes through validation:
1. **Existence Check**: Ensures file exists on disk
2. **Permission Check**: Verifies you can read the file
3. **Format Check**: Uses Qt's `QImageReader` to verify it's a valid image
4. **Duplicate Check**: Prevents adding the same file twice

### Queue Persistence
**Current Behavior:**
- Queue is **memory-only** (not saved between sessions)
- If you close the application, the queue is cleared
- Recommendation: Process your queue before closing

**Future Enhancement:**
- Queue could be saved to a file
- Resume queue on application restart

### Processing Logic
When you click "Process Queue":
1. All queue items are validated again (files may have been deleted)
2. Invalid files are skipped with a warning
3. Valid files are moved to `pendingImages` for processing
4. The queue is cleared (items are now "in progress")
5. Processing happens sequentially, one image at a time
6. Processed images are moved to the `processed/` folder
7. Results are saved to the database and shown in the table

---

## Troubleshooting

### Problem: Tree view is empty
**Solution:**
- Check that the selected folder actually contains image files
- Verify the folder permissions
- Try selecting a different folder
- Check the status bar for error messages

### Problem: Double-click doesn't work
**Solution:**
- Make sure you're double-clicking on a **file**, not a folder
- Folders can be expanded with a single click
- Only image files can be added to the queue
- Check that the file extension is supported

### Problem: Queue doesn't update
**Solution:**
- Check the "Items in queue: X" counter
- Scroll in the queue list widget
- Look for error messages in the status bar
- Try removing and re-adding the file

### Problem: "Process Queue" button is disabled
**Solution:**
- Ensure the queue has at least one item
- Check that an AI model is selected
- Wait for "Model Status: Ready" in the right panel
- Verify your AI server (Ollama/LM Studio) is running

---

## Feature Summary

? **Implemented Features:**
- Folder tree navigation
- Image file filtering
- Double-click to add to queue
- File validation on add
- Duplicate detection
- Queue display with file names
- Multi-select removal from queue
- Clear all queue items
- Process entire queue
- Status updates and error messages

?? **Potential Future Enhancements:**
- Multi-select in tree view (add multiple files at once)
- Drag & drop files to queue
- Context menu (right-click options)
- Queue persistence (save/load queue)
- Queue reordering (drag to reorder priority)
- Filter by file type in tree view
- Search/filter files by name

---

## Summary

The **Folder Browser & Queue** feature is fully implemented and ready to use! Simply:

1. **Browse** to your comics folder
2. **Double-click** image files to add them to the queue
3. **Process** the queue when ready

This workflow makes it easy to batch process multiple comic book covers efficiently.

For questions or issues, check the status bar at the bottom of the window for helpful messages.

Happy comic cataloging! ???
