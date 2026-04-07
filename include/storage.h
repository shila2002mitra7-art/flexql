#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <ctime>

#include "index/primary_index.h"
#include "cache/lru_cache.h"
#include "concurrency/db_lock.h"

struct Row
{
    std::vector<std::string> values;
    std::time_t expiryTime;
};

struct Table
{
    std::vector<std::string> columns;
    std::vector<std::string> types;
    std::unordered_map<std::string, size_t> columnIndex;
    std::vector<Row> rows;

    // primary key index on first column
    PrimaryIndex primaryIndex;
};

class Database
{
private:
    std::unordered_map<std::string, Table> tables;

    // query cache
    LRUCache queryCache;

    // concurrency lock
    DBLock dbLock;

public:
    Database() : queryCache(100) {}

    // CORE TABLE OPS
    bool createTable(const std::string &name,
                     const std::vector<std::string> &columns,
                     const std::vector<std::string> &types,
                     std::string &error);

    bool insertRow(const std::string &name,
                   const std::vector<std::string> &values,
                   std::string &error);

    bool insertRows(const std::string &name,
                    const std::vector<std::vector<std::string>> &rows,
                    std::string &error);

    // SELECT OPS
    bool selectAll(const std::string &name,
                   std::string &result,
                   std::string &error);

    bool selectColumns(const std::string &name,
                       const std::vector<std::string> &selectedCols,
                       std::string &result,
                       std::string &error);

    bool selectWhere(const std::string &name,
                     int columnIndex,
                     const std::string &value,
                     std::string &result,
                     std::string &error);

    bool selectWhereColumn(const std::string &name,
                           const std::string &columnName,
                           const std::string &value,
                           std::string &result,
                           std::string &error);

    bool selectColumnsWhereColumn(const std::string &name,
                                  const std::vector<std::string> &cols,
                                  const std::string &columnName,
                                  const std::string &op,
                                  const std::string &value,
                                  std::string &result,
                                  std::string &error);

    bool selectColumnsWhereColumn(
        const std::string &name,
        const std::vector<std::string> &selectedCols,
        const std::string &whereColumn,
        const std::string &whereValue,
        std::string &result,
        std::string &error);

    // JOIN
    bool innerJoin(const std::string &tableA,
                   const std::string &tableB,
                   const std::string &columnA,
                   const std::string &columnB,
                   const std::vector<std::string> &selectedCols,
                   const std::string &whereColumn,
                   const std::string &whereOp,
                   const std::string &whereValue,
                   std::string &result,
                   std::string &error);
};

#endif
