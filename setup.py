"""Build configuration for the evacam_py C++ extension.

Project metadata lives in pyproject.toml; this file only describes how to
compile the pybind11 extension from the existing C++ source tree. It mirrors
the Makefile's pybind target: every src/**/*.cpp except app/main.cpp, plus the
binding translation unit, compiled against the include/ tree and linked with
yaml-cpp and OpenMP.
"""

import os
import platform
import subprocess
from glob import glob

from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup

ROOT = os.path.abspath(os.path.dirname(__file__))


def _run(cmd):
    """Return whitespace-split stdout of `cmd`, or [] if it is unavailable."""
    try:
        return subprocess.check_output(cmd, text=True).split()
    except (OSError, subprocess.CalledProcessError):
        return []


def _collect_sources():
    sources = sorted(glob(os.path.join("src", "**", "*.cpp"), recursive=True))
    main_cpp = os.path.normpath(os.path.join("src", "app", "main.cpp"))
    sources = [s for s in sources if os.path.normpath(s) != main_cpp]
    sources.append(os.path.join("bindings", "EvaCAM_Pybind.cpp"))
    return sources


# Mirror the include layout from the Makefile.
include_dirs = [
    "include",
    "include/app",
    "include/cam",
    "include/circuit",
    "include/config",
    "include/factories",
    "include/input",
    "include/model",
    "include/output",
    "include/technology",
]
library_dirs = []
libraries = []
extra_compile_args = []
extra_link_args = []

# yaml-cpp. The sources include the library header directly as `<yaml.h>`
# (not `<yaml-cpp/yaml.h>`), so the *yaml-cpp* header directory itself must be
# on the include path -- matching the Makefile's `-I/usr/include/yaml-cpp`.
# pkg-config's includedir points one level above that, so we add both.
libraries.append("yaml-cpp")
header_bases = _run(["pkg-config", "--variable=includedir", "yaml-cpp"])
header_bases += ["/usr/include", "/usr/local/include", "/opt/homebrew/include"]
for base in header_bases:
    yaml_subdir = os.path.join(base, "yaml-cpp")
    if os.path.isdir(yaml_subdir):
        include_dirs.append(base)
        include_dirs.append(yaml_subdir)
        break
yaml_libdir = _run(["pkg-config", "--variable=libdir", "yaml-cpp"])
if yaml_libdir:
    library_dirs.append(yaml_libdir[0])

system = platform.system()
if system == "Darwin":
    # Apple clang/libc++ has no <bits/stdc++.h>; use the in-repo shim, and pull
    # OpenMP from Homebrew's libomp.
    include_dirs.append("compat")
    extra_compile_args += ["-Xpreprocessor", "-fopenmp"]
    extra_link_args += ["-lomp"]
    libomp = _run(["brew", "--prefix", "libomp"])
    if libomp:
        prefix = libomp[0]
        include_dirs.append(os.path.join(prefix, "include"))
        library_dirs.append(os.path.join(prefix, "lib"))
        extra_link_args.append("-L" + os.path.join(prefix, "lib"))
else:
    # Linux/GCC: <bits/stdc++.h> and -fopenmp are available natively.
    extra_compile_args += ["-fopenmp"]
    extra_link_args += ["-fopenmp"]

ext_modules = [
    Pybind11Extension(
        "evacam_py",
        sources=_collect_sources(),
        include_dirs=include_dirs,
        library_dirs=library_dirs,
        libraries=libraries,
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args,
        cxx_std=17,
    )
]

setup(ext_modules=ext_modules, cmdclass={"build_ext": build_ext})
