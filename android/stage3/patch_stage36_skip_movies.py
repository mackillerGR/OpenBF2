#!/usr/bin/env python3
"""Stage 3.6: bypass Battlefield 2 intro movie queue with native +restart flag.

The ARM64 test13 BF2.cpp androidCommandLine builds:
  +modPath mods/bf2 +fullscreen 0 +multi 1 +szx ...
Its BF2Engine.cpp parser treats +restart as setting skipMovies=true. The
engine's playMovie()/checkMovie() then skip all intro/legal videos cleanly.

Only the *known exact* test13 ARM64 binary is patched. Replacement is byte-
length preserving within the existing command-line template string. No other
libmain functions or data are altered. Fail closed for an unknown binary.
"""
import hashlib
from pathlib import Path
import subprocess
import sys

stage = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path("stage3").resolve()
so = stage / "gameapp/src/main/jniLibs/arm64-v8a/libmain.so"
ORIGINAL_SHA256 = "9407a192c9bed13a3c479d3110495ecfd3bc6eb0a274dd96c2e13a6af74f9835"
before = b"+modPath mods/bf2 +fullscreen 0 +multi 1 +szx "
after  = b"+modPath mods/bf2 +restart 1    +multi 1 +szx "
if len(before) != len(after):
    raise SystemExit("Refuse binary patch: lengths differ")
data=so.read_bytes()
hash_ = hashlib.sha256(data).hexdigest()
if hash_ != ORIGINAL_SHA256:
    raise SystemExit(f"Refuse binary patch: unexpected libmain.so SHA256 {hash_}")
if not data.startswith(b"\x7fELF"):
    raise SystemExit("Refuse binary patch: ELF signature missing")
if data.count(before) != 1:
    raise SystemExit(f"Refuse binary patch: expected one command-line constant, got {data.count(before)}")
offset=data.index(before)
patched=data[:offset]+after+data[offset+len(before):]
assert len(patched)==len(data)
assert patched.count(after)==1
so.write_bytes(patched)
if so.read_bytes()!=patched:
    raise SystemExit("Patched file verification failed")
print(f"Stage3.6 movie bypass enabled at file offset 0x{offset:x}")
print(f"Old ELF SHA256: {hash_}")
print(f"New ELF SHA256: {hashlib.sha256(patched).hexdigest()}")
print("Launch command now includes +restart 1, which enables BF2Engine skipMovies.")
