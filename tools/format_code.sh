#! /bin/sh

# Ensure we run from the repository root so `git ls-files` lists repo-relative paths.
repo_root=$(git rev-parse --show-toplevel 2>/dev/null || printf '%s' ".")
cd "$repo_root" || exit 1

if ! command -v clang-format >/dev/null 2>&1; then
    printf '%s\n' "clang-format not found in PATH" >&2
    exit 2
fi

for file in $(git ls-files | grep -E '\.(c|h|cpp)$')
do
    clang-format -i "$file"
done
