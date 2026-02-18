#!/bin/bash

# Script to create PR body using named arguments
# Usage: create-pr-body.sh --arch ARCH --build-time TIME --total-time TIME --passed N --failed N [--arch ...] --run-id ID --comparison SECTION --repo REPO [--commit-file FILE]

set -euo pipefail

# Arrays to track architectures and their data
declare -a ARCHS=()
declare -A ARCH_DATA

# Global parameters
RUN_ID=""
COMPARISON_SECTION=""
REPO=""
COMMIT_MESSAGE_FILE=""

CURRENT_ARCH=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      [[ $# -lt 2 ]] && { echo "Error: --arch requires a value" >&2; exit 1; }
      CURRENT_ARCH="$2"
      # Only add to ARCHS array if not already present
      if [[ ! " ${ARCHS[@]:-} " =~ " ${CURRENT_ARCH} " ]]; then
        ARCHS+=("$CURRENT_ARCH")
      fi
      shift 2
      ;;
    --build-time)
      [[ $# -lt 2 ]] && { echo "Error: --build-time requires a value" >&2; exit 1; }
      [[ -z "$CURRENT_ARCH" ]] && { echo "Error: --arch must be specified before --build-time" >&2; exit 1; }
      ARCH_DATA["${CURRENT_ARCH}_build_time"]="$2"
      shift 2
      ;;
    --total-time)
      [[ $# -lt 2 ]] && { echo "Error: --total-time requires a value" >&2; exit 1; }
      [[ -z "$CURRENT_ARCH" ]] && { echo "Error: --arch must be specified before --total-time" >&2; exit 1; }
      ARCH_DATA["${CURRENT_ARCH}_total_time"]="$2"
      shift 2
      ;;
    --passed)
      [[ $# -lt 2 ]] && { echo "Error: --passed requires a value" >&2; exit 1; }
      [[ -z "$CURRENT_ARCH" ]] && { echo "Error: --arch must be specified before --passed" >&2; exit 1; }
      ARCH_DATA["${CURRENT_ARCH}_passed"]="$2"
      shift 2
      ;;
    --failed)
      [[ $# -lt 2 ]] && { echo "Error: --failed requires a value" >&2; exit 1; }
      [[ -z "$CURRENT_ARCH" ]] && { echo "Error: --arch must be specified before --failed" >&2; exit 1; }
      ARCH_DATA["${CURRENT_ARCH}_failed"]="$2"
      shift 2
      ;;
    --run-id)
      [[ $# -lt 2 ]] && { echo "Error: --run-id requires a value" >&2; exit 1; }
      RUN_ID="$2"
      shift 2
      ;;
    --comparison)
      [[ $# -lt 2 ]] && { echo "Error: --comparison requires a value" >&2; exit 1; }
      COMPARISON_SECTION="$2"
      shift 2
      ;;
    --repo)
      [[ $# -lt 2 ]] && { echo "Error: --repo requires a value" >&2; exit 1; }
      REPO="$2"
      shift 2
      ;;
    --commit-file)
      [[ $# -lt 2 ]] && { echo "Error: --commit-file requires a value" >&2; exit 1; }
      COMMIT_MESSAGE_FILE="$2"
      shift 2
      ;;
    *)
      echo "Error: Unknown option: $1" >&2
      echo "Usage: $0 --arch ARCH --build-time TIME --total-time TIME --passed N --failed N [--arch ...] --run-id ID --comparison SECTION --repo REPO [--commit-file FILE]" >&2
      exit 1
      ;;
  esac
done

# Validate required parameters
[[ ${#ARCHS[@]} -eq 0 ]] && { echo "Error: At least one --arch required" >&2; exit 1; }
[[ -z "$RUN_ID" ]] && { echo "Error: --run-id required" >&2; exit 1; }
[[ -z "$COMPARISON_SECTION" ]] && { echo "Error: --comparison required" >&2; exit 1; }
[[ -z "$REPO" ]] && { echo "Error: --repo required" >&2; exit 1; }
[[ -z "$COMMIT_MESSAGE_FILE" ]] && COMMIT_MESSAGE_FILE="/tmp/commit_message.txt"

# Check if commit message file exists
if [ ! -f "$COMMIT_MESSAGE_FILE" ]; then
  echo "Error: Commit message file not found: $COMMIT_MESSAGE_FILE" >&2
  exit 1
fi

# Validate each arch has all required data
for arch in "${ARCHS[@]}"; do
  [[ -z "${ARCH_DATA[${arch}_build_time]:-}" ]] && { echo "Error: Missing --build-time for $arch" >&2; exit 1; }
  [[ -z "${ARCH_DATA[${arch}_total_time]:-}" ]] && { echo "Error: Missing --total-time for $arch" >&2; exit 1; }
  [[ -z "${ARCH_DATA[${arch}_passed]:-}" ]] && { echo "Error: Missing --passed for $arch" >&2; exit 1; }
  [[ -z "${ARCH_DATA[${arch}_failed]:-}" ]] && { echo "Error: Missing --failed for $arch" >&2; exit 1; }
done

# Convert seconds to minutes for better readability
convert_time() {
  local seconds="${1%s}"  # Remove 's' suffix if present
  local minutes=$((seconds / 60))
  local remaining_seconds=$((seconds % 60))
  echo "${minutes}m ${remaining_seconds}s"
}

# Determine if multi-arch
MULTIARCH=false
if [ ${#ARCHS[@]} -gt 1 ]; then
  MULTIARCH=true
fi

# Convert times for all architectures
for arch in "${ARCHS[@]}"; do
  ARCH_DATA["${arch}_build_time_readable"]=$(convert_time "${ARCH_DATA[${arch}_build_time]}")
  ARCH_DATA["${arch}_total_time_readable"]=$(convert_time "${ARCH_DATA[${arch}_total_time]}")
done

# Generate PR body
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

# Build Stage - conditional formatting
if [ "$MULTIARCH" = true ]; then
  cat << EOF

| Architecture | Build Time | Total Time |
|--------------|------------|------------|
EOF
  for arch in "${ARCHS[@]}"; do
    echo "| ${arch} | ${ARCH_DATA[${arch}_build_time_readable]} | ${ARCH_DATA[${arch}_total_time_readable]} |"
  done
else
  ARCH1="${ARCHS[0]}"
  cat << EOF
- Status: Passed (${ARCH1})
- Build Time: ${ARCH_DATA[${ARCH1}_build_time_readable]}
- Total Time: ${ARCH_DATA[${ARCH1}_total_time_readable]}
EOF
fi

cat << EOF

- [View build logs](https://github.com/${REPO}/actions/runs/${RUN_ID})

### ✅ Boot Verification
EOF

# Boot Verification - conditional formatting
if [ "$MULTIARCH" = true ]; then
  echo "- Status: Passed (all architectures)"
else
  echo "- Status: Passed (${ARCHS[0]})"
fi

cat << EOF
- [View boot logs](https://github.com/${REPO}/actions/runs/${RUN_ID})

### ✅ Kernel Selftests
EOF

# Kernel Selftests - conditional formatting
if [ "$MULTIARCH" = true ]; then
  cat << EOF

| Architecture | Passed | Failed |
|--------------|---------|--------|
EOF
  for arch in "${ARCHS[@]}"; do
    echo "| ${arch} | ${ARCH_DATA[${arch}_passed]} | ${ARCH_DATA[${arch}_failed]} |"
  done
else
  ARCH1="${ARCHS[0]}"
  cat << EOF

- **Architecture:** ${ARCH1}
- **Passed:** ${ARCH_DATA[${ARCH1}_passed]}
- **Failed:** ${ARCH_DATA[${ARCH1}_failed]}
EOF
fi

cat << EOF

- [View kselftest logs](https://github.com/${REPO}/actions/runs/${RUN_ID})

${COMPARISON_SECTION}

---
🤖 This PR was automatically generated by GitHub Actions
Run ID: ${RUN_ID}
EOF
