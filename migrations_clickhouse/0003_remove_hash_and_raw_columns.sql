DROP TABLE IF EXISTS leak_records_without_hash_columns;
DROP TABLE IF EXISTS leak_records_with_hash_columns_backup;

CREATE TABLE leak_records_without_hash_columns
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

INSERT INTO leak_records_without_hash_columns
    (id, job_id, source_id, breach_id, line_number, record_kind, identifier, password, url, url_domain, ingested_at)
SELECT
    id,
    job_id,
    source_id,
    breach_id,
    line_number,
    record_kind,
    identifier,
    password,
    url,
    url_domain,
    ingested_at
FROM leak_records;

RENAME TABLE leak_records TO leak_records_with_hash_columns_backup,
             leak_records_without_hash_columns TO leak_records;

DROP TABLE leak_records_with_hash_columns_backup;
