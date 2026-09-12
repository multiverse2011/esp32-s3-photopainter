"""Glyph coverage tests for the renderer.  Requires Pillow and the bundled fonts."""

from __future__ import annotations

import unittest

from custom_components.photopainter.renderer import supported_text


class GlyphCoverageTests(unittest.TestCase):
    def test_emoji_without_a_glyph_is_dropped(self) -> None:
        self.assertEqual(supported_text("\U0001F382ゆうり", 24), "ゆうり")

    def test_the_space_an_emoji_leaves_behind_goes_with_it(self) -> None:
        self.assertEqual(supported_text("\U0001F382 ゆうり", 24), "ゆうり")
        self.assertEqual(supported_text("ゆうり \U0001F389", 24), "ゆうり")

    def test_japanese_latin_and_covered_symbols_survive(self) -> None:
        self.assertEqual(supported_text("映画 A2℃", 24), "映画 A2℃")

    def test_a_title_of_only_emoji_becomes_empty(self) -> None:
        self.assertEqual(supported_text("\U0001F382\U0001F389", 24), "")

    def test_inner_spacing_is_left_alone(self) -> None:
        self.assertEqual(supported_text("朝 会", 24), "朝 会")


if __name__ == "__main__":
    unittest.main()
