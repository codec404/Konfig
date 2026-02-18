-- Migration 006: Functions and Triggers
-- Description: Creates utility functions and automated triggers
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Update Timestamp Function
-- ═══════════════════════════════════════════════════════════════════

CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

-- ═══════════════════════════════════════════════════════════════════
-- Triggers
-- ═══════════════════════════════════════════════════════════════════

-- Rollout state updated_at trigger
CREATE TRIGGER update_rollout_state_updated_at
    BEFORE UPDATE ON rollout_state
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- Service instances updated_at trigger
CREATE TRIGGER update_service_instances_updated_at
    BEFORE UPDATE ON service_instances
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- Validation schemas updated_at trigger
CREATE TRIGGER update_validation_schemas_updated_at
    BEFORE UPDATE ON validation_schemas
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- Migration complete
SELECT 'Migration 006: Functions and triggers created' as status;