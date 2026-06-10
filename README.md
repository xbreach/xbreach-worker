# xbreach-worker

C++ ingestion worker for XBreach. It claims pending jobs from Postgres, reads the raw leak
file from local storage, parses and normalizes the records, generates hashes/HMAC, and
batch-writes the result to ClickHouse.

> Full configuration and run instructions are documented incrementally as the worker is
> built (see issues in the `worker MVP` milestone). This file currently covers the build
> foundation.

## Build

Dependencies are managed with [vcpkg](https://github.com/microsoft/vcpkg) (manifest mode,
`vcpkg.json`). Set `VCPKG_ROOT` to your vcpkg checkout, then:

```sh
# Windows (MSVC) — run these from an "x64 Native Tools Command Prompt for VS"
cmake --preset windows
cmake --build --preset windows
ctest --preset windows

# Linux (Ninja) — also used by the Docker build
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the development workflow and coding conventions.
