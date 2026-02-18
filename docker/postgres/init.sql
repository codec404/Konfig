-- Database initialization script for Dynamic Configuration Service
-- Runs migrations in order from db/migrations/

\i /docker-entrypoint-initdb.d/migrations/000_migration_tracker.sql
\i /docker-entrypoint-initdb.d/migrations/001_core_tables.sql
\i /docker-entrypoint-initdb.d/migrations/002_rollout_tables.sql
\i /docker-entrypoint-initdb.d/migrations/003_client_instances.sql
\i /docker-entrypoint-initdb.d/migrations/004_audit_health.sql
\i /docker-entrypoint-initdb.d/migrations/005_validation_tables.sql
\i /docker-entrypoint-initdb.d/migrations/006_functions_triggers.sql
\i /docker-entrypoint-initdb.d/migrations/007_views.sql
\i /docker-entrypoint-initdb.d/migrations/008_permissions.sql

-- Log completion
SELECT 'All migrations applied successfully' as status;
