"""Asset-free source guards; not a substitute for interactive testing."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]

class StoryTimezoneContract(unittest.TestCase):
    def test_both_ports(self):
        for port in ("android", "ipad"):
            with self.subTest(port=port):
                src = ROOT / "ports" / port / "src"
                menu = (src / "menu.c").read_text()
                app = (src / "app.c").read_text()
                for token in ("m->story_day_select = 1;", "m->story_zone = m->zone;",
                              "m->sel = m->story_n % 5;", "m->story_zones_saved = m->zones;",
                              "m->zones = m->story_zones_saved;",
                              "m->song = 1 + rand() % (card_bgm_lines(m) - 2);"):
                    self.assertIn(token, menu)
                # Cancellation returns to the same rival without committing the zone.
                cancel = menu.split("if (back && m->screen == MENU_BGM && m->story_day_select", 1)[1].split("\n  }", 1)[0]
                for token in ("m->story_day_select = 0;", "m->zones = m->story_zones_saved;",
                              "m->zone = m->story_previous_zone;", "st_rival_enter(m);",
                              "m->sel = m->story_n % 5;"):
                    self.assertIn(token, cancel)
                self.assertNotIn("m->story_zone =", cancel)
                self.assertNotIn("m->bgm =", cancel)
                if port == "android":
                    self.assertIn("#if defined(__ANDROID__) || defined(WMMT_BGM_TEST)\n  int bgm = !m->vs.day || m->vs.me == m->vs.host;\n#else\n  int bgm = !m->story_day_select", menu)
                    self.assertIn("#if !defined(__ANDROID__) && !defined(WMMT_BGM_TEST)\n  if (m->story_day_select) return;\n#endif", menu)
                    self.assertIn("if (m->vs.day && m->vs.me != m->vs.host) return;", menu)
                    self.assertIn("m->vs.state != MENU_VS_OFF ? 0 : menu_card_bgm_source(m)", menu)
                    story = menu.split("if (m->story_day_select) {\n      snd_menu(SND_SELECT); m->story_zone = m->zone;", 1)[1].split("} else day_decide(m);", 1)[0]
                    self.assertIn("#if defined(__ANDROID__) || defined(WMMT_BGM_TEST)", story)
                    self.assertIn("m->song = line == 0 ? 1 + rand() % (lines - 2) : line == lines - 1 ? -1 : line;", story)
                    self.assertNotIn("m->bgm =", story)
                    self.assertNotIn("m->bgm_source =", story)
                    self.assertIn("m->sel = m->song;", menu)
                else:
                    self.assertIn("!m->story_day_select && (!m->vs.day", menu)
                    self.assertIn("m->story_day_select || (m->vs.day", menu)
                for token in ("%s/story_timezone", "story_tz ? fgetc(story_tz) == '1' : 0",
                              "story_was_selecting && G.menu.story_day_select && G.menu.deciding",
                              "G.menu.game == MENU_GAME_TA ? fopen(path, \"w\") : NULL",
                              "G.menu.story_day_select ? 0 : G.story_race",
                              "G.menu.zone >= G.menu.zones"):
                    self.assertIn(token, app)

if __name__ == "__main__":
    unittest.main()
