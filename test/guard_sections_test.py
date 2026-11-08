#!/usr/bin/env python3
"""Tests for tools/recomp/guard_sections.py, the lower bound it gives the generated physics'
yt::CAreaProcessorJunction::MoveToPrev (src/phys/phys_rc.c: f_89f8d9c).

Three layers (the harness is the Android 1.31 snapshot's tests/test_guard_sections.py):
  * a synthetic C fragment that always runs: the structure, the failure modes, idempotence;
  * a stand-in for that function, written the way tools/recomp/recomp.py writes its functions, patched,
    compiled and run against a fake emulated memory: unpatched it reads area_array[-1] (the crash),
    patched index 0 stays put with the stack as the original's return leaves it, and index 1 and 2
    still step back one area (needs a C compiler, else skipped);
  * when WMMT_PHYS_RC names a generated src/phys/phys_rc.c, the real function the same way, and the
    Loop variant's byte-for-byte unchanged.

Never touches the project's files: the source is copied to a temporary directory.

  python3 test/guard_sections_test.py
"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GUARD = ROOT / "tools/recomp/guard_sections.py"
REAL = Path(os.environ["WMMT_PHYS_RC"]) if os.environ.get("WMMT_PHYS_RC") else None

DECL = "  uint32_t eax = rc_cpu.eax, ecx = rc_cpu.ecx, edx = rc_cpu.edx, esp = rc_cpu.esp, ebp = rc_cpu.ebp;\n"
SYNTH = """/* _ZN2yt22CAreaProcessorJunction10MoveToPrevERNS_18SAreaProcessorInfoE */
void f_89f8d9c(void) {
  RC_ENTRY(0x89f8d9cu);
  RC_FLAGS;
""" + DECL + """  /* Minimal synthetic markers for testing the patcher's pattern checks. */
  --ecx;  /* 89f8da5: dec ecx */
  eax = items[ecx];  /* 89f8daf: mov eax, dword ptr [eax + ecx*4] */
}
"""

# the function as recomp.py's emit() writes it (its header, the registers it uses copied in, one
# line per instruction with its address and text, ret saving the registers), standing in for the
# real one: this = [ebp+8], info = [ebp+12], info->index = [info+4], info->area = [info], this->array = [this+4]
STANDIN = """/* _ZN2yt22CAreaProcessorJunction10MoveToPrevERNS_18SAreaProcessorInfoE */
void f_89f8d9c(void) {
  RC_ENTRY(0x89f8d9cu);
  RC_FLAGS;
""" + DECL + """  RC_T(0x89f8d9cu); esp -= 4; W32(esp, ebp);  /* 89f8d9c: push ebp */
  RC_T(0x89f8d9du); ebp = esp;  /* 89f8d9d: mov ebp, esp */
  RC_T(0x89f8d9fu); edx = M32(ebp + 12u);  /* 89f8d9f: mov edx, dword ptr [ebp + 0xc] */
  RC_T(0x89f8da2u); ecx = M32(edx + 4u);  /* 89f8da2: mov ecx, dword ptr [edx + 4] */
  RC_T(0x89f8da5u); ecx = ecx - 1u;  /* 89f8da5: dec ecx */
  RC_T(0x89f8da6u); W32(edx + 4u, ecx);  /* 89f8da6: mov dword ptr [edx + 4], ecx */
  RC_T(0x89f8da9u); eax = M32(ebp + 8u);  /* 89f8da9: mov eax, dword ptr [ebp + 8] */
  RC_T(0x89f8dacu); eax = M32(eax + 4u);  /* 89f8dac: mov eax, dword ptr [eax + 4] */
  RC_T(0x89f8dafu); eax = M32(eax + ecx * 4u);  /* 89f8daf: mov eax, dword ptr [eax + ecx*4] */
  RC_T(0x89f8db2u); W32(edx, eax);  /* 89f8db2: mov dword ptr [edx], eax */
  RC_T(0x89f8db4u); { uint32_t v_ = M32(esp); esp += 4; ebp = v_; }  /* 89f8db4: pop ebp */
  RC_T(0x89f8db5u); esp += 4; rc_cpu.eax = eax; rc_cpu.ecx = ecx; rc_cpu.edx = edx; rc_cpu.esp = esp; rc_cpu.ebp = ebp; return;  /* 89f8db5: ret */
}
"""


def guard(path):
    return subprocess.run([sys.executable, str(GUARD), str(path)], capture_output=True, text=True)


def test_synthetic(tmp):
    src = tmp / "synth.c"
    src.write_text(SYNTH, encoding="utf-8")
    r = guard(src)
    assert r.returncode == 0, r.stderr
    once = src.read_text(encoding="utf-8")
    assert "PORT GUARD" in once and once.count("if (ecx == 0)") == 1
    # the guard sits before the decrement and after the registers are copied in
    assert once.index(DECL.strip()) < once.index("if (ecx == 0)") < once.index("/* 89f8da5: dec ecx */")
    r2 = guard(src)
    assert r2.returncode == 0 and "already guarded" in r2.stdout
    assert src.read_text(encoding="utf-8") == once
    # no matching function: must fail loudly, not silently pass
    other = tmp / "other.c"
    other.write_text("void f_1(void) {\n}\n", encoding="utf-8")
    assert guard(other).returncode == 2
    # two of them (nothing says which is meant), and the address under another function's name
    # (another build of the game): both refused, the file left as it was
    for name, text in (("twice", SYNTH + "\n" + SYNTH),
                       ("renamed", SYNTH.replace("CAreaProcessorJunction10MoveToPrev", "CAreaProcessorLoop10MoveToPrev"))):
        f = tmp / (name + ".c")
        f.write_text(text, encoding="utf-8")
        r = guard(f)
        assert r.returncode == 2 and r.stderr, (name, r.returncode)
        assert f.read_text(encoding="utf-8") == text, name
    # truncated body: must fail
    bad = tmp / "bad.c"
    bad.write_text(SYNTH.replace("/* 89f8da5: dec ecx */", ""), encoding="utf-8")
    assert guard(bad).returncode == 2
    assert guard(bad).stderr
    # a function that does not copy in one of the registers the guard stores: the generated file
    # would not compile, so it must be refused (and left as it was)
    for reg in ("eax", "ecx", "edx", "esp", "ebp"):
        lacking = tmp / ("lacking_%s.c" % reg)
        text = SYNTH.replace(" %s = rc_cpu.%s," % (reg, reg), "").replace(", %s = rc_cpu.%s;" % (reg, reg), ";")
        assert text != SYNTH, reg
        lacking.write_text(text, encoding="utf-8")
        r = guard(lacking)
        assert r.returncode == 2 and "does not copy in" in r.stderr, (reg, r.stderr)
        assert lacking.read_text(encoding="utf-8") == text
    # a function without the declaration line at all
    nodecl = tmp / "nodecl.c"
    nodecl.write_text(SYNTH.replace(DECL, ""), encoding="utf-8")
    assert guard(nodecl).returncode == 2
    # a damaged guard (the marker without its text) must fail
    damaged = tmp / "damaged.c"
    damaged.write_text(once.replace("if (ecx == 0)", "if (ecx == 1)"), encoding="utf-8")
    assert guard(damaged).returncode == 2
    # not UTF-8
    binary = tmp / "binary.c"
    binary.write_bytes(b"\xff\xfe\x00")
    assert guard(binary).returncode == 2
    # no file
    assert guard(tmp / "missing.c").returncode != 0


def function_text(text, signature):
    i = text.index(signature)
    j = text.find("\n}\n", i)
    assert j > 0
    return text[i:j + 3]


HARNESS = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi; } rc_cpu_t;
rc_cpu_t rc_cpu;
#define RC_ENTRY(a) ((void)0)
#define RC_FLAGS int CF = 0, ZF = 0, SF = 0, OF = 0, PF = 0; (void)CF; (void)ZF; (void)SF; (void)OF; (void)PF
#define RC_T(a) ((void)0)
static void rc_fail(const char *m) __attribute__((noreturn, unused));
static void rc_fail(const char *m) { fprintf(stderr, "rc_fail: %s\n", m); exit(3); }

#define MEM_BASE 0x092f0000u
#define MEM_SIZE 0x00b50000u
static uint8_t mem[MEM_SIZE];
static uint32_t off(uint32_t a) {
  uint32_t o = a - MEM_BASE;
  if (o >= MEM_SIZE) { fprintf(stderr, "BADADDR %#x\n", a); exit(4); }
  return o;
}
static uint32_t M32(uint32_t a) { uint32_t v; memcpy(&v, mem + off(a), 4); return v; }
static void W32(uint32_t a, uint32_t v) { memcpy(mem + off(a), &v, 4); }
static float __attribute__((unused)) MF(uint32_t a) { float v; memcpy(&v, mem + off(a), 4); return v; }
static int __attribute__((unused)) parity(uint32_t r) {
  r &= 0xff; r ^= r >> 4; r ^= r >> 2; r ^= r >> 1; return !(r & 1);
}

__FUNC__

#define INFO 0x09940300u
#define PROC 0x09940200u
#define ARRAY 0x09940100u

static void setup(void) {
  memset(mem, 0, sizeof mem);
  W32(INFO, 0x11111111u);      /* area: sentinel */
  W32(INFO + 4, 0);            /* index */
  W32(PROC + 4, ARRAY);        /* this->array */
  W32(ARRAY - 4, 0x433e4e4au); /* array[-1]: the value seen in the crash */
  W32(ARRAY + 0, 0x092f2000u);
  W32(ARRAY + 4, 0x092f3000u);
  W32(ARRAY + 8, 0x092f4000u);
}

static void run(uint32_t index) {
  setup();
  W32(INFO + 4, index);
  uint32_t sp = 0x09e30000u;    /* emulated stack */
  W32(sp, 0u);                  /* return address */
  W32(sp + 4, PROC);
  W32(sp + 8, INFO);
  rc_cpu.esp = sp;
  rc_cpu.ebp = 0x09e30f00u;     /* the caller's frame */
  f_89f8d9c();
  printf("in=%u index=%u area=%#x esp=%#x\n", index, M32(INFO + 4), M32(INFO), rc_cpu.esp);
  if (rc_cpu.ebp != 0x09e30f00u) { puts("FAIL caller's frame"); exit(1); } /* (cdecl: the callee keeps ebp) */
  if (index == 0) {
    if (M32(INFO + 4) != 0 || M32(INFO) != 0x11111111u) { puts("FAIL index0 moved"); exit(1); }
    if (rc_cpu.esp != sp + 4) { puts("FAIL index0 stack"); exit(1); }
  } else {
    if (M32(INFO + 4) != index - 1) { puts("FAIL index"); exit(1); }
    if (M32(INFO) != 0x092f2000u + 0x1000u * (index - 1)) { puts("FAIL area"); exit(1); }
  }
}

int main(void) { run(0); run(1); run(2); puts("PASS guard"); return 0; }
"""


def build_and_run(work, func, name):
    """The function's text in the harness: compiled (any warning an error) and run."""
    harness = work / (name + ".c")
    harness.write_text(HARNESS.replace("__FUNC__", func), encoding="utf-8")
    exe = work / name
    cc = [shutil.which("cc") or "cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror", str(harness), "-o", str(exe)]
    r = subprocess.run(cc, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-2000:]
    return subprocess.run([str(exe)], capture_output=True, text=True)


def test_standin(tmp):
    if not shutil.which("cc"):
        print("skip: no C compiler for the stand-in run")
        return
    work = tmp / "standin"
    work.mkdir()
    src = work / "standin.c"
    src.write_text(STANDIN, encoding="utf-8")
    # the harness can fail: unpatched, index 0 takes array[-1] for the area (the crash)
    r = build_and_run(work, function_text(STANDIN, "void f_89f8d9c(void) {"), "unpatched")
    assert r.returncode == 1 and "FAIL index0 moved" in r.stdout, (r.returncode, r.stdout, r.stderr)
    r = guard(src)
    assert r.returncode == 0, r.stderr
    r = build_and_run(work, function_text(src.read_text(encoding="utf-8"), "void f_89f8d9c(void) {"), "patched")
    assert r.returncode == 0 and "PASS guard" in r.stdout, (r.returncode, r.stdout[-2000:], r.stderr[-2000:])


def run_real(tmp):
    if REAL is None or not REAL.is_file():
        print("skip: set WMMT_PHYS_RC to an available generated phys_rc.c")
        return
    work = tmp / "real"
    work.mkdir()
    patched = work / "phys_rc.c"
    shutil.copyfile(REAL, patched)
    original = shutil.copyfile(REAL, work / "orig.c")
    assert guard(patched).returncode == 0
    assert guard(patched).returncode == 0  # idempotent
    orig_text = Path(original).read_text(encoding="utf-8")
    new_text = patched.read_text(encoding="utf-8")
    loop = "void f_89f8cc0(void) {"
    assert function_text(orig_text, loop) == function_text(new_text, loop), "Loop MoveToPrev changed"
    r = build_and_run(work, function_text(new_text, "void f_89f8d9c(void) {"), "h")
    assert r.returncode == 0, (r.returncode, r.stdout[-2000:], r.stderr[-2000:])
    print(r.stdout.strip())


def main():
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        test_synthetic(tmp)
        print("synthetic guard tests passed")
        test_standin(tmp)
        print("stand-in run passed")
        run_real(tmp)


if __name__ == "__main__":
    main()
