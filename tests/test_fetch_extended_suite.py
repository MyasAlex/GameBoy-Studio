"""Pinned downloads and ZIP containment checks for the hardware ROM suite."""
import hashlib
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile

import fetch_extended_suite as fetcher


class ExtendedSuiteFetchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / 'suite.zip'
        self.output = self.root / 'extracted'

    def write_archive(self, names):
        with zipfile.ZipFile(self.archive, 'w') as bundle:
            for name in names:
                bundle.writestr(name, b'test-data')

    def test_verified_archive_extracts_nested_files(self):
        self.write_archive(['same-suite/ppu/test.gb', 'reference.png'])
        digest = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.assertEqual(fetcher.verify_archive(self.archive, digest), digest)
        self.assertEqual(fetcher.extract_archive(self.archive, self.output), 2)
        self.assertEqual((self.output / 'same-suite' / 'ppu' / 'test.gb').read_bytes(), b'test-data')

    def test_cached_archive_with_wrong_pinned_hash_is_rejected(self):
        self.write_archive(['test.gb'])
        with self.assertRaisesRegex(ValueError, 'SHA256 mismatch'):
            fetcher.download_archive(self.archive)
        self.assertFalse(self.output.exists())

    def test_unsafe_paths_are_rejected_before_any_file_is_extracted(self):
        for unsafe in ('../outside.gb', r'..\outside.gb', '/outside.gb',
                       'C:/outside.gb', 'test.gb:alternate'):
            with self.subTest(member=unsafe):
                self.write_archive(['safe.gb', unsafe])
                with self.assertRaisesRegex(ValueError, 'Unsafe ZIP member'):
                    fetcher.extract_archive(self.archive, self.output)
                self.assertFalse(self.output.exists())
                self.assertFalse((self.root / 'outside.gb').exists())

    def test_zip_symbolic_link_is_rejected(self):
        with zipfile.ZipFile(self.archive, 'w') as bundle:
            entry = zipfile.ZipInfo('link')
            entry.create_system = 3
            entry.external_attr = (stat.S_IFLNK | 0o777) << 16
            bundle.writestr(entry, '../outside')
        with self.assertRaisesRegex(ValueError, 'Unsafe ZIP member'):
            fetcher.extract_archive(self.archive, self.output)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
