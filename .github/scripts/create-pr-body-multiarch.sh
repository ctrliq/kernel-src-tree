#!/bin/bash

# Script to create PR body using named arguments
# Usage: create-pr-body.sh --arch ARCH --build-time TIME --total-time TIME
#          [--kselftest-passed N --kselftest-failed N --kselftest-status TEXT]
#          [--ltp-passed N --ltp-failed N --ltp-status TEXT]
#          [--arch ...] --run-id ID --repo REPO --compared-against BRANCH
#          [--ltp-details TEXT] [--commit-file FILE]

set -euo pipefail

declare -a ARCHS=()
declare -A ARCH_DATA

RUN_ID=""
REPO=""
COMMIT_MESSAGE_FILE=""
COMPARED_AGAINST=""
LTP_DETAILS=""

CURRENT_ARCH=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      CURRENT_ARCH="$2"
      if [[ ! " ${ARCHS[@]:-} " =~ " ${CURRENT_ARCH} " ]]; then
        ARCHS+=("$CURRENT_ARCH")
      fi
      shift 2
      ;;
    --build-time)
      ARCH_DATA["${CURRENT_ARCH}_build_time"]="$2"; shift 2 ;;
    --total-time)
      ARCH_DATA["${CURRENT_ARCH}_total_time"]="$2"; shift 2 ;;
    --kselftest-passed)
      ARCH_DATA["${CURRENT_ARCH}_kselftest_passed"]="$2"; shift 2 ;;
    --kselftest-failed)
      ARCH_DATA["${CURRENT_ARCH}_kselftest_failed"]="$2"; shift 2 ;;
    --kselftest-status)
      ARCH_DATA["${CURRENT_ARCH}_kselftest_status"]="$2"; shift 2 ;;
    --ltp-passed)
      ARCH_DATA["${CURRENT_ARCH}_ltp_passed"]="$2"; shift 2 ;;
    --ltp-failed)
      ARCH_DATA["${CURRENT_ARCH}_ltp_failed"]="$2"; shift 2 ;;
    --ltp-status)
      ARCH_DATA["${CURRENT_ARCH}_ltp_status"]="$2"; shift 2 ;;
    --run-id)
      RUN_ID="$2"; shift 2 ;;
    --repo)
      REPO="$2"; shift 2 ;;
    --compared-against)
      COMPARED_AGAINST="$2"; shift 2 ;;
    --ltp-details)
      LTP_DETAILS="$2"; shift 2 ;;
    --commit-file)
      COMMIT_MESSAGE_FILE="$2"; shift 2 ;;
    *)
      echo "Error: Unknown option: $1" >&2; exit 1 ;;
  esac
done

[[ ${#ARCHS[@]} -eq 0 ]] && { echo "Error: At least one --arch required" >&2; exit 1; }
[[ -z "$RUN_ID" ]] && { echo "Error: --run-id required" >&2; exit 1; }
[[ -z "$REPO" ]] && { echo "Error: --repo required" >&2; exit 1; }
[[ -z "$COMMIT_MESSAGE_FILE" ]] && COMMIT_MESSAGE_FILE="/tmp/commit_message.txt"

if [ ! -f "$COMMIT_MESSAGE_FILE" ]; then
  echo "Error: Commit message file not found: $COMMIT_MESSAGE_FILE" >&2
  exit 1
fi

for arch in "${ARCHS[@]}"; do
  [[ -z "${ARCH_DATA[${arch}_build_time]:-}" ]] && { echo "Error: Missing --build-time for $arch" >&2; exit 1; }
  [[ -z "${ARCH_DATA[${arch}_total_time]:-}" ]] && { echo "Error: Missing --total-time for $arch" >&2; exit 1; }
done

convert_time() {
  local seconds="${1%s}"
  local minutes=$((seconds / 60))
  local remaining_seconds=$((seconds % 60))
  echo "${minutes}m ${remaining_seconds}s"
}

MULTIARCH=false
[ ${#ARCHS[@]} -gt 1 ] && MULTIARCH=true

for arch in "${ARCHS[@]}"; do
  ARCH_DATA["${arch}_build_time_readable"]=$(convert_time "${ARCH_DATA[${arch}_build_time]}")
  ARCH_DATA["${arch}_total_time_readable"]=$(convert_time "${ARCH_DATA[${arch}_total_time]}")
done

# Check if any arch has kselftest or LTP data
HAS_KSELFTEST=false
HAS_LTP=false
for arch in "${ARCHS[@]}"; do
  [ -n "${ARCH_DATA[${arch}_kselftest_passed]:-}" ] && HAS_KSELFTEST=true
  [ -n "${ARCH_DATA[${arch}_ltp_passed]:-}" ] && HAS_LTP=true
done

cat << EOF
## Summary
This PR has been automatically created after successful completion of all CI stages.

## Commit Message(s)

EOF

cat "$COMMIT_MESSAGE_FILE"
echo ""

cat << EOF

## Test Results

### ✅ Build Stage
EOF

if [ "$MULTIARCH" = true ]; then
  echo ""
  echo "| Architecture | Build Time | Total Time |"
  echo "|--------------|------------|------------|"
  for arch in "${ARCHS[@]}"; do
    echo "| ${arch} | ${ARCH_DATA[${arch}_build_time_readable]} | ${ARCH_DATA[${arch}_total_time_readable]} |"
  done
else
  ARCH1="${ARCHS[0]}"
  echo "- Status: Passed (${ARCH1})"
  echo "- Build Time: ${ARCH_DATA[${ARCH1}_build_time_readable]}"
  echo "- Total Time: ${ARCH_DATA[${ARCH1}_total_time_readable]}"
fi

echo ""
echo "- [View build logs](https://github.com/${REPO}/actions/runs/${RUN_ID})"

cat << EOF

### ✅ Boot Verification
EOF

if [ "$MULTIARCH" = true ]; then
  echo "- Status: Passed (all architectures)"
else
  echo "- Status: Passed (${ARCHS[0]})"
fi
echo "- [View boot logs](https://github.com/${REPO}/actions/runs/${RUN_ID})"

if [ "$HAS_KSELFTEST" = true ]; then
  cat << EOF

### ✅ Kernel Selftests

| Architecture | Passed | Failed | Compared Against | Status |
|--------------|--------|--------|-----------------|--------|
EOF
  for arch in "${ARCHS[@]}"; do
    passed="${ARCH_DATA[${arch}_kselftest_passed]:-N/A}"
    failed="${ARCH_DATA[${arch}_kselftest_failed]:-N/A}"
    status="${ARCH_DATA[${arch}_kselftest_status]:-⚠️ No baseline available}"
    echo "| ${arch} | ${passed} | ${failed} | ${COMPARED_AGAINST:-N/A} | ${status} |"
  done
  echo ""
  echo "- [View kselftest logs](https://github.com/${REPO}/actions/runs/${RUN_ID})"
fi

if [ "$HAS_LTP" = true ]; then
  cat << EOF

### ✅ LTP Results

| Architecture | Passed | Failed | Compared Against | Status |
|--------------|--------|--------|-----------------|--------|
EOF
  for arch in "${ARCHS[@]}"; do
    ltp_passed="${ARCH_DATA[${arch}_ltp_passed]:-N/A}"
    ltp_failed="${ARCH_DATA[${arch}_ltp_failed]:-N/A}"
    ltp_status="${ARCH_DATA[${arch}_ltp_status]:-⚠️ No baseline available}"
    echo "| ${arch} | ${ltp_passed} | ${ltp_failed} | ${COMPARED_AGAINST:-N/A} | ${ltp_status} |"
  done
  echo ""
  echo "- [View LTP logs](https://github.com/${REPO}/actions/runs/${RUN_ID})"

  if [ -n "$LTP_DETAILS" ]; then
    echo ""
    echo "$LTP_DETAILS"
  fi
fi

cat << EOF

---
🤖 This PR was automatically generated by GitHub Actions
Run ID: ${RUN_ID}
EOF
