#!/usr/bin/env python3
"""The For Racing meter's REV lamp: the lamp's sprites embedded by tools/racing_lamp_data.py
(src/racing_lamp_data.h) and hud.c's use of them (test/racing_lamp_test.c).

Everything is made in a temporary tree from sprites of its own, so no arcade data is needed:
  1. four 64x64 RGBA PNGs of distinct pixels -> the generator -> the header there; --verify
     passes, and fails for a sprite or a header that was changed after
  2. the generator's refusals: a sprite missing, of another size, not a PNG, no sprite folder:
     it fails and writes no header
  3. tools/gen_from_original.sh runs the generator as a best effort (it has set -e) and
     .gitignore leaves the header out
  4. test/racing_lamp_test.c, built with a copy of src/ that has test/racing_lamp_meterdb.h as
     its meterdb.h and that header, is run

Usage: python3 test/racing_lamp_test.py [--src DIR] [--keep]
  --src DIR  where hud.c and the headers come from (default: src/ of this repository; the headers
             the data tools write, tools/gen_from_original.sh, must be there, as for the other tests)
  --keep     leave the temporary tree (it is named at the end)
Needs gcc with zlib's header, and Pillow.
"""
import random
import re
import shutil
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("racing_lamp_test.py needs Pillow (pip install pillow)")

REPO = Path(__file__).resolve().parents[1]
NAMES = ("off_m", "off_n", "on_m", "on_n")
fails = 0


def check(cond, what):
    global fails
    if not cond:
        print("FAIL", what)
        fails += 1
    return bool(cond)


def run(cmd, cwd):
    return subprocess.run([str(c) for c in cmd], cwd=str(cwd), capture_output=True, text=True)


# how much of each sprite is pixels (the rest is zero): the morning's lamp off and the night's lamp lit
# compress small, the other two do not, so that the four compressed arrays are of different lengths
# (hud.c must hand each its own to the inflate)
DENSITY = {"off_m": 0.05, "off_n": 1.0, "on_m": 1.0, "on_n": 0.05}


def make_sprites(directory, seed=1):
    directory.mkdir(parents=True, exist_ok=True)
    for i, name in enumerate(NAMES):
        rng = random.Random(seed * 100 + i)
        raw = bytes(rng.randrange(256) if rng.random() < DENSITY[name] else 0 for _ in range(64 * 64 * 4))
        Image.frombytes("RGBA", (64, 64), raw).save(directory / ("racing_rev_" + name + ".png"))


def embedded(header, name):
    """a sprite as the header holds it, inflated (read here, not by the generator's own --verify)"""
    m = re.search(r"lamp_" + name + r"\[\] = \{(.*?)\};", header.read_text(), re.S)
    return zlib.decompress(bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", m.group(1)))) if m else b""


def make_tree(root, src):
    """tools/racing_lamp_data.py and an empty src/ (where it writes), as the generator expects"""
    (root / "tools").mkdir(parents=True)
    (root / "src").mkdir()
    shutil.copy(REPO / "tools/racing_lamp_data.py", root / "tools/racing_lamp_data.py")
    return root / "tools/racing_lamp_data.py", root / "src/racing_lamp_data.h"


def test_generator(tmp, src):
    root = tmp / "tree"
    gen, header = make_tree(root, src)
    sprites = tmp / "sprites"
    make_sprites(sprites)
    run_dir = tmp / "run"
    run_dir.mkdir()

    r = run([sys.executable, gen, sprites], run_dir)
    check(r.returncode == 0, "the generator runs: " + r.stderr[-400:])
    check(header.exists(), "the generator writes src/racing_lamp_data.h")
    check(len(r.stdout.split("\n")) >= 4 and all(n in r.stdout for n in NAMES), "the generator names the four sprites")
    text = header.read_text() if header.exists() else ""
    size = {n: len(re.findall(r"0x[0-9a-f]{2}", text.split("lamp_" + n + "[] = {")[1].split("};")[0])) if header.exists() else 0
            for n in NAMES}
    check(size["off_m"] < size["off_n"] and size["on_n"] < size["on_m"] and size["off_m"] != size["on_n"],
          "the four arrays are of different lengths: %s" % size)
    check("#ifndef RACING_LAMP_DATA_H" in text and "#endif" in text, "the header has its guard")
    for n in NAMES:
        check("static const unsigned char lamp_%s[] = {" % n in text, "the header has lamp_" + n)
        png = Image.open(sprites / ("racing_rev_" + n + ".png")).convert("RGBA").tobytes()
        check(len(png) == 64 * 64 * 4 and embedded(header, n) == png, "the header holds the pixels of racing_rev_" + n)

    r = run([sys.executable, gen, sprites, "--verify"], run_dir)
    check(r.returncode == 0 and "match" in r.stdout, "--verify passes for the sprites it was made from: " + r.stderr[-400:])

    # a sprite that changed after (one pixel): the header no longer matches it
    path = sprites / "racing_rev_on_n.png"
    im = Image.open(path).convert("RGBA")
    px = im.load()
    px[3, 4] = tuple((v + 1) % 256 for v in px[3, 4])
    im.save(path)
    r = run([sys.executable, gen, sprites, "--verify"], run_dir)
    check(r.returncode != 0, "--verify fails for a sprite that changed")
    make_sprites(sprites)

    # a header that changed after (one hex byte of the second sprite's data)
    good = header.read_text()
    start = good.index("lamp_off_n[] = {")
    pos = good.index("0x", start + 40)
    bad = good[: pos + 2] + ("1" if good[pos + 2] != "1" else "2") + good[pos + 3 :]
    header.write_text(bad)
    r = run([sys.executable, gen, sprites, "--verify"], run_dir)
    check(r.returncode != 0, "--verify fails for a header that changed")
    header.write_text(good)
    r = run([sys.executable, gen, sprites, "--verify"], run_dir)
    check(r.returncode == 0, "--verify passes again for the header put back")

    # the sprites of another disc (other pixels): a new header, which --verify passes and the old one fails
    other = tmp / "sprites_other"
    make_sprites(other, seed=2)
    r = run([sys.executable, gen, other], run_dir)
    check(r.returncode == 0 and header.read_text() != good, "other sprites make another header")
    r = run([sys.executable, gen, sprites, "--verify"], run_dir)
    check(r.returncode != 0, "--verify of the first sprites fails against the other header")
    header.write_text(good)


def test_refusals(tmp):
    cases = {}

    def case(name, build):
        root = tmp / ("refuse_" + name)
        gen, header = make_tree(root, None)
        sprites = root / "sprites"
        make_sprites(sprites)
        build(sprites)
        r = run([sys.executable, gen, sprites], tmp)
        check(r.returncode != 0, "the generator refuses: " + name)
        check(not header.exists(), "no header for: " + name)
        cases[name] = r

    case("a missing sprite", lambda d: (d / "racing_rev_on_m.png").unlink())
    case("a sprite of another size", lambda d: Image.new("RGBA", (32, 32), (1, 2, 3, 4)).save(d / "racing_rev_off_n.png"))
    case("a sprite that is not a PNG", lambda d: (d / "racing_rev_off_m.png").write_text("not an image"))
    case("no sprite folder", lambda d: shutil.rmtree(d))
    # a sprite without alpha is read as opaque RGBA (convert), as the arcade's PNGs are
    root = tmp / "refuse_rgb"
    gen, header = make_tree(root, None)
    sprites = root / "sprites"
    make_sprites(sprites)
    Image.new("RGB", (64, 64), (9, 8, 7)).save(sprites / "racing_rev_on_n.png")
    r = run([sys.executable, gen, sprites], tmp)
    check(r.returncode == 0 and header.exists(), "a 64x64 sprite without alpha is taken as opaque RGBA")
    px = embedded(header, "on_n")
    check(len(px) == 64 * 64 * 4 and px[:4] == bytes((9, 8, 7, 255)) and px[-4:] == bytes((9, 8, 7, 255)),
          "and is held as RGBA, alpha 255")


def test_scripts():
    sh = (REPO / "tools/gen_from_original.sh").read_text()
    check("\nset -e" in sh, "gen_from_original.sh stops at a failing step (so the lamp's must be best effort)")
    steps = [l for l in sh.splitlines()
             if "racing_lamp_data.py" in l and not l.lstrip().startswith(("#", "echo"))]
    check(len(steps) == 1, "gen_from_original.sh runs the lamp generator once")
    if steps:
        check("||" in steps[0], "the lamp generator's step does not stop the others when it fails")
        check('"$D/sprite"' in steps[0], "the lamp generator is given the game's sprite folder")
    check("/src/racing_lamp_data.h" in (REPO / ".gitignore").read_text().splitlines(), ".gitignore leaves the lamp's header out")
    for build in ("build_android.sh", "build.sh"):
        text = (REPO / build).read_text()
        check(" -lz" in text, build + " links zlib (hud.c inflates the lamp's art)")


def test_c(tmp, src):
    tree = tmp / "tree"
    (tree / "test").mkdir(exist_ok=True)
    for h in src.glob("*.h"):
        if h.name not in ("meterdb.h", "racing_lamp_data.h"):
            shutil.copy(h, tree / "src" / h.name)
    shutil.copy(src / "hud.c", tree / "src/hud.c")
    shutil.copy(REPO / "test/racing_lamp_meterdb.h", tree / "src/meterdb.h")
    for name in ("glstub.h", "racing_lamp_test.c"):
        shutil.copy(REPO / "test" / name, tree / "test" / name)
    check((tree / "src/racing_lamp_data.h").exists(), "the lamp's header is in the tree to build")
    cmd = ["gcc", "-std=gnu99", "-DRC_FPT=double", "-DGL_ZERO=0", "-DGL_ONE_MINUS_DST_COLOR=0x0307", "-Isrc", "-Itest",
           "test/racing_lamp_test.c", "-lm", "-lz", "-Wl,--unresolved-symbols=ignore-all", "-o", "racing_lamp_test"]
    r = run(cmd, tree)
    if not check(r.returncode == 0, "test/racing_lamp_test.c builds:\n" + r.stderr[:3000]):
        return
    run_dir = tmp / "run"
    run_dir.mkdir(exist_ok=True)
    r = run([tree / "racing_lamp_test", run_dir], run_dir)
    sys.stdout.write(r.stdout)
    if r.returncode != 0 and not r.stdout:
        print("racing_lamp_test died with", r.returncode, r.stderr[-400:])
    check(r.returncode == 0, "test/racing_lamp_test.c passes")


def main():
    args = sys.argv[1:]
    keep = "--keep" in args
    src = REPO / "src"
    if "--src" in args:
        src = Path(args[args.index("--src") + 1]).resolve()
    tmp = Path(tempfile.mkdtemp(prefix="racing_lamp_test_"))
    try:
        test_generator(tmp, src)
        test_refusals(tmp)
        test_scripts()
        test_c(tmp, src)
    finally:
        if keep:
            print("kept", tmp)
        else:
            shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        print("racing_lamp_test.py: %d FAILED" % fails)
        return 1
    print("racing_lamp_test.py: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
