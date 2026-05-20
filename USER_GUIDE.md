# ComicVision Sorter - User Guide

## Quick Start

### Running the Application
```
G:\Big\Comicer\comicvision-sorter-stage1\build\Release\ComicVisionSorterStage1.exe
```

### Main Features

#### 1. Folder Browser & Queue System ?
Located on the **LEFT PANEL** of the application:

**Folder Browser (Top)**
- Click **"Browse..."** to select a folder containing comic images
- Tree view shows folders and image files
- Supported formats: JPG, PNG, GIF, BMP, WebP, TIFF

**View Modes** ? NEW!
- **Tree View** - Traditional hierarchical file browser
- **Thumbnail View** - Visual grid of image previews (96x96 pixels)
- Click **"??? Thumbnails"** button to switch to thumbnail view
- Click **"?? List"** button to return to tree view
- Perfect for browsing comic book covers visually

**Processing Queue (Bottom)**
- Double-click files in the tree to add them to queue
- View queue count and file list
- Remove or clear queue items
- Process entire queue at once

**Show/Hide Toggle** ? NEW!
- Click **"? Hide Browser"** button (top-right) to hide the left panel
- Get more screen space for viewing results
- Click **"? Show Browser"** to bring it back

#### 2. AI-Powered Comic Analysis
Located on the **RIGHT PANEL**:

**Server Selection**
- Ollama (local AI server)
- LM Studio (local AI server)

**Model Selection**
- Choose vision-capable AI models
- Wait for "Model Status: Ready"

**Processing**
- Click "Process Queue" to analyze all queued images
- Results appear in the table below
- Files move to `processed/` folder after completion

### Step-by-Step Workflow

1. **Launch Application**
   - Run the executable from build folder

2. **Browse for Comics**
   - Click "Browse..." in left panel
   - Navigate to your comics folder

3. **Add to Queue**
   - Double-click image files to add to queue
   - Add as many as you want

4. **Select AI Model**
   - Choose server (Ollama/LM Studio)
   - Select a vision model
   - Wait for "Ready" status

5. **Process**
   - Click "Process Queue"
   - Watch progress in status bar
   - Review results in table

### Database

- **Location**: `comicvision.db` in application directory
- **Auto-created**: First run
- **Contents**: All processed comic metadata

### Directories

```
Application Directory/
??? images/          # Default browse location
??? processed/       # Processed images moved here
??? comicvision.db   # Database file
```

### Troubleshooting

**Left panel not visible?**
- Make sure you're running the latest build
- Path: `comicvision-sorter-stage1\build\Release\ComicVisionSorterStage1.exe`

**Tree view empty?**
- Click "Browse..." to select a folder
- Check folder has supported image files

**Double-click doesn't work?**
- Make sure you're double-clicking **files**, not folders
- Check file is a valid image format
- Look at status bar for error messages

**"Process Queue" disabled?**
- Add at least one file to queue
- Select an AI model
- Wait for "Model Status: Ready"
- Ensure AI server is running

### Advanced Features

**Python Grading Engine**
- Available in Image Viewer
- 7-phase comic condition analysis
- Click "Grade Comic" in image viewer tab

**Remote SAM3 Processing**
- Defect detection using GPU
- See `modal_sam3.py` for usage
- Requires Modal account and setup

## Development

### Build Requirements
- Qt 6.7.0+
- CMake 3.16+
- C++17 compiler
- Visual Studio 2022 (Windows)

### Building
```bash
cd comicvision-sorter-stage1
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Project Structure
```
comicvision-sorter-stage1/
??? src/
?   ??? main.cpp              # Entry point
?   ??? mainwindow.cpp/h      # Main UI
?   ??? comicprocessor.cpp/h  # AI processing
?   ??? imageviewerwidget.cpp/h  # Image viewer
??? CMakeLists.txt
??? phased_grading_engine.py  # Grading system
??? modal_sam3.py             # Remote SAM3
```

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review the status bar messages in the app
3. Check `docs/` folder for additional documentation

## Version

**Last Updated**: December 18, 2025
**Build**: Release
**Status**: Stable
