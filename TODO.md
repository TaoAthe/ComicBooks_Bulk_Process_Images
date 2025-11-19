# TODO

- [ ] Add checkbox to generate eBay listing from image. Format for eBay.

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