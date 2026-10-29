#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace easydesk::storage {

class SqliteStatement final {
public:
    explicit SqliteStatement(sqlite3_stmt* statement);
    ~SqliteStatement();

    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;
    SqliteStatement(SqliteStatement&& other) noexcept;
    SqliteStatement& operator=(SqliteStatement&& other) noexcept;

    void bindText(int index, std::string_view value);
    void bindBlob(int index, std::span<const std::uint8_t> value);
    void bindInt(int index, int value);
    void bindInt64(int index, std::int64_t value);
    bool step();

    std::string columnText(int index) const;
    std::vector<std::uint8_t> columnBlob(int index) const;
    int columnInt(int index) const;
    std::int64_t columnInt64(int index) const;

private:
    sqlite3_stmt* statement_;
};

class SqliteDatabase final {
public:
    explicit SqliteDatabase(const std::string& path);
    ~SqliteDatabase();

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;

    void execute(std::string_view sql);
    SqliteStatement prepare(std::string_view sql);

private:
    sqlite3* database_ = nullptr;
};

}  // namespace easydesk::storage
