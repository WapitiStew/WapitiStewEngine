"""Standalone HTML source maps must not leak or invent source inputs."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "doxy"))
from RepairSourceLinks import repair_source_links


class SourceLinkTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="wse-doc-links-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.output = self.root / "generated"
        self.design = self.root / "doc/design/en/Design.md"
        self.source = self.root / "core/Widget.cpp"
        for path in (self.design, self.source):
            self.write(path, "fixture")
        self.write(self.output / "xml/index.xml", '<doxygenindex>'
                   '<compound kind="page" refid="design"/>'
                   '<compound kind="file" refid="widget"/>'
                   '<compound kind="file" refid="stale"/></doxygenindex>')
        for refid, location in (("design", "doc/design/en/Design.md"),
                                ("widget", "core/Widget.cpp"), ("stale", "outside/Absent.cpp")):
            self.write(self.output / "xml" / (refid + ".xml"),
                       '<doxygen><compounddef><location file="' + location + '"/></compounddef></doxygen>')
        self.write(self.output / "html/widget_source.html", "source")
        self.write(self.output / "html/stale_source.html", "unselected stale output")

    @staticmethod
    def write(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def repair(self, text):
        page = self.output / "html/design.html"
        self.write(page, text)
        repair_source_links(self.output, [self.design, self.source], [self.root])
        return page.read_text(encoding="utf-8")

    def test_parsed_source_links_to_generated_listing(self):
        result = self.repair('<a class="el" href="../../../core/Widget.cpp">Widget</a>')
        self.assertEqual(result, '<a href="widget_source.html">Widget</a>')

    def test_hyphenated_generated_identifier_links_to_selected_source(self):
        index = self.output / "xml/index.xml"
        self.write(index, index.read_text(encoding="utf-8").replace(
            'refid="widget"', 'refid="addon-source_2Widget"'))
        (self.output / "xml/widget.xml").rename(self.output / "xml/addon-source_2Widget.xml")
        (self.output / "html/widget_source.html").rename(
            self.output / "html/addon-source_2Widget_source.html")
        result = self.repair('<a href="../../../core/Widget.cpp">Widget</a>')
        self.assertEqual(result, '<a href="addon-source_2Widget_source.html">Widget</a>')

    def test_generated_identifier_cannot_escape_output(self):
        index = self.output / "xml/index.xml"
        original = index.read_text(encoding="utf-8")
        for refid in ("../widget", "/widget", "a/b", "a\\b", "a:widget", "a.widget"):
            with self.subTest(refid=refid):
                self.write(index, original.replace('refid="widget"', 'refid="' + refid + '"'))
                with self.assertRaisesRegex(ValueError, "Invalid generated documentation identifier"):
                    self.repair('<a href="../../../core/Widget.cpp">Widget</a>')

    def test_unselected_source_is_path_text_and_never_copied(self):
        result = self.repair('<a href="../../../outside/Absent.cpp">Missing</a>')
        self.assertNotIn('href=', result)
        self.assertIn('<code>(outside/Absent.cpp)</code>', result)
        self.assertFalse((self.output / "html/outside").exists())

    def test_generated_and_external_links_are_unchanged(self):
        source = '<a href="widget.html#member">API</a><a href="https://example.org/a">External</a>'
        self.assertEqual(self.repair(source), source)

    def test_selected_markdown_page_preserves_explicit_anchor(self):
        result = self.repair('<a href="Design.md#owned-values">Ownership</a>')
        self.assertEqual(result, '<a href="design.html#owned-values">Ownership</a>')
        self.assertEqual(self.repair(result), result)

    def test_parent_markdown_reference_and_unicode_anchor(self):
        result = self.repair('<a href="../en/Design.md#データの高速アクセス">Access</a>')
        self.assertEqual(result, '<a href="design.html#データの高速アクセス">Access</a>')

    def test_unselected_markdown_is_not_linked_to_stale_output(self):
        self.write(self.output / "html/Other.html", "stale output")
        result = self.repair('<a href="Other.md#section">Other</a>')
        self.assertNotIn('href=', result)
        self.assertIn('<code>(doc/design/en/Other.md)</code>', result)

    def test_outside_root_reference_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "escapes"):
            self.repair('<a href="../../../../outside.txt">Outside</a>')

    def test_reference_page_fallback_and_repeat_are_stable(self):
        (self.output / "html/widget_source.html").rename(self.output / "html/widget.html")
        result = self.repair('<a href="../../../core/Widget.cpp">Widget</a>')
        self.assertEqual(result, '<a href="widget.html">Widget</a>')
        self.assertEqual(self.repair(result), result)


if __name__ == "__main__":
    unittest.main()
