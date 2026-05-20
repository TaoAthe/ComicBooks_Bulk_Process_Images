# TODO — ComicVision Sorter

Last updated: 2026-04-30

---

## Priority 1 — eBay Bulk CSV Export (Core Blocker)

> Goal: user scans comics → presses "Export eBay CSV" → uploads file to Seller Hub.

- [ ] **Add "Export to eBay CSV" button** in SpreadsheetDialog toolbar
- [ ] **Implement eBay bulk CSV writer** in `ComicProcessor` or a new `EbayCsvExporter` class:
  - Headers: `Action, ItemID, SKU, Title, Description, Category, Price, Quantity, Condition, PictureURL1`
  - Use `QFile` + `QTextStream` with `setCodec("UTF-8")`
  - Quote all fields; escape internal quotes by doubling (`""`)
  - Finish in < 10 s for 100 rows
- [ ] **SKU auto-generation**: `title + "-" + issue + "-" + QUuid short hash` (replace full UUID in `parseResponse`)
- [ ] **eBay Title** format: `"<Series> #<Issue> - <Publisher> (<Year>)"`
- [ ] **eBay Description** template: `"This comic features: <ocrRawText>. Condition: <condition>. Processed from: <filename>."`
- [ ] **Default field values enforced on export**:
  - Category = `180089` (US Comics), overridable
  - Condition = `3000` (Very Good), overridable
  - Quantity = `1`
  - Action = `Add`
- [ ] **Flag UNKNOWN fields**: warn dialog listing incomplete rows before export
- [ ] **PictureURL1**: use absolute path to processed image; document that user must host images before upload
- [ ] **UPC/ISBN extraction**: regex on OCR text (`ISBN[:\s]*[\d-]{10,17}`) → populate `ItemID` if found

---

## Priority 2 — Spreadsheet Inventory Grid

> Goal: a living, editable eBay inventory grid that drives the export.

- [ ] **Fix `SpreadsheetDialog::loadData()`**: currently queries `processed_images` table which may not exist; switch to `comics` table and map columns correctly
- [ ] **Row state coloring**:
  - Yellow background = Pending Review (new/unedited)
  - Green background = Approved (ready to export)
  - Gray background = Exported (already in CSV)
- [ ] **Lock read-only cells**: Action, Category, internal ID, image thumbnail
- [ ] **Add "Approve Row" button / right-click context menu** to mark rows green
- [ ] **Add "Export Selected Rows" vs "Export All Approved"** toggle on export
- [ ] **Persist edits back to DB**: `onCellChanged` should UPDATE the `comics` row for editable columns (Title, Issue, Publisher, Year, Genre, Description, ConditionID)
- [ ] **Add eBay-specific columns** to grid: Price, Condition (dropdown), Category ID, Shipping Profile
- [ ] **"Batch Edit eBay Details" dialog**: select multiple rows → set Price, Condition, Category for all at once
- [ ] **Status column**: `Pending | Approved | Exported`

---

## Priority 3 — Right-Pane eBay Field Editing

- [ ] Add editable fields to right pane (after AI result loads): **Price**, **Quantity**, **Condition** (QComboBox: New/Like New/Very Good/Good/Acceptable), **Category ID**, **Shipping Profile**
- [ ] Save these fields back to `ComicEntry` and persist to DB on change
- [ ] Add eBay checkbox: "Include in next eBay export" (maps to Approved status)

---

## Priority 4 — AI Parsing Improvements

- [ ] **Expand `parseResponse`** to extract: `Year`, `Series` (separate from title), `Main Characters`, `Genre`
- [ ] **Regex for condition keywords**: "near mint", "very good", "good", "fair", "poor" → map to eBay condition codes (4000, 3000, 2750, 2500, 1000)
- [ ] **Series vs Title disambiguation**: if AI says "The Amazing Spider-Man #15", split to series=`The Amazing Spider-Man`, issue=`15`
- [ ] **ISBN/UPC regex** (see Priority 1)
- [ ] **Custom prompt**: expose token count / prompt in Settings dialog

---

## Priority 5 — Grading Engine Integration

- [ ] **Wire `phased_grading_engine.py` into C++ via QProcess**: triggered by "Grade Comic" button in ImageViewerWidget
- [ ] **Pass image path as argument** to Python script; receive JSON result via stdout
- [ ] **Display grade result** in a dedicated panel: overall grade (e.g., 9.0/NM), per-phase notes
- [ ] **Store grade in DB**: add `grade TEXT, grade_notes TEXT` columns to `comics` table (ALTER TABLE migration)
- [ ] **Grade-to-ConditionID mapping**: NM(9.0+)→4000, VF(8.0)→3000, FN(6.0)→2750, GD(2.0)→2000

---

## Priority 6 — Image Management

- [ ] **"Prepare Images for eBay" button**: ZIP all processed images for selected rows into `ebay_images_<date>.zip`
- [ ] **Thumbnail in main table**: display 48×48 thumbnail column in main QTableWidget (currently only in SpreadsheetDialog)
- [ ] **Undo move-to-processed**: option to return image to `images/` folder from ImageViewer

---

## Priority 7 — Non-Functional / Polish

- [ ] **Validate required eBay fields before export**: show warning with list of incomplete rows (missing Title, Price, Condition)
- [ ] **Settings dialog**: configure default Price, Condition, Category ID, Shipping Profile, base image URL
- [ ] **eBay upload instructions tooltip/link**: open `https://www.ebay.com/sh/lst/upload` from app
- [ ] **Update README**: add eBay workflow section (Export CSV → Host images → Upload to Seller Hub)
- [ ] **Progress dialog for export**: `QProgressDialog` during CSV write for large batches

---

## Completed ✅

- [x] Folder browser (tree view + thumbnail view)
- [x] Double-click queue system
- [x] Ollama / LM Studio model integration
- [x] SQLite database persistence
- [x] Image viewer with zoom
- [x] Pause/resume batch processing
- [x] Show/hide left panel toggle
- [x] Basic CSV export (standard, non-eBay)
- [x] `ComicEntry` struct with eBay fields
- [x] SpreadsheetDialog scaffold (loads, displays table)

## eBay Bulk Upload Integration (from Specs2.txt)

### CSV Export Enhancements
- [ ] Modify "Export to CSV" functionality to include option for eBay bulk format (checkbox in export dialog or separate "Export to eBay CSV" button).
- [ ] Generate CSV using eBay's standard bulk listing template headers (Action, ItemID, SKU, Title, Description, Category, Price, Quantity, Condition, PictureURL1, etc.).
- [ ] Map ComicEntry fields to eBay columns: SKU (auto-generate unique), Title (concatenate metadata), Description (auto-generate detailed body), Category (default to 63, allow override), Price (user input), Quantity (default 1), Condition (default 3000, prompt options), PictureURL1 (processedImagePath or URL).
- [ ] Handle other fields: UPC/ISBN (from OCR), Format (FixedPrice), Shipping options (user-configured).
- [ ] Flag "UNKNOWN" metadata and prompt for manual input during export.
- [ ] Use QFile and QTextStream for CSV writing with proper quoting/escaping.
- [ ] Support batch exports for all/selected inventory entries.

### GUI Enhancements
- [ ] Add editable fields in right pane for eBay details: Price, Quantity, Condition (dropdown), Category ID, Shipping Profile.
- [ ] Save eBay fields back to extended ComicEntry struct.
- [ ] Add "Batch Edit eBay Details" dialog for setting common values across multiple entries.
- [ ] Add "Prepare Images for eBay" button: Generate ZIP of processed images or list of URLs.
- [ ] Update status messages for eBay CSV export success.

### Data Structure Updates
- [ ] Extend ComicEntry struct with: sku, eBayTitle, eBayDescription, price, quantity, condition, categoryId, pictureUrl.

### AI Parsing Logic Updates
- [ ] Enhance post-OCR parsing to extract eBay-relevant fields (e.g., UPC/ISBN via regex).
- [ ] Allow manual entry in GUI for missing fields.

### File Management Additions
- [ ] During export, optionally create companion folder/ZIP with images for CSV PictureURL fields.
- [ ] Ensure CSV paths are absolute or URLs (user-configured base URL).

### Non-Functional Requirements
- [ ] Ensure export handles up to 100 entries in <10 seconds.
- [ ] Ensure CSV is UTF-8 encoded.
- [ ] Add validation for required eBay fields before export; warn if incomplete.
- [ ] Provide link/instructions in app for uploading to eBay Seller Hub.

### Implementation and Testing
- [ ] Build on existing CSV export code.
- [ ] Hardcode or load eBay template headers from configurable file.
- [ ] Test: Generate sample CSV, upload to eBay Sandbox, verify listings.
- [ ] Update README with eBay upload instructions.

## Editable Spreadsheet Module for Comic Scanner App

### Purpose
- [ ] Add an internal spreadsheet-style grid that displays required eBay listing fields.
- [ ] Auto-populate rows as new comics are scanned.
- [ ] Allow user edits (inline).
- [ ] Lock non-editable metadata fields.
- [ ] Export clean eBay-compatible CSV.
- [ ] This becomes the "inventory brain" for managing comics.

### Columns to Include
#### Required Columns (Editable)
- [ ] Action: No (hardcoded to "Add").
- [ ] Category: No (unless override, default 180089 for US Comics).
- [ ] Title: Yes (AI-generated, editable for cleanup).
- [ ] ConditionID: Yes (default 3000 - Very Good, user may override).
- [ ] Description: Yes (auto-build template, editable).
- [ ] IssueNumber: Yes (editable).
- [ ] Series: Yes (editable).
- [ ] Publisher: Yes (editable).
- [ ] Year: Yes (editable).
- [ ] Genre: Yes (editable).
- [ ] Topic: Yes (editable).

#### Read-only Cells
- [ ] Action (hardcoded).
- [ ] Category (fixed unless override).
- [ ] Internal IDs.
- [ ] Scanner-generated fields.

### User Controls + UX
- [ ] Editable Cells: Title, IssueNumber, Series, Publisher, Year, Genre, Topic, ConditionID, Description.
- [ ] Row States: Pending Review (yellow), Approved (green), Exported (gray).

### Export Logic
- [ ] Filter only approved rows when exporting to eBay.
- [ ] Strip internal columns.
- [ ] Convert to eBay's exact header names.
- [ ] Preserve required fields.
- [ ] Save as clean CSV.