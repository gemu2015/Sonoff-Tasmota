#!/usr/bin/env bash
# Merge upstream Tasmota (arendst/Tasmota:development) into this fork.
#
# Strategy:
#   1. Fetch upstream
#   2. Create a merge branch off `universal` (so a botched merge is throwaway)
#   3. Run `git merge` — files marked `merge=ours` in .gitattributes
#      auto-resolve to the fork's version.
#   4. List remaining conflicts grouped by category for manual review.
#
# After resolving + testing, fast-forward `universal` to the merge branch.
#
# Prerequisites (run once):
#   git remote add upstream https://github.com/arendst/Tasmota.git
#   git config merge.ours.driver true

set -euo pipefail

UPSTREAM_REMOTE="${UPSTREAM_REMOTE:-upstream}"
UPSTREAM_BRANCH="${UPSTREAM_BRANCH:-development}"
LOCAL_BRANCH="${LOCAL_BRANCH:-universal}"
MERGE_BRANCH="merge-upstream-$(date +%Y-%m-%d)"

echo "=== Pre-flight ==="
if ! git config merge.ours.driver >/dev/null 2>&1; then
    echo "Registering merge.ours.driver…"
    git config merge.ours.driver true
fi

if ! git remote get-url "$UPSTREAM_REMOTE" >/dev/null 2>&1; then
    echo "ERROR: remote '$UPSTREAM_REMOTE' not configured." >&2
    echo "  git remote add upstream https://github.com/arendst/Tasmota.git" >&2
    exit 1
fi

if [[ -n "$(git status --porcelain)" ]]; then
    echo "ERROR: working tree not clean. Commit or stash first." >&2
    exit 1
fi

echo "=== Fetching $UPSTREAM_REMOTE ==="
git fetch "$UPSTREAM_REMOTE" "$UPSTREAM_BRANCH"

AHEAD=$(git rev-list --count "$LOCAL_BRANCH..$UPSTREAM_REMOTE/$UPSTREAM_BRANCH")
echo "Upstream is $AHEAD commits ahead of $LOCAL_BRANCH."

if [[ "$AHEAD" == "0" ]]; then
    echo "Nothing to merge."
    exit 0
fi

echo "=== Creating merge branch '$MERGE_BRANCH' ==="
git checkout -b "$MERGE_BRANCH" "$LOCAL_BRANCH"

echo "=== Merging $UPSTREAM_REMOTE/$UPSTREAM_BRANCH ==="
# --no-commit so we can inspect the result before committing
if git merge --no-commit --no-ff "$UPSTREAM_REMOTE/$UPSTREAM_BRANCH"; then
    echo
    echo "=== Clean merge — no conflicts. Review and commit. ==="
    git status --short
    echo
    echo "Next steps:"
    echo "  - Build & test"
    echo "  - git commit"
    echo "  - git checkout $LOCAL_BRANCH && git merge --ff-only $MERGE_BRANCH"
    exit 0
fi

# Conflicts present
echo
echo "=== Conflicts remaining after merge=ours auto-resolve ==="
CONFLICTS=$(git diff --name-only --diff-filter=U | sort)
COUNT=$(echo "$CONFLICTS" | grep -c . || echo 0)
echo "$COUNT files in conflict."
echo

# Group conflicts by directory for easier triage
echo "--- by directory ---"
echo "$CONFLICTS" | sed 's|/[^/]*$||' | sort | uniq -c | sort -rn
echo
echo "--- full list ---"
echo "$CONFLICTS"
echo
echo "Resolve each, then:"
echo "  git add <files>"
echo "  git commit       (writes the merge commit)"
echo "  # Build & test"
echo "  git checkout $LOCAL_BRANCH"
echo "  git merge --ff-only $MERGE_BRANCH"
echo
echo "If you want to abort:"
echo "  git merge --abort"
echo "  git checkout $LOCAL_BRANCH"
echo "  git branch -D $MERGE_BRANCH"
