#!/bin/bash
# Generate translation templates for all supported languages
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTRACTOR="$SCRIPT_DIR/extract-translations.py"

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
)

for lang in "${!LANGS[@]}"; do
    python3 "$EXTRACTOR" --lang "$lang" --display-name "${LANGS[$lang]}"
done

echo ""
echo "Done. Languages generated: ${#LANGS[@]}"
