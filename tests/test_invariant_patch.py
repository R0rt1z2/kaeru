import os
import pytest
import importlib.util
import sys

# Load the actual production module
spec = importlib.util.spec_from_file_location("patch", os.path.join(os.path.dirname(__file__), "../utils/patch.py"))
patch_mod = importlib.util.load_from_spec(spec) if hasattr(importlib.util, 'load_from_spec') else None

@pytest.mark.parametrize("device_name", [
    "../../../etc/passwd",          # exact exploit case
    "....//....//etc/passwd",       # boundary obfuscated traversal
    "valid_device_name",            # valid input (should pass)
])
def test_output_path_stays_within_root(device_name, tmp_path):
    """Invariant: The output filename derived from device must never resolve outside the working directory."""
    root = str(tmp_path)
    original_cwd = os.getcwd()
    os.chdir(root)
    try:
        # Simulate what the vulnerable line does: construct output filename from device
        output_filename = '%s-patched.bin' % device_name
        resolved = os.path.realpath(os.path.abspath(output_filename))
        root_real = os.path.realpath(root)

        # Security invariant: resolved path must be inside root
        assert resolved.startswith(root_real + os.sep) or resolved == root_real, (
            f"Path traversal detected! '{device_name}' resolves to '{resolved}' "
            f"which is outside root '{root_real}'"
        )
    finally:
        os.chdir(original_cwd)