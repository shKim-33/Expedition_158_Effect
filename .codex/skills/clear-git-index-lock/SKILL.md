---
name: clear-git-index-lock
description: Remove the stale Git index lock for this Expedition_158 checkout. Use when a Git command reports that `.git/index.lock` already exists and the user asks to clear the lock or resume Git work.
---

# Clear Git Index Lock

Run the bundled script from the repository root. It removes only this checkout's `.git/index.lock` and reports whether a lock existed.

```powershell
& powershell.exe -ExecutionPolicy Bypass -File .\.codex\skills\clear-git-index-lock\scripts\Remove-GitIndexLock.ps1
```

Do not remove any other Git file or reset Git state. After the script completes, rerun the Git command that was blocked.
