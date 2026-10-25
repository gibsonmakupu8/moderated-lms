-- Moderated LMS - SQLite schema (identical to src/Schema.cpp)
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS departments (
    department_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT NOT NULL UNIQUE CHECK (length(trim(name)) > 0)
);

CREATE TABLE IF NOT EXISTS users (
    user_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT NOT NULL UNIQUE CHECK (length(username) BETWEEN 3 AND 30),
    full_name     TEXT NOT NULL CHECK (length(trim(full_name)) > 0),
    role          TEXT NOT NULL CHECK (role IN ('ADMIN','LECTURER','HOD','STUDENT')),
    password_hash TEXT NOT NULL,
    department_id INTEGER REFERENCES departments(department_id),
    active        INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    -- a Head of Department must belong to a department
    CHECK (role <> 'HOD' OR department_id IS NOT NULL)
);

CREATE TABLE IF NOT EXISTS courses (
    course_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    course_code   TEXT NOT NULL UNIQUE CHECK (length(trim(course_code)) > 0),
    title         TEXT NOT NULL CHECK (length(trim(title)) > 0),
    department_id INTEGER NOT NULL REFERENCES departments(department_id)
);

CREATE TABLE IF NOT EXISTS course_lecturers (
    course_id   INTEGER NOT NULL REFERENCES courses(course_id),
    lecturer_id INTEGER NOT NULL REFERENCES users(user_id),
    PRIMARY KEY (course_id, lecturer_id)
);

CREATE TABLE IF NOT EXISTS enrolments (
    enrolment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id    INTEGER NOT NULL REFERENCES courses(course_id),
    student_id   INTEGER NOT NULL REFERENCES users(user_id),
    UNIQUE (course_id, student_id)
);

CREATE TABLE IF NOT EXISTS assignments (
    assignment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id     INTEGER NOT NULL REFERENCES courses(course_id),
    title         TEXT NOT NULL CHECK (length(trim(title)) > 0),
    description   TEXT NOT NULL DEFAULT '',
    deadline      TEXT NOT NULL,
    max_mark      REAL NOT NULL CHECK (max_mark > 0),
    created_by    INTEGER NOT NULL REFERENCES users(user_id),
    created_at    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS extensions (
    extension_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id),
    student_id    INTEGER NOT NULL REFERENCES users(user_id),
    new_deadline  TEXT NOT NULL,
    reason        TEXT NOT NULL CHECK (length(trim(reason)) > 0),
    granted_by    INTEGER NOT NULL REFERENCES users(user_id),
    granted_at    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS submissions (
    submission_id INTEGER PRIMARY KEY AUTOINCREMENT,
    assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id),
    student_id    INTEGER NOT NULL REFERENCES users(user_id),
    submitted_at  TEXT NOT NULL,
    status        TEXT NOT NULL CHECK (status IN ('DRAFT','SUBMITTED','LATE','MARKED',
                      'AWAITING_MODERATION','RETURNED_FOR_CORRECTION','MODERATED','RELEASED')),
    content_path  TEXT NOT NULL,
    attempt_no    INTEGER NOT NULL CHECK (attempt_no > 0),
    days_late     INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0),
    UNIQUE (assignment_id, student_id, attempt_no)
);

CREATE TABLE IF NOT EXISTS marks (
    mark_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id       INTEGER NOT NULL UNIQUE REFERENCES submissions(submission_id),
    raw_mark            REAL NOT NULL CHECK (raw_mark >= 0),
    penalty_percent     REAL NOT NULL DEFAULT 0 CHECK (penalty_percent BETWEEN 0 AND 100),
    final_mark          REAL NOT NULL CHECK (final_mark >= 0),
    feedback            TEXT NOT NULL CHECK (length(trim(feedback)) > 0),
    marked_by           INTEGER NOT NULL REFERENCES users(user_id),
    marked_at           TEXT NOT NULL,
    moderation_status   TEXT NOT NULL DEFAULT 'PENDING' CHECK (moderation_status IN ('PENDING','APPROVED','RETURNED')),
    moderated_by        INTEGER REFERENCES users(user_id),
    moderated_at        TEXT,
    moderation_comment  TEXT,
    released_by         INTEGER REFERENCES users(user_id),
    released_at         TEXT
);

CREATE TABLE IF NOT EXISTS audit_entries (
    audit_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    mark_id    INTEGER NOT NULL REFERENCES marks(mark_id),
    changed_by INTEGER NOT NULL REFERENCES users(user_id),
    changed_at TEXT NOT NULL,
    event_type TEXT NOT NULL CHECK (event_type IN ('MARK_CREATED','MARK_CHANGED',
                   'SUBMITTED_FOR_MODERATION','APPROVED','RETURNED','RELEASED')),
    old_value  TEXT,
    new_value  TEXT,
    reason     TEXT NOT NULL CHECK (length(trim(reason)) > 0)
);

-- Audit entries are evidence: they can be added but never edited or removed.
CREATE TRIGGER IF NOT EXISTS audit_entries_no_update BEFORE UPDATE ON audit_entries
BEGIN SELECT RAISE(ABORT, 'audit entries cannot be modified'); END;
CREATE TRIGGER IF NOT EXISTS audit_entries_no_delete BEFORE DELETE ON audit_entries
BEGIN SELECT RAISE(ABORT, 'audit entries cannot be deleted'); END;

CREATE INDEX IF NOT EXISTS idx_users_role         ON users(role);
CREATE INDEX IF NOT EXISTS idx_courses_department ON courses(department_id);
CREATE INDEX IF NOT EXISTS idx_enrol_student      ON enrolments(student_id);
CREATE INDEX IF NOT EXISTS idx_assign_course      ON assignments(course_id);
CREATE INDEX IF NOT EXISTS idx_ext_lookup         ON extensions(assignment_id, student_id);
CREATE INDEX IF NOT EXISTS idx_sub_assignment     ON submissions(assignment_id, student_id);
CREATE INDEX IF NOT EXISTS idx_sub_student        ON submissions(student_id);
CREATE INDEX IF NOT EXISTS idx_sub_status         ON submissions(status);
CREATE INDEX IF NOT EXISTS idx_audit_mark         ON audit_entries(mark_id);
