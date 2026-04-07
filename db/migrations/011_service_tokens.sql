-- Migration 011: Service tokens for SDK authentication
-- Stores hashed service tokens; raw token is shown once and never persisted.

CREATE TABLE IF NOT EXISTS service_tokens (
    id           TEXT PRIMARY KEY,
    service_name TEXT        NOT NULL,
    namespace    TEXT        NOT NULL DEFAULT '',
    token_hash   TEXT        NOT NULL UNIQUE,
    prefix       TEXT        NOT NULL,
    label        TEXT        NOT NULL DEFAULT '',
    created_by   TEXT        NOT NULL,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_used_at TIMESTAMPTZ,
    revoked      BOOLEAN     NOT NULL DEFAULT FALSE
);

CREATE INDEX IF NOT EXISTS service_tokens_service_name_idx ON service_tokens (service_name);
CREATE INDEX IF NOT EXISTS service_tokens_token_hash_idx   ON service_tokens (token_hash);

SELECT '011: service_tokens table created' AS status;
