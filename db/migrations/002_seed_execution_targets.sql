-- Seed execution targets with empty endpoints/tokens

INSERT INTO execution_targets(title, mode, endpoint, token, is_active, created_at, updated_at)
SELECT 'Local', 'local', '', '', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP
WHERE NOT EXISTS (SELECT 1 FROM execution_targets WHERE lower(mode) = 'local');

INSERT INTO execution_targets(title, mode, endpoint, token, is_active, created_at, updated_at)
SELECT 'Server', 'server', '', '', 1, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP
WHERE NOT EXISTS (SELECT 1 FROM execution_targets WHERE lower(mode) = 'server');
