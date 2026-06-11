# xbreach-worker

C++ ingestion worker for XBreach.

The worker consumes pending ingestion jobs created by `xbreach-ingest`, reads the uploaded
leak files from shared storage, parses credential records, normalizes identifiers and URLs,
and writes searchable rows to ClickHouse. Job status, progress and per-line parse errors are
reported back to PostgreSQL.

## Role in the pipeline

```
ingest  ──upload──▶ filesystem (raw files)  +  Postgres (ingestion_jobs: pending)
worker  ──claim (FOR UPDATE SKIP LOCKED)──▶ Postgres
        ──read──▶ filesystem            ──parse/normalize──▶
        ──batch insert──▶ ClickHouse (leak_records)
        ──progress / status / errors──▶ Postgres
```

The worker is fully standalone: it shares no code with the Python ingest and talks only to
Postgres, the local filesystem and ClickHouse. It resolves a job's file as
`<XBREACH_DATA_PATH>/<ingestion_jobs.local_path>` (the layout ingest writes:
`raw/year=YYYY/month=MM/day=DD/{job_id}/original.<ext>`).

## Requirements

- CMake ≥ 3.21 and Ninja
- A C++20 compiler (MSVC on Windows, GCC/Clang on Linux)
- [vcpkg](https://github.com/microsoft/vcpkg) (manifest mode); set `VCPKG_ROOT`
- Docker (for the containerized run)

Dependencies (resolved by vcpkg via `vcpkg.json`): libpqxx, clickhouse-cpp, OpenSSL, zstd,
zlib, spdlog, GoogleTest.

## Structure

```text
xbreach-worker/
├── src/
│   ├── db/
│   ├── domain/
│   ├── io/
│   ├── parse/
│   ├── worker/
│   └── main.cpp
├── tests/
├── migrations_clickhouse/
├── Dockerfile
├── docker-compose.yml
├── docker-compose.full.yml
├── CMakeLists.txt
└── README.md
```

## Build

```sh
# Windows (MSVC) — run from an "x64 Native Tools Command Prompt for VS"
cmake --preset windows
cmake --build --preset windows
ctest --preset windows

# Linux (Ninja) — also used by the Docker build
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

The Windows preset uses the `x64-windows-static` triplet (static CRT) to avoid a libpqxx/MSVC
DLL symbol clash; the Linux preset uses the default static `x64-linux` triplet.

## Configuration

All settings come from `XBREACH_*` environment variables (see [.env.example](.env.example)):

| Variable | Default | Purpose |
|---|---|---|
| `XBREACH_POSTGRES_HOST` / `_PORT` / `_DB` / `_USER` / `_PASSWORD` | `postgres` / `5432` / `xbreach` / `xbreach` / `xbreach` | Postgres (job lifecycle) |
| `XBREACH_DATA_PATH` | `/data/xbreach` | Root of the raw files written by ingest |
| `XBREACH_CLICKHOUSE_HOST` / `_PORT` / `_DB` / `_USER` / `_PASSWORD` | `clickhouse` / `9000` / `xbreach` / `xbreach` / `xbreach` | ClickHouse (native protocol) |
| `XBREACH_APP_ID` / `XBREACH_NODE_ID` | `2` / `1` | Snowflake id components (app_id=2 distinguishes the worker from ingest=1) |
| `XBREACH_CLICKHOUSE_BATCH_SIZE` | `50000` | Rows per ClickHouse insert |
| `XBREACH_PROGRESS_FLUSH_EVERY` | `10000` | Lines between Postgres progress updates |
| `XBREACH_MAX_JOB_ERRORS` | `1000` | Cap on per-line error rows recorded per job |
| `XBREACH_BANNER_SCAN_LINES` | `50` | Leading lines where unrecognized text is treated as banner noise |
| `XBREACH_STORE_PLAINTEXT_PASSWORD` | `true` | Store the plaintext password column |
| `XBREACH_WORKER_POLL_INTERVAL_SECONDS` | `2.0` | Idle poll interval |
| `XBREACH_CLICKHOUSE_MIGRATIONS_PATH` | `migrations_clickhouse` | Directory of ClickHouse migration `.sql` files |

## ClickHouse schema

`migrations_clickhouse/0001_create_leak_records.sql` creates `leak_records` (MergeTree,
monthly partitions, ordered by `(source_id, identifier, id)`). The worker
applies pending migrations on startup, tracking them in a `schema_migrations` table.

## Run

### Native

```sh
export VCPKG_ROOT=/path/to/vcpkg
# set XBREACH_* (at least Postgres + ClickHouse hosts), then:
./build/linux/xbreach-worker
```

### Docker Compose

Postgres and the raw files are provided by the **xbreach-ingest** stack, which also creates
the shared `xbreach-internal` network. Bring it up first, then start the worker stack:

```sh
# 1) in xbreach-ingest
docker compose up -d

# 2) in xbreach-worker (this repo)
docker compose up --build
```

This starts ClickHouse (native protocol on 9000) and the worker on the same network and
`/data/xbreach` volume.

### Full local stack

For end-to-end development, run ingest, Postgres, Redis, ClickHouse and the worker from this
repository:

```sh
docker compose -f docker-compose.full.yml up --build
```

The ingest API is exposed at `http://localhost:8000`.

## Healthcheck

The worker does not expose an HTTP endpoint. Check service state through Docker Compose and
ClickHouse:

```sh
docker compose ps
docker exec -it xbreach-worker-clickhouse-1 clickhouse-client \
  --user xbreach \
  --password xbreach \
  --database xbreach \
  --query "SELECT count() FROM leak_records"
```

## End-to-end verification

1. Bring up the ingest stack and this stack (above).
2. Upload a leak file through the ingest API (creates a `pending` job and a raw file).
3. Watch the job in Postgres move `pending → running → completed`, with
   `total/parsed/rejected/inserted` advancing.
4. Confirm rows in ClickHouse: `SELECT count() FROM leak_records WHERE job_id = <id>`.
5. Try a `.gz`/`.zst` file and a file with a banner at the top (the banner is skipped, not
   counted as rejected).

## Testing

```sh
ctest --preset windows   # or: linux
```

Unit tests cover the parser, normalizer, the streaming file reader (gz/zst/csv/txt), the job
processor (over a fixture file, with fakes) and the worker loop. The Postgres and ClickHouse
layers are exercised by the end-to-end run above.

## Tips

- Start `xbreach-ingest` first when using the split compose setup; it provides Postgres,
  Redis, raw file storage and the shared Docker network.
- If the ingest network was created with a project prefix, set
  `XBREACH_DOCKER_NETWORK=xbreach-ingest_xbreach-internal` before running this stack.
- Keep `XBREACH_APP_ID=2` for the worker so generated Snowflake IDs remain distinguishable
  from ingest IDs.
- Use `XBREACH_STORE_PLAINTEXT_PASSWORD=false` when the deployment should keep only
  identifiers, URL metadata and job context.
- ClickHouse migrations run on worker startup and are tracked in `schema_migrations`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the issue → branch → PR → merge workflow, the
Conventional Commit style, and the zero co-author rule.
