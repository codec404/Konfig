-- Migration 002: Rollout Management Tables
-- Description: Creates tables for managing gradual rollouts
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Rollout State Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS rollout_state (
    id SERIAL PRIMARY KEY,
    config_id VARCHAR(255) REFERENCES config_metadata(config_id),
    strategy VARCHAR(50) NOT NULL,
    target_percentage INTEGER DEFAULT 100,
    current_percentage INTEGER DEFAULT 0,
    status VARCHAR(50) NOT NULL,
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_rollout_config_id ON rollout_state(config_id);
CREATE INDEX IF NOT EXISTS idx_rollout_status ON rollout_state(status);

-- Migration complete
SELECT 'Migration 002: Rollout tables created' as status;