"""
tests/test_grading.py
=====================
pytest test suite for phased_grading_engine.py

Run from repo root:
    python -m pytest tests/test_grading.py -v

Requires: pytest, requests (for import)
Does NOT require a live LLM — all LLM calls are mocked.
"""

import sys
import json
import types
import importlib
from pathlib import Path
from unittest.mock import patch, MagicMock

import pytest

# Make sure the repo root is on sys.path so we can import the engine
ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(ROOT))

import phased_grading_engine as ge


# ─────────────────────────────────────────────────────────────────────────────
# Fixtures
# ─────────────────────────────────────────────────────────────────────────────

DUMMY_SPINE_RESULT = {
    "ticks": 2,
    "color_breaking_ticks": 1,
    "spine_stresses": 1,
    "spine_splits": []
}

DUMMY_CORNER_RESULT = {
    "corner_blunting": "light",
    "micro_tears": 0,
    "edge_wear": "minimal"
}

DUMMY_SURFACE_RESULT = {
    "creases": 1,
    "color_breaking_folds": 0,
    "dents": 0,
    "scuffing": "none",
    "gloss_loss": "minor"
}

DUMMY_STRUCTURAL_RESULT = {
    "staple_condition": "intact",
    "spine_roll": False,
    "tears": 0,
    "water_damage": False,
    "stains": []
}

DUMMY_COLOR_RESULT = {
    "color_vibrancy": "high",
    "gloss_uniformity": "uniform",
    "fade": False
}

DUMMY_PAPER_RESULT = {
    "paper_tone": "OW/W",
    "brittle": False,
    "oxidation": "minimal"
}


# ─────────────────────────────────────────────────────────────────────────────
# 1. Module-level import / API surface checks
# ─────────────────────────────────────────────────────────────────────────────

class TestModuleImport:
    def test_module_imports_cleanly(self):
        """phased_grading_engine must import without errors."""
        assert ge is not None

    def test_grade_comic_callable(self):
        """Public API function grade_comic must exist and be callable."""
        assert callable(getattr(ge, "grade_comic", None)), \
            "grade_comic() not found — add it as the public entry point"

    def test_encode_image_helper_exists(self):
        assert callable(getattr(ge, "_encode_image", None))

    def test_call_llm_helper_exists(self):
        assert callable(getattr(ge, "_call_llm", None))

    def test_phase_functions_exist(self):
        """All 6 phase functions must be defined."""
        phases = [
            "_phase_spine_analysis",
            "_phase_corner_edge_analysis",
            "_phase_surface_analysis",
            "_phase_structural_analysis",
            "_phase_color_gloss_analysis",
            "_phase_paper_quality_analysis",
        ]
        for fn in phases:
            assert callable(getattr(ge, fn, None)), f"{fn}() not found"


# ─────────────────────────────────────────────────────────────────────────────
# 2. _encode_image
# ─────────────────────────────────────────────────────────────────────────────

class TestEncodeImage:
    def test_returns_base64_string(self, tmp_path):
        img = tmp_path / "test.jpg"
        img.write_bytes(b"\xff\xd8\xff" + b"\x00" * 100)  # minimal fake JPEG
        result = ge._encode_image(str(img))
        import base64 as b64
        # Should be valid base64 decodable bytes
        decoded = b64.b64decode(result)
        assert len(decoded) > 0

    def test_raises_on_missing_file(self, tmp_path):
        with pytest.raises((FileNotFoundError, OSError)):
            ge._encode_image(str(tmp_path / "nonexistent.jpg"))


# ─────────────────────────────────────────────────────────────────────────────
# 3. _call_llm (mocked — no live server required)
# ─────────────────────────────────────────────────────────────────────────────

class TestCallLlm:
    def test_ollama_success(self):
        mock_response = MagicMock()
        mock_response.raise_for_status = MagicMock()
        mock_response.json.return_value = {"response": "LLM output text"}

        with patch("requests.post", return_value=mock_response) as mock_post:
            result = ge._call_llm("Analyze this comic.", server="Ollama", model="llava")

        assert result == "LLM output text"
        call_args = mock_post.call_args
        assert "11434" in call_args[0][0]  # Ollama port

    def test_lmstudio_success(self):
        mock_response = MagicMock()
        mock_response.raise_for_status = MagicMock()
        mock_response.json.return_value = {
            "choices": [{"message": {"content": "LM Studio output"}}]
        }

        with patch("requests.post", return_value=mock_response):
            result = ge._call_llm("Analyze.", server="LMStudio", model="qwen-vl")

        assert result == "LM Studio output"

    def test_network_error_returns_error_string(self):
        with patch("requests.post", side_effect=Exception("Connection refused")):
            result = ge._call_llm("Analyze.", server="Ollama")

        assert result.startswith("Error")

    def test_image_included_in_ollama_payload(self):
        mock_response = MagicMock()
        mock_response.raise_for_status = MagicMock()
        mock_response.json.return_value = {"response": "ok"}

        with patch("requests.post", return_value=mock_response) as mock_post:
            ge._call_llm("prompt", image_b64="abc123==", server="Ollama", model="llava")

        payload = mock_post.call_args[1]["json"]
        assert "images" in payload
        assert payload["images"] == ["abc123=="]


# ─────────────────────────────────────────────────────────────────────────────
# 4. Phase 1 — Spine Analysis
# ─────────────────────────────────────────────────────────────────────────────

class TestPhaseSpineAnalysis:
    def _run(self, llm_return: str) -> dict:
        with patch.object(ge, "_call_llm", return_value=llm_return):
            return ge._phase_spine_analysis("fake_b64", llm_client=None)

    def test_returns_dict(self):
        result = self._run(json.dumps(DUMMY_SPINE_RESULT))
        assert isinstance(result, dict)

    def test_required_keys_present(self):
        result = self._run(json.dumps(DUMMY_SPINE_RESULT))
        for key in ("ticks", "color_breaking_ticks", "spine_stresses", "spine_splits"):
            assert key in result, f"Missing key: {key}"

    def test_color_breaking_lte_total_ticks(self):
        data = {**DUMMY_SPINE_RESULT, "ticks": 3, "color_breaking_ticks": 2}
        result = self._run(json.dumps(data))
        assert result["color_breaking_ticks"] <= result["ticks"]

    def test_fallback_on_bad_json(self):
        result = self._run("This is not JSON at all.")
        assert isinstance(result, dict)
        assert result["ticks"] == 0

    def test_fallback_on_llm_error(self):
        result = self._run("Error: Connection refused")
        assert isinstance(result, dict)
        assert result["ticks"] == 0

    def test_spine_splits_is_list(self):
        result = self._run(json.dumps(DUMMY_SPINE_RESULT))
        assert isinstance(result["spine_splits"], list)


# ─────────────────────────────────────────────────────────────────────────────
# 5. Phase 2 — Corner & Edge Analysis
# ─────────────────────────────────────────────────────────────────────────────

class TestPhaseCornerEdgeAnalysis:
    def _run(self, llm_return: str) -> dict:
        with patch.object(ge, "_call_llm", return_value=llm_return):
            return ge._phase_corner_edge_analysis("fake_b64", llm_client=None)

    def test_returns_dict(self):
        result = self._run(json.dumps(DUMMY_CORNER_RESULT))
        assert isinstance(result, dict)

    def test_fallback_on_bad_json(self):
        result = self._run("garbage output")
        assert isinstance(result, dict)


# ─────────────────────────────────────────────────────────────────────────────
# 6. grade_comic public API (all phases mocked)
# ─────────────────────────────────────────────────────────────────────────────

class TestGradeComicPublicApi:
    """
    Tests the public grade_comic() function end-to-end with all LLM calls
    mocked.  This verifies the pipeline glue without any network I/O.
    """

    def _mock_all_phases(self):
        return {
            "_phase_spine_analysis": DUMMY_SPINE_RESULT,
            "_phase_corner_edge_analysis": DUMMY_CORNER_RESULT,
            "_phase_surface_analysis": DUMMY_SURFACE_RESULT,
            "_phase_structural_analysis": DUMMY_STRUCTURAL_RESULT,
            "_phase_color_gloss_analysis": DUMMY_COLOR_RESULT,
            "_phase_paper_quality_analysis": DUMMY_PAPER_RESULT,
        }

    def test_returns_dict(self, tmp_path):
        img = tmp_path / "cover.jpg"
        img.write_bytes(b"\xff\xd8\xff" + b"\x00" * 50)

        patches = self._mock_all_phases()
        mock_synthesis = {
            "grade_numeric": 8.5,
            "grade_text": "VF+ 8.5",
            "reasoning": "Minor wear only.",
            "confidence": 85,
            "phases": patches,
        }

        with patch.object(ge, "_encode_image", return_value="fakeb64"), \
             patch.object(ge, "_phase_spine_analysis", return_value=DUMMY_SPINE_RESULT), \
             patch.object(ge, "_phase_corner_edge_analysis", return_value=DUMMY_CORNER_RESULT), \
             patch.object(ge, "_phase_surface_analysis", return_value=DUMMY_SURFACE_RESULT), \
             patch.object(ge, "_phase_structural_analysis", return_value=DUMMY_STRUCTURAL_RESULT), \
             patch.object(ge, "_phase_color_gloss_analysis", return_value=DUMMY_COLOR_RESULT), \
             patch.object(ge, "_phase_paper_quality_analysis", return_value=DUMMY_PAPER_RESULT), \
             patch.object(ge, "_synthesize_grade", return_value=mock_synthesis, create=True):
            result = ge.grade_comic(str(img))

        assert isinstance(result, dict)

    def test_result_has_grade_numeric(self, tmp_path):
        img = tmp_path / "cover.jpg"
        img.write_bytes(b"\xff\xd8\xff" + b"\x00" * 50)

        mock_synthesis = {
            "grade_numeric": 9.0,
            "grade_text": "NM 9.0",
            "reasoning": "Near mint copy.",
            "confidence": 90,
            "phases": {},
        }

        with patch.object(ge, "_encode_image", return_value="fakeb64"), \
             patch.object(ge, "_phase_spine_analysis", return_value=DUMMY_SPINE_RESULT), \
             patch.object(ge, "_phase_corner_edge_analysis", return_value=DUMMY_CORNER_RESULT), \
             patch.object(ge, "_phase_surface_analysis", return_value=DUMMY_SURFACE_RESULT), \
             patch.object(ge, "_phase_structural_analysis", return_value=DUMMY_STRUCTURAL_RESULT), \
             patch.object(ge, "_phase_color_gloss_analysis", return_value=DUMMY_COLOR_RESULT), \
             patch.object(ge, "_phase_paper_quality_analysis", return_value=DUMMY_PAPER_RESULT), \
             patch.object(ge, "_synthesize_grade", return_value=mock_synthesis, create=True):
            result = ge.grade_comic(str(img))

        assert "grade_numeric" in result
        assert 0.5 <= result["grade_numeric"] <= 10.0

    def test_grade_numeric_in_valid_range(self, tmp_path):
        img = tmp_path / "cover.jpg"
        img.write_bytes(b"\xff\xd8\xff" + b"\x00" * 50)

        mock_synthesis = {
            "grade_numeric": 11.0,  # invalid — engine should clamp or reject
            "grade_text": "???",
            "reasoning": "",
            "confidence": 0,
            "phases": {},
        }

        with patch.object(ge, "_encode_image", return_value="fakeb64"), \
             patch.object(ge, "_phase_spine_analysis", return_value=DUMMY_SPINE_RESULT), \
             patch.object(ge, "_phase_corner_edge_analysis", return_value=DUMMY_CORNER_RESULT), \
             patch.object(ge, "_phase_surface_analysis", return_value=DUMMY_SURFACE_RESULT), \
             patch.object(ge, "_phase_structural_analysis", return_value=DUMMY_STRUCTURAL_RESULT), \
             patch.object(ge, "_phase_color_gloss_analysis", return_value=DUMMY_COLOR_RESULT), \
             patch.object(ge, "_phase_paper_quality_analysis", return_value=DUMMY_PAPER_RESULT), \
             patch.object(ge, "_synthesize_grade", return_value=mock_synthesis, create=True):
            result = ge.grade_comic(str(img))

        # grade_numeric must be clamped to [0.5, 10.0]
        assert result["grade_numeric"] <= 10.0, "grade_numeric must be clamped to max 10.0"


# ─────────────────────────────────────────────────────────────────────────────
# 7. Grade-to-ConditionID mapping (utility — to be added to engine)
# ─────────────────────────────────────────────────────────────────────────────

class TestGradeToConditionId:
    """
    Tests for a grade_to_ebay_condition_id() helper.
    Mapping:
        9.0+  → 4000  (Near Mint)
        8.0   → 3000  (Very Good)
        6.0   → 2750  (Fine)
        4.0   → 2500  (Very Good-)
        2.0   → 2000  (Good)
        < 2.0 → 1000  (Poor)
    """

    def _convert(self, grade: float) -> int:
        fn = getattr(ge, "grade_to_ebay_condition_id", None)
        if fn is None:
            pytest.skip("grade_to_ebay_condition_id() not yet implemented")
        return fn(grade)

    def test_nm_grade(self):
        assert self._convert(9.5) == 4000

    def test_vf_grade(self):
        assert self._convert(8.0) == 3000

    def test_fn_grade(self):
        assert self._convert(6.0) == 2750

    def test_gd_grade(self):
        assert self._convert(2.0) == 2000

    def test_poor_grade(self):
        assert self._convert(1.0) == 1000

    def test_perfect_10(self):
        assert self._convert(10.0) == 4000

    def test_boundary_9_0(self):
        assert self._convert(9.0) == 4000
