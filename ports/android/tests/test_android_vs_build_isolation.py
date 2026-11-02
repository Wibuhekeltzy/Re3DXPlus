"""Compile the real profile compatibility predicate against representative peers."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class VersusBuildTest(unittest.TestCase):
    def test_profiles(self):
        source = (ROOT / 'src/app.c').read_text()
        names = ('VS_PROFILE_BUILD', 'VS_BUILD_NAMESPACE', 'VS_BUILD_REVISION', 'VS_PROFILE')
        definitions = '\n'.join(re.search(r'^#define ' + name + r'\s+[^\n]+', source, re.M).group() for name in names)
        fn = re.search(r'static int vs_same_build\(const net_player \*pl\) \{[^}]+\}', source).group()
        harness = '#include <assert.h>\n#include "net.h"\n' + definitions + '\n' + fn + '''
int main(void) {
 net_player p = {0}; p.profile_len = VS_PROFILE;
 p.profile[VS_PROFILE_BUILD] = VS_BUILD_NAMESPACE;
 p.profile[VS_PROFILE_BUILD+1] = VS_BUILD_REVISION;
 assert(vs_same_build(&p));
 p.profile[VS_PROFILE_BUILD] = 'W'; p.profile[VS_PROFILE_BUILD+1] = 168;
 assert(vs_same_build(&p));
 p.profile[VS_PROFILE_BUILD] = 'A'; p.profile[VS_PROFILE_BUILD+1] = 1;
 assert(!vs_same_build(&p));
 p.profile[VS_PROFILE_BUILD] = VS_BUILD_NAMESPACE; p.profile[VS_PROFILE_BUILD+1] = VS_BUILD_REVISION + 1;
 assert(!vs_same_build(&p));
 p.profile_len = VS_PROFILE - 1; assert(!vs_same_build(&p));
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            c, exe = Path(d)/'test.c', Path(d)/'test'
            c.write_text(harness)
            subprocess.run(['cc', '-std=c99', '-Wall', '-Werror', '-I', str(ROOT/'src'), str(c), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

if __name__ == '__main__': unittest.main()
