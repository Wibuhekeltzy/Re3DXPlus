"""Host check of production chapter selection, using synthetic menu state only."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / 'ports/android/src'


def function(text, marker):
    start = text.index(marker)
    brace = text.index('{', start)
    depth = 0
    for i in range(brace, len(text)):
        depth += (text[i] == '{') - (text[i] == '}')
        if depth == 0:
            return text[start:i + 1]
    raise ValueError(marker)


class SelectiveChapters(unittest.TestCase):
    def test_pages_do_not_clear_real_progress(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        if not compiler:
            self.skipTest('no C compiler')
        text = (SRC / 'menu.c').read_text()
        markers = ['static int st_div_of(', 'static int st_loop(',
                   'static int st_division(', 'static int st_cleared(',
                   'static void st_states_of(', 'void menu_story_clear(']
        fragments = [function(text, m) for m in markers]
        code = '''#include "menu.h"\n#include <assert.h>\n#include <string.h>\n'''
        code += 'typedef struct { int level[4], ep[4][5], lv, slot; } st_states;\n'
        code += '\n'.join(fragments)
        code += '''
int main(void) {
  menu_state m = {0}; st_states s;
  m.st_div = 3; m.st_bits = 0x1234;
  for (int p = 1; p <= 10; ++p) {
    m.story_test = p;
    assert(st_division(&m) == (p-1)%5 && st_loop(&m) == (p-1)/5);
    st_states_of(&m, &s);
    for (int lv = 0; lv < 4; ++lv) {
      assert(s.level[lv] == 1);
      for (int ep = 0; ep < 5; ++ep) assert(s.ep[lv][ep] == 1);
    }
    menu_story_clear(&m, 19);
    assert(m.st_div == 3 && m.st_bits == 0x1234 && !m.garage_changed);
  }
  m.story_test = 0; menu_story_clear(&m, 79);
  assert(m.st_div == 4 && !m.st_bits && m.garage_changed);
  return 0;
}\n'''
        with tempfile.TemporaryDirectory() as tmp:
            source, binary = Path(tmp) / 'test.c', Path(tmp) / 'test'
            source.write_text(code)
            subprocess.run([compiler, '-std=c99', '-I', str(SRC), str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
