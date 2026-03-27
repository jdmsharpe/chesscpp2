#!/bin/bash
# One-time setup for new clones: configures git hooks and installs Python deps.
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== Chess++ developer setup ==="

# Point git hooks to the tracked .githooks/ directory
git -C "$REPO_ROOT" config core.hooksPath .githooks
echo "✓ git hooks → .githooks/"

# Install Python dependencies
if command -v pip &>/dev/null; then
  pip install -r "$REPO_ROOT/requirements.txt"
  echo "✓ Python dependencies installed"
else
  echo "⚠ pip not found, skipping Python dependencies"
fi

echo "Done. Run 'mkdir -p build && cd build && cmake .. && cmake --build .' to build."
