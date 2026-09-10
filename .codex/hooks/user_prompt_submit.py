# -*- coding: utf-8 -*-
import json
import os
import re
import sys
import tempfile
from datetime import datetime, timezone


def read_input_json():
    raw_input = sys.stdin.read()
    if not raw_input or not raw_input.strip():
        return None

    try:
        return json.loads(raw_input)
    except json.JSONDecodeError:
        return None


def write_json(payload):
    sys.stdout.write(json.dumps(payload, ensure_ascii=False))


def get_engine_shader_guard_path():
    return os.path.join(tempfile.gettempdir(), "expedition_158-engine-shader-guard.json")


def reset_engine_shader_guard_state():
    state = {
        "turn_started_at": datetime.now(timezone.utc).astimezone().isoformat(),
        "read_paths": [],
    }
    with open(get_engine_shader_guard_path(), "w", encoding="utf-8") as state_file:
        json.dump(state, state_file, ensure_ascii=False, separators=(",", ":"))


def add_mode_hint(mode_hints, matched_rules, mode, rule):
    if mode not in mode_hints:
        mode_hints.append(mode)
    matched_rules.append(rule)


def has_explicit_edit_request(prompt):
    return bool(
        re.search(
            r"(?i)(PLEASE\s+IMPLEMENT\s+THIS\s+PLAN|please\s+implement|구현해줘|수정해줘|반영해줘|적용해줘|edit\s+the\s+files|implement\s+this)",
            prompt,
        )
    )


def main():
    input_json = read_input_json()
    if input_json is None:
        return 0

    prompt = str(input_json.get("prompt") or "")
    if not prompt.strip():
        return 0

    mode_hints = []
    matched_rules = []
    explicit_edit_request = has_explicit_edit_request(prompt)

    if not explicit_edit_request:
        if re.search(
            r"(?i)(diff\s*코드\s*블록|코드\s*블록|패치\s*코드|대화창에\s*코드|diff\s*로)",
            prompt,
        ):
            add_mode_hint(mode_hints, matched_rules, "chat_code_block", "explicit code/diff block request")

        if re.search(
            r"(?i)(확인만|검토만|리뷰만|문제(가)?\s*있는지\s*확인만|문제가\s*있는지\s*확인만|문제\s*있는지\s*확인만|비판적으로\s*(평가|검토)|평가만)",
            prompt,
        ):
            add_mode_hint(mode_hints, matched_rules, "review_only", "explicit review-only request")

        if re.search(
            r"(?i)(문서만|문서\s*작성|문서\s*정리|계획\s*문서|docs-only|doc-only|documentation-only)",
            prompt,
        ):
            add_mode_hint(mode_hints, matched_rules, "docs_only", "explicit documentation request")

    if re.search(
        r"(?i)(plan\s+first|plan-first|계획부터|플랜부터|짧은\s*계획|먼저\s*계획|라이트\s*플랜|표준\s*플랜|딥\s*플랜|가볍게만\s*잡아줘|깊게\s*잡아줘|책임\s*경계|정본|owner\s*boundary|ownership|public\s+api|구조\s*변경|아키텍처\s*수정)",
        prompt,
    ):
        add_mode_hint(mode_hints, matched_rules, "plan_first", "plan-first or boundary-sensitive request")

    if re.search(
        r"(?i)(Engine\b|EngineSDK|shared\s+editor|importer|asset\s+pipeline|public\s+api|새\s*문법|새\s*라이브러리|새\s*구현\s*패턴|승인|approval)",
        prompt,
    ):
        add_mode_hint(mode_hints, matched_rules, "approval_sensitive", "approval-sensitive surface mentioned")

    if re.search(
        r"(?i)(\bDT_|DT_[^\s]*\.(json|xlsx)|DT_GameObject|data\s*table|엑셀\s*행|테이블\s*행|데이터\s*테이블)",
        prompt,
    ):
        add_mode_hint(mode_hints, matched_rules, "dt_table_protected", "DT data-table surface mentioned")

    if not mode_hints:
        reset_engine_shader_guard_state()
        return 0

    reset_engine_shader_guard_state()

    log_path = os.path.join(tempfile.gettempdir(), "expedition_158-codex-prompt-mode.log")
    log_record = {
        "timestamp": datetime.now(timezone.utc).astimezone().isoformat(),
        "event": "UserPromptSubmit",
        "modes": mode_hints,
        "rules": matched_rules,
    }

    with open(log_path, "a", encoding="utf-8") as log_file:
        log_file.write(json.dumps(log_record, ensure_ascii=False, separators=(",", ":")) + "\n")

    context_lines = [
        "Expedition_158 prompt mode hints detected by project hook: "
        + ", ".join(sorted(mode_hints))
        + "."
    ]

    if "review_only" in mode_hints:
        context_lines.append(
            "If review_only is present and the user did not explicitly ask for edits, stay in review/verification mode and do not modify files."
        )

    if "docs_only" in mode_hints:
        context_lines.append(
            "If docs_only is present, keep the turn focused on documentation and avoid drifting into implementation code."
        )

    if "chat_code_block" in mode_hints:
        context_lines.append(
            "If chat_code_block is present, provide code or diff guidance in the conversation instead of editing files directly."
        )

    if "plan_first" in mode_hints:
        context_lines.append(
            "If plan_first is present, lock scope and clarify goal, risk, and implementation boundary before editing."
        )

    if "approval_sensitive" in mode_hints:
        context_lines.append(
            "If approval_sensitive is present, treat engine/shared/importer/public-API/new-pattern work as approval-gated unless the user already approved it."
        )

    context_lines.append(
        "If you touch any Engine_Shader_* HLSL/HLSLI path, first read the target file and its Engine/Client counterpart in the same turn before attempting any write."
    )

    if "dt_table_protected" in mode_hints:
        context_lines.append(
            "If dt_table_protected is present, do not write DT_* files under Client/Bin/Resources/Data/json or Client/Bin/Resources/Data/xlsx. Mentions in docs/code are allowed. If a DT row is needed, ask the user with a Markdown table containing target file, sheet/table, key/id, columns, values, and memo. Write memo in Korean and briefly describe what the ID represents."
        )

    write_json(
        {
            "continue": True,
            "hookSpecificOutput": {
                "hookEventName": "UserPromptSubmit",
                "additionalContext": "\n".join(context_lines),
            },
        }
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
