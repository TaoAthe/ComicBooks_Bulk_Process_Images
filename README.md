# ComicVision Sorter - Stage 1

## Setup
1. Install Qt 6 and CMake.
2. Install Ollama and run `ollama run llava`.
3. Build: `mkdir build && cd build && cmake .. && make`.
4. Run the executable.
5. Place a sample comic image in `images/`.
6. Click "Process Images" – Check console for AI response.

## Test
- Expected: App scans, sends to Ollama, logs raw response (e.g., "Title: Moon Knight, Issue: #1..."), moves image.
- If Ollama not running: Error in status.