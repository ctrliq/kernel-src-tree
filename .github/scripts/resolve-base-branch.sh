#!/bin/bash
# resolve-base-branch.sh — single source of truth for "which base branch does
# this head branch target?". Echoes the resolved base branch to stdout; exits
# non-zero (with a ::error::) if it cannot be determined.
#
# This consolidates logic that was previously duplicated across several jobs in
# kernel-build-and-test-multiarch.yml (the `# TODO: Use a centralized place to
# get the base branches` note). New consumers (e.g. the fast-gate job) call this
# instead of inlining the whitelist + regex yet again.
#
# Resolution order (first hit wins):
#   1. Open PR from this head branch  → its baseRefName        (needs gh + GH_TOKEN)
#   2. BASE_REF env (populated on pull_request events)          → use as-is
#   3. RLC branch pattern   {user}_rlc-N/VERSION                → rlc-N/VERSION
#   4. Legacy branch pattern {user}_BASE or {user}-BASE         → BASE (must be whitelisted)
#
# Inputs (env):
#   HEAD_REF       full remote head branch name (used for the gh pr list query)
#   HEAD_REF_BASE  head ref with any skip-suffix stripped (used for regex extraction);
#                  falls back to HEAD_REF if unset
#   BASE_REF       base branch from PR metadata, if any (may be empty on push)
#   GH_TOKEN       token for the `gh pr list` lookup (optional; step 1 skipped if absent)
#
# Keep VALID_BASES in sync with the compare-kselftest / compare-ltp / notify jobs
# until those are refactored to call this script too.

set -uo pipefail

HEAD_REF="${HEAD_REF:-}"
HEAD_REF_BASE="${HEAD_REF_BASE:-$HEAD_REF}"
BASE_REF="${BASE_REF:-}"

VALID_BASES="ciqlts9_2 ciqlts9_4 ciqlts8_6 ciqlts9_6 ciq-6.12.y ciq-6.12.y-next ciq-6.18.y ciq-6.18.y-next ciqcbr7_9"

BASE_BRANCH=""
BRANCH_NAME="$HEAD_REF_BASE"

# 1. Existing open PR from this head branch wins (matches the real remote name).
if [ -n "$HEAD_REF" ] && command -v gh >/dev/null 2>&1; then
    EXISTING_PR=$(gh pr list --head "$HEAD_REF" --state open --json baseRefName --jq '.[0].baseRefName' 2>/dev/null || echo "")
    if [ -n "$EXISTING_PR" ] && [ "$EXISTING_PR" != "null" ]; then
        BASE_BRANCH="$EXISTING_PR"
    fi
fi

# 2. Base from PR metadata.
if [ -z "$BASE_BRANCH" ] && [ -n "$BASE_REF" ]; then
    BASE_BRANCH="$BASE_REF"
fi

# 3 & 4. Derive from the branch name.
if [ -z "$BASE_BRANCH" ]; then
    if [[ "$BRANCH_NAME" =~ ^\{[^}]+\}_(rlc-[0-9]+/.+)$ ]]; then
        BASE_BRANCH="${BASH_REMATCH[1]}"
    elif [[ "$BRANCH_NAME" =~ \{[^}]+\}[_-](.+) ]]; then
        EXTRACTED_BASE="${BASH_REMATCH[1]}"
        if echo "$VALID_BASES" | grep -wq "$EXTRACTED_BASE"; then
            BASE_BRANCH="$EXTRACTED_BASE"
        else
            echo "::error::Extracted base '$EXTRACTED_BASE' is not in whitelist: $VALID_BASES" >&2
            exit 1
        fi
    fi
fi

if [ -z "$BASE_BRANCH" ]; then
    echo "::error::Could not determine base branch from '$BRANCH_NAME'" >&2
    echo "::error::  Legacy pattern : {user}_BASE or {user}-BASE  (BASE in: $VALID_BASES)" >&2
    echo "::error::  RLC pattern    : {user}_rlc-N/VERSION" >&2
    exit 1
fi

echo "$BASE_BRANCH"
