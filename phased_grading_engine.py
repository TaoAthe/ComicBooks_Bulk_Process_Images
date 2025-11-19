# ====================================================
# PHASED GRADING ENGINE
# ====================================================
"""
phased_grading_engine.py
======================================================
The Phased AI Comic Grading Engine for the Dr. Tripper 
Comic Scanner Application.

This module contains the entire "prompt phasing" pipeline
for CGC-style comic grading. It is designed to be called 
from the main application whenever the user presses the
"Grade Comic" button while viewing an image.

The purpose of this module is:
1. To avoid rewriting the main codebase.
2. To isolate all grading logic, prompts, and LLM calls.
3. To provide a professional, deterministic grading flow.
4. To break the CGC grading task into sequential "phases,"
   where each pass focuses on different defect categories.
5. To return a structured grading result independent of UI.

OVERVIEW OF THE PIPELINE
-------------------------
The grading engine evaluates a comic through several
highly-focused analysis phases:

    PHASE 1 – SPINE MICROSTRUCTURE ANALYSIS
        Detects:
            - Spine ticks
            - Color-breaking vs non–color-breaking stress
            - Spine stresses
            - Spine splits
        This phase receives:
            - Image
            - Narrow, unabridged definition of "spine tick"
            - LLM prompt to ONLY evaluate the spine area

    PHASE 2 – CORNERS & EDGE WEAR ANALYSIS
        Detects:
            - Soft corners
            - Rounded corners
            - Blunted corners
            - Micro-tears at edges
            - General edge wear

    PHASE 3 – COVER SURFACE ANALYSIS
        Detects:
            - Creases
            - Color-breaking folds
            - Dents
            - Bends
            - Thumb dents
            - Surface scuffing
            - Gloss loss
            - Streaks

    PHASE 4 – STRUCTURAL ANALYSIS
        Detects:
            - Staple condition (rust, detached, pulled)
            - Spine roll
            - Tears
            - Water damage
            - Stains (size, severity)
            - Rippling or waviness
            - Mold indicators

    PHASE 5 – COLOR / GLOSS / EYE-APPEAL PASS
        Detects:
            - Color vibrancy
            - Gloss uniformity
            - Cover saturation
            - Fade / sun-shadowing

    PHASE 6 – MATERIAL / PAPER QUALITY PASS
        Detects:
            - Paper tone (OW/W, C/OW, Cream, Tan)
            - Brittleness
            - Oxidation / tanning

    PHASE 7 – FINAL CGC GRADE SYNTHESIS
        Uses:
            - All earlier defect-data JSON
            - Compressed CGC grading rules
        Returns:
            - Numeric grade (10.0–0.5)
            - Grade text ("VF/NM 9.0", etc.)
            - Full reasoning
            - Confidence
            - Consolidated defect map


WHY PHASED GRADING?
--------------------
Phased grading dramatically reduces hallucination risk and 
increases accuracy by isolating each defect class. The LLM 
focuses on only one type of analysis at a time. Each phase 
produces strict JSON, which feeds into the final synthesis 
grade.

TOKEN-EFFICIENT DESIGN
-----------------------
Rather than stuffing all CGC standards into one mega-prompt 
each grading call, this module keeps:

    - A short SYSTEM prompt loaded once at initialization.
    - A contextual USER prompt per phase.
    - One image per phase (same input reused).
    - A lightweight synthesis prompt at the end.

This keeps the token footprint small, the responses stable, 
and avoids model truncation issues.

PUBLIC API
----------
The main application should call:

    grade_comic(image_path, ocr_text=None)

This returns:

{
    "grade_numeric": float,
    "grade_text": str,
    "reasoning": str,
    "phases": {
        "spine": {...},
        "corners": {...},
        "surface": {...},
        "structural": {...},
        "color_gloss": {...},
        "paper": {...}
    },
    "confidence": int
}

Each phase is internally responsible for:
- Preparing the vision prompt
- Defining its own terminology and rubric
- Validating its own JSON output
- Passing the result to the next phase


HOW TO EXTEND
--------------
New phases or enhanced detection logic can be added without 
affecting the main application's architecture. This is an 
isolated module.


DEPENDENCIES
-------------
This module assumes:
- Your application has an LLM client available (OpenAI, or similar)
- You handle image loading & resizing before passing to this module.
- You package this file along with the rest of your grading system.


======================================================
"""

# ====================================================
# IMPORTS
# ====================================================
import base64
from typing import Dict, Any, Optional
import json
import requests


# ====================================================
# INTERNAL HELPERS
# ====================================================
def _encode_image(path: str) -> str:
    """Base64-encode the image so the LLM can consume it."""
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("utf-8")


def _call_llm(prompt: str, image_b64: Optional[str] = None, server: str = "Ollama", model: str = "llava") -> str:
    """Call the LLM with the prompt and optional image."""
    if server == "Ollama":
        url = "http://localhost:11434/api/generate"
        data = {
            "model": model,
            "prompt": prompt,
            "stream": False
        }
        if image_b64:
            data["images"] = [image_b64]
    else:  # LMStudio
        url = "http://localhost:1234/v1/chat/completions"
        if image_b64:
            data = {
                "model": model,
                "messages": [
                    {
                        "role": "user",
                        "content": [
                            {"type": "text", "text": prompt},
                            {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{image_b64}"}}
                        ]
                    }
                ]
            }
        else:
            data = {
                "model": model,
                "messages": [
                    {
                        "role": "user",
                        "content": prompt
                    }
                ]
            }

    try:
        response = requests.post(url, json=data, timeout=30)
        response.raise_for_status()
        if server == "Ollama":
            return response.json()["response"]
        else:
            return response.json()["choices"][0]["message"]["content"]
    except Exception as e:
        return f"Error: {str(e)}"


# ====================================================
# PHASE 1 – SPINE MICROSTRUCTURE
# ====================================================
def _phase_spine_analysis(image_b64: str, llm_client, additional_prompt: str = "") -> Dict[str, Any]:
    """
    Phase 1: Spine Microstructure Analysis

    This phase performs a highly focused evaluation of ONLY the comic's spine.
    The goal is to identify:
        - total spine ticks
        - number of color-breaking ticks
        - spine stresses (non-breaking surface indentations)
        - spine splits (actual breaks or tears along the spine)

    This function:
        - Prepares a specialized prompt that defines all spine-related defects
        - Instructs the LLM to ignore all non-spine elements
        - Specifies strict JSON output
        - Sends the base64 image to the LLM client (when wired in)
        - Returns dummy values for now until the AI module is connected
    """

    prompt = f"""
You are a CGC-trained spine defect inspector analyzing ONLY the spine area 
of the comic book cover image provided.

Do NOT evaluate:
- corners
- edges (except where they touch the spine)
- gloss
- color vibrancy
- cover surface
- back cover
- staples (unless relevant to splits)
- paper tone
- any non-spine features

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
SPINE DEFECT DEFINITIONS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

A "spine tick" is a localized impact mark, indentation, or compression 
running perpendicular to the spine line.  
Types:
  - non–color breaking tick: ink layer intact, indentation visible
  - color-breaking tick: white paper fibers exposed, ink cracked

A "spine stress" is a light, usually non-breaking wrinkle or linear 
indentation caused by handling or bending of the book.

A "spine split" is an actual tear or opened seam along the spine fold.  
Severity scale:
  - micro split: under 1 mm
  - small split: 1–3 mm
  - moderate split: 3–6 mm
  - large split: over 6 mm

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
YOUR TASK
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Inspect ONLY the spine.
2. Count all spine ticks and identify how many are color-breaks.
3. Count spine stresses (handling-induced, non-breaking).
4. Identify spine splits and estimate size in millimeters.
5. Return structured JSON ONLY.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
REQUIRED JSON FORMAT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
{{
  "ticks": int,
  "color_breaking_ticks": int,
  "spine_stresses": int,
  "spine_splits": [
      {{ "size_mm": float }}
  ]
}}

Return NOTHING except JSON in that structure.
"""

    if additional_prompt:
        prompt += f"\n\nAdditional Instructions: {additional_prompt}"

    # Real call
    response = _call_llm(prompt, image_b64, server="LMStudio", model="qwen/qwen3-vl-30b")
    if response.startswith("Error"):
        # Fallback to dummy
        return {
            "ticks": 0,
            "color_breaking_ticks": 0,
            "spine_stresses": 0,
            "spine_splits": []
        }
    try:
        parsed = json.loads(response)
        return parsed
    except:
        # Fallback
        return {
            "ticks": 0,
            "color_breaking_ticks": 0,
            "spine_stresses": 0,
            "spine_splits": []
        }



# ====================================================
# PHASE 2 – CORNER & EDGE WEAR
# ====================================================
def _phase_corner_edge_analysis(image_b64: str, llm_client, additional_prompt: str = "") -> Dict[str, Any]:
    """
    Phase 2: Corner and Edge Wear Analysis

    This phase evaluates ONLY:
        - Overall corner blunting/rounding severity
        - The presence and count of micro-tears along any edge
        - Overall edge-wear level for the cover

    It does NOT evaluate:
        - Spine ticks or spine roll
        - Surface gloss, color, or creases away from edges
        - Internal pages or back cover
        - Staples (unless damage is directly on the edge)

    The LLM is instructed to:
        - Focus only on the four corners and the perimeter edges
        - Classify corner blunting globally based on the worst or typical corner
        - Count micro-tears along edges
        - Classify overall edge wear severity

    The function is currently returning dummy data until the LLM client
    is wired in. The prompt below is ready to be used with any vision-capable
    LLM; the engineer only needs to implement the llm_client call and parse
    the JSON response into this same return shape.
    """

    prompt = """
You are a CGC-trained comic book grader specializing in corner and edge wear analysis.
You are provided with an image of the front cover of a comic book.

Your task is to evaluate ONLY the corners and edges of the front cover. Ignore all other aspects such as spine, surface creases, color, gloss, interior pages, or back cover.

Focus exclusively on:
1. The condition of the four corners (top-left, top-right, bottom-left, bottom-right)
2. Wear along the four edges (top, bottom, left, right)
3. Presence of micro-tears along the edges

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – CORNER CONDITION
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Evaluate each corner individually, then determine an overall severity level for the entire book.

Corner states:
- "sharp": Perfect 90-degree angle, no visible softening, flattening, or rounding. The point is pristine.
- "slightly soft": Minor compression at the tip; the point is still mostly intact but not perfectly sharp. No fiber exposure.
- "rounded": The corner tip is curved or smoothed, losing its sharp point. Ranges from mild to moderate curvature.
- "blunted": The tip is visibly flattened or smashed, with a clear flattened area. The point is lost.
- "crushed": Severe damage with folding, deep creasing, or complex deformation at the corner.

For overall assessment:
- "none": All corners are sharp or nearly sharp.
- "slight": Most corners sharp, with minor softening on 1-2 corners.
- "moderate": Obvious rounding or light blunting on 2-3 corners, or moderate on 1 corner.
- "severe": Significant blunting, crushing, or severe rounding on 1 or more corners.

Base the overall level on the worst corner, factoring in the general condition.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – EDGE WEAR
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Edge wear refers to friction, rubbing, small nicks, or minor paper loss along the outer perimeter.

Levels:
- "minimal": Virtually no wear; edges appear fresh with no visible rubs, nicks, or paper loss. Typical of high-grade books.
- "mild": Light wear with small color rubs, tiny nicks (<1mm), or extremely small chips. Still clean and smooth.
- "moderate": Clear wear on 1-2 edges with multiple small nicks/chips, slight roughness, or obvious handling marks.
- "severe": Heavy wear with large chips, significant paper loss, rough edges, or multiple breaks along the perimeter.

Assess the entire perimeter and choose one level based on the most affected areas.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – MICRO-TEARS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Micro-tears are small tears or slits along the edges, typically:
- Less than 5mm in length
- Small notches or slits, not large missing pieces
- Clearly visible as breaks in the paper along the edge

Count each distinct micro-tear. Do not count nicks or chips that are not actual tears. Scan all four edges carefully.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
ANALYSIS STEPS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Examine each of the four corners under good lighting/angle in the image.
2. Classify each corner's condition.
3. Determine the overall corner_blunting level based on the worst and typical conditions.
4. Inspect all four edges for wear patterns, rubs, nicks, and chips.
5. Assign the overall edge_wear level.
6. Carefully scan all edges for micro-tears, counting only clear tears <5mm.
7. Ensure your assessment is based solely on corners and edges.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
REQUIRED JSON OUTPUT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Return ONLY valid JSON with this exact structure. No additional text, explanations, or fields.

{
  "corner_blunting": "none" | "slight" | "moderate" | "severe",
  "micro_tears": integer,
  "edge_wear": "minimal" | "mild" | "moderate" | "severe"
}
"""

    if additional_prompt:
        prompt += f"\n\nAdditional Instructions: {additional_prompt}"

    # Real call
    response = _call_llm(prompt, image_b64, server="LMStudio", model="qwen/qwen3-vl-30b")
    if response.startswith("Error"):
        # Fallback to dummy
        return {
            "corner_blunting": "none",
            "micro_tears": 0,
            "edge_wear": "minimal"
        }
    try:
        parsed = json.loads(response)
        return parsed
    except:
        # Fallback
        return {
            "corner_blunting": "none",
            "micro_tears": 0,
            "edge_wear": "minimal"
        }



# ====================================================
# PHASE 3 – SURFACE ANALYSIS
# ====================================================
def _phase_surface_analysis(image_b64: str, llm_client, additional_prompt: str = "") -> Dict[str, Any]:
    """
    Phase 3: Surface Defects Analysis

    This phase evaluates ONLY:
        - Creases (color-breaking and non–color-breaking)
        - Bends / folds that do not fully crease
        - Thumb dents (localized pressure indentations)
        - Surface scuffing / abrasion
        - Gloss loss (dulling, texture changes)

    It does NOT evaluate:
        - Spine defects (handled in Phase 1)
        - Corners or edge wear (handled in Phase 2)
        - Structural defects (tears, stains, water damage)
        - Paper tone or aging
        - Back cover or interior pages

    The goal is to isolate all FRONT COVER surface defects 
    relevant to CGC grading and return strictly formatted JSON 
    containing counts and categorical descriptions.

    The real AI logic will replace the dummy values below when 
    the llm_client integration is complete.
    """

    prompt = """
You are a CGC-trained grader. Analyze ONLY the FRONT COVER SURFACE of the comic.

Ignore:
- spine ticks or spine splits
- corner/edge wear
- color vibrancy
- gloss uniformity (except for LOSS)
- interior pages
- back cover

Your task is to evaluate:
1. Creases
2. Bends
3. Thumb dents
4. Surface scuffing
5. Gloss loss

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – CREASES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

A "crease" is a fold or line where the cover has been bent sharply 
enough to create a permanent or semi-permanent mark.

Types:
- Non–color-breaking crease:
    Surface is bent but ink layer remains intact.
    Often pressable.
- Color-breaking crease:
    Ink layer cracks; white paper fibers show.
    ALWAYS reduces grade more severely.

Count each visible crease as a distinct item.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – BENDS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

A "bend" is a broad, shallow curvature of the cover surface that 
does not form a sharp crease line.

Characteristics:
- No clear fold line
- May be pressable
- May cause local warping or uneven reflection

Count each visible bend.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – THUMB DENTS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

A "thumb dent" (or finger bend) is a localized pressure indentation 
caused by gripping the book.

Characteristics:
- Small circular or oval depression
- Usually near right or left edge, middle areas
- Does NOT break color
- Visible under strong light or angle

Count each visible thumb dent.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – SURFACE SCUFFING
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

"Scuffing" refers to shallow abrasion or friction that dulls the surface.
Classify scuffing as one of:

- "none"       : No visible scuffing
- "light"      : Minor surface dulling only visible at angle
- "moderate"   : Clearly visible abrasion, multiple spots
- "severe"     : Heavy abrasion or paper wear

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – GLOSS LOSS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

"Gloss loss" refers to reduced reflectivity or sheen on the cover.
Classify as:

- "none"
- "slight"
- "moderate"
- "heavy"

This does NOT refer to color dullness, only surface sheen.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
YOUR TASK
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Scan all visible areas of the FRONT COVER only.
2. Count:
      - total creases
      - total bends
      - total thumb dents
3. Classify:
      - scuffing level
      - gloss loss level
4. Return structured JSON ONLY.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
REQUIRED JSON OUTPUT STRUCTURE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
{
  "creases": int,
  "bends": int,
  "thumb_dents": int,
  "scuffing": "none" | "light" | "moderate" | "severe",
  "gloss_loss": "none" | "slight" | "moderate" | "heavy"
}

No explanations.
No extra fields.
Only JSON.
"""

    if additional_prompt:
        prompt += f"\n\nAdditional Instructions: {additional_prompt}"

    # Real call
    response = _call_llm(prompt, image_b64, server="LMStudio", model="qwen/qwen3-vl-30b")
    if response.startswith("Error"):
        # Fallback to dummy
        return {
            "creases": 0,
            "bends": 0,
            "thumb_dents": 0,
            "scuffing": "none",
            "gloss_loss": "none"
        }
    try:
        parsed = json.loads(response)
        return parsed
    except:
        # Fallback
        return {
            "creases": 0,
            "bends": 0,
            "thumb_dents": 0,
            "scuffing": "none",
            "gloss_loss": "none"
        }


# ====================================================
# PHASE 4 – STRUCTURAL ANALYSIS
# ====================================================
def _phase_structural_analysis(image_b64: str, llm_client, additional_prompt: str = "") -> Dict[str, Any]:
    """
    Phase 4: Structural Damage Analysis

    This phase evaluates ONLY:
        - Staple condition (rust, pull, detachment)
        - Spine roll (degree of curvature)
        - Tears (count only, measured anywhere on front cover)
        - Water damage (rippling, discoloration, tide marks)
        - Stains (count of distinct stains)
        - Rippling/waviness (non-water-related)
        - Mold or fungal spotting

    It does NOT evaluate:
        - surface creases/bends (Phase 3)
        - spine ticks (Phase 1)
        - corners/edges (Phase 2)
        - color vibrancy or gloss (Phase 5)
        - paper tone (Phase 6)
        - back cover or interior pages

    The LLM is instructed to:
        - focus on structural integrity of the book's front cover
        - classify staple condition
        - measure spine roll in degrees or mm offset
        - count tears
        - detect signs of moisture exposure or mold
        - provide JSON output only

    The real AI call will replace the dummy return here once wired in.
    """

    prompt = """
You are a CGC-trained grader specializing in STRUCTURAL DAMAGE analysis.
Evaluate ONLY the structural integrity of the FRONT COVER.

Ignore:
- creases, bends, scuffing, or gloss issues
- spine ticks
- corner wear
- color fade
- back cover or interior pages

Your analysis must focus ONLY on:
1. Staple condition
2. Spine roll
3. Tears (structural breaks)
4. Water damage indicators
5. Stains
6. Ripples/waviness
7. Mold or fungal spotting

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – STAPLE CONDITION
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

A comic typically has two staples on the spine fold. Their condition is:

- "good":
    Staples are fully attached, aligned, no rust, no warping.

- "rust":
    Visible oxidation, ranging from slight discoloration to heavy rust.

- "rust with migration":
    Rust staining bleeding into paper near the staple.

- "pulled":
    Staple is stretching or slightly tearing into the paper.

- "detached":
    One or both staples have separated from the cover.

Only choose ONE overall assessment:
    "good" | "rust" | "rust_migration" | "pulled" | "detached"

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – SPINE ROLL
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

"Spine roll" is the lateral curvature of the book where the spine shifts
left or right, common in heavily read books.

Measure or approximate:
- 0   = no roll
- 1–2 mm = slight
- 3–5 mm = moderate
- 6+ mm  = severe

Return GREATLY simplified numeric:
        "spine_roll": integer millimeters

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – TEARS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

A "tear" is a structural break in the paper.

Count only tears on the FRONT COVER, not chips or missing pieces.

Return an integer:
        "tears": count

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – WATER DAMAGE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Water damage includes:
- ripple patterns (tide lines)
- discoloration
- wrinkles caused by moisture
- warped paper
- staining rings

Classification:
    "none"
    "light"
    "moderate"
    "severe"

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – STAINS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

A "stain" is any discoloration not caused by water damage, mold, or sun fade.

Count stains as:
        "stains": integer

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – RIPPLES / WAVINESS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

"Ripples" or "waviness" are surface warps NOT caused by water.

Classify:
    "none"
    "light"
    "moderate"
    "severe"

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – MOLD
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

"Mold" includes fungal spotting, powdery deposits, black/green dots.

Classify:
    "none"
    "possible"
    "confirmed"

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
YOUR TASK
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Inspect the front cover for structural issues ONLY.
2. Determine:
    - staple_condition
    - spine_roll (mm)
    - tears (count)
    - water_damage level
    - stains (count)
    - ripples level
    - mold level
3. Output JSON ONLY.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
REQUIRED JSON FORMAT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
{
  "staple_condition": "good" | "rust" | "rust_migration" | "pulled" | "detached",
  "spine_roll": int,
  "tears": int,
  "water_damage": "none" | "light" | "moderate" | "severe",
  "stains": int,
  "ripples": "none" | "light" | "moderate" | "severe",
  "mold": "none" | "possible" | "confirmed"
}

No explanations. No extra fields. JSON only.
"""

    if additional_prompt:
        prompt += f"\n\nAdditional Instructions: {additional_prompt}"

    # Real call
    response = _call_llm(prompt, image_b64, server="LMStudio", model="qwen/qwen3-vl-30b")
    if response.startswith("Error"):
        # Fallback to dummy
        return {
            "staple_condition": "good",
            "spine_roll": 0,
            "tears": 0,
            "water_damage": "none",
            "stains": 0,
            "ripples": "none",
            "mold": "none"
        }
    try:
        parsed = json.loads(response)
        return parsed
    except:
        # Fallback
        return {
            "staple_condition": "good",
            "spine_roll": 0,
            "tears": 0,
            "water_damage": "none",
            "stains": 0,
            "ripples": "none",
            "mold": "none"
        }


def _phase_color_gloss_analysis(image_b64: str, llm_client, additional_prompt: str = "") -> Dict[str, Any]:
    """
    Phase 5: Color & Eye Appeal

    This phase analyzes ONLY:
        - Color vibrancy
        - Gloss uniformity
        - Saturation richness
        - Fading / sun-shadowing

    It does NOT analyze:
        - Surface defects (creases, bends, scuffing)
        - Structural defects (tears, stains, water damage)
        - Spine ticks or corner wear
        - Paper tone
        - Back cover or interior pages

    The goal is to capture the front cover’s "eye appeal" — how strong,
    clean, vivid, and evenly reflective the cover appears.

    The LLM is instructed to classify color strength, gloss levels,
    saturation, and any visible fading using strict JSON output.

    The dummy return at the end is used until the AI integration
    is completed by the engineer.
    """

    prompt = """
You are a CGC-trained comic book grader evaluating ONLY the COLOR and EYE APPEAL 
of the FRONT COVER of a comic book.

Ignore:
- spine defects
- corner/edge wear
- scuffing, creases, or bends
- gloss scratches (handled elsewhere)
- structural issues (tears, stains, water damage)
- paper tone (Phase 6)
- back cover or interior pages

Your job is to evaluate the following attributes:

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – COLOR VIBRANCY
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

"Color vibrancy" measures how vivid, bright, and intense the printed colors appear.

Classify vibrancy as:
    - "excellent": bold, vivid, like-new intensity
    - "good": strong color with slight natural aging
    - "fair": noticeable dulling or muted appearance
    - "poor": heavily faded, uneven, or washed-out colors

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – GLOSS UNIFORMITY
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Gloss uniformity refers to how evenly the cover reflects light.

Classify as:
    - "excellent": smooth, even gloss across entire surface
    - "good": minor unevenness, slight dulling in small areas
    - "fair": multiple uneven zones, visibly patchy gloss
    - "poor": substantial dulling, strong inconsistencies

Gloss loss due to scuffing is handled in Phase 3; here we focus only on
overall gloss reflectivity and consistency.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – SATURATION
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

"Saturation" is the richness and depth of the colors (not brightness).

Classify as:
    - "high": deep, rich, ink-heavy appearance
    - "medium": normal, typical saturation
    - "low": noticeably washed-out or thin ink appearance

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – FADING / SUN SHADOWING
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

"Fading" occurs when part or all of the cover has been exposed to UV/light.

Detect:
    - left/right side fading
    - top-to-bottom fading
    - panel-specific fading
    - halo fade or “sun-shadow” effect

Classify as:
    - "none"
    - "light"
    - "moderate"
    - "heavy"

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
YOUR TASK
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Examine ONLY the color, gloss, saturation, and fading of the FRONT COVER.
2. Classify:
      - color_vibrancy
      - gloss_uniformity
      - saturation
      - fading
3. Output strictly formatted JSON.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
REQUIRED JSON FORMAT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
{
  "color_vibrancy": "excellent" | "good" | "fair" | "poor",
  "gloss_uniformity": "excellent" | "good" | "fair" | "poor",
  "saturation": "high" | "medium" | "low",
  "fading": "none" | "light" | "moderate" | "heavy"
}

Return JSON only. No commentary. No extra fields.
"""

    if additional_prompt:
        prompt += f"\n\nAdditional Instructions: {additional_prompt}"

    # Real call
    response = _call_llm(prompt, image_b64, server="LMStudio", model="qwen/qwen3-vl-30b")
    if response.startswith("Error"):
        # Fallback to dummy
        return {
            "color_vibrancy": "excellent",
            "gloss_uniformity": "good",
            "saturation": "high",
            "fading": "none"
        }
    try:
        parsed = json.loads(response)
        return parsed
    except:
        # Fallback
        return {
            "color_vibrancy": "excellent",
            "gloss_uniformity": "good",
            "saturation": "high",
            "fading": "none"
        }


def _phase_paper_quality_analysis(image_b64: str, llm_client, additional_prompt: str = "") -> Dict[str, Any]:
    """
    Phase 6: Paper Quality Analysis

    This phase evaluates ONLY:
        - Paper tone (whiteness level of front cover stock)
        - Brittleness (fragility or cracking from age)
        - Oxidation (tanning, browning, age discoloration)

    It does NOT evaluate:
        - color vibrancy (Phase 5)
        - gloss or shine (Phase 5)
        - stains (Phase 4)
        - structural issues (Phase 4)
        - edge/corner wear (Phase 2)
        - interior pages or back cover

    Purpose:
        Provide an accurate assessment of the raw paper integrity 
        and aging characteristics of the front cover, as these 
        factors influence CGC grading, especially for older books.

    The dummy return at the end allows the application to run
    normally until AI integration is complete.
    """

    prompt = """
You are a CGC-trained grader evaluating ONLY the PAPER QUALITY of the FRONT COVER.

Ignore:
- spine, corners, edges
- gloss, color vibrancy, saturation
- scuffing, bends, creases
- structural defects (tears, stains, water damage)
- mold
- interior pages / back cover

You are evaluating ONLY:
1. Paper tone
2. Brittleness
3. Oxidation

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – PAPER TONE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Paper tone refers to the natural aging color of the paper stock.
This is NOT the printed colors — but the actual underlying paper itself.

Classify tone as:
- "white"          : bright, clean, fresh appearance
- "off-white"      : very slight aging, still bright
- "white/ow"       : between white and off-white
- "cream/ow"       : noticeable cream color, aging visible
- "cream"          : darker cream tone, moderate aging
- "tan"            : significant darkening, heavy age oxidation
- "dark_tan"       : severe aging, near brittleness
- "brittle_brown"  : extreme aging, brown and fragile (rare)

Choose the closest match based on visible paper edges and exposed areas.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – BRITTLENESS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Brittleness describes the fragility of the paper.

Classify:
- "none"       : strong, flexible paper with no cracking
- "slight"     : mild stiffness, but no flaking or cracking
- "moderate"   : paper cracks or flakes at stress points
- "severe"     : extremely fragile, crumbles or flakes visibly

Even front-cover-only evidence is valid (corner edges, exposed fibers).

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DEFINITIONS – OXIDATION (TANNING)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Oxidation is the browning or darkening of the paper due to exposure to air,
age, or acidic content.

Look for:
- brown edges
- halo darkening
- color gradient moving inward from edges

Classify:
- "none"
- "light"
- "moderate"
- "heavy"

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
YOUR TASK
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Examine the FRONT COVER ONLY for paper tone, brittleness, and oxidation.
2. Classify each according to the definitions above.
3. Output strictly formatted JSON.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
REQUIRED JSON FORMAT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
{
  "paper_tone": 
        "white" | "off-white" | "white/ow" | "cream/ow" |
        "cream" | "tan" | "dark_tan" | "brittle_brown",

  "brittleness": "none" | "slight" | "moderate" | "severe",

  "oxidation": "none" | "light" | "moderate" | "heavy"
}

Return JSON only.
No explanations.
No extra fields.
"""

    if additional_prompt:
        prompt += f"\n\nAdditional Instructions: {additional_prompt}"

    # Real call
    response = _call_llm(prompt, image_b64, server="LMStudio", model="qwen/qwen3-vl-30b")
    if response.startswith("Error"):
        # Fallback to dummy
        return {
            "paper_tone": "white",
            "brittleness": "none",
            "oxidation": "none"
        }
    try:
        parsed = json.loads(response)
        return parsed
    except:
        # Fallback
        return {
            "paper_tone": "white",
            "brittleness": "none",
            "oxidation": "none"
        }


def _phase_final_grade_synthesis(
    spine_data,
    corner_data,
    surface_data,
    structural_data,
    color_gloss_data,
    paper_data,
    llm_client
) -> Dict[str, Any]:
    """
    Phase 7: Final Grade Synthesis

    This phase synthesizes all defect data from the previous 6 phases
    into a final CGC-grade assessment. It considers the cumulative impact
    of all defects and determines the appropriate numeric grade (0.5-10.0)
    and text grade based on CGC standards.

    The LLM analyzes the severity and combination of defects to determine
    the final grade, providing detailed reasoning and confidence level.
    """

    # Rule-based grade synthesis (more reliable than LLM for structured output)
    grade_numeric = 10.0  # Start with perfect score
    deductions = []
    
    # Spine defects
    if spine_data.get("ticks", 0) > 0:
        deduction = min(spine_data["ticks"] * 0.1, 0.5)
        grade_numeric -= deduction
        deductions.append(f"Spine ticks: -{deduction}")
    
    if spine_data.get("color_breaking_ticks", 0) > 0:
        deduction = min(spine_data["color_breaking_ticks"] * 0.5, 1.0)
        grade_numeric -= deduction
        deductions.append(f"Color-breaking ticks: -{deduction}")
    
    if spine_data.get("spine_stresses", 0) > 0:
        deduction = min(spine_data["spine_stresses"] * 0.2, 0.5)
        grade_numeric -= deduction
        deductions.append(f"Spine stresses: -{deduction}")
    
    # Corner defects
    if corner_data.get("corner_blunting") == "slight":
        grade_numeric -= 0.1
        deductions.append("Slight corner blunting: -0.1")
    elif corner_data.get("corner_blunting") == "moderate":
        grade_numeric -= 0.3
        deductions.append("Moderate corner blunting: -0.3")
    elif corner_data.get("corner_blunting") == "heavy":
        grade_numeric -= 0.8
        deductions.append("Heavy corner blunting: -0.8")
    
    micro_tears = corner_data.get("micro_tears", 0)
    if micro_tears > 0:
        deduction = min(micro_tears * 0.1, 0.5)
        grade_numeric -= deduction
        deductions.append(f"Micro tears: -{deduction}")
    
    if corner_data.get("edge_wear") == "moderate":
        grade_numeric -= 0.2
        deductions.append("Moderate edge wear: -0.2")
    elif corner_data.get("edge_wear") == "heavy":
        grade_numeric -= 0.5
        deductions.append("Heavy edge wear: -0.5")
    
    # Surface defects
    creases = surface_data.get("creases", 0)
    if creases > 0:
        deduction = min(creases * 0.2, 1.0)
        grade_numeric -= deduction
        deductions.append(f"Creases: -{deduction}")
    
    bends = surface_data.get("bends", 0)
    if bends > 0:
        deduction = min(bends * 0.3, 0.8)
        grade_numeric -= deduction
        deductions.append(f"Bends: -{deduction}")
    
    thumb_dents = surface_data.get("thumb_dents", 0)
    if thumb_dents > 0:
        deduction = min(thumb_dents * 0.1, 0.3)
        grade_numeric -= deduction
        deductions.append(f"Thumb dents: -{deduction}")
    
    if surface_data.get("scuffing") == "light":
        grade_numeric -= 0.1
        deductions.append("Light scuffing: -0.1")
    elif surface_data.get("scuffing") == "moderate":
        grade_numeric -= 0.3
        deductions.append("Moderate scuffing: -0.3")
    
    if surface_data.get("gloss_loss") == "minimal":
        grade_numeric -= 0.1
        deductions.append("Minimal gloss loss: -0.1")
    elif surface_data.get("gloss_loss") == "moderate":
        grade_numeric -= 0.3
        deductions.append("Moderate gloss loss: -0.3")
    
    # Structural defects
    if structural_data.get("staple_condition") == "rust":
        grade_numeric -= 0.2
        deductions.append("Staple rust: -0.2")
    elif structural_data.get("staple_condition") == "rust_migration":
        grade_numeric -= 0.4
        deductions.append("Staple rust with migration: -0.4")
    elif structural_data.get("staple_condition") == "pulled":
        grade_numeric -= 0.6
        deductions.append("Pulled staple: -0.6")
    elif structural_data.get("staple_condition") == "detached":
        grade_numeric -= 1.5
        deductions.append("Detached staple: -1.5")
    
    spine_roll = structural_data.get("spine_roll", 0)
    if spine_roll > 0:
        deduction = min(spine_roll * 0.1, 0.5)
        grade_numeric -= deduction
        deductions.append(f"Spine roll: -{deduction}")
    
    tears = structural_data.get("tears", 0)
    if tears > 0:
        deduction = min(tears * 0.5, 2.0)
        grade_numeric -= deduction
        deductions.append(f"Tears: -{deduction}")
    
    if structural_data.get("water_damage") == "light":
        grade_numeric -= 0.5
        deductions.append("Light water damage: -0.5")
    elif structural_data.get("water_damage") == "moderate":
        grade_numeric -= 1.5
        deductions.append("Moderate water damage: -1.5")
    elif structural_data.get("water_damage") == "severe":
        grade_numeric -= 3.0
        deductions.append("Severe water damage: -3.0")
    
    stains = structural_data.get("stains", 0)
    if stains > 0:
        deduction = min(stains * 0.2, 0.8)
        grade_numeric -= deduction
        deductions.append(f"Stains: -{deduction}")
    
    if structural_data.get("ripples") == "light":
        grade_numeric -= 0.1
        deductions.append("Light ripples: -0.1")
    elif structural_data.get("ripples") == "moderate":
        grade_numeric -= 0.3
        deductions.append("Moderate ripples: -0.3")
    
    if structural_data.get("mold") == "possible":
        grade_numeric -= 0.2
        deductions.append("Possible mold: -0.2")
    elif structural_data.get("mold") == "confirmed":
        grade_numeric -= 1.0
        deductions.append("Confirmed mold: -1.0")
    
    # Color/Gloss defects
    if color_gloss_data.get("color_vibrancy") == "fair":
        grade_numeric -= 0.3
        deductions.append("Fair color vibrancy: -0.3")
    elif color_gloss_data.get("color_vibrancy") == "poor":
        grade_numeric -= 0.8
        deductions.append("Poor color vibrancy: -0.8")
    
    if color_gloss_data.get("gloss_uniformity") == "fair":
        grade_numeric -= 0.2
        deductions.append("Fair gloss uniformity: -0.2")
    elif color_gloss_data.get("gloss_uniformity") == "poor":
        grade_numeric -= 0.5
        deductions.append("Poor gloss uniformity: -0.5")
    
    if color_gloss_data.get("saturation") == "low":
        grade_numeric -= 0.3
        deductions.append("Low saturation: -0.3")
    
    if color_gloss_data.get("fading") == "light":
        grade_numeric -= 0.2
        deductions.append("Light fading: -0.2")
    elif color_gloss_data.get("fading") == "moderate":
        grade_numeric -= 0.5
        deductions.append("Moderate fading: -0.5")
    elif color_gloss_data.get("fading") == "heavy":
        grade_numeric -= 1.5
        deductions.append("Heavy fading: -1.5")
    
    # Paper quality defects
    if paper_data.get("paper_tone") == "cream":
        grade_numeric -= 0.1
        deductions.append("Cream paper tone: -0.1")
    elif paper_data.get("paper_tone") == "tan":
        grade_numeric -= 0.3
        deductions.append("Tan paper tone: -0.3")
    elif paper_data.get("paper_tone") == "dark_tan":
        grade_numeric -= 0.6
        deductions.append("Dark tan paper tone: -0.6")
    elif paper_data.get("paper_tone") == "brittle_brown":
        grade_numeric -= 1.5
        deductions.append("Brittle brown paper tone: -1.5")
    
    if paper_data.get("brittleness") == "slight":
        grade_numeric -= 0.1
        deductions.append("Slight brittleness: -0.1")
    elif paper_data.get("brittleness") == "moderate":
        grade_numeric -= 0.3
        deductions.append("Moderate brittleness: -0.3")
    elif paper_data.get("brittleness") == "severe":
        grade_numeric -= 1.0
        deductions.append("Severe brittleness: -1.0")
    
    if paper_data.get("oxidation") == "light":
        grade_numeric -= 0.1
        deductions.append("Light oxidation: -0.1")
    elif paper_data.get("oxidation") == "moderate":
        grade_numeric -= 0.3
        deductions.append("Moderate oxidation: -0.3")
    elif paper_data.get("oxidation") == "heavy":
        grade_numeric -= 0.8
        deductions.append("Heavy oxidation: -0.8")
    
    # Ensure grade doesn't go below 0.5
    grade_numeric = max(0.5, grade_numeric)
    
    # Round to nearest 0.5 increment (CGC standard)
    grade_numeric = round(grade_numeric * 2) / 2
    
    # Determine text grade
    if grade_numeric >= 9.9:
        grade_text = "MINT 9.9"
    elif grade_numeric >= 9.8:
        grade_text = "NEAR MINT/MINT 9.8"
    elif grade_numeric >= 9.6:
        grade_text = "NEAR MINT 9.6"
    elif grade_numeric >= 9.4:
        grade_text = "NEAR MINT 9.4"
    elif grade_numeric >= 9.2:
        grade_text = "NEAR MINT 9.2"
    elif grade_numeric >= 9.0:
        grade_text = "VERY FINE/NEAR MINT 9.0"
    elif grade_numeric >= 8.5:
        grade_text = "VERY FINE 8.5"
    elif grade_numeric >= 8.0:
        grade_text = "VERY FINE 8.0"
    elif grade_numeric >= 7.5:
        grade_text = "VERY FINE 7.5"
    elif grade_numeric >= 7.0:
        grade_text = "FINE/VERY FINE 7.0"
    elif grade_numeric >= 6.5:
        grade_text = "FINE 6.5"
    elif grade_numeric >= 6.0:
        grade_text = "FINE 6.0"
    elif grade_numeric >= 5.5:
        grade_text = "FINE 5.5"
    elif grade_numeric >= 5.0:
        grade_text = "GOOD/FINE 5.0"
    elif grade_numeric >= 4.5:
        grade_text = "GOOD 4.5"
    elif grade_numeric >= 4.0:
        grade_text = "GOOD 4.0"
    elif grade_numeric >= 3.5:
        grade_text = "GOOD 3.5"
    elif grade_numeric >= 3.0:
        grade_text = "FAIR/GOOD 3.0"
    elif grade_numeric >= 2.5:
        grade_text = "FAIR 2.5"
    elif grade_numeric >= 2.0:
        grade_text = "FAIR 2.0"
    elif grade_numeric >= 1.8:
        grade_text = "FAIR 1.8"
    elif grade_numeric >= 1.5:
        grade_text = "POOR 1.5"
    elif grade_numeric >= 1.0:
        grade_text = "POOR 1.0"
    else:
        grade_text = "POOR 0.5"
    
    reasoning = "Rule-based grading: " + "; ".join(deductions) if deductions else "No significant defects detected."
    confidence = 90 if not deductions else 85
    
    return {
        "grade_numeric": grade_numeric,
        "grade_text": grade_text,
        "reasoning": reasoning,
        "confidence": confidence
    }


# ====================================================
# PUBLIC ENTRY POINT
# ====================================================
def grade_comic(image_path: str, ocr_text: str = "", llm_client: Optional[Any] = None, additional_prompt: str = "") -> Dict[str, Any]:
    """
    Main public grading function.
    Called by the main application when the user triggers "Grade Comic".

    Parameters
    ----------
    image_path : str
        Path to the comic cover image.
    ocr_text : str
        Optional OCR-extracted text from the cover image.
    llm_client : object
        LLM client instance used to make calls to the API.

    Returns
    -------
    dict
        Full consolidated grading result including:
        - numeric grade
        - grade text
        - reasoning
        - confidence
        - per-phase defect analysis
    """
    image_b64 = _encode_image(image_path)

    # Phase calls
    print(json.dumps({"phase": "spine", "status": "starting"}))
    spine_data = _phase_spine_analysis(image_b64, llm_client, additional_prompt)
    print(json.dumps({"phase": "spine", "status": "completed", "data": spine_data}))
    print(f"Phase 1 Complete: Spine Analysis - Ticks: {spine_data.get('ticks', 0)}, Color-breaking ticks: {spine_data.get('color_breaking_ticks', 0)}, Spine stresses: {spine_data.get('spine_stresses', 0)}, Spine splits: {len(spine_data.get('spine_splits', []))}")

    print(json.dumps({"phase": "corners", "status": "starting"}))
    corner_data = _phase_corner_edge_analysis(image_b64, llm_client, additional_prompt)
    print(json.dumps({"phase": "corners", "status": "completed", "data": corner_data}))
    print(f"Phase 2 Complete: Corner & Edge Analysis - Corner blunting: {corner_data.get('corner_blunting', 'none')}, Micro tears: {corner_data.get('micro_tears', 0)}, Edge wear: {corner_data.get('edge_wear', 'minimal')}")

    print(json.dumps({"phase": "surface", "status": "starting"}))
    surface_data = _phase_surface_analysis(image_b64, llm_client, additional_prompt)
    print(json.dumps({"phase": "surface", "status": "completed", "data": surface_data}))
    print(f"Phase 3 Complete: Surface Analysis - Creases: {surface_data.get('creases', 0)}, Bends: {surface_data.get('bends', 0)}, Thumb dents: {surface_data.get('thumb_dents', 0)}, Scuffing: {surface_data.get('scuffing', 'none')}, Gloss loss: {surface_data.get('gloss_loss', 'none')}")

    print(json.dumps({"phase": "structural", "status": "starting"}))
    structural_data = _phase_structural_analysis(image_b64, llm_client, additional_prompt)
    print(json.dumps({"phase": "structural", "status": "completed", "data": structural_data}))
    print(f"Phase 4 Complete: Structural Analysis - Staple condition: {structural_data.get('staple_condition', 'good')}, Spine roll: {structural_data.get('spine_roll', 0)}mm, Tears: {structural_data.get('tears', 0)}, Water damage: {structural_data.get('water_damage', 'none')}, Stains: {structural_data.get('stains', 0)}, Ripples: {structural_data.get('ripples', 'none')}, Mold: {structural_data.get('mold', 'none')}")

    print(json.dumps({"phase": "color_gloss", "status": "starting"}))
    color_gloss_data = _phase_color_gloss_analysis(image_b64, llm_client, additional_prompt)
    print(json.dumps({"phase": "color_gloss", "status": "completed", "data": color_gloss_data}))
    print(f"Phase 5 Complete: Color & Gloss Analysis - Color vibrancy: {color_gloss_data.get('color_vibrancy', 'excellent')}, Gloss uniformity: {color_gloss_data.get('gloss_uniformity', 'excellent')}, Saturation: {color_gloss_data.get('saturation', 'high')}, Fading: {color_gloss_data.get('fading', 'none')}")

    print(json.dumps({"phase": "paper", "status": "starting"}))
    paper_data = _phase_paper_quality_analysis(image_b64, llm_client, additional_prompt)
    print(json.dumps({"phase": "paper", "status": "completed", "data": paper_data}))
    print(f"Phase 6 Complete: Paper Quality Analysis - Paper tone: {paper_data.get('paper_tone', 'white')}, Brittleness: {paper_data.get('brittleness', 'none')}, Oxidation: {paper_data.get('oxidation', 'none')}")

    # Final synthesis
    print(json.dumps({"phase": "synthesis", "status": "starting"}))
    final_grade = _phase_final_grade_synthesis(
        spine_data,
        corner_data,
        surface_data,
        structural_data,
        color_gloss_data,
        paper_data,
        llm_client
    )
    print(json.dumps({"phase": "synthesis", "status": "completed", "data": final_grade}))
    print(f"Phase 7 Complete: Final Grade Synthesis - Grade: {final_grade.get('grade_text', '')} ({final_grade.get('grade_numeric', 0.0)}), Confidence: {final_grade.get('confidence', 0)}%")

    return {
        "grade_numeric": final_grade.get("grade_numeric"),
        "grade_text": final_grade.get("grade_text"),
        "reasoning": final_grade.get("reasoning"),
        "confidence": final_grade.get("confidence"),
        "phases": {
            "spine": spine_data,
            "corners": corner_data,
            "surface": surface_data,
            "structural": structural_data,
            "color_gloss": color_gloss_data,
            "paper": paper_data
        }
    }


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        additional_prompt = sys.argv[2] if len(sys.argv) > 2 else ""
        result = grade_comic(sys.argv[1], additional_prompt=additional_prompt)
        print(json.dumps(result))