#!/bin/bash

# Builds a Slack chat.postMessage payload for a kernelCI failure and writes
# it to stdout (or to a file via --output). The caller is responsible for
# posting it (e.g. via slackapi/slack-github-action with a bot token).
#
# Usage:
#   notify-slack-kernelci.sh \
#     --channel-id CHANNEL_ID \
#     --base-branch BRANCH --head-ref BRANCH --head-sha SHA \
#     --pr-number N --is-pr true|false \
#     --repo OWNER/REPO --run-id ID \
#     --failed-stages "stage1, stage2, ..." \
#     [--mention-id SLACK_USER_ID] \
#     [--output PATH]

set -euo pipefail

CHANNEL_ID=""
BASE_BRANCH=""
HEAD_REF=""
HEAD_SHA=""
PR_NUMBER="0"
IS_PR="false"
REPO=""
RUN_ID=""
FAILED_STAGES=""
MENTION_ID=""
OUTPUT=""

# Guard so a missing flag value gives a clear error under `set -u` instead of
# the opaque "unbound variable" message when we try to read $2.
require_value() {
  if [ $# -lt 2 ]; then
    echo "Error: $1 requires a value" >&2
    exit 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --channel-id)              require_value "$@"; CHANNEL_ID="$2"; shift 2 ;;
    --base-branch)             require_value "$@"; BASE_BRANCH="$2"; shift 2 ;;
    --head-ref)                require_value "$@"; HEAD_REF="$2"; shift 2 ;;
    --head-sha)                require_value "$@"; HEAD_SHA="$2"; shift 2 ;;
    --pr-number)               require_value "$@"; PR_NUMBER="$2"; shift 2 ;;
    --is-pr)                   require_value "$@"; IS_PR="$2"; shift 2 ;;
    --repo)                    require_value "$@"; REPO="$2"; shift 2 ;;
    --run-id)                  require_value "$@"; RUN_ID="$2"; shift 2 ;;
    --failed-stages)           require_value "$@"; FAILED_STAGES="$2"; shift 2 ;;
    --mention-id)              require_value "$@"; MENTION_ID="$2"; shift 2 ;;
    --output)                  require_value "$@"; OUTPUT="$2"; shift 2 ;;
    *) echo "Error: Unknown option: $1" >&2; exit 1 ;;
  esac
done

for var in CHANNEL_ID BASE_BRANCH HEAD_REF HEAD_SHA REPO RUN_ID FAILED_STAGES; do
  if [ -z "${!var}" ]; then
    # Render the var name back into the actual CLI flag (lowercase + _→-)
    flag="--${var,,}"
    flag="${flag//_/-}"
    echo "Error: $flag is required" >&2
    exit 1
  fi
done

SHORT_SHA="${HEAD_SHA:0:12}"
RUN_URL="https://github.com/$REPO/actions/runs/$RUN_ID"
COMMIT_URL="https://github.com/$REPO/commit/$HEAD_SHA"

MENTION=""
if [ -n "$MENTION_ID" ]; then
  MENTION="<@${MENTION_ID}> "
fi

PR_LINE=""
if [ "$IS_PR" = "true" ] && [ "$PR_NUMBER" != "0" ]; then
  PR_LINE=$'\n'"*PR:* <https://github.com/$REPO/pull/$PR_NUMBER|#$PR_NUMBER>"
fi

MESSAGE=$(
  printf '%s:x: *kernelCI failed on `%s`*\n' "$MENTION" "$BASE_BRANCH"
  printf '*Branch:* `%s`\n' "$HEAD_REF"
  printf '*Commit:* <%s|%s>' "$COMMIT_URL" "$SHORT_SHA"
  printf '%s\n' "$PR_LINE"
  printf '*Failed stages:* %s\n' "$FAILED_STAGES"
  printf '*Run:* <%s|view logs>' "$RUN_URL"
)

# Build chat.postMessage payload: channel + text. mrkdwn is on by default.
PAYLOAD=$(jq -n \
  --arg channel "$CHANNEL_ID" \
  --arg text "$MESSAGE" \
  '{channel: $channel, text: $text}')

if [ -n "$OUTPUT" ]; then
  printf '%s\n' "$PAYLOAD" > "$OUTPUT"
  echo "Payload written to $OUTPUT" >&2
else
  printf '%s\n' "$PAYLOAD"
fi
