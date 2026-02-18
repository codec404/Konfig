-- Migration 000: Migration Tracking System
-- Description: Creates table to track applied migrations
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Schema Migrations Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS schema_migrations (
    id SERIAL PRIMARY KEY,
    migration_number INTEGER UNIQUE NOT NULL,
    migration_name VARCHAR(255) NOT NULL,
    applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(50) DEFAULT 'success',
    execution_time_ms INTEGER,
    checksum VARCHAR(64)
);

CREATE INDEX IF NOT EXISTS idx_migration_number ON schema_migrations(migration_number);
CREATE INDEX IF NOT EXISTS idx_migration_status ON schema_migrations(status);
CREATE INDEX IF NOT EXISTS idx_migration_applied_at ON schema_migrations(applied_at);

-- Migration complete
SELECT 'Migration 000: Migration tracker created' as status;