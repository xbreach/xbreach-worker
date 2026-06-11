CREATE TABLE IF NOT EXISTS leak_records
(
    id UInt64,
    job_id UInt64,
    source_id UInt64,
    breach_id UInt64 DEFAULT 0,
    line_number UInt64,
    record_kind String,
    identifier String,
    password String,
    url String,
    url_domain String,
    ingested_at DateTime64(3, 'UTC') DEFAULT now64(3)
)
ENGINE = MergeTree
PARTITION BY toYYYYMM(ingested_at)
ORDER BY (source_id, identifier, id);
