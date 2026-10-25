// Thin RAII wrapper around SQLite. Only the repositories (data-access layer) use Statement, so no
// SQL exists outside src/SqliteRepositories.cpp and the schema. Every value is bound as a parameter.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "lms/Common.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace lms {

// Lets services group several repository calls into one atomic unit without knowing about SQLite.
class ITransactionRunner {
public:
    virtual ~ITransactionRunner() = default;
    // Runs work inside a transaction: commits when it returns, rolls back when it throws.
    virtual void runInTransaction(const std::function<void()>& work) = 0;
};

class Database : public ITransactionRunner {
public:
    // path may be ":memory:" (tests). Enables foreign keys and a busy timeout.
    explicit Database(const std::string& path);
    ~Database() override;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Creates all tables, indexes and triggers if they do not exist yet.
    void initialiseSchema();
    void execute(const std::string& sql);  // fixed SQL without user data (schema, PRAGMA)
    Id lastInsertId() const;
    sqlite3* handle() const { return db_; }

    void runInTransaction(const std::function<void()>& work) override;

private:
    sqlite3* db_ = nullptr;
    int depth_ = 0;  // nested calls join the outer transaction
};

// A prepared statement. Parameters are 1-based, as in SQLite.
class Statement {
public:
    Statement(const Database& db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bind(int index, const std::string& value);
    Statement& bind(int index, const char* value);
    Statement& bind(int index, std::int64_t value);
    Statement& bind(int index, int value);
    Statement& bind(int index, double value);
    Statement& bindNull(int index);
    Statement& bind(int index, const std::optional<Id>& value);
    Statement& bind(int index, const std::optional<std::string>& value);

    bool step();      // true while a row is available
    void run();       // executes a statement that returns no rows (INSERT/UPDATE/DELETE)

    std::string text(int column) const;
    Id integer(int column) const;
    double real(int column) const;
    bool isNull(int column) const;
    std::optional<Id> optionalInteger(int column) const;
    std::optional<std::string> optionalText(int column) const;

private:
    void check(int rc, const char* what) const;
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace lms
