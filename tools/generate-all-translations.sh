#!/bin/bash
# Generate translation templates for all supported languages
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTRACTOR="$SCRIPT_DIR/extract-translations.py"

PYENV_PYTHON=""
if [ -n "$PYENV_ROOT" ]; then
    PYENV_PYTHON="$PYENV_ROOT/shims/python3"
elif [ -f "$HOME/.pyenv/shims/python3" ]; then
    PYENV_PYTHON="$HOME/.pyenv/shims/python3"
fi

PYTHON=""
for candidate in python3 python "$PYENV_PYTHON"; do
    [ -z "$candidate" ] && continue
    if command -v "$candidate" &>/dev/null || [ -x "$candidate" ]; then
        if "$candidate" -c "import sys; sys.exit(0)" &>/dev/null 2>&1; then
            PYTHON="$candidate"
            break
        fi
    fi
done
[ -z "$PYTHON" ] && { echo "Error: python3 not found."; exit 1; }

declare -A LANGS=(
    [fr]="Français"
    [de]="Deutsch"
    [es]="Español"
    [it]="Italiano"
    [cn]="中文"
    [jp]="日本語"
    [ru]="Русский"
    [ar]="العربية"
    [kr]="한국어"
    [tg]="Tagalog"
    [th]="ภาษาไทย"
    [vn]="Tiếng Việt"
    [pl]="Polski"
    [gr]="Ελληνικά"
    [ro]="Română"
    [pt]="Português"
    [hi]="हिन्दी"
    [bn]="বাংলা"
    [ta]="தமிழ்"
    [id]="Bahasa"
    [nl]="Nederlands"
    [cs]="Čeština"
    [tr]="Türkçe"
    [hu]="Magyar"
    [he]="עברית"
    [sr]="Српски"
    [hr]="Hrvatski"
    [sl]="Slovenščina"
    [sq]="Shqip"
    [bg]="Български"
    [et]="Eesti"
    [lv]="Latviešu"
    [lt]="Lietuvių"
    [fi]="Suomi"
    [sv]="Svenska"
    [no]="Norsk"
    [da]="Dansk"
    [is]="Íslenska"
    [mn]="Монгол"
)

for lang in "${!LANGS[@]}"; do
    "$PYTHON" "$EXTRACTOR" --lang "$lang" --display-name "${LANGS[$lang]}"
done

echo ""
echo "Done. Languages generated: ${#LANGS[@]}"
