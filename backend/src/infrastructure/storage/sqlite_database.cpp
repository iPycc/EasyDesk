#include "infrastructure/storage/sqlite_database.hpp"

#include <sqlite3.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace easydesk::storage {
namespace {

[[noreturn]] void throwSqlite(sqlite3* database, std::string_view operation, int code) {
    const char* detail = database == nullptr ? sqlite3_errstr(code) : sqlite3_errmsg(database);
    throw std::runtime_error(std::string(operation) + ": " + detail);
}

int checkedLength(std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("SQLite value is too large");
    }
    return static_cast<int>(size);
}

}  // namespace

SqliteStatement::SqliteStatement(sqlite3_stmt* statement) : statement_(statement) {}

SqliteStatement::~SqliteStatement() {
    if (statement_ != nullptr) {
        sqlite3_finalize(statement_);
    }
}

SqliteStatement::SqliteStatement(SqliteStatement&& other) noexcept
    : statement_(std::exchange(other.statement_, nullptr)) {}

SqliteStatement& SqliteStatement::operator=(SqliteStatement&& other) noexcept {
    if (this != &other) {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
        statement_ = std::exchange(other.statement_, nullptr);
    }
    return *this;
}

void SqliteStatement::bindText(int index, std::string_view value) {
    const int result = sqlite3_bind_text(statement_, index, value.data(), checkedLength(value.size()),
                                         SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        throwSqlite(sqlite3_db_handle(statement_), "bind text", result);
    }
}

void SqliteStatement::bindBlob(int index, std::span<const std::uint8_t> value) {
    const int result = sqlite3_bind_blob(statement_, index, value.data(), checkedLength(value.size()),
                                         SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        throwSqlite(sqlite3_db_handle(statement_), "bind blob", result);
    }
}

void SqliteStatement::bindInt(int index, int value) {
    const int result = sqlite3_bind_int(statement_, index, value);
    if (result != SQLITE_OK) {
        throwSqlite(sqlite3_db_handle(statement_), "bind integer", result);
    }
}

void SqliteStatement::bindInt64(int index, std::int64_t value) {
    const int result = sqlite3_bind_int64(statement_, index, value);
    if (result != SQLITE_OK) {
        throwSqlite(sqlite3_db_handle(statement_), "bind 64-bit integer", result);
    }
}

bool SqliteStatement::step() {
    const int result = sqlite3_step(statement_);
    if (result == SQLITE_ROW) {
        return true;
    }
    if (result == SQLITE_DONE) {
        return false;
    }
    throwSqlite(sqlite3_db_handle(statement_), "step statement", result);
}

std::string SqliteStatement::columnText(int index) const {
    const auto* value = sqlite3_column_text(statement_, index);
    const int bytes = sqlite3_column_bytes(statement_, index);
    return value == nullptr ? std::string() : std::string(reinterpret_cast<const char*>(value), bytes);
}

std::vector<std::uint8_t> SqliteStatement::columnBlob(int index) const {
    const auto* value = static_cast<const std::uint8_t*>(sqlite3_column_blob(statement_, index));
    const int bytes = sqlite3_column_bytes(statement_, index);
    return value == nullptr ? std::vector<std::uint8_t>()
                            : std::vector<std::uint8_t>(value, value + bytes);
}

int SqliteStatement::columnInt(int index) const {
    return sqlite3_column_int(statement_, index);
}

std::int64_t SqliteStatement::columnInt64(int index) const {
    return sqlite3_column_int64(statement_, index);
}

SqliteDatabase::SqliteDatabase(const std::string& path) {
    constexpr int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int result = sqlite3_open_v2(path.c_str(), &database_, flags, nullptr);
    if (result != SQLITE_OK) {
        sqlite3* failedDatabase = std::exchange(database_, nullptr);
        const std::string detail = failedDatabase == nullptr ? sqlite3_errstr(result)
                                                             : sqlite3_errmsg(failedDatabase);
        if (failedDatabase != nullptr) {
            sqlite3_close(failedDatabase);
        }
        throw std::runtime_error("open database: " + detail);
    }

    sqlite3_extended_result_codes(database_, 1);
    sqlite3_busy_timeout(database_, 5000);
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA journal_mode = WAL;");
    execute("PRAGMA synchronous = NORMAL;");
}

SqliteDatabase::~SqliteDatabase() {
    if (database_ != nullptr) {
        sqlite3_close(database_);
    }
}

void SqliteDatabase::execute(std::string_view sql) {
    char* errorMessage = nullptr;
    const std::string statement(sql);
    const int result = sqlite3_exec(database_, statement.c_str(), nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        const std::string detail = errorMessage == nullptr ? sqlite3_errmsg(database_) : errorMessage;
        sqlite3_free(errorMessage);
        throw std::runtime_error("execute SQL: " + detail);
    }
}

SqliteStatement SqliteDatabase::prepare(std::string_view sql) {
    sqlite3_stmt* statement = nullptr;
    const int result = sqlite3_prepare_v2(database_, sql.data(), checkedLength(sql.size()), &statement,
                                          nullptr);
    if (result != SQLITE_OK) {
        throwSqlite(database_, "prepare statement", result);
    }
    return SqliteStatement(statement);
}

}  // namespace easydesk::storage
