# -*- coding: utf-8 -*-
import argparse
import hmac
import json
import os
import re
import secrets
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path


kSchemaVersion = 1
kMarkerFileName = ".codex-workspace.json"
kRegistryFileName = "registry.json"
kWorkspaceIdPattern = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$")


def write_json(payload):
    sys.stdout.write(json.dumps(payload, ensure_ascii=False) + "\n")


def repository_root():
    return Path(__file__).resolve().parents[2]


def workspace_base_dir():
    return repository_root() / ".codex" / "workspaces"


def registry_path():
    return workspace_base_dir() / kRegistryFileName


def canonical_path(path):
    return str(Path(path).resolve(strict=False))


def utc_now():
    return datetime.now(timezone.utc).astimezone().isoformat()


def load_registry():
    path = registry_path()
    if not path.exists():
        return {"schemaVersion": kSchemaVersion, "workspaces": {}}

    try:
        with path.open("r", encoding="utf-8") as registry_file:
            registry = json.load(registry_file)
    except (OSError, json.JSONDecodeError):
        return {"schemaVersion": kSchemaVersion, "workspaces": {}}

    if not isinstance(registry, dict) or not isinstance(registry.get("workspaces"), dict):
        return {"schemaVersion": kSchemaVersion, "workspaces": {}}

    return registry


def save_registry(registry):
    path = registry_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = path.with_suffix(".tmp")
    with temporary_path.open("w", encoding="utf-8") as registry_file:
        json.dump(registry, registry_file, ensure_ascii=False, indent=2)
        registry_file.write("\n")
    os.replace(temporary_path, path)


def save_marker(root, marker):
    marker_path = root / kMarkerFileName
    with marker_path.open("w", encoding="utf-8") as marker_file:
        json.dump(marker, marker_file, ensure_ascii=False, indent=2)
        marker_file.write("\n")


def prune_missing_workspaces(registry):
    workspaces = registry["workspaces"]
    missing_ids = [
        workspace_id
        for workspace_id, record in workspaces.items()
        if (
            not isinstance(record, dict)
            or not isinstance(record.get("root"), str)
            or not record["root"]
            or not Path(record["root"]).exists()
        )
    ]
    for workspace_id in missing_ids:
        del workspaces[workspace_id]
    return bool(missing_ids)


def resolve_workspace_root(path_text, require_exists):
    if not path_text:
        return None

    candidate = Path(path_text)
    if not candidate.is_absolute():
        return None

    try:
        resolved = candidate.resolve(strict=require_exists)
        base = workspace_base_dir().resolve(strict=True)
    except OSError:
        return None

    if resolved.parent != base:
        return None

    return resolved


def get_registered_workspace_record(path_text, require_exists=True):
    root = resolve_workspace_root(path_text, require_exists)
    if root is None:
        return None

    registry = load_registry()
    marker_path = root / kMarkerFileName
    try:
        with marker_path.open("r", encoding="utf-8") as marker_file:
            marker = json.load(marker_file)
    except (OSError, json.JSONDecodeError):
        return None

    if not isinstance(marker, dict):
        return None

    workspace_id = marker.get("workspaceId")
    token = marker.get("token")
    if not isinstance(workspace_id, str) or not isinstance(token, str):
        return None

    record = registry["workspaces"].get(workspace_id)
    if not isinstance(record, dict):
        return None

    expected_root = canonical_path(root)
    expected_repository = canonical_path(repository_root())
    if (
        marker.get("schemaVersion") != kSchemaVersion
        or record.get("schemaVersion") != kSchemaVersion
        or marker.get("root") != expected_root
        or record.get("root") != expected_root
        or marker.get("repositoryRoot") != expected_repository
        or record.get("repositoryRoot") != expected_repository
        or not isinstance(record.get("token"), str)
        or not hmac.compare_digest(token, record["token"])
    ):
        return None

    return {"root": root, "workspaceId": workspace_id, "record": record}


def is_registered_workspace_root(path_text):
    return get_registered_workspace_record(path_text, require_exists=True) is not None


def create_workspace(args):
    base = workspace_base_dir()
    base.mkdir(parents=True, exist_ok=True)
    registry = load_registry()
    if prune_missing_workspaces(registry):
        save_registry(registry)

    workspace_id = args.workspace_id or f"workspace-{uuid.uuid4().hex}"
    if not kWorkspaceIdPattern.fullmatch(workspace_id):
        raise ValueError("workspace id must contain only letters, digits, underscores, and hyphens")

    root = base / workspace_id
    if root.exists() or workspace_id in registry["workspaces"]:
        raise ValueError(f"workspace already exists: {workspace_id}")

    root.mkdir()
    output_dir = root / "out"
    int_dirs = {
        "Engine": root / "int" / "Engine",
        "Client": root / "int" / "Client",
        "MainEditor": root / "int" / "MainEditor",
    }
    output_dir.mkdir()
    for int_dir in int_dirs.values():
        int_dir.mkdir(parents=True)

    token = secrets.token_urlsafe(32)
    root_text = canonical_path(root)
    repository_text = canonical_path(repository_root())
    record = {
        "schemaVersion": kSchemaVersion,
        "workspaceId": workspace_id,
        "token": token,
        "root": root_text,
        "repositoryRoot": repository_text,
        "createdAt": utc_now(),
        "purpose": args.purpose,
    }
    save_marker(root, record)
    registry["workspaces"][workspace_id] = record
    save_registry(registry)

    write_json(
        {
            "workspaceId": workspace_id,
            "root": root_text,
            "outDir": canonical_path(output_dir) + os.sep,
            "intDirs": {project: canonical_path(path) + os.sep for project, path in int_dirs.items()},
        }
    )


def inspect_workspace(args):
    workspace = get_registered_workspace_record(args.root, require_exists=True)
    if workspace is None:
        raise ValueError("workspace marker and registry do not match")

    write_json(
        {
            "workspaceId": workspace["workspaceId"],
            "root": canonical_path(workspace["root"]),
            "valid": True,
        }
    )


def finalize_workspace(args):
    root = resolve_workspace_root(args.root, require_exists=False)
    if root is None:
        raise ValueError("workspace root must be a direct child of .codex/workspaces")
    if root.exists():
        raise ValueError("workspace still exists; remove it before finalizing")

    registry = load_registry()
    root_text = canonical_path(root)
    matching_ids = [
        workspace_id
        for workspace_id, record in registry["workspaces"].items()
        if isinstance(record, dict) and record.get("root") == root_text
    ]
    if len(matching_ids) != 1:
        raise ValueError("workspace registry entry was not found")

    del registry["workspaces"][matching_ids[0]]
    save_registry(registry)
    write_json({"root": root_text, "finalized": True})


def parse_arguments():
    parser = argparse.ArgumentParser(description="Manage Codex-owned temporary workspaces.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    create_parser = subparsers.add_parser("create")
    create_parser.add_argument("--purpose", default="temporary")
    create_parser.add_argument("--workspace-id")
    create_parser.set_defaults(handler=create_workspace)

    inspect_parser = subparsers.add_parser("inspect")
    inspect_parser.add_argument("--root", required=True)
    inspect_parser.set_defaults(handler=inspect_workspace)

    finalize_parser = subparsers.add_parser("finalize")
    finalize_parser.add_argument("--root", required=True)
    finalize_parser.set_defaults(handler=finalize_workspace)

    return parser.parse_args()


def main():
    arguments = parse_arguments()
    try:
        arguments.handler(arguments)
    except ValueError as error:
        print(f"managed workspace: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
