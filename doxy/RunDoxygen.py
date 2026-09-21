#!/usr/bin/env python3
"""Generate an audience/visibility profile in Japanese and English."""

import argparse
import json
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

from GenerateDoxyfile import generate_doxyfiles, generate_layoutfiles, read_csv_range
from RepairSourceLinks import repair_source_links

ROOT = Path(__file__).resolve().parent.parent
RESOURCE = ROOT / "doxy/Resource"
PROFILES = ROOT / "doxy/profiles"
HEADER_SUFFIXES = {".h", ".hpp", ".hxx"}
SOURCE_SUFFIXES = HEADER_SUFFIXES | {".c", ".cc", ".cpp", ".cxx"}


def within(path, root):
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def quote(value):
    value = str(value).replace("\\", "/")
    if any(char in value for char in ('"', "\n", "\r")):
        raise ValueError("Doxygen paths must not contain quotes or line breaks")
    return '"' + value + '"'


@dataclass(frozen=True)
class Profile:
    audience: str
    visibility: str
    source_root: Path

    @property
    def name(self):
        return self.audience + "-" + self.visibility

    def output(self, check_only=False):
        return self.source_root / "doxy/generated" / self.name / ("check" if check_only else "full")


def load_profile(path=None):
    path = Path(path or PROFILES / "developer-public.json").resolve()
    data = json.loads(path.read_text(encoding="utf-8"))
    if set(data) != {"version", "audience", "visibility", "source_root"} or data["version"] != 1:
        raise ValueError("Unsupported documentation profile schema")
    if data["audience"] not in ("user", "developer") or data["visibility"] not in ("public", "confidential"):
        raise ValueError("Invalid documentation audience or visibility")
    source = (path.parent / data["source_root"]).resolve()
    if not within(path, source) or not (source / "api").is_dir():
        raise ValueError("The profile must belong to its source tree, which must contain api/")
    if data["visibility"] == "public" and source != ROOT.resolve():
        raise ValueError("Public profiles may read only the engine source tree")
    if data["visibility"] == "confidential":
        # A public SDK may be a submodule under an overlay's external/ directory. Only the
        # selected source directories are parsed; their scope must remain disjoint from it.
        selected = [source / name for name in ("api", "core", "platform", "doc")]
        if within(source, ROOT) or any(within(ROOT, directory) for directory in selected):
            raise ValueError("Confidential inputs must remain outside the public source scope")
    profile = Profile(data["audience"], data["visibility"], source)
    for check_only in (False, True):
        output = profile.output(check_only)
        if not within(output, source) or (profile.visibility == "confidential"
                                         and (within(output, ROOT) or within(ROOT, output))):
            raise ValueError("Documentation output escapes its source tree")
    return profile


def collect_files(root, relative, suffixes):
    directory = root / relative
    if not directory.is_dir():
        raise ValueError("A required documentation input directory is missing: " + relative)
    files = []
    for path in sorted(directory.rglob("*")):
        if path.is_file() and path.suffix.lower() in suffixes:
            if not within(path, directory) or not within(path, root):
                raise ValueError("Documentation input escapes its selected source directory")
            files.append(path.resolve())
    return files


def input_files(profile, language):
    roots = [ROOT] if profile.visibility == "public" else [ROOT, profile.source_root]
    files = []
    for root in roots:
        directories = ("api",) if profile.audience == "user" else ("api", "core", "platform")
        for directory in directories:
            files.extend(collect_files(root, directory, HEADER_SUFFIXES if profile.audience == "user" else SOURCE_SUFFIXES))
        if profile.audience == "developer":
            for locale in (("ja", "en") if language == "ja" else ("en",)):
                for directory in ("doc/" + locale, "doc/design/" + locale):
                    if (root / directory).is_dir():
                        files.extend(collect_files(root, directory, {".md"}))
    # Only the API landing page and its translation accompany User headers.
    files.extend(collect_files(ROOT, "doxy/pages", {".md"}))
    if profile.audience == "developer":
        files.extend(collect_files(ROOT, "tools/pi", {".md"}))
        files.append(ROOT / "doxy/README.md")
    return list(dict.fromkeys(files))


def remove_stale_user_xml(profile, output):
    """User sites omit raw XML, including output from earlier runner versions."""
    xml = output / "xml"
    if xml.resolve() != output.resolve() / "xml" or not within(xml, profile.source_root):
        raise ValueError("Stale User XML escapes its documentation output tree")
    if xml.exists():
        shutil.rmtree(xml)


def generate_profiles(directory, check_only=False, profile=None, languages=("ja", "en")):
    profile = profile or load_profile()
    directory = Path(directory)
    for target in [directory] + [profile.output(check_only) / ("lang_" + language) for language in languages]:
        if profile.visibility == "confidential" and (within(target, ROOT) or not within(target, profile.source_root)):
            raise ValueError("Confidential configuration, logs and output must stay in the overlay outside the public tree")
        if target != directory and not within(target, profile.source_root):
            raise ValueError("Documentation language output escapes its source tree")
    directory.mkdir(parents=True, exist_ok=True)
    html_header = (ROOT / "doxy/Style/header.html").read_text(encoding="utf-8")
    for language in {"en", "ja"} - set(languages):
        html_header = re.sub(r'\s*<option data-lang="' + language + r'"[^\n]+\n', "\n", html_header)
    header_path = directory / "header.html"
    header_path.write_text(html_header, encoding="utf-8")
    csv_path = str(RESOURCE / "transrate.csv")
    bounds = read_csv_range(csv_path, 1, 4, 1, 1)
    header = read_csv_range(csv_path, *(int(row[0]) for row in bounds))

    def section(index):
        return read_csv_range(csv_path, *(int(header[i][0]) for i in range(index, index + 4)))

    countries, doxy_parameters, layout_parameters = section(28), section(4), section(8)
    configs = []
    for country in countries:
        language = country[0]
        if language not in languages:
            continue
        output = profile.output(check_only) / ("lang_" + language)
        output.mkdir(parents=True, exist_ok=True)
        layout = directory / ("layout_" + language + ".xml")
        country[int(doxy_parameters[0][1])] = str(directory / ("doxyfile." + language))
        country[int(doxy_parameters[5][1])] = quote(layout)
        developer = profile.audience == "developer"
        if developer:
            generate_layoutfiles(str(RESOURCE / "layout.xml"), layout_parameters, [country], str(directory))
        else:
            tree = ET.parse(RESOURCE / "layout.xml")
            for tab in tree.findall("./navindex/tab"):
                if tab.get("type") in ("pages", "examples"):
                    tab.set("visible", "no")
            tree.write(layout, encoding="utf-8", xml_declaration=True)
        main_page = ROOT / ("doc/" + language + "/README.md" if developer else "doxy/pages/user." + language + ".md")
        settings = {
            "PROJECT_NAME": quote("WSE " + profile.audience.title() + " / " + profile.visibility.title()),
            "PROJECT_BRIEF": quote("API reference" if not developer else "API and implementation reference"),
            "INPUT": " ".join(quote(path) for path in input_files(profile, language)),
            "OUTPUT_DIRECTORY": quote(output),
            "WARN_LOGFILE": quote(output / "warnings.log"),
            "USE_MDFILE_AS_MAINPAGE": quote(main_page),
            "HTML_HEADER": quote(header_path),
            "PROJECT_LOGO": quote(ROOT / "resource/logo.png"),
            "PROJECT_ICON": quote(ROOT / "resource/icon.ico"),
            "HTML_FOOTER": quote(ROOT / "doxy/Style/footer.html"),
            "HTML_EXTRA_STYLESHEET": quote(ROOT / "doxy/Style/custom.css"),
            "HTML_EXTRA_FILES": quote(ROOT / "doxy/Style/filter-tree.js"),
            # Keep the catalog's navigation URLs identical on Windows and Linux.
            "CASE_SENSE_NAMES": "NO",
            "STRIP_FROM_PATH": " ".join(quote(path) for path in dict.fromkeys([ROOT, profile.source_root])),
            "EXTRACT_PRIVATE": "YES" if developer else "NO",
            "EXTRACT_STATIC": "YES" if developer else "NO",
            "EXTRACT_LOCAL_CLASSES": "YES" if developer else "NO",
            "INTERNAL_DOCS": "YES" if developer else "NO",
            "SOURCE_BROWSER": "YES" if developer else "NO",
            "INLINE_SOURCES": "YES" if developer else "NO",
            "VERBATIM_HEADERS": "YES" if developer else "NO",
            "XML_PROGRAMLISTING": "YES" if developer else "NO",
            "GENERATE_XML": "YES" if developer else "NO",
            "SEARCH_INCLUDES": "NO",
            "EXCLUDE_SYMLINKS": "YES",
            "EXAMPLE_PATH": "",
        }
        if check_only:
            settings.update(GENERATE_HTML="NO", GENERATE_LATEX="NO", GENERATE_XML="YES")
        configs.extend(generate_doxyfiles(str(RESOURCE / "Doxyfile"), doxy_parameters, [country], settings))
    return configs


def run_profile(executable, config, warning_log):
    warning_log.parent.mkdir(parents=True, exist_ok=True)
    warning_log.unlink(missing_ok=True)
    # Process output can contain implementation details; retain it in this profile's output tree.
    with (warning_log.parent / "doxygen.stdout.log").open("w", encoding="utf-8") as log:
        # Doxygen first tries Markdown links relative to its working directory.
        # A parent checkout can contain a different file at that relative path.
        # Run inside this edition's output, with all configured inputs absolute.
        result = subprocess.run([executable, str(Path(config).resolve())], cwd=warning_log.parent,
                                stdout=log, stderr=subprocess.STDOUT, check=False)
    return result.returncode == 0 and warning_log.is_file() and not warning_log.read_text(encoding="utf-8").strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--preset", choices=("user-public", "developer-public"))
    selection.add_argument("--profile", type=Path, help="Profile JSON; controlled profiles belong to their overlay")
    parser.add_argument("--language", choices=("en", "ja", "both"), default="both")
    parser.add_argument("--check", action="store_true", help="Check documentation with XML output only")
    parser.add_argument("--generate-only", action="store_true", help="Write Doxyfiles and layouts without running Doxygen")
    args = parser.parse_args()
    try:
        profile = load_profile(PROFILES / (args.preset + ".json") if args.preset else args.profile)
        languages = ("ja", "en") if args.language == "both" else (args.language,)
        directory = profile.output(args.check) / "config"
        configs = generate_profiles(directory, args.check, profile, languages)
    except (ValueError, OSError, TypeError) as error:
        parser.exit(1, "Invalid documentation profile: " + str(error) + "\n")
    if args.generate_only:
        print("Generated " + profile.name + " configuration: " + ascii(str(directory)))
        return 0
    executable = shutil.which("doxygen")
    if not executable:
        parser.exit(1, "Doxygen 1.13.2 or later is required on PATH.\n")
    version = subprocess.run([executable, "--version"], capture_output=True, text=True, check=True).stdout
    match = re.match(r"(\d+)\.(\d+)\.(\d+)", version)
    if not match or tuple(map(int, match.groups())) < (1, 13, 2):
        parser.exit(1, "Doxygen 1.13.2 or later is required for the version 2 layout.\n")
    if not args.check and not shutil.which("dot"):
        parser.exit(1, "Graphviz dot is required on PATH for full documentation generation.\n")
    passed = True
    for config in configs:
        language = Path(config).suffix[1:]
        output = profile.output(args.check) / ("lang_" + language)
        if profile.audience == "user" and not args.check:
            try:
                remove_stale_user_xml(profile, output)
            except (ValueError, OSError) as error:
                parser.exit(1, "Cannot prepare User documentation output: " + str(error) + "\n")
        success = run_profile(executable, config, output / "warnings.log")
        if success and not args.check and profile.audience == "developer":
            roots = [ROOT] if profile.visibility == "public" else [ROOT, profile.source_root]
            try:
                repair_source_links(output, input_files(profile, language), roots)
            except (ValueError, OSError, ET.ParseError) as error:
                parser.exit(1, "Cannot finalize Developer source links: " + str(error) + "\n")
        print(f"Doxygen {profile.name} {language}: {'PASS' if success else 'FAIL'} ({str(output)!a})", flush=True)
        passed = success and passed
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
