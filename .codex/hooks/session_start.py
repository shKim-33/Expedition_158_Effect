# -*- coding: utf-8 -*-
import json
import sys


def write_json(payload):
    sys.stdout.write(json.dumps(payload, ensure_ascii=False))


def main():
    context_lines = [
        "Expedition_158 session guardrails: use PowerShell-native commands by default.",
        "Treat EngineSDK as generated build/staging output: ignore routine mirror diffs unless the user asks about build/link/include/staging or clean-diff cleanup.",
        "Engine/shared/importer/public-API/new-pattern work is approval-gated.",
        "Prefer ownership and runtime boundary over call-site convenience.",
        "Engine_Shader_* HLSL/HLSLI edits require same-turn original/counterpart reads before any write attempt.",
    ]

    write_json(
        {
            "continue": True,
            "hookSpecificOutput": {
                "hookEventName": "SessionStart",
                "additionalContext": "\n".join(context_lines),
            },
        }
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
