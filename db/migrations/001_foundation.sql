-- Core schema for LabTester

CREATE TABLE IF NOT EXISTS student_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_student_groups_name
    ON student_groups(name);

CREATE TABLE IF NOT EXISTS students (
    id INTEGER PRIMARY KEY,
    group_id INTEGER,
    full_name TEXT NOT NULL,
    is_active INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(group_id) REFERENCES student_groups(id)
);

CREATE TABLE IF NOT EXISTS lab_works (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    description TEXT,
    language TEXT NOT NULL DEFAULT 'C++',
    manifest_rel_path TEXT,
    starter_code_rel_path TEXT,
    test_suite_rel_path TEXT,
    header_rel_path TEXT,
    is_active INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS submission_entries (
    id INTEGER PRIMARY KEY,
    student_id INTEGER NOT NULL,
    lab_work_id INTEGER NOT NULL,
    source_rel_path TEXT NOT NULL,
    source_original_name TEXT NOT NULL,
    status TEXT NOT NULL,
    is_deleted INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(student_id) REFERENCES students(id),
    FOREIGN KEY(lab_work_id) REFERENCES lab_works(id)
);

CREATE INDEX IF NOT EXISTS idx_submission_entries_student
    ON submission_entries(student_id);

CREATE INDEX IF NOT EXISTS idx_submission_entries_lab
    ON submission_entries(lab_work_id);

CREATE TABLE IF NOT EXISTS execution_targets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    mode TEXT,
    endpoint TEXT,
    token TEXT,
    is_active INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS check_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id INTEGER NOT NULL,
    student_id INTEGER NOT NULL,
    lab_work_id INTEGER NOT NULL,
    execution_target_id INTEGER,
    status TEXT NOT NULL,
    tier_basic_passed INTEGER NOT NULL DEFAULT 1,
    tier_advanced_passed INTEGER NOT NULL DEFAULT 1,
    tier_performance_passed INTEGER NOT NULL DEFAULT 1,
    stars INTEGER NOT NULL DEFAULT 0,
    total_tests INTEGER NOT NULL DEFAULT 0,
    passed_tests INTEGER NOT NULL DEFAULT 0,
    failed_tests INTEGER NOT NULL DEFAULT 0,
    duration_ms INTEGER NOT NULL DEFAULT 0,
    runner_message TEXT,
    report_rel_path TEXT,
    sync_state TEXT NOT NULL DEFAULT 'LocalOnly',
    remote_id TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(submission_id) REFERENCES submission_entries(id),
    FOREIGN KEY(student_id) REFERENCES students(id),
    FOREIGN KEY(lab_work_id) REFERENCES lab_works(id),
    FOREIGN KEY(execution_target_id) REFERENCES execution_targets(id)
);

CREATE INDEX IF NOT EXISTS idx_check_runs_submission
    ON check_runs(submission_id);

CREATE INDEX IF NOT EXISTS idx_check_runs_student
    ON check_runs(student_id);

CREATE INDEX IF NOT EXISTS idx_check_runs_lab
    ON check_runs(lab_work_id);

CREATE TABLE IF NOT EXISTS check_case_results (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    check_run_id INTEGER NOT NULL,
    test_name TEXT NOT NULL,
    tier TEXT,
    passed INTEGER NOT NULL,
    input_data TEXT,
    expected_output TEXT,
    actual_output TEXT,
    message TEXT,
    failure_details TEXT,
    duration_ms INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(check_run_id) REFERENCES check_runs(id)
);

CREATE INDEX IF NOT EXISTS idx_check_case_results_run
    ON check_case_results(check_run_id);

CREATE TABLE IF NOT EXISTS sync_queue (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    entry_type TEXT NOT NULL DEFAULT '',
    entry_id INTEGER,
    action TEXT NOT NULL DEFAULT '',
    payload_json TEXT,
    status TEXT NOT NULL DEFAULT 'Pending',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
