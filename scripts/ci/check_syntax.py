# Validate the syntax of tracked JSON and YAML files without changing them.

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import yaml


def tracked_files(*patterns: str) -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z", "--", *patterns],
        check=True,
        capture_output=True,
    )
    return [Path(path) for path in result.stdout.decode().split("\0") if path]


def validate_json(path: Path) -> None:
    with path.open(encoding="utf-8") as file:
        json.load(file)


def validate_yaml(path: Path) -> None:
    with path.open(encoding="utf-8") as file:
        list(yaml.safe_load_all(file))


def main() -> int:
    checks = [
        ("JSON", tracked_files("*.json"), validate_json),
        ("YAML", tracked_files("*.yml", "*.yaml"), validate_yaml),
    ]
    failures = 0

    for file_type, paths, validator in checks:
        for path in paths:
            try:
                validator(path)
            except (OSError, UnicodeError, ValueError, yaml.YAMLError) as error:
                print(f"{file_type} syntax error: {path}: {error}", file=sys.stderr)
                failures += 1

    if failures:
        print(f"{failures} file(s) failed syntax validation.", file=sys.stderr)
        return 1

    print(
        "Validated "
        f"{len(checks[0][1])} JSON and {len(checks[1][1])} YAML file(s)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
