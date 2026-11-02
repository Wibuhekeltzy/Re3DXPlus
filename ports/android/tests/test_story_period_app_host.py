"""Compile actual app/menu control fragments with a stub HUD, no device or assets."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
FIXTURE = ROOT / "tests/story_period_app_host.c"

def block(src, marker):
    i = src.index(marker)
    left = src.index("{", i)
    depth = 0
    for j in range(left, len(src)):
        if src[j] == "{": depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0: return src[i:j+1]
    raise ValueError(marker)

def between(src, start, end):
    i = src.index(start)
    return src[i:src.index(end, i)]

class StoryPeriodAppHost(unittest.TestCase):
    def test_production_fragments(self):
        cc = shutil.which("clang") or shutil.which("cc")
        if not cc: self.skipTest("C compiler unavailable")
        app = (SRC / "app.c").read_text()
        menu = (SRC / "menu.c").read_text()
        fixture = FIXTURE.read_text()
        go = block(menu, "static void go(menu_state *m, int screen) {")
        finish = block(menu, "void menu_finish_story(menu_state *m, int won, int next, float gap, int fade) {")
        back = block(menu, "if (m->finish_story == 3) {")
        ending = between(app, "      int pick = G.menu.story_pick;", "        /* the points first:") + "      }\n"
        start = block(app, "{\n        int select = !ten && !vs && G.menu.game == MENU_GAME_STORY")
        pause = between(app, "      G.story_race = 0;\n      G.story_cut_pending = 0;", "      hud_init(")
        for marker, text in [("/* GO */", go), ("/* FINISH */", finish), ("/* BACK */", back), ("/* END */", ending), ("/* START */", start), ("/* PAUSE */", pause)]:
            fixture = fixture.replace(marker, text)
        with tempfile.TemporaryDirectory(prefix="story-period-") as tmp:
            c = Path(tmp) / "fixture.c"
            exe = Path(tmp) / "fixture"
            c.write_text(fixture)
            subprocess.run([cc, "-std=gnu11", "-I", str(SRC), str(c), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

if __name__ == "__main__": unittest.main()
