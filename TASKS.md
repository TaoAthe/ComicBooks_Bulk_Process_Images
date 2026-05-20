# TASKS — ComicVision Sorter
<!-- Master task tracker. Update status: [ ] not started | [~] in progress | [x] done -->

Last updated: 2026-05-20

---

## BUGS (found in smoke review)

- [x] **B1 — SpreadsheetDialog queries wrong table**
  `SpreadsheetDialog::loadData()` runs `SELECT * FROM processed_images` but the DB only has a `comics` table. Every open returns no data.
  _File:_ `src/spreadsheetdialog.cpp` line ~53 — change table name to `comics` and remap column names.

- [ ] **B2 — "Grade Comic" button wired to launch.bat instead of grading engine**
  `ImageViewerWidget::onGradeComic()` calls `QProcess::startDetached("cmd.exe", launch.bat)` which relaunches the whole app. `gradingProcess` is never set so the progress bar and result panel are permanently dead.
  _File:_ `src/imageviewerwidget.cpp` — replace with `QProcess` call to `phased_grading_engine.py`.

- [x] **B3 — addToQueue silently rejects files outside root path**
  `BackendController::addToQueue` checks `absoluteFilePath().startsWith(m_rootPath)`. Any file on a different drive or folder gets silently dropped. Users navigating outside Pictures folder can never queue anything.
  _File:_ `src/backendcontroller.cpp` — remove or loosen the root restriction; update root when path changes.

- [x] **B4 — generateEbay hardcoded false in frontend**
  `handleProcessQueue` always sends `generateEbay: false`. No toggle exists in the UI, so eBay title generation is permanently disabled from the web frontend.
  _File:_ `frontend/src/ComicGrader.jsx` — add a checkbox; wire its state into the POST body.

- [x] **B5 — Ollama request format is wrong**
  Both the `Ollama` and `LMStudio` branches in `sendToAI` build identical JSON (OpenAI messages array). Ollama's native `/api/chat` format differs: it uses `images[]` base64 array + a flat `prompt` string, not `content[{type:image_url}]`.
  _File:_ `src/comicprocessor.cpp` — fix the Ollama branch to use correct Ollama API format.

- [x] **B6 — Tests crash (missing Qt DLLs in test output dir)**
  `build_root/tests/Release/` has no Qt6*.dll files → exit code `0xc0000135` on both test executables.
  _File:_ `tests/CMakeLists.txt` — add `add_custom_command` post-build step to run `windeployqt` on each test binary.

- [x] **B7 — MainWindow compiled but never instantiated**
  `main.cpp` only starts `ServiceApp`. `mainwindow.cpp`/`.h` are compiled into the binary but the class is never used. Either wire it back as an optional GUI mode or delete the dead files.
  _File:_ `src/main.cpp`, `src/mainwindow.cpp`, `src/mainwindow.h`

- [ ] **B8 — launch.bat windeployqt path is machine-specific**
  `WINDEPLOYQT` is hardcoded to `comicvision-sorter-stage1\6.7.0\msvc2019_64\bin\windeployqt6.exe` (a gitignored submodule path). Fails on any other machine.
  _File:_ `launch.bat` — find via `where windeployqt6` or use `%QTDIR%\bin`.

---

## PRIORITY 1 — eBay Bulk CSV Export (Core Blocker)

- [x] **P1-A — Implement `EbayCsvExporter` class**
  Headers: `Action, ItemID, SKU, Title, Description, Category, Price, Quantity, Condition, PictureURL1`
  Quote all fields; escape internal quotes by doubling (`""`). Target: < 10 s for 100 rows.
  _File:_ new `src/ebayexporter.cpp` / `.h`

- [x] **P1-B — Add `/api/export/csv` endpoint**
  GET → streams UTF-8 CSV of all comics (or filtered set). Response `Content-Disposition: attachment; filename=comics.csv`.
  _File:_ `src/apiserver.cpp`, `src/backendcontroller.cpp`

- [x] **P1-C — Add Export button to frontend**
  Button triggers `GET /api/export/csv` and downloads the file via an `<a href>` blob.
  _File:_ `frontend/src/ComicGrader.jsx`

- [ ] **P1-D — SKU auto-generation**
  Replace full UUID in `parseResponse` with `title-issue-<short hash>` format.
  _File:_ `src/comicprocessor.cpp`

- [ ] **P1-E — Flag UNKNOWN fields before export**
  Warn dialog listing rows where Title/Price/Condition are empty or "Unknown".
  _File:_ frontend + export endpoint validation

- [ ] **P1-F — UPC/ISBN regex extraction**
  Regex `ISBN[:\s]*[\d-]{10,17}` on OCR/notes text → populate ItemID if found.
  _File:_ `src/comicprocessor.cpp::parseResponse`

---

## PRIORITY 2 — Spreadsheet Inventory Grid

- [x] **P2-A — Fix `SpreadsheetDialog::loadData()`** ← same as B1, tracked here for priority context

- [ ] **P2-B — Row state coloring**
  Yellow = Pending Review, Green = Approved, Gray = Exported.
  _File:_ `src/spreadsheetdialog.cpp`

- [ ] **P2-C — "Approve Row" button / right-click context menu**
  Mark row green; persist `status` field to DB.
  _File:_ `src/spreadsheetdialog.cpp`, `src/spreadsheetdialog.h`

- [ ] **P2-D — Add eBay-specific columns to grid**
  Price, Condition (dropdown), Category ID, Shipping Profile.
  _File:_ `src/spreadsheetdialog.cpp`

- [ ] **P2-E — "Batch Edit eBay Details" dialog**
  Select multiple rows → set Price, Condition, Category for all at once.

- [ ] **P2-F — Status column** (`Pending | Approved | Exported`)

- [ ] **P2-G — Persist edits back to DB**
  `onCellChanged` must UPDATE the correct `comics` row for editable columns.
  _File:_ `src/spreadsheetdialog.cpp`

---

## PRIORITY 3 — Frontend Inline Edit / Delete

- [x] **P3-A — Add inline edit fields to results table**
  Title, Issue, Publisher, Year, Condition, Value are editable; PUT to `/api/comics/update`.
  _File:_ `frontend/src/ComicGrader.jsx`

- [x] **P3-B — Add Delete button per row**
  DELETE to `/api/comics?id=<id>` then refresh results.
  _File:_ `frontend/src/ComicGrader.jsx`

- [x] **P3-C — Add generateEbay toggle** ← same as B4

- [x] **P3-D — Image preview in results**
  Show thumbnail via `/image?id=<id>` endpoint (endpoint already exists in apiserver.cpp).
  _File:_ `frontend/src/ComicGrader.jsx`

---

## PRIORITY 4 — AI Parsing Improvements

- [ ] **P4-A — Fix Ollama request format** ← same as B5

- [ ] **P4-B — Expand `parseResponse`**
  Extract: `Year`, `Series` (separate from Title), `Main Characters`, `Genre`, `eBay Title`.
  _File:_ `src/comicprocessor.cpp::parseResponse`

- [ ] **P4-C — Condition → eBay code mapping**
  "Near Mint"→4000, "Very Fine"→3000, "Fine"→2750, "Very Good"→2500, "Good"→2000, "Fair"→1500, "Poor"→1000
  _File:_ `src/comicprocessor.cpp`

- [ ] **P4-D — Series vs Title disambiguation**
  Split "The Amazing Spider-Man #15" → series=`The Amazing Spider-Man`, issue=`15`.
  _File:_ `src/comicprocessor.cpp::parseResponse`

---

## PRIORITY 5 — Grading Engine Integration

- [ ] **P5-A — Wire `phased_grading_engine.py` via QProcess** ← same as B2
  Pass image path as CLI arg; read JSON lines from stdout.
  _File:_ `src/imageviewerwidget.cpp`

- [ ] **P5-B — Store grade result in DB**
  `ALTER TABLE comics ADD COLUMN grade TEXT, grade_notes TEXT` (run if missing).
  _File:_ `src/comicprocessor.cpp` or `src/backendcontroller.cpp`

- [ ] **P5-C — Grade → ConditionID mapping**
  NM(≥9.0)→4000, VF(8.0)→3000, FN(6.0)→2750, GD(2.0)→2000
  _File:_ `src/imageviewerwidget.cpp` or grading result handler

- [ ] **P5-D — Expose grade result in frontend**
  Show per-phase notes + final grade in expanded result row.

---

## PRIORITY 6 — Image Management

- [ ] **P6-A — "Prepare Images for eBay" button**
  ZIP all processed images for selected rows → `ebay_images_<date>.zip`.

- [ ] **P6-B — Thumbnail in frontend results table** (via `/image?id=`)

- [ ] **P6-C — Undo move-to-processed**
  Return image to `images/` folder from ImageViewer.

---

## PRIORITY 7 — Polish / Non-Functional

- [ ] **P7-A — Settings dialog (Qt)**
  Configure default Price, Condition, Category ID, Shipping Profile, base image URL.

- [ ] **P7-B — Validate required fields before CSV export**
  Show dialog listing incomplete rows (missing Title, Price, Condition).

- [ ] **P7-C — Document `import_gcd.py` setup**
  GCD enrichment silently skips if `gcd.db` absent. Add setup instructions to README.

- [ ] **P7-D — Progress dialog during CSV export**
  `QProgressDialog` for batches > 20 rows.

- [ ] **P7-E — Fix launch.bat windeployqt path** ← same as B8

- [ ] **P7-F — eBay upload link in UI**
  Button/tooltip opening `https://www.ebay.com/sh/lst/upload`.

---

## COMPLETED ✅

- [x] Folder browser (tree view + thumbnail view)
- [x] Double-click queue system
- [x] Ollama / LM Studio model integration
- [x] SQLite database with `comics` table
- [x] API server (TCP, port 5500) with CORS
- [x] React frontend scaffold (Vite + Tailwind)
- [x] `/api/queue`, `/api/process`, `/api/comics`, `/api/models`, `/image` endpoints
- [x] `SpreadsheetDialog` UI shell (columns defined, refresh button)
- [x] `ImageViewerWidget` (zoom, pan, WebP fallback via ImageMagick)
- [x] `phased_grading_engine.py` — 7-phase pipeline written
- [x] `test_comicprocessor.cpp` — parseResponse unit tests
- [x] `test_ebaycsv.cpp` — CSV helper unit tests (stubs ready)
- [x] GCD enrichment stub in `ComicProcessor` (opens `gcd.db` if present)
