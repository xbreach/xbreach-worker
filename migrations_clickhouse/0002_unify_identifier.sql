ALTER TABLE leak_records DROP COLUMN IF EXISTS email;
ALTER TABLE leak_records DROP COLUMN IF EXISTS email_domain;
ALTER TABLE leak_records RENAME COLUMN IF EXISTS username TO identifier;
