CREATE TABLE IF NOT EXISTS leak_records
(
    id UInt64,
    job_id UInt64,
    source_id UInt64,
    breach_id UInt64 DEFAULT 0,
    line_number UInt64,
    record_kind String,
    email String,
    email_domain String,
    username String,
    password String,
    password_sha256 String,
    email_hmac String,
    url String,
    url_domain String,
    raw_line String,
    ingested_at DateTime64(3, 'UTC') DEFAULT now64(3)
)
ENGINE = MergeTree
PARTITION BY toYYYYMM(ingested_at)
ORDER BY (source_id, email_hmac, password_sha256, id);
