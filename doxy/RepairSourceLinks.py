"""Keep authored source maps usable in a standalone Developer HTML directory."""

import html
import re
import xml.etree.ElementTree as ET
from pathlib import Path
from urllib.parse import unquote, urlsplit


def repair_source_links(output, inputs, roots):
    """Link parsed sources to Doxygen; display other checkout paths without dead links.

    Only the selected inputs supply XML mappings. No linked file is copied, parsed or
    added to the audience/visibility selection by this presentation step.
    """
    output = Path(output)
    selected = {Path(path).resolve() for path in inputs}
    roots = [Path(root).resolve() for root in roots]

    def resolve_location(location):
        path = Path(location)
        candidates = {candidate.resolve() for candidate in
                      ([path] if path.is_absolute() else [root / path for root in roots])}
        matches = candidates & selected
        return next(iter(matches)) if len(matches) == 1 else None

    sources, pages = {}, {}
    index = ET.parse(output / "xml/index.xml")
    for entry in index.findall("./compound"):
        if entry.get("kind") not in ("file", "page"):
            continue
        refid = entry.get("refid", "")
        if not re.fullmatch(r"[A-Za-z0-9_-]+", refid):
            raise ValueError("Invalid generated documentation identifier")
        compound = ET.parse(output / "xml" / (refid + ".xml")).find("./compounddef")
        location = compound.find("./location") if compound is not None else None
        source = resolve_location(location.get("file", "")) if location is not None else None
        if source is None:
            continue
        if entry.get("kind") == "page" and source.suffix == ".md":
            pages[refid + ".html"] = source
        elif entry.get("kind") == "file":
            for name in (refid + "_source.html", refid + ".html"):
                if (output / "html" / name).is_file():
                    sources[source] = name
                    break

    page_targets = {source: page for page, source in pages.items()}
    pattern = re.compile(r'<a\b[^>]*\bhref="([^"]+)"[^>]*>(.*?)</a>', re.DOTALL)
    for page, markdown in pages.items():
        path = output / "html" / page
        if not path.is_file():
            continue

        def replace(match):
            link = urlsplit(html.unescape(match.group(1)))
            if link.scheme or link.netloc or not (
                    link.path.startswith("../") or link.path.lower().endswith(".md")):
                return match.group(0)
            target = (markdown.parent / unquote(link.path)).resolve()
            if target in page_targets:
                # Doxygen can leave Markdown page links with explicit anchors unresolved.
                # Preserve the authored anchor on the selected generated page.
                href = page_targets[target]
                if link.query:
                    href += "?" + link.query
                if link.fragment:
                    href += "#" + link.fragment
                return '<a href="' + html.escape(href, quote=True) + '">' + match.group(2) + '</a>'
            if target in sources:
                # Markdown source anchors are not Doxygen's generated symbol anchors.
                return '<a href="' + sources[target] + '">' + match.group(2) + '</a>'
            # A source map may name tests, build scripts or language adapters outside INPUT.
            # Keep the path useful to a checkout reader without inventing a hosted URL.
            for root in roots:
                if target.is_relative_to(root):
                    relative = target.relative_to(root).as_posix()
                    return ('<span class="wse-source-path">' + match.group(2) +
                            ' <code>(' + html.escape(relative) + ')</code></span>')
            raise ValueError("Authored source link escapes the selected documentation roots")

        original = path.read_text(encoding="utf-8")
        repaired = pattern.sub(replace, original)
        if repaired != original:
            path.write_text(repaired, encoding="utf-8")
