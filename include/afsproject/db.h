#ifndef DB_H_
#define DB_H_

#include "sqlite3.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// const char *MIGRATION_DIR = "db/migration";// NOLINT

namespace afs {

class SQLiteException : public std::runtime_error
{
public:
  explicit SQLiteException(const std::string &msg);
};

class SQLiteDB
{
public:
  struct Deleter
  {
    void operator()(sqlite3 *db) const// NOLINT
    {
      if (db != nullptr) { sqlite3_close(db); }
    }
  };

  explicit SQLiteDB(const std::string &filename);

  void execute(const std::string &sql);

  class Transaction// NOLINT
  {
  private:
    SQLiteDB &m_db;// NOLINT
    bool commited = false;

  public:
    explicit Transaction(SQLiteDB &db);// NOLINT
    void commit();
    ~Transaction();
  };

  class Statement// NOLINT
  {
  public:
    Statement(SQLiteDB &db, const std::string &sql);// NOLINT
    ~Statement();
    void bindText(int index, const std::string &text);
    int step();
    int columnInt(int index);
    void reset();
  
  private:
    sqlite3_stmt *m_stmt = nullptr;
  };

  [[nodiscard]] sqlite3 *get() const;
private:
  std::unique_ptr<sqlite3, Deleter> m_db = nullptr;
};

bool run(SQLiteDB &db, const fs::path &migration_dir);// NOLINT
void migrateFile(SQLiteDB &db, const fs::path &filepath);// NOLINT

}// namespace afs

#endif
