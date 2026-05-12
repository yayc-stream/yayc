#!/usr/bin/env python3
# If openai is missing, try to re-exec with pyenv python
import sys, os
try:
    import openai
except ImportError:
    pyenv_python = os.path.expanduser("~/.pyenv/shims/python3")
    if os.path.exists(pyenv_python) and os.path.abspath(sys.executable) != os.path.abspath(pyenv_python):
        os.execv(pyenv_python, [pyenv_python] + sys.argv)
    print("Error: 'openai' module not found. Run: pip install openai", file=sys.stderr)
    sys.exit(1)

"""
Translate YAYC i18n JSON files using an OpenAI-compatible LLM API.
Keys are English source strings; empty values need translation.
Already-translated entries are skipped.

Usage:
  ./translate.py assets/i18n/it.json [--endpoint URL] [--api-key KEY] [--model NAME] [--batch-size N] [--ralph]
"""

import json
import argparse
import time
from pathlib import Path
from openai import OpenAI

GREEN  = "\033[32m"
YELLOW = "\033[33m"
CYAN   = "\033[36m"
RESET  = "\033[0m"

def log(level: str, msg: str):
    prefix = {"info": "[ info ]", "warn": "[ warn ]", "skip": "[ skip ]", "ok": "[  ok  ]", "err": "[ ERR  ]"}.get(level, "[      ]")
    print(f"{prefix} {msg}", flush=True)

def truncate(text: str, max_len: int = 80) -> str:
    return text if len(text) <= max_len else text[:max_len] + "…"

def get_default_model(client: OpenAI) -> str:
    return client.models.list().data[0].id

def translate_batch(client: OpenAI, model: str, strings: list[str], lang: str, display_name: str, timeout: int = 600, think_budget: int = None) -> tuple[dict, object]:
    messages = [
        {
            "role": "system",
            "content": (
                f"You are a translator. Translate each English UI string into {display_name} ({lang}). "
                "Preserve formatting, placeholders (like %1, %2), newlines (\\n), and HTML tags. "
                "Keep very short strings (single words, symbols like '/') natural in context. "
                "Respond ONLY with a valid JSON object mapping each English string to its translation, "
                "with no explanation:\n"
                '{ "<english string>": "<translation>", ... }'
            )
        },
        {
            "role": "user",
            "content": json.dumps({s: "" for s in strings}, ensure_ascii=False)
        }
    ]
    extra = {}
    if think_budget is not None and think_budget >= 0:
        extra["extra_body"] = {"budget_tokens": think_budget}
    response = client.chat.completions.create(
        model=model,
        messages=messages,
        temperature=0.2,
        response_format={"type": "json_object"},
        timeout=timeout,
        **extra
    )
    raw = response.choices[0].message.content.strip()
    return json.loads(raw), response.usage


def main():
    parser = argparse.ArgumentParser(
        description="Translate YAYC i18n JSON via OpenAI-compatible API.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  ./translate.py assets/i18n/it.json\n"
            "  ./translate.py assets/i18n/de.json --endpoint http://localhost:8081/v1 --api-key token\n"
            "  ./translate.py assets/i18n/fr.json --model gpt-4o --ralph\n"
            "  ./translate.py assets/i18n/ja.json --batch-size 20"
        )
    )
    parser.add_argument("file",         metavar="FILE",                  help="Path to i18n JSON file (e.g. assets/i18n/it.json)")
    parser.add_argument("--endpoint",   "-e", default="http://localhost:8081/v1", help="API base URL")
    parser.add_argument("--api-key",    "-k", default="none",            help="API key")
    parser.add_argument("--model",      "-m", default=None,              help="Model name (auto-detected if omitted)")
    parser.add_argument("--batch-size", "-b", default=40, type=int,      help="Strings per API call (default: 40)")
    parser.add_argument("--timeout",      "-t", default=600, type=int,   help="Request timeout in seconds (default: 600)")
    parser.add_argument("--think-budget", default=500, type=int,         help="Cap thinking tokens (llama.cpp budget_tokens, 0 = disable, default: 500)")
    parser.add_argument("--ralph",        action="store_true",           help="Retry until no errors")
    parser.add_argument("--dry-run",    action="store_true",             help="Print curl commands instead of calling API")
    args = parser.parse_args()

    lang_file = Path(args.file)
    if not lang_file.exists():
        log("err", f"{lang_file} not found.")
        return

    # Pull lang code from filename, validate against lang.code in JSON
    lang = lang_file.stem

    with open(lang_file, "r", encoding="utf-8") as f:
        probe = json.load(f)

    json_lang = probe.get("lang.code")
    if json_lang is None:
        log("warn", "lang.code missing from JSON — skipping validation. Re-run extract-translations.py to add it.")
    elif json_lang != lang:
        log("err", f"lang.code mismatch: filename says '{lang}', JSON says '{json_lang}'. Aborting.")
        return

    client = OpenAI(base_url=args.endpoint, api_key=args.api_key)
    model  = args.model or get_default_model(client)
    log("info", f"Model: {model}")
    log("info", f"File:  {lang_file}")

    ralph_pass        = 0
    grand_total_tokens = 0
    t_start           = time.time()

    while True:
        ralph_pass += 1

        with open(lang_file, "r", encoding="utf-8") as f:
            data = json.load(f)

        display_name = data.get("lang.display_name", lang)

        # Collect untranslated strings (skip lang.display_name)
        todo = [k for k, v in data.items() if k != "lang.display_name" and not v]

        if not todo:
            log("info", "All strings already translated.")
            break

        log("info", f"Strings to translate: {len(todo)}  |  Language: {display_name} ({lang})  |  Batch size: {args.batch_size}")

        total_tokens    = 0
        translated_count = 0
        error_count     = 0
        batches         = [todo[i:i + args.batch_size] for i in range(0, len(todo), args.batch_size)]

        for idx, batch in enumerate(batches, 1):
            log("info", f"Batch [{idx}/{len(batches)}] — {len(batch)} strings")
            if args.dry_run:
                payload = {
                    "model": model,
                    "temperature": 0.2,
                    "response_format": {"type": "json_object"},
                    **({"budget_tokens": args.think_budget} if args.think_budget is not None else {}),
                    "messages": [
                        {"role": "system", "content": (
                            f"You are a translator. Translate each English UI string into {display_name} ({lang}). "
                            "Preserve formatting, placeholders (like %1, %2), newlines (\\n), and HTML tags. "
                            "Keep very short strings (single words, symbols like '/') natural in context. "
                            "Respond ONLY with a valid JSON object mapping each English string to its translation, "
                            "with no explanation:\n"
                            '{ "<english string>": "<translation>", ... }'
                        )},
                        {"role": "user", "content": json.dumps({s: "" for s in batch}, ensure_ascii=False)}
                    ]
                }
                payload_json = json.dumps(payload, ensure_ascii=False, indent=2)
                print(f"\ncurl {args.endpoint}/chat/completions \\")
                print(f"  -H 'Content-Type: application/json' \\")
                print(f"  -H 'Authorization: Bearer {args.api_key}' \\")
                print(f"  -d '{payload_json}'\n")
                continue
            t0 = time.time()
            try:
                result, usage = translate_batch(client, model, batch, lang, display_name, args.timeout, args.think_budget)
                elapsed = time.time() - t0
                if usage:
                    total_tokens        += usage.total_tokens
                    grand_total_tokens  += usage.total_tokens
                    log("info", f"  tokens: {usage.prompt_tokens} in / {usage.completion_tokens} out ({elapsed:.1f}s)")

                for src in batch:
                    translation = result.get(src, "").strip()
                    if not translation:
                        log("warn", f"  missing: {truncate(src)}")
                        error_count += 1
                        continue
                    data[src] = translation
                    log("ok", f"  {truncate(src)} → {truncate(translation)}")
                    translated_count += 1

                # Save after every batch
                with open(lang_file, "w", encoding="utf-8") as f:
                    json.dump(data, f, indent=2, ensure_ascii=False)

            except json.JSONDecodeError as ex:
                log("err", f"Batch [{idx}] — bad JSON response: {ex}")
                error_count += len(batch)
            except Exception as ex:
                log("err", f"Batch [{idx}] — {ex}")
                error_count += len(batch)

        elapsed_total = time.time() - t_start
        log("info", "─" * 50)
        log("info", f"Pass {ralph_pass} done in {elapsed_total:.1f}s")
        log("info", f"  translated : {translated_count}")
        log("info", f"  errors     : {error_count}")
        log("info", f"  tokens     : {total_tokens}")
        log("info", f"  saved to   : {lang_file}")

        if not args.ralph or error_count == 0:
            break
        log("info", f"--ralph: {error_count} error(s), retrying...")

    log("info", f"Total tokens: {grand_total_tokens}")


if __name__ == "__main__":
    main()
