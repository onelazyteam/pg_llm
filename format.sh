#!/bin/bash
git diff --cached --name-only --diff-filter=ACMR | grep -E '\.(c|cpp|h|hpp)$' | xargs clang-format -i
