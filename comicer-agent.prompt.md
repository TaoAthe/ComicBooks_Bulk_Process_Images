---
mode: agent
description: ComicVision Sorter development agent — C++/Qt comic cataloging and eBay listing app
---

# ComicVision Sorter — Development Agent

## Project Overview

You are working on **ComicVision Sorter**, a Qt6 C++ desktop application for AI-powered comic book cover scanning, metadata extraction, CGC-style grading, and eBay bulk listing export. The project lives at `g:\Big\Comicer`.

## Tech Stack

| Layer | Technology |
|---|---|
| UI Framework | Qt 6.7.0 (Widgets, Network, Sql) |
| Language | C++17 |
| Build | CMake 3.16+, MSVC/Clang |
| Database | SQLite via Qt SQL |
| AI Backend | Ollama (`http://localhost:11434`) or LM Studio (local, selectable) |
| Grading Engine | Python (`phased_grading_engine.py`) called via QProcess |
| Frontend (aux) | React + Vite + Tailwind (`frontend/`) |
| Build Output | `comicvision-sorter-stage1\build\Release\ComicVisionSorterStage1.exe` |

## Project Structure

```
g:\Big\Comicer\
├── src/                        # C++ source (primary codebase)
│   ├── main.cpp
│   ├── mainwindow.h/cpp        # Main UI — dual-pane, folder browser + processing queue
│   ├── comicprocessor.h/cpp    # AI calls, image scanning, DB save
│   ├── imageviewerwidget.h/cpp # Image viewer with zoom and grading panel
│   ├── spreadsheetdialog.h/cpp # eBay-style spreadsheet inventory grid
│   ├── apiserver.h/cpp         # Local HTTP API server
│   ├── backendcontroller.h/cpp # Backend logic controller
│   └── serviceapp.h/cpp        # Service/app lifecycle
├── phased_grading_engine.py    # CGC-style multi-phase AI grading pipeline
├── frontend/src/ComicGrader.jsx # React frontend component
├── docs/
│   ├── Specs.md                # Stage 1 original specification
│   ├── TODO.md                 # Active task list
│   └── CODE_REVIEW.md          # Architecture review notes
├── specs2.txt                  # eBay bulk upload specification
├── Templates/                  # Sample eBay CSV template
└── comicvision-sorter-stage1/  # Older build tree (reference)
```

## Core Data Structure

```cpp
struct ComicEntry {
    // eBay listing fields
    QString action = "Add";
    QString category = "180089";   // eBay Comics category
    QString title;
    QString conditionID = "3000";  // Very Good
    QString publisher;
    QString series;
    QString issueNumber;
    QString publicationYear;
    QString genre = "Superheroes";
    QString mainCharacters;
    QString description;
    // eBay extended fields
    QString sku;                   // Auto-generated
    QString ebay_title;
    QString ebay_description;
    QString price;
    QString quantity = "1";
    QString condition = "3000";
    QString category_id = "63";
    QString picture_url;
    // Internal
    QString imagePath;
    QString metadata;
    QString value;
    QString notes;
};
```

## Current Feature Status

| Feature | Status |
|---|---|
| Folder browser (tree view + thumbnail view) | Done |
| Double-click queue system | Done |
| Ollama / LM Studio AI integration | Done |
| SQLite database persistence | Done |
| Image viewer with zoom | Done |
| Pause/resume batch processing | Done |
| Show/hide left panel toggle | Done |
| Phased Python grading engine | Partial |
| eBay CSV export (standard) | Partial |
| eBay bulk upload CSV format | TODO |
| Spreadsheet inventory grid | TODO |
| eBay field editing (Price, Condition, etc.) | TODO |
| Batch edit eBay details dialog | TODO |

## Active TODO Priorities (from docs/TODO.md)

### eBay Bulk CSV Export
- Generate CSV matching eBay Seller Hub bulk upload headers:
  `Action, ItemID, SKU, Title, Description, Category, Price, Quantity, Condition, PictureURL1`
- Auto-generate SKU: `title + "-" + issue + "-" + hash`
- Auto-generate eBay description from OCR text
- Default Category: `180089` (US Comics), allow override
- Default Condition: `3000` (Very Good)
- Flag `UNKNOWN` metadata and warn on export
- UTF-8 encoded, properly quoted CSV via `QFile` + `QTextStream`

### Spreadsheet Inventory Grid (`spreadsheetdialog.h/cpp`)
- Display eBay listing fields in a grid; auto-populate on scan
- Editable columns: Title, IssueNumber, Series, Publisher, Year, Genre, Topic, ConditionID, Description
- Read-only: Action, Category, internal IDs
- Row states: Pending Review (yellow), Approved (green), Exported (gray)
- "Export to eBay CSV" button on dialog

### GUI Enhancements
- Right pane editable fields: Price, Quantity, Condition (dropdown), Category ID, Shipping Profile
- "Batch Edit eBay Details" dialog for multi-select rows
- "Prepare Images for eBay" button (ZIP of processed images)

## Phased Grading Engine (phased_grading_engine.py)

The Python grading pipeline runs 6 sequential AI vision passes against a comic image:

| Phase | Focus |
|---|---|
| 1 | Spine microstructure (ticks, stress, splits) |
| 2 | Corners & edge wear (soft/rounded/blunted corners, micro-tears) |
| 3 | Cover surface (creases, folds, dents, scuffing, gloss loss) |
| 4 | Structural (staples, spine roll, tears, water damage, stains) |
| 5 | Color / gloss / eye-appeal |
| 6 | Paper quality (tone, brittleness, oxidation) |

Returns a structured CGC-style grade. Called from C++ via `QProcess`.

## Build Instructions

```powershell
# From build_root/
cmake --build . --config Release
# Or open ComicVisionSorterStage1.sln in Visual Studio
```

## AI Integration

- **Ollama**: `POST http://localhost:11434/api/generate`
  - Model examples: `llava`, `llava:13b`, `llava-phi3`
  - Body: `{ model, prompt, images: [base64], stream: false }`
- **LM Studio**: OpenAI-compatible endpoint, user-configured port
- Image sent as base64 via Qt Network (`QNetworkAccessManager`)
- Vision capability detection on model select

## Coding Conventions

- Qt signals/slots pattern throughout (`Q_OBJECT`, `connect()`)
- `QSqlQuery` with parameter binding for all DB writes (no string concatenation)
- Layout with `QSplitter`, `QGroupBox`, `QVBoxLayout` / `QHBoxLayout`
- Status bar updates via `statusBar()->showMessage()`
- File I/O via `QFile` / `QTextStream` (not `std::fstream`)
- Error dialogs via `QMessageBox::warning()`
- Avoid raw `new` for Qt widgets that get a parent; they auto-destruct

## Key Constraints

- Windows-only (Qt 6.7.0 on Windows 10/11)
- No direct eBay API calls — export only, user uploads CSV manually to Seller Hub
- Images supported: JPG, JPEG, PNG, GIF, BMP, WebP, TIFF
- Database file: `comicvision.db` in application directory
- Processed images move to `processed/` subfolder after AI analysis
- Performance: CSV export must handle 100 entries in < 10 seconds
