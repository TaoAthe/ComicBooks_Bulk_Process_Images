# Project Resources & Configuration Reference

This file documents the key configuration details, connection strings, and resources used in the `comicvision-sorter-stage1` project. Use this as a reference when setting up new instances or related projects.

## API Keys & Endpoints

### Google Gemini API

* **API Key:** `AIzaSyAbWhdCvju5LnIPA4E8j7nO1oLzvn3U_Ko`
* **Base URL:** `https://generativelanguage.googleapis.com`
* WE ARE USING Gemini 2.5 Pro as default.  Hide other models
* **Endpoints Used:**
  * Generate Content: `/v1beta/models/{model}:generateContent`
  * List Models: `/v1beta/models`

### Local AI Server (LM Studio)

* **Base URL:** `http://localhost:1234`
* **Endpoints Used:**
  * Chat Completions: `/v1/chat/completions`
  * List Models: `/v1/models`

## Database Configuration

* **Type:** SQLite
* **Database File:** `comicvision.db` (Located in the application directory)
* **Tables:**
  * `comics`: Stores comic book metadata and images.
    * Columns: `id`, `image_path`, `image_data` (BLOB), `action`, `category`, `title`, `issue`, `publisher`, `year`, `genre`, `main_characters`, `condition`, `value`, `notes`
  * `processed_images`: Stores a log of processed images.
    * Columns: `image_path`, `metadata`

## Key Source Files

* **C++ Backend:**

  * `src/comicprocessor.cpp`: Handles API requests, database interactions, and image processing. Contains the hardcoded API keys and URLs.
  * `src/mainwindow.cpp`: Main UI logic.
* **Python Grading Engine:**

  * `phased_grading_engine.py`: Implements the multi-phase comic grading logic (Spine, Corners, Surface, etc.).
  * `modal_sam3.py`: SAM3 integration for defect detection.
* YOU CAN FIND THESE FILES HERE: G:\Big\Comicer

## Build & Deployment

* **Build System:** CMake
* **Output Directory:** `build/Release` (Windows)
* **Required Directories:**
  * `images/`: Directory for input images.
  * `processed/`: Directory for processed images.

## Notes

* The Gemini API implementation includes a rate-limit retry mechanism (handles HTTP 429 errors).
* The application supports both local (LM Studio) and cloud (Gemini) AI models for image analysis.


INSTRUCTIONS: Build a NEW idea, an efficient ebay lister app that can intelligently process comic books - creating a gallery of comics in on
