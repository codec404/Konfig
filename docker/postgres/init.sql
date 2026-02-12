-- Database initialization script for Dynamic Configuration Service

-- Config metadata table
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

CREATE INDEX idx_service_name ON config_metadata(service_name);
CREATE INDEX idx_config_id ON config_metadata(config_id);
CREATE INDEX idx_version ON config_metadata(version);

-- Config data table
CREATE TABLE IF NOT EXISTS config_data (
    id SERIAL PRIMARY KEY,
    config_id VARCHAR(255) REFERENCES config_metadata(config_id) ON DELETE CASCADE,
    content TEXT NOT NULL,
    content_hash VARCHAR(64) NOT NULL,
    size_bytes BIGINT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_config_data_config_id ON config_data(config_id);

-- Rollout state table
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

CREATE INDEX idx_rollout_config_id ON rollout_state(config_id);
CREATE INDEX idx_rollout_status ON rollout_state(status);

-- Service instances table
CREATE TABLE IF NOT EXISTS service_instances (
    id SERIAL PRIMARY KEY,
    service_name VARCHAR(255) NOT NULL,
    instance_id VARCHAR(255) NOT NULL,
    current_config_version BIGINT,
    last_heartbeat TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(50) DEFAULT 'connected',
    metadata JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(service_name, instance_id)
);

CREATE INDEX idx_service_instances_name ON service_instances(service_name);
CREATE INDEX idx_service_instances_status ON service_instances(status);

-- Audit log table
CREATE TABLE IF NOT EXISTS audit_log (
    id SERIAL PRIMARY KEY,
    config_id VARCHAR(255),
    action VARCHAR(100) NOT NULL,
    performed_by VARCHAR(255),
    details JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_audit_config_id ON audit_log(config_id);
CREATE INDEX idx_audit_created_at ON audit_log(created_at);

-- Health check table
CREATE TABLE IF NOT EXISTS health_checks (
    id SERIAL PRIMARY KEY,
    service_name VARCHAR(255) NOT NULL,
    instance_id VARCHAR(255) NOT NULL,
    config_version BIGINT,
    status VARCHAR(50) NOT NULL,
    error_message TEXT,
    metrics JSONB,
    checked_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_health_service ON health_checks(service_name, instance_id);
CREATE INDEX idx_health_checked_at ON health_checks(checked_at);

-- Insert sample data
INSERT INTO config_metadata (config_id, service_name, version, format, created_by, description)
VALUES 
    ('app-config-v1', 'example-service', 1, 'json', 'admin', 'Initial configuration'),
    ('app-config-v2', 'example-service', 2, 'json', 'admin', 'Updated configuration');

INSERT INTO config_data (config_id, content, content_hash, size_bytes)
VALUES 
    ('app-config-v1', '{"database": {"host": "localhost", "port": 5432}, "cache": {"enabled": true}}', 
     'abc123', 85),
    ('app-config-v2', '{"database": {"host": "localhost", "port": 5432}, "cache": {"enabled": true, "ttl": 3600}}',
     'def456', 105);

-- Function to update updated_at timestamp
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

-- Triggers
CREATE TRIGGER update_rollout_state_updated_at BEFORE UPDATE ON rollout_state
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_service_instances_updated_at BEFORE UPDATE ON service_instances
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- Grant permissions
GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO configuser;
GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO configuser;