#!/usr/bin/env python3
"""
Extract all Localization.tr() strings from QML files and generate a translation template.
If the output file already exists, keeps existing translations and adds only new strings.
Usage: ./extract-translations.py --lang <code> [--display-name <name>]
Example: ./extract-translations.py --lang it --display-name "Italiano"
"""

import re
import json
import sys
import argparse
from pathlib import Path

def extract_strings(qml_dir):
    """Extract all Localization.tr() strings from QML files."""
    strings = set()

    for qml_file in sorted(qml_dir.glob("*.qml")):
        with open(qml_file, 'r', encoding='utf-8') as f:
            content = f.read()

        # Find all Localization.tr( and extract the full string content
        i = 0
        while True:
            # Find next Localization.tr(
            match = re.search(r'Localization\.tr\(\s*"', content[i:])
            if not match:
                break

            start = i + match.start() + len(match.group())
            # Now extract the string, handling multi-line concatenations
            s = ""
            j = start
            while j < len(content):
                char = content[j]

                if char == '"':
                    # End of a string segment
                    # Check if there's a + after it (concatenation)
                    j += 1
                    # Skip whitespace and +
                    while j < len(content) and content[j] in ' \t\n':
                        j += 1
                    if j < len(content) and content[j] == '+':
                        j += 1
                        while j < len(content) and content[j] in ' \t\n':
                            j += 1
                        if j < len(content) and content[j] == '"':
                            # Another string follows, continue
                            j += 1
                            continue
                    # No more concatenations, we're done
                    break
                elif char == '\\' and j + 1 < len(content):
                    # Escape sequence
                    s += content[j:j+2]
                    j += 2
                else:
                    s += char
                    j += 1

            if s:
                strings.add(s)

            i = j

    return sorted(strings)

def generate_json(strings, lang_code, display_name):
    """Generate translation template JSON."""
    data = {"lang.code": lang_code, "lang.display_name": display_name}
    for s in strings:
        data[s] = ""
    return json.dumps(data, indent=2, ensure_ascii=False)

def main():
    parser = argparse.ArgumentParser(
        description="Extract Localization.tr() strings and generate translation template.")
    parser.add_argument("--lang", required=True, metavar="CODE",
                        help="Language code (e.g. it, de, fr)")
    parser.add_argument("--display-name", metavar="NAME",
                        help="Display name for the language (e.g. \"Italiano\")")
    args = parser.parse_args()

    lang_code = args.lang
    display_name = args.display_name

    # Find project root (where yayc.pro is)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    qml_dir = project_root / "src" / "qml"

    if not qml_dir.exists():
        print(f"Error: QML directory not found: {qml_dir}")
        sys.exit(1)

    print(f"Extracting strings from {qml_dir}...")
    strings = extract_strings(qml_dir)

    # Output directory
    output_dir = project_root / "assets" / "i18n"
    output_dir.mkdir(parents=True, exist_ok=True)
    output_file = output_dir / f"{lang_code}.json"

    if output_file.exists():
        # Update: keep existing translations, add new strings
        with open(output_file, 'r', encoding='utf-8') as f:
            existing = json.load(f)

        existing["lang.code"] = lang_code
        if display_name is None:
            display_name = existing.get("lang.display_name", lang_code)
        else:
            existing["lang.display_name"] = display_name

        new_count = sum(1 for s in strings if s not in existing)
        for s in strings:
            if s not in existing:
                existing[s] = ""

        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(existing, f, indent=2, ensure_ascii=False)

        print(f"✓ Added {new_count} new strings to {output_file}")
        if new_count == 0:
            print("  Already up to date.")
    else:
        # Create new file
        if display_name is None:
            display_name = lang_code

        json_content = generate_json(strings, lang_code, display_name)

        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(json_content)

        print(f"✓ Extracted {len(strings)} strings")
        print(f"✓ Written to: {output_file}")
        print("\nNext step: Fill in the translations for each key.")

if __name__ == '__main__':
    main()
