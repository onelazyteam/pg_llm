#!/bin/bash

echo "📎 Setting up clang-format git hook..."
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit-clang-format
echo "✅ Git hook installed."

echo "📎 Checking if clang-format is installed..."
if ! command -v clang-format &> /dev/null; then
  echo "⚠️  clang-format not found. Please install it manually (e.g., brew install clang-format)"
else
  echo "✅ clang-format found: $(clang-format --version)"
fi

