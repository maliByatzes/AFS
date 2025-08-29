#include <afsproject/db.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace afs {

SQLiteException::SQLiteException(const std::string &msg) : std::runtime_error("SQLite Error" + msg) {}

SQLiteDB::SQLiteDB(const std::string &filename)
{
  sqlite3 *temp_db = nullptr;

  const int ret =
    sqlite3_open_v2(filename.c_str(), &temp_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, nullptr);

  if (ret != SQLITE_OK) {
    const std::string error_msg = sqlite3_errmsg(temp_db);
    if (temp_db != nullptr) { sqlite3_close(temp_db); }
    throw SQLiteException(error_msg);
  }

  m_db.reset(temp_db);

  execute("PRAGMA journal_mode = wal;");
  execute("PRAGMA foreign_keys = ON;");
}

void SQLiteDB::execute(const std::string &sql)
{
  char *err_msg = nullptr;
  if (sqlite3_exec(m_db.get(), sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
    const std::string error_msg = err_msg;
    sqlite3_free(err_msg);
    throw SQLiteException(error_msg);
  }
}

SQLiteDB::Transaction::Transaction(SQLiteDB &db) : m_db(db) { m_db.execute("BEGIN;"); }// NOLINT

void SQLiteDB::Transaction::commit()
{
  m_db.execute("COMMIT;");
  commited = true;
}

SQLiteDB::Transaction::~Transaction()
{
  if (!commited) {
    try {
      m_db.execute("ROLLBACK;");
    } catch (const SQLiteException &e) {
      std::cerr << "Rollback failed: " << e.what() << "\n";
    }
  }
}

SQLiteDB::Statement::Statement(SQLiteDB &db, const std::string &sql)// NOLINT
{
  if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &m_stmt, nullptr) != SQLITE_OK) {
    throw SQLiteException(sqlite3_errmsg(db.get()));
  }
}

SQLiteDB::Statement::~Statement()
{
  if (m_stmt != nullptr) { sqlite3_finalize(m_stmt); }
}

void SQLiteDB::Statement::bindText(int index, const std::string &text)
{
  sqlite3_bind_text(m_stmt, index, text.c_str(), -1, SQLITE_TRANSIENT);
}

int SQLiteDB::Statement::step()
{
  const int result = sqlite3_step(m_stmt);
  if (result != SQLITE_ROW && result != SQLITE_DONE) {
    throw SQLiteException(sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
  }
  return result;
}

int SQLiteDB::Statement::columnInt(int index) { return sqlite3_column_int(m_stmt, index); }

void SQLiteDB::Statement::reset() { sqlite3_reset(m_stmt); }

sqlite3 *SQLiteDB::get() const { return m_db.get(); }

bool run(SQLiteDB &db, const fs::path &migration_dir)// NOLINT
{
  try {
    db.execute("CREATE TABLE IF NOT EXISTS migrations (name TEXT PRIMARY KEY);");

    if (!fs::exists(migration_dir) || !fs::is_directory(migration_dir)) {
      std::cerr << "Error: Cannot open migration directory: " << migration_dir << "\n";
      return false;
    }

    std::vector<fs::path> migration_files;
    for (const auto &entry : fs::directory_iterator(migration_dir)) {
      if (entry.is_regular_file()) { migration_files.push_back(entry.path()); }
    }

    std::ranges::sort(migration_files);

    for (const auto &file : migration_files) { migrateFile(db, file); }
  } catch (const SQLiteException &e) {
    std::cerr << "Migration failed: " << e.what() << "\n";
    return false;
  }

  return true;
}

void migrateFile(SQLiteDB &db, const fs::path &filepath)// NOLINT
{
  const std::string filename = filepath.filename().string();

  SQLiteDB::Statement stmt(db, "SELECT COUNT(*) FROM migrations WHERE name = ?;");
  stmt.bindText(1, filename);
  stmt.step();
  const int count = stmt.columnInt(0);

  if (count > 0) {
    std::cout << "Skipping migration " << filename << " (already applied).\n";
    return;
  }

  SQLiteDB::Transaction transaction(db);

  std::ifstream file_stream(filepath);
  if (!file_stream) { throw std::runtime_error("Could not open file: " + filepath.string()); }
  const std::string migration_sql((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());

  db.execute(migration_sql);

  SQLiteDB::Statement insert_stmt(db, "INSERT INTO migrations (name) VALUES (?);");
  insert_stmt.bindText(1, filename);
  insert_stmt.step();

  transaction.commit();
  std::cout << "Successfully applied migration: " << filename << "\n";
}

}// namespace afs
