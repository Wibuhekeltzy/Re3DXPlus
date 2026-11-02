import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import junction_pack as junction


class PrivatePathsTest(unittest.TestCase):
    def setUp(self):
        # Respect a caller-selected TMPDIR for synthetic test data.
        self.tmp = tempfile.TemporaryDirectory()
        self.base = Path(self.tmp.name).resolve()
        self.root = self.base / 'source'
        self.root.mkdir()
        self.private = self.base / 'private'
        self.private.mkdir()
        self.outside = self.base / 'outside'
        self.outside.mkdir()
        self.output = self.private / 'output'

    def tearDown(self):
        self.tmp.cleanup()

    def run_main(self, header=None):
        args = ['junction_pack.py', str(self.root), str(self.output),
                '--private-root', str(self.private)]
        if header is not None:
            args += ['--table-header', str(header)]
        with patch.object(sys, 'argv', args):
            junction.main()

    def test_main_symlink_escape_rejected_before_read(self):
        target = self.outside / 'main'
        target.write_bytes(b'synthetic')
        (self.root / 'main').symlink_to(target)
        with self.assertRaisesRegex(ValueError, 'input escapes root'):
            self.run_main()
        self.assertFalse(self.output.exists())

    def test_animation_and_sprite_symlink_escapes(self):
        target = self.outside / 'synthetic'
        target.write_bytes(b'synthetic')
        anime = self.root / 'data' / 'anime'
        anime.mkdir(parents=True)
        (anime / 'animation.anm').symlink_to(target)
        (self.root / 'sprite.png').symlink_to(target)
        for path in ('data/anime/animation.anm', 'sprite.png'):
            with self.subTest(path=path), self.assertRaisesRegex(ValueError, 'input escapes root'):
                junction.private_input(self.root, path)

    def test_invalid_header_rejected_before_any_output(self):
        existing = self.private / 'existing.h'
        existing.write_bytes(b'keep')
        dangling = self.private / 'dangling.h'
        dangling.symlink_to(self.private / 'missing')
        for header in (existing, dangling, self.outside / 'bad.h', self.output):
            with self.subTest(header=header), self.assertRaises((ValueError, FileExistsError)):
                self.run_main(header)
            self.assertFalse(self.output.exists())
        self.assertEqual(existing.read_bytes(), b'keep')

    def test_header_exclusive_and_private(self):
        header = self.private / 'new.h'
        junction.write_table_header(header, b'\0' * 384, self.private)
        self.assertEqual(header.stat().st_mode & 0o777, 0o600)
        before = header.read_bytes()
        with self.assertRaises(FileExistsError):
            junction.write_table_header(header, b'\1' * 384, self.private)
        self.assertEqual(header.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
