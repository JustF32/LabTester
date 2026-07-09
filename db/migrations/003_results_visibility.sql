-- Add results visibility flag for check runs

ALTER TABLE check_runs
ADD COLUMN is_visible_in_results INTEGER NOT NULL DEFAULT 1;
