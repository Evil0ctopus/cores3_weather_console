Import("env")

import subprocess
from pathlib import Path


def git_version(project_dir: Path) -> str:
    try:
        short_sha = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=project_dir,
            text=True,
        ).strip()
        branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            cwd=project_dir,
            text=True,
        ).strip()
        dirty = subprocess.run(
            ["git", "diff", "--quiet"],
            cwd=project_dir,
            check=False,
        ).returncode != 0
        suffix = "-dirty" if dirty else ""
        return f"{branch}-{short_sha}{suffix}"
    except Exception:
        return "dev"


version = git_version(Path(env["PROJECT_DIR"]))
header_path = Path(env["PROJECT_DIR"]) / "include" / "generated_git_version.h"
header_path.write_text(
    '#pragma once\n\n#define APP_GIT_VERSION "{}"\n'.format(version),
    encoding="utf-8",
)