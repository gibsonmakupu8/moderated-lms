#include "lms/Database.hpp"

#include <sqlite3.h>

#include "lms/Schema.hpp"

namespace lms {

Database::Database(const std::string& path) {
    int rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        std::string message = db_ ? sqlite3_errmsg(db_) : "out of memory";
        if (db_) sqlite3_close_v2(db_);
        db_ = nullptr;
        throw DatabaseError("Cannot open database '" + path + "': " + message);
    }
    sqlite3_busy_timeout(db_, 5000);
    execute("PRAGMA foreign_keys = ON;");  // required by the design (SDD 6.1/6.5)
}

Database::~Database() {
    if (db_) sqlite3_close_v2(db_);
}

void Database::initialiseSchema() { execute(kSchemaSql); }

void Database::execute(const std::string& sql) {
    char* error = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        throw DatabaseError("Database error: " + message);
    }
}

Id Database::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

void Database::runInTransaction(const std::function<void()>& work) {
    if (depth_ > 0) {  // already inside a transaction: join it
        work();
        return;
    }
    execute("BEGIN IMMEDIATE;");
    ++depth_;
    try {
        work();
    } catch (...) {
        --depth_;
        try {
            execute("ROLLBACK;");
        } catch (...) {  // keep the original error
        }
        throw;
    }
    --depth_;
    execute("COMMIT;");
}

// ---------------------------------------------------------------------------------------------

Statement::Statement(const Database& db, const std::string& sql) : db_(db.handle()) {
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK) throw DatabaseError(std::string("Database error (prepare): ") + sqlite3_errmsg(db_));
}

Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

void Statement::check(int rc, const char* what) const {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE) {
        throw DatabaseError(std::string("Database error (") + what + "): " + sqlite3_errmsg(db_));
    }
}

Statement& Statement::bind(int index, const std::string& value) {
    check(sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT), "bind text");
    return *this;
}
Statement& Statement::bind(int index, const char* value) { return bind(index, std::string(value)); }
Statement& Statement::bind(int index, std::int64_t value) {
    check(sqlite3_bind_int64(stmt_, index, value), "bind integer");
    return *this;
}
Statement& Statement::bind(int index, int value) { return bind(index, static_cast<std::int64_t>(value)); }
Statement& Statement::bind(int index, double value) {
    check(sqlite3_bind_double(stmt_, index, value), "bind real");
    return *this;
}
Statement& Statement::bindNull(int index) {
    check(sqlite3_bind_null(stmt_, index), "bind null");
    return *this;
}
Statement& Statement::bind(int index, const std::optional<Id>& value) {
    return value ? bind(index, static_cast<std::int64_t>(*value)) : bindNull(index);
}
Statement& Statement::bind(int index, const std::optional<std::string>& value) {
    return value ? bind(index, *value) : bindNull(index);
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw DatabaseError(std::string("Database error (step): ") + sqlite3_errmsg(db_));
}

void Statement::run() {
    while (step()) {
    }
}

std::string Statement::text(int column) const {
    const unsigned char* raw = sqlite3_column_text(stmt_, column);
    return raw ? std::string(reinterpret_cast<const char*>(raw)) : std::string();
}
Id Statement::integer(int column) const { return sqlite3_column_int64(stmt_, column); }
double Statement::real(int column) const { return sqlite3_column_double(stmt_, column); }
bool Statement::isNull(int column) const { return sqlite3_column_type(stmt_, column) == SQLITE_NULL; }
std::optional<Id> Statement::optionalInteger(int column) const {
    if (isNull(column)) return std::nullopt;
    return integer(column);
}
std::optional<std::string> Statement::optionalText(int column) const {
    if (isNull(column)) return std::nullopt;
    return text(column);
}

}  // namespace lms
