CREATE TABLE IF NOT EXISTS submissions (
    id INTEGER PRIMARY KEY,
    student_name TEXT NOT NULL,
    lab_title TEXT NOT NULL,
    language TEXT NOT NULL,
    status TEXT NOT NULL,
    created_at TEXT
);

CREATE TABLE IF NOT EXISTS test_runs (
    submission_id INTEGER PRIMARY KEY,
    total_tests INTEGER NOT NULL,
    passed_tests INTEGER NOT NULL,
    failed_tests INTEGER NOT NULL,
    status TEXT NOT NULL,
    message TEXT,
    executed_at TEXT,
    FOREIGN KEY(submission_id) REFERENCES submissions(id)
);

CREATE TABLE IF NOT EXISTS test_case_results (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id INTEGER NOT NULL,
    test_name TEXT NOT NULL,
    passed INTEGER NOT NULL,
    message TEXT,
    duration_ms INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(submission_id) REFERENCES submissions(id)
);
