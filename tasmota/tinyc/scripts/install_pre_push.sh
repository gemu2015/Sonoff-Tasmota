#!/bin/bash
# Installs the pre-push hook of this repository: .git/hooks/pre-push calls scripts/pre_push_secrets.py from the working tree,
# so an update of the script takes effect without installing again.   Usage:  tasmota/tinyc/scripts/install_pre_push.sh [--force]
# An existing hook is not replaced without --force (then it is kept as pre-push.bak).
set -e
TOP="$(git rev-parse --show-toplevel)"
HOOK="$(git rev-parse --absolute-git-dir)/hooks/pre-push"
if [ -e "$HOOK" ] && [ "$1" != "--force" ]; then
  echo "There is already a pre-push hook: $HOOK  (run with --force to replace it, a copy is kept as pre-push.bak)"; exit 1
fi
[ -e "$HOOK" ] && cp "$HOOK" "$HOOK.bak"
mkdir -p "$(dirname "$HOOK")"
cat > "$HOOK" <<HOOKEOF
#!/bin/sh
# installed by tasmota/tinyc/scripts/install_pre_push.sh
exec python3 "$TOP/tasmota/tinyc/scripts/pre_push_secrets.py" "\$@"
HOOKEOF
chmod +x "$HOOK"
echo "pre-push hook installed: $HOOK"
echo "Your list of secrets (optional): $(git rev-parse --absolute-git-dir)/secrets.txt   one entry per line:  label|value"
