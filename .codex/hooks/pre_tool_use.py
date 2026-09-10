# -*- coding: utf-8 -*-
import json
import os
import re
import sys
import tempfile
from pathlib import Path


HOOK_DIR = Path(__file__).resolve().parent
if str(HOOK_DIR) not in sys.path:
    sys.path.insert(0, str(HOOK_DIR))

from managed_workspace import is_registered_workspace_root


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


def load_engine_shader_guard_state():
    path = get_engine_shader_guard_path()
    if not os.path.exists(path):
        return {"read_paths": []}

    try:
        with open(path, "r", encoding="utf-8") as state_file:
            loaded = json.load(state_file)
    except (OSError, json.JSONDecodeError):
        return {"read_paths": []}

    if not isinstance(loaded, dict):
        return {"read_paths": []}

    read_paths = loaded.get("read_paths")
    if not isinstance(read_paths, list):
        loaded["read_paths"] = []
    return loaded


def save_engine_shader_guard_state(state):
    path = get_engine_shader_guard_path()
    with open(path, "w", encoding="utf-8") as state_file:
        json.dump(state, state_file, ensure_ascii=False, separators=(",", ":"))


def get_tool_text(tool_input):
    if isinstance(tool_input, dict):
        values = []
        for key in ("command", "cmd", "input", "patch", "content"):
            value = tool_input.get(key)
            if value is not None:
                values.append(str(value))
        return "\n".join(values)

    if tool_input is None:
        return ""

    return str(tool_input)


def normalize_path(path_text):
    normalized = str(path_text).strip().strip("\"'`<>").replace("\\", "/")
    normalized = re.sub(r"^[A-Za-z]:/", "", normalized)
    normalized = normalized.lstrip("./")
    return normalized


def is_engine_shader_path(path_text):
    normalized = normalize_path(path_text)
    return re.search(
        r"(?i)^(?:Engine|Client)/Bin/ShaderFiles/Engine_Shader_[^/]+\.(?:hlsl|hlsli)$",
        normalized,
    ) is not None


def extract_engine_shader_paths(tool_text):
    path_pattern = r"(?i)(?:[A-Za-z]:/)?(?:\.?/)?(?:Engine|Client)/Bin/ShaderFiles/Engine_Shader_[^\\/'\"`\s>]+?\.(?:hlsl|hlsli)"
    found = []
    for match in re.finditer(path_pattern, tool_text.replace("\\", "/")):
        normalized = normalize_path(match.group(0))
        if normalized not in found:
            found.append(normalized)
    return found


def extract_patch_target_paths(tool_text):
    targets = []
    for match in re.finditer(
        r"(?m)^\*\*\*\s+(?:Add|Update|Delete)\s+File:\s*(.+?)\s*$|^\*\*\*\s+Move to:\s*(.+?)\s*$",
        tool_text,
    ):
        candidate = match.group(1) or match.group(2)
        normalized = normalize_path(candidate)
        if normalized not in targets:
            targets.append(normalized)
    return targets


def get_counterpart_paths(path_text):
    normalized = normalize_path(path_text)
    suffix = normalized.split("/Bin/ShaderFiles/", 1)[-1]

    counterparts = []
    if normalized.startswith("Engine/"):
        counterparts.append(normalized)
        client_path = "Client/Bin/ShaderFiles/" + suffix
        if client_path != normalized:
            counterparts.append(client_path)
    elif normalized.startswith("Client/"):
        engine_path = "Engine/Bin/ShaderFiles/" + suffix
        counterparts.append(normalized)
        counterparts.append(engine_path)
    else:
        counterparts.append(normalized)

    deduped = []
    for item in counterparts:
        if item not in deduped:
            deduped.append(item)
    return deduped


def add_read_paths(state, paths):
    known = set(state.get("read_paths") or [])
    changed = False
    for path_text in paths:
        normalized = normalize_path(path_text)
        if normalized and normalized not in known:
            known.add(normalized)
            changed = True
    if changed:
        state["read_paths"] = sorted(known)
        save_engine_shader_guard_state(state)


def tool_is_read_only(tool_name, tool_text):
    normalized_tool_name = str(tool_name or "")
    if "apply_patch" in normalized_tool_name:
        return False

    read_patterns = [
        r"(?i)\b(Get-Content|type|gc|rg|Select-String|git\s+diff|git\s+show|fc)\b",
    ]
    return any(re.search(pattern, tool_text) for pattern in read_patterns)


def tool_writes_engine_shader(tool_name, tool_text):
    normalized_tool_name = str(tool_name or "")
    if "apply_patch" in normalized_tool_name:
        return any(is_engine_shader_path(path_text) for path_text in extract_patch_target_paths(tool_text))

    if not extract_engine_shader_paths(tool_text):
        return False

    write_patterns = [
        r"(?i)\b(Set-Content|Add-Content|Out-File|Copy-Item|Move-Item|Remove-Item|New-Item)\b",
        r"(?i)\bpython\b[\s\S]*(?:open\s*\(|write_text|dump\s*\()",
        r"(?i)>\s*[\"'`]?(?:[A-Za-z]:/)?(?:\.?/)?(?:Engine|Client)/Bin/ShaderFiles/Engine_Shader_",
    ]
    return any(re.search(pattern, tool_text) for pattern in write_patterns)


def get_engine_shader_write_targets(tool_name, tool_text):
    normalized_tool_name = str(tool_name or "")
    if "apply_patch" in normalized_tool_name:
        return [path_text for path_text in extract_patch_target_paths(tool_text) if is_engine_shader_path(path_text)]
    return extract_engine_shader_paths(tool_text)


def get_missing_engine_shader_reads(state, targets):
    read_paths = set(state.get("read_paths") or [])
    missing = {}
    for target in targets:
        required_reads = get_counterpart_paths(target)
        missing_reads = [path_text for path_text in required_reads if path_text not in read_paths]
        if missing_reads:
            missing[target] = missing_reads
    return missing


def is_protected_dt_table_path(path_text):
    normalized = str(path_text).strip().strip("\"'`<>").replace("\\", "/")
    normalized = normalized.lstrip("./")
    return re.search(
        r"(?i)(?:^|/)Client/Bin/Resources/Data/(?:json/DT_[^/]+\.json|xlsx/DT_[^/]+\.xlsx)$",
        normalized,
    ) is not None


def patch_targets_protected_dt_table(tool_text):
    for match in re.finditer(
        r"(?m)^\*\*\*\s+(?:Add|Update|Delete)\s+File:\s*(.+?)\s*$|^\*\*\*\s+Move to:\s*(.+?)\s*$",
        tool_text,
    ):
        target_path = match.group(1) or match.group(2)
        if is_protected_dt_table_path(target_path):
            return True
    return False


def shell_writes_protected_dt_table(tool_text):
    protected_path = r"(?i)(?:^|[\\/'\"`\s<])Client[\\/]+Bin[\\/]+Resources[\\/]+Data[\\/]+(?:json[\\/]+DT_[^\\/'\"`\s>]*\.json|xlsx[\\/]+DT_[^\\/'\"`\s>]*\.xlsx)\b"
    if not re.search(protected_path, tool_text):
        return False

    write_patterns = [
        r"(?i)\b(Set-Content|Add-Content|Out-File|Export-(?:Csv|Excel)|Copy-Item|Move-Item|Remove-Item|New-Item)\b",
        r"(?i)\bpython\b[\s\S]*(?:open\s*\(|write_text|dump\s*\()",
        r"(?m)>\s*[\"'`]?(?:\.?[\\/]+)?Client[\\/]+Bin[\\/]+Resources[\\/]+Data[\\/]+(?:json[\\/]+DT_[^\\/'\"`\s>]*\.json|xlsx[\\/]+DT_[^\\/'\"`\s>]*\.xlsx)\b",
    ]
    return any(re.search(pattern, tool_text) for pattern in write_patterns)


def is_dt_table_protected_write(tool_text):
    return patch_targets_protected_dt_table(tool_text) or shell_writes_protected_dt_table(tool_text)


def is_recursive_force_remove(tool_text):
    return re.search(
        r"(?i)\bRemove-Item\b[^\r\n;|&]*\s-Recurse\b[^\r\n;|&]*\s-Force\b",
        tool_text,
    ) is not None


def is_managed_workspace_remove(tool_text):
    match = re.fullmatch(
        r"\s*Remove-Item\s+-LiteralPath\s+(['\"])(?P<path>[^'\"]+)\1\s+-Recurse\s+-Force\s*",
        tool_text,
        flags=re.IGNORECASE,
    )
    if match is None:
        return False

    path_text = match.group("path")
    if any(character in path_text for character in ("$", "*", "?", "`", ";", "|", "&")):
        return False

    return is_registered_workspace_root(path_text)


def main():
    input_json = read_input_json()
    if input_json is None:
        return 0

    tool_name = input_json.get("tool_name") or ""
    tool_input = input_json.get("tool_input") or {}
    tool_text = get_tool_text(tool_input)

    if not tool_text.strip():
        return 0

    state = load_engine_shader_guard_state()

    if tool_is_read_only(tool_name, tool_text):
        read_paths = extract_engine_shader_paths(tool_text)
        if read_paths:
            add_read_paths(state, read_paths)

    if is_dt_table_protected_write(tool_text):
        write_json(
            {
                "hookSpecificOutput": {
                    "hookEventName": "PreToolUse",
                    "permissionDecision": "deny",
                    "permissionDecisionReason": "DT data-table files are protected only under Client/Bin/Resources/Data/json and Client/Bin/Resources/Data/xlsx. Do not write those DT_* files directly. Docs/code mentions of DT_* are allowed. If a DT row is needed, ask the user with a Markdown row-value table containing target file, sheet/table, key/id, columns, values, and memo. Write memo in Korean.",
                }
            }
        )
        return 0

    if is_recursive_force_remove(tool_text) and is_managed_workspace_remove(tool_text):
        return 0

    deny_rules = [
        (
            r"(?i)\bgit\s+reset\s+--hard\b",
            "Destructive git reset is blocked by the Expedition_158 project hook.",
        ),
        (
            r"(?i)\bgit\s+checkout\s+--\b",
            "Destructive git checkout path restore is blocked by the Expedition_158 project hook.",
        ),
        (
            r"(?i)\bgit\s+clean\s+-(?:[^\s]*f[^\s]*d|[^\s]*d[^\s]*f)\b",
            "Destructive git clean is blocked by the Expedition_158 project hook.",
        ),
        (
            r"(?i)\brm\s+-[^\r\n;|&]*r[^\r\n;|&]*f\b",
            "Recursive force remove is blocked by the Expedition_158 project hook.",
        ),
        (
            r"(?i)\bRemove-Item\b[^\r\n;|&]*\s-Recurse\b[^\r\n;|&]*\s-Force\b",
            "Recursive force Remove-Item is blocked by the Expedition_158 project hook.",
        ),
    ]

    for pattern, reason in deny_rules:
        if re.search(pattern, tool_text):
            write_json(
                {
                    "hookSpecificOutput": {
                        "hookEventName": "PreToolUse",
                        "permissionDecision": "deny",
                        "permissionDecisionReason": reason,
                    }
                }
            )
            return 0

    if tool_writes_engine_shader(tool_name, tool_text):
        write_targets = get_engine_shader_write_targets(tool_name, tool_text)
        missing = get_missing_engine_shader_reads(state, write_targets)
        if missing:
            target, missing_reads = next(iter(missing.items()))
            reason = (
                "Engine_Shader_* writes are blocked until the target and its Engine/Client counterpart are read in the same turn. "
                f"Missing reads for {target}: " + ", ".join(missing_reads)
            )
            write_json(
                {
                    "hookSpecificOutput": {
                        "hookEventName": "PreToolUse",
                        "permissionDecision": "deny",
                        "permissionDecisionReason": reason,
                    }
                }
            )
            return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
