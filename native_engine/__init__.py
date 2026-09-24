# ============================================================================
#  Native Engine — a from-scratch ray-traced 3D game engine.
#  C++17 core (path tracing, BVH, multithreaded) + Python runtime.
# ============================================================================
from .engine import (App, Vec3, Material, Object, Window, run_editor,
                     K_SPACE, K_SHIFT, K_ESCAPE, K_F1)
from . import build as build_module
from .build import build as build_native
from . import server

__version__ = "1.2.0"


def version():
    """Full engine version string from the native core."""
    from .engine import _load  # ensures library exists
    lib = _load()
    return lib.ne_version().decode()


__all__ = [
    "App", "Vec3", "Material", "Object", "Window", "run_editor",
    "K_SPACE", "K_SHIFT", "K_ESCAPE", "K_F1",
    "build_native", "build_module", "server", "version", "__version__",
]
