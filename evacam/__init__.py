"""Pure-Python helpers for the EvaCAM package."""

from importlib import resources
from pathlib import Path


def config_lib_path() -> Path:
    """Return the installed default EvaCAM config library root."""
    root = resources.files(__name__).joinpath("data", "config", "lib")
    if not root.is_dir():
        raise FileNotFoundError("packaged EvaCAM config library is missing")
    return Path(str(root))


__all__ = ["config_lib_path"]
