# SAM3 Debugging & Fixing Instructions for GPT-5.1

## Overview
This document provides explicit technical instructions for fixing the SAM3 integration in the comic grading engine.

## Files Involved
- `modal_sam3.py` — Modal GPU worker. **This file is correct and requires no changes.**
- `phased_grading_engine.py` — Local grading engine. **All SAM3-related bugs are here.**

## Root Cause Summary
SAM3 returns a **list** of detection dictionaries.  
The grading engine incorrectly treats the returned list as a **dict**, leading to errors such as:

```
'list' object has no attribute 'get'
```

Because the error is caught by try/except, the engine silently falls back to dummy values, causing:
- All SAM3 phases to fail
- Zero defects detected
- Every comic graded NM 9.4

## Required Fix (Apply to All Four Phases)

### Normalize SAM3 Results
After:
```
sam3_results = Sam3Worker().run.remote(...).get()
```

Insert:

```
if isinstance(sam3_results, dict):
    if "result" in sam3_results:
        sam3_results = sam3_results["result"]
    else:
        sam3_results = [sam3_results]

if not isinstance(sam3_results, list):
    sam3_results = []
```

### Parse Safely
Avoid:
```
sam3_results.get(...)
item.get("detections")
```

Correct parsing:
```
for item in sam3_results:
    prompt = item["prompt"]
    detections = item["detections"]
```

### Early Fallback
On exception:
```
return { "bounding_boxes": [] }
```

## Expected Outcome After Fix
- SAM3 returns real detections
- Fallback no longer triggers
- Grading varies appropriately
- No more `'list' object has no attribute get'`
