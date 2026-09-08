#!/bin/bash
# Copy the log off this SD card and into a private git repo.
#
# The Pi already authenticates to github as georgebenett over ssh, so this
# needs no new credentials, no daemon and no cloud dependency. git also
# gives point-in-time restore for free: every backup is a commit, so a log
# corrupted by a bad shutdown can be recovered from the commit before it.
#
# Run by cat-backup.timer, hourly. Safe to run by hand.
set -e
cd "$(dirname "$0")"

REPO="${CAT_BACKUP_REPO:-$HOME/cat_backup}"
if [ ! -d "$REPO/.git" ]; then
    echo "no backup clone at $REPO - see 'Backing up the log' in README.md" >&2
    exit 1
fi

# Nothing to do before she has any history; not an error.
[ -f cat_log.csv ] || { echo "no cat_log.csv yet"; exit 0; }

cp -f cat_log.csv "$REPO/cat_log.csv"
[ -f cat_cfg.txt ] && cp -f cat_cfg.txt "$REPO/cat_cfg.txt"

cd "$REPO"
git add -A
# --quiet exits 0 when there is nothing staged, so this is the "unchanged" path.
if git diff --cached --quiet; then
    echo "unchanged"
    exit 0
fi

n=$(grep -c . cat_log.csv 2>/dev/null || echo 0)
git commit -q -m "$(date '+%Y-%m-%d %H:%M') - $n events"

# Don't let a wifi drop fail the unit: the next run picks it up, and the
# commit is already safely on disk.
if git push -q origin main 2>/dev/null; then
    echo "pushed ($n events)"
else
    echo "committed locally, push failed - will retry next run" >&2
fi
