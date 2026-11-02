#!/usr/bin/env python3
"""Compile the real menu state machine with host test stubs."""
import os, shutil, subprocess, sys, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[3]
android = root / "ports/android"
generated = Path(os.environ["HUD_GENERATED"]) if os.environ.get("HUD_GENERATED") else android / "src"
if sys.platform != "darwin" or not shutil.which("clang") or not (generated / "cardb.h").is_file():
    print("SKIP: macOS clang or generated headers unavailable")
    sys.exit(0)
with tempfile.TemporaryDirectory(prefix="vs-pre-race-") as tmp:
    exe = Path(tmp) / "test"
    subprocess.run(["clang", "-std=gnu99", "-U__APPLE__", "-DWMMT_PRE_RACE_TEST", "-DWMMT_BGM_TEST",
        "-I", str(android / "test"), "-I", str(generated), f'-DPORT_MENU="{android / "src/menu.c"}"',
        "-ffunction-sections", "-fdata-sections", str(android / "tests/vs_pre_race_tune_host.c"),
        str(android / "src/pre_race_tune.c"), "-Wl,-dead_strip", "-Wl,-undefined,dynamic_lookup", "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
