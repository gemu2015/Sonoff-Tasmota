# Tasmota Merge

A macOS-style helper for merging upstream `arendst/Tasmota` changes into a
long-running fork file by file. Downloads the current upstream branch as a
ZIP, compares every source file against the fork, and presents a browser
UI for per-file decisions (Keep mine / Take theirs / Custom merge).

> Built to scratch the fork-maintenance itch documented in
> [`tasmota/UNSOLVED.md`](../../tasmota/UNSOLVED.md) entry #4. No more
> manual file diffing.

---

## Quick start (macOS)

**Recommended:** double-click **Tasmota Merge.command**. This opens a
Terminal window and runs the local Python server (stdlib-only — no
`pip install`); the browser pops up at `http://127.0.0.1:8500/`.

> Why .command and not .app: macOS sandbox/TCC silently denies
> Finder-launched .app bundles read access to files in `~/Desktop`,
> `~/Documents`, `~/Downloads`. If your fork lives in any of those
> folders (the default location for many users), the .app will get
> `Operation not permitted` errors when reading source files and
> produce a misleading diff. The .command launcher runs through
> Terminal, which inherits the user permissions you've already
> granted Terminal.app — so it Just Works. The UI surfaces a clear
> red banner if it detects TCC denials so you can switch.

The .app bundle is included for completeness — use it only if your
fork lives outside protected user folders, OR if you've granted the
.app explicit Full Disk Access in System Settings.

Pure-terminal usage is also supported:

```bash
cd tools/tasmota_merge
python3 tasmota_merge_server.py
```

The fork root is auto-detected by walking up from this directory until a
folder with both `tasmota/` and `platformio.ini` is found.

---

## Workflow

1. **Configure** — review the auto-detected fork path and the upstream
   ZIP URL (default: `arendst/Tasmota` `development` branch). Switch to a
   release-tag URL like
   `https://github.com/arendst/Tasmota/archive/refs/tags/v15.1.0.zip`
   for stable comparisons.

2. **Download &amp; Compare** — fetches the ZIP (~80 MB), extracts to
   `~/.tasmota_merge_cache/`, and indexes every file. Re-running with
   "Reuse cached ZIP" skips the download.

3. **Review files** — table lists every file that differs:
   - **Modified** — exists in both, content differs
   - **New (up)** — exists upstream only (would be added to fork)
   - **Fork-only** — exists in fork only (your additions; never overwritten)

   The toolbar's *"Hide policy-covered"* checkbox (default ON) hides
   files that already got an automatic decision from the policy lists
   — so you only see what genuinely needs your judgement. Filter the
   remaining residual by path substring, category, or decision status.
   Click any row to open the side-by-side diff.

   See [Policies](#policies) below for setting up the auto-decisions.

4. **Side-by-side diff** — fork on the left, upstream on the right. Lines
   are highlighted: red on the fork side = lines that don't appear in
   upstream, green on the upstream side = lines that don't appear in
   fork. The detailed unified diff is in a collapsible section below.

   Pick one of:
   - **Keep mine** — fork wins, no write
   - **Take theirs** — overwrite fork file with upstream
   - **Save custom merge** — paste/edit a final blob in the merge editor
     (use "Load fork as starting point" / "Load upstream as starting
     point" to seed it)
   - **Mark resolved** — record decision without writing (audit-only)

5. **Apply decisions** — back in the file list, hit *Apply decisions*.
   The tool writes Take-theirs and Custom-merge files to the fork tree
   AND backs up the previous content to
   `<fork>/.tasmota_merge_backups/<timestamp>/<rel_path>`. Files are NOT
   git-committed — review with `git diff` and commit yourself.

---

## Policies

For a fork that's diverged for years, "review every file" produces
hundreds of decisions. Most of those decisions are mechanical:
*"this whole subtree is fork-owned, never overwrite"* or *"I never
modify Berry, always take upstream"*. Policies capture exactly that.

Two persistent glob lists, edited via the **Edit policies** button on
the file list toolbar:

- **Always keep mine** — files the fork OWNS. Auto-decision `keep`,
  not overwritten.
- **Always take theirs** — files the fork doesn't modify. Auto-decision
  `take`, overwritten with upstream on Apply.

Files matching either list still appear in the data structure (so the
diff stats stay honest), but get hidden from the active review queue
unless you uncheck "Hide policy-covered". Manual decisions always win
over policy — useful for the occasional file where you need to deviate
from your own rule.

### Default seed (first run)

When `<fork>/.tasmota_merge_policies.json` doesn't exist yet, the tool
seeds it with sensible starter values:

```json
{
  "always_keep": [
    "tasmota/tinyc/**",
    "tasmota/include/xdrv_124_tinyc_vm.h",
    "tasmota/tasmota_xdrv_driver/xdrv_124_tinyc.ino",
    "tasmota/UNSOLVED.md",
    "tools/tasmota_merge/**",
    "lib/lib_display/UDisplay_legacy/**",
    "tasmota/Plugins/xdrv_14_mp3*"
  ],
  "always_take": [
    "tasmota/berry/**"
  ]
}
```

Edit at any time via **Edit policies**, or directly in the JSON file
(both are equivalent — the modal is just a friendly editor).

### Glob syntax

| Pattern                         | Matches                                                   |
| ------------------------------- | --------------------------------------------------------- |
| `tasmota/tinyc/**`              | Everything under `tasmota/tinyc/` at any depth            |
| `tasmota/tinyc/`                | Same as above (trailing `/` is shorthand for `/**`)       |
| `tasmota/UNSOLVED.md`           | Exact path                                                |
| `tasmota/Plugins/xdrv_14_mp3*`  | Wildcard within a directory                               |
| `__pycache__/*`                 | Directory name appearing at any depth                     |
| `.DS_Store`                     | Basename match at any depth (no slashes → basename rule)  |
| `**/.DS_Store`                  | Same as above (explicit form)                             |
| `# comment`                     | Lines starting with `#` are ignored                       |

Conflict tie-break: if a file matches BOTH lists, **`always_keep`
wins** (safer default — never overwrite fork files when in doubt).

### Iterating

Each time upstream is merged, you'll discover patterns you missed.
Hit **Edit policies** → add the new glob → save. The file list
re-evaluates immediately, and the new patterns persist in
`.tasmota_merge_policies.json` for future merges.

---

## State &amp; resumability

Two on-disk files in the fork root:

- `.tasmota_merge_state.json` — manual per-file decisions for the
  current merge session. Cleared via **Clear decisions** in the
  toolbar.
- `.tasmota_merge_policies.json` — durable policy lists. Survives
  across upstream merges. Reset to defaults via the modal's **Reset
  to defaults** button.

Closing the app mid-session loses nothing. Reopening re-loads both
files; existing manual decisions show in row colours, policy-covered
rows render dimmed.

---

## Defaults

| Knob              | Default                                                                                        |
| ----------------- | ---------------------------------------------------------------------------------------------- |
| Upstream URL      | `https://github.com/arendst/Tasmota/archive/refs/heads/development.zip`                        |
| Include prefixes  | `tasmota/`, `lib/`, `platformio.ini`, `platformio_override_sample.ini`, `pio-tools/`, `boards/`, `tools/` |
| Exclude globs     | `*.tcb *.bin *.elf *.o *.a *.zip *.gz *.png *.jpg *.icns *.pdf __pycache__/* .git/* .pioenvs/* .pio/* build_output/* .DS_Store Thumbs.db *.swp *.swo *~ .tasmota_merge_state.json .tasmota_merge_backups/*` |
| Cache dir         | `~/.tasmota_merge_cache/`                                                                      |
| Server port       | 8500                                                                                           |

The matcher is gitignore-ish:

- pattern with no slash → match against basename at any depth
  (`.DS_Store` matches `boards/.DS_Store`, `lib/foo/bar/.DS_Store`)
- `dirname/*` or `dirname/**` → match if any path component equals
  `dirname` (`__pycache__/*` matches `tools/x/__pycache__/foo.pyc`)
- pattern with slash → fnmatch against full relative path

Override on the *Advanced filters* expander in the setup screen.

---

## Endpoints (for scripting / debugging)

| Method | Path                            | Notes                                  |
| ------ | ------------------------------- | -------------------------------------- |
| GET    | `/`                             | The UI                                 |
| GET    | `/api/defaults`                 | Detected fork + default URL/prefixes  |
| GET    | `/api/status`                   | Phase + progress %                     |
| GET    | `/api/files`                    | Full diff list                         |
| GET    | `/api/file?path=<rel>`          | Fork text + upstream text (UTF-8)      |
| GET    | `/api/diff?path=<rel>`          | Unified diff text                      |
| POST   | `/api/start`                    | Kick off download/extract/diff         |
| POST   | `/api/decide`                   | `{path, action, merged_content?}`      |
| POST   | `/api/apply`                    | Apply all decisions to the fork tree   |
| POST   | `/api/clear_decisions`          | Wipe the on-disk decisions file        |
| GET    | `/api/policies`                 | Current policy lists                   |
| POST   | `/api/policies`                 | `{always_keep: [...], always_take: [...]}` |
| POST   | `/api/reset_policies`           | Restore default seed policies          |
| POST   | `/api/shutdown`                 | Stop the server                        |

`action` is one of `keep`, `take`, `merge`, `resolve`, or `reset`.

---

## Limits / non-goals

- No hunk-level merging (line-by-line cherry-pick). For that complexity,
  fall back to `git merge` or 3-way diff tools. This tool is meant for
  the bulk file-by-file pass: out of 2,000+ files, you can usually
  decide most with one click each.
- No AI-assisted suggestions yet. Future: a "summarize what upstream
  changed" hint per file.
- No auto-commit. Always inspect with `git diff` after Apply.
- Binary files surface in the list but the side-by-side panes show
  `(binary)` instead of content. Use Keep mine / Take theirs without
  needing the editor.
- Files >2 MB show as `too-large` in the panes (download + edit
  externally if you need them).

---

## Troubleshooting

**"Fork path does not exist"** — the auto-detect couldn't find it. Type
the absolute path to your fork root in the setup screen.

**Download stalls / times out** — the upstream ZIP is ~80 MB and GitHub
sometimes throttles. Try again, or download the ZIP manually into
`~/.tasmota_merge_cache/upstream.zip` and tick *Reuse cached ZIP*.

**App won't open ("damaged" or "unidentified developer")** — macOS
gatekeeper. `xattr -dr com.apple.quarantine "Tasmota Merge.app"` clears
it. Or run from the terminal: `python3 tasmota_merge_server.py`.

**State file conflicts** — the per-fork state file is at
`<fork>/.tasmota_merge_state.json`. Add it to `.gitignore` if you don't
want it committed.

---

## Files in this directory

```
tools/tasmota_merge/
├── tasmota_merge_server.py        # Backend — source of truth
├── tasmota_merge.html              # UI — source of truth
├── Tasmota Merge.command           # ⭐ Recommended launcher (runs via Terminal — bypasses TCC)
├── sync_resources.sh               # Copy source files into the .app bundle
├── README.md
└── Tasmota Merge.app/              # macOS .app bundle — only safe outside user folders
    └── Contents/
        ├── Info.plist
        ├── MacOS/run               # Bash launcher (logs to ~/Library/Logs/TasmotaMerge.log)
        └── Resources/
            ├── tasmota_merge_server.py     # copy of top-level
            ├── tasmota_merge.html          # copy of top-level
            └── AppIcon.icns
```

### Dev workflow

The Resources/ files are **real copies**, not symlinks — when Finder /
Launch Services launches a .app, macOS sandbox/TCC blocks Python from
reading through symlinks that point outside the bundle (`open() →
Operation not permitted`).

After editing `tasmota_merge_server.py` or `tasmota_merge.html`:

```bash
./sync_resources.sh   # copies source files → .app/Contents/Resources/
```

If you only run the server from the terminal (`python3
tasmota_merge_server.py`), no sync is needed — that path uses the
top-level files directly.

### Diagnostics

If the .app appears to do nothing when launched, the launcher logs to
`~/Library/Logs/TasmotaMerge.log` — check there for the actual reason
(common: Python not on the standard search paths, or
`Resources/tasmota_merge_server.py` missing because `sync_resources.sh`
hasn't been run after recent edits).
