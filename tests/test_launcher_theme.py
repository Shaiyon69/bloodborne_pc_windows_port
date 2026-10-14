import re
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / 'launcher.py'
PALETTE_BLOCK = re.compile(r'PALETTE = \{.*?\n\}\n', re.S)
HEX_COLOR = re.compile(r'"#[0-9a-fA-F]{3,8}"')


class LauncherThemeTests(unittest.TestCase):
    def setUp(self):
        self.text = SOURCE.read_text(encoding='utf-8')

    def test_every_colour_comes_from_the_palette(self):
        body = PALETTE_BLOCK.sub('', self.text)
        self.assertEqual([], HEX_COLOR.findall(body))

    def test_palette_is_monochrome(self):
        block = PALETTE_BLOCK.search(self.text)
        self.assertIsNotNone(block)
        values = re.findall(r'"(#[0-9a-fA-F]{6})"', block.group(0))
        self.assertTrue(values)
        for value in values:
            r, g, b = (int(value[i:i + 2], 16) for i in (1, 3, 5))
            self.assertEqual((r, r), (g, b), f'{value} is not a shade of gray')


if __name__ == '__main__':
    unittest.main()
