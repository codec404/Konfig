-- Migration 005: Validation System
-- Description: Creates validation schemas, rules, and history tables
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Validation Schemas Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS validation_schemas (
    schema_id VARCHAR(255) PRIMARY KEY,
    service_name VARCHAR(255) NOT NULL,
    schema_type VARCHAR(50) NOT NULL,  -- json-schema, custom
    schema_content TEXT NOT NULL,
    description TEXT,
    created_by VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT true
);

CREATE INDEX IF NOT EXISTS idx_validation_schema_service ON validation_schemas(service_name);
CREATE INDEX IF NOT EXISTS idx_validation_schema_type ON validation_schemas(schema_type);
CREATE INDEX IF NOT EXISTS idx_validation_schema_created ON validation_schemas(created_at);

-- ═══════════════════════════════════════════════════════════════════
-- Validation History Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS validation_history (
    id SERIAL PRIMARY KEY,
    service_name VARCHAR(255) NOT NULL,
    config_content TEXT NOT NULL,
    validation_result BOOLEAN NOT NULL,
    errors TEXT,
    warnings TEXT,
    validated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    validated_by VARCHAR(255)
);

CREATE INDEX IF NOT EXISTS idx_validation_history_service ON validation_history(service_name);
CREATE INDEX IF NOT EXISTS idx_validation_history_validated_at ON validation_history(validated_at);
CREATE INDEX IF NOT EXISTS idx_validation_history_result ON validation_history(validation_result);

-- ═══════════════════════════════════════════════════════════════════
-- Validation Rules Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS validation_rules (
    rule_id VARCHAR(255) PRIMARY KEY,
    service_name VARCHAR(255) NOT NULL,
    field_path VARCHAR(255) NOT NULL,  -- e.g., "settings.max_connections"
    rule_type VARCHAR(50) NOT NULL,     -- required, range, format, custom
    rule_config TEXT NOT NULL,          -- JSON config for the rule
    error_message TEXT,
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_validation_rules_service ON validation_rules(service_name);
CREATE INDEX IF NOT EXISTS idx_validation_rules_type ON validation_rules(rule_type);
CREATE INDEX IF NOT EXISTS idx_validation_rules_active ON validation_rules(is_active);

-- Sample validation rules
INSERT INTO validation_rules (rule_id, service_name, field_path, rule_type, rule_config, error_message)
VALUES
    ('rule-001', 'payment-service', 'settings.max_connections', 'range',
     '{"min": 1, "max": 1000}',
     'max_connections must be between 1 and 1000')
ON CONFLICT (rule_id) DO NOTHING;

INSERT INTO validation_rules (rule_id, service_name, field_path, rule_type, rule_config, error_message)
VALUES
    ('rule-002', 'payment-service', 'settings.timeout_ms', 'range',
     '{"min": 100, "max": 60000}',
     'timeout_ms must be between 100 and 60000')
ON CONFLICT (rule_id) DO NOTHING;

INSERT INTO validation_rules (rule_id, service_name, field_path, rule_type, rule_config, error_message)
VALUES
    ('rule-003', 'payment-service', 'database.host', 'required',
     '{}',
     'database.host is required')
ON CONFLICT (rule_id) DO NOTHING;

-- Sample validation schema
INSERT INTO validation_schemas (schema_id, service_name, schema_type, schema_content, description, created_by)
VALUES
    ('payment-service-schema-v1', 'payment-service', 'json-schema',
     '{"type": "object", "required": ["settings", "database"], "properties": {"settings": {"type": "object"}, "database": {"type": "object"}}}',
     'Payment service JSON schema',
     'admin')
ON CONFLICT (schema_id) DO NOTHING;

-- Migration complete
SELECT 'Migration 005: Validation tables created' as status;