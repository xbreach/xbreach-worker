# Contributing to xbreach-worker

`xbreach-worker` is the C++ ingestion worker of XBreach. It claims pending jobs from
Postgres, reads the raw leak file from local storage, parses and normalizes the records,
and batch-writes the result to ClickHouse.

## Workflow

All work is **issue-driven** and lands on `main` only through reviewed pull requests.

1. **Open an issue** from one of the templates (feature / bug / task) with clear
   **acceptance criteria**. Assign `type:` and `area:` labels and the current milestone.
2. **Create a branch** from `main` named `<type>/worker-<short-desc>-<issue-number>`,
   e.g. `feat/worker-parser-4`.
3. **Commit** following the rules below.
4. **Open a PR** filling in the template, with `Closes #<issue>` and a Conventional title.
5. **Merge** into `main` only when the work is done: it builds, `ctest` passes,
   `clang-format` is clean, and the PR checklist is complete.

`main` is protected: no direct pushes; changes go through a PR.

## Commit messages

[Conventional Commits](https://www.conventionalcommits.org/), in **English**, lowercase,
with a scope:

```
feat(worker): add line parser with delimiter detection
fix(io): handle truncated zstd frames
chore(build): pin clickhouse-cpp version
docs(readme): document clickhouse env vars
test(parse): cover ulp url:user:pass format
```

No capital letter after the colon, present tense, no trailing period.

**Hard rule: ZERO co-author.** Never add a `Co-Authored-By` trailer or any AI/assistant
attribution to a commit. The history must stay clean.

## Build & test

```sh
cmake --preset windows      # or: linux
cmake --build --preset windows
ctest --preset windows
clang-format --dry-run -Werror $(git ls-files '*.cpp' '*.hpp')
```

Dependencies are managed with [vcpkg](https://github.com/microsoft/vcpkg) in manifest mode
(`vcpkg.json`); point CMake at the vcpkg toolchain file (see `README.md`).
