-- Migration 001: Core Configuration Tables
-- Description: Creates config_metadata and config_data tables
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Config Metadata Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS config_metadata (
    id SERIAL PRIMARY KEY,
    config_id VARCHAR(255) UNIQUE NOT NULL,
    service_name VARCHAR(255) NOT NULL,
    version BIGINT NOT NULL,
    format VARCHAR(50) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by VARCHAR(255),
    description TEXT,
    is_active BOOLEAN DEFAULT true,
    UNIQUE(service_name, version)
);

CREATE INDEX IF NOT EXISTS idx_service_name ON config_metadata(service_name);
CREATE INDEX IF NOT EXISTS idx_config_id ON config_metadata(config_id);
CREATE INDEX IF NOT EXISTS idx_version ON config_metadata(version);

-- ═══════════════════════════════════════════════════════════════════
-- Config Data Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS config_data (
    id SERIAL PRIMARY KEY,
    config_id VARCHAR(255) REFERENCES config_metadata(config_id) ON DELETE CASCADE,
    content TEXT NOT NULL,
    content_hash VARCHAR(64) NOT NULL,
    size_bytes BIGINT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_config_data_config_id ON config_data(config_id);

-- Insert sample data
INSERT INTO config_metadata (config_id, service_name, version, format, created_by, description)
VALUES
    ('app-config-v1', 'example-service', 1, 'json', 'admin', 'Initial configuration'),
    ('app-config-v2', 'example-service', 2, 'json', 'admin', 'Updated configuration')
ON CONFLICT (config_id) DO NOTHING;

INSERT INTO config_data (config_id, content, content_hash, size_bytes)
VALUES
    ('app-config-v1',
     '{"database": {"host": "localhost", "port": 5432}, "cache": {"enabled": true}}',
     'abc123', 85),
    ('app-config-v2',
     '{"database": {"host": "localhost", "port": 5432}, "cache": {"enabled": true, "ttl": 3600}}',
     'def456', 105)
ON CONFLICT DO NOTHING;

-- Migration complete
SELECT 'Migration 001: Core tables created' as status;