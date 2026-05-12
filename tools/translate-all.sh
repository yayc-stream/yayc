#!/bin/bash
# Translate all i18n JSON files (skips en.json)
# Usage: ./translate-all.sh [--endpoint URL] [--api-key KEY] [--model NAME] [--ralph]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
I18N_DIR="$SCRIPT_DIR/../assets/i18n"

# Find a python3 that has the openai module
# Also search pyenv shims if available
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
        if "$candidate" -c "import openai" &>/dev/null 2>&1; then
            PYTHON="$candidate"
            break
        fi
    fi
done
if [ -z "$PYTHON" ]; then
    echo "Error: no python with 'openai' module found. Run: pip install openai"
    exit 1
fi
echo "Using python: $PYTHON"

for file in "$I18N_DIR"/*.json; do
    lang="$(basename "$file" .json)"
    if [ "$lang" = "en" ]; then
        continue
    fi
    echo "=== Translating: $file ==="
    "$PYTHON" "$SCRIPT_DIR/translate.py" "$file" --think-budget 0 "$@"
done
