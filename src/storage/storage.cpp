#include "storage.h"
#include <iostream>
#include <cctype>
#include <ctime>
#include <sstream>
#include <mutex>
#include <algorithm>
#include <stdexcept>

namespace
{
bool tryParseNumber(const std::string &value, double &parsed)
{
    if (value.empty())
        return false;

    try
    {
        size_t consumed = 0;
        parsed = std::stod(value, &consumed);
        return consumed == value.size();
    }
    catch (const std::exception &)
    {
        return false;
    }
}

bool compareValues(const std::string &left,
                   const std::string &op,
                   const std::string &right)
{
    if (op == "=")
        return left == right;

    double leftNum = 0.0;
    double rightNum = 0.0;
    bool leftIsNumber = tryParseNumber(left, leftNum);
    bool rightIsNumber = tryParseNumber(right, rightNum);

    if (leftIsNumber && rightIsNumber)
    {
        if (op == ">")
            return leftNum > rightNum;
        if (op == "<")
            return leftNum < rightNum;
        if (op == ">=")
            return leftNum >= rightNum;
        if (op == "<=")
            return leftNum <= rightNum;
        return false;
    }

    if (op == ">")
        return left > right;
    if (op == "<")
        return left < right;
    if (op == ">=")
        return left >= right;
    if (op == "<=")
        return left <= right;
    return false;
}

std::string joinRow(const std::vector<std::string> &values)
{
    std::string row;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
            row += "|";
        row += values[i];
    }
    row += "\n";
    return row;
}

std::string serializeRows(const std::vector<std::vector<std::string>> &rows)
{
    std::string result;
    for (const auto &row : rows)
        result += joinRow(row);
    return result;
}

std::string normalizeColumnName(const std::string &columnName)
{
    size_t dotPos = columnName.find('.');
    if (dotPos != std::string::npos)
        return columnName.substr(dotPos + 1);
    return columnName;
}

int findColumnIndex(const Table &table, const std::string &columnName)
{
    const std::string normalized = normalizeColumnName(columnName);

    for (size_t i = 0; i < table.columns.size(); ++i)
    {
        if (table.columns[i] == normalized)
            return static_cast<int>(i);
    }

    return -1;
}

bool isRowExpired(const Row &row)
{
    return std::time(nullptr) > row.expiryTime;
}
}

// DATETIME validator
static bool isDateTime(const std::string &s)
{
    return s.size() == 19 &&
           s[4] == '-' && s[7] == '-' &&
           s[10] == ' ' &&
           s[13] == ':' && s[16] == ':' &&
           isdigit(s[0]) && isdigit(s[1]) &&
           isdigit(s[2]) && isdigit(s[3]) &&
           isdigit(s[5]) && isdigit(s[6]) &&
           isdigit(s[8]) && isdigit(s[9]) &&
           isdigit(s[11]) && isdigit(s[12]) &&
           isdigit(s[14]) && isdigit(s[15]) &&
           isdigit(s[17]) && isdigit(s[18]);
}

// CREATE TABLE
bool Database::createTable(const std::string &name,
                           const std::vector<std::string> &columns,
                           const std::vector<std::string> &types,
                           std::string &error)
{
    std::lock_guard<std::mutex> lock(dbLock.mtx);

    if (tables.find(name) != tables.end())
    {
        error = "Table already exists";
        return false;
    }

    if (columns.empty() || columns.size() != types.size())
    {
        error = "Invalid table schema";
        return false;
    }

    Table t;
    t.columns = columns;
    t.types = types;

    tables[name] = t;
    queryCache.clear();

    std::cout << "Table created: " << name << std::endl;
    return true;
}

// INSERT
bool Database::insertRow(const std::string &name,
                         const std::vector<std::string> &values,
                         std::string &error)
{
    std::lock_guard<std::mutex> lock(dbLock.mtx);

    if (tables.find(name) == tables.end())
    {
        error = "Table does not exist";
        return false;
    }

    Table &t = tables[name];

    if (values.size() != t.columns.size())
    {
        error = "Column count mismatch";
        return false;
    }

    // duplicate primary key
    if (!values.empty() && t.primaryIndex.exists(values[0]))
    {
        error = "Primary key already exists";
        return false;
    }

    // type validation
    for (size_t i = 0; i < values.size(); i++)
    {
        if (t.types[i] == "INT")
        {
            for (char c : values[i])
            {
                if (!isdigit(c) && c != '-')
                {
                    error = "Type error at " + t.columns[i];
                    return false;
                }
            }
        }
        else if (t.types[i] == "DECIMAL")
        {
            bool dot = false;
            for (char c : values[i])
            {
                if (!isdigit(c) && c != '.' && c != '-')
                {
                    error = "Type error at " + t.columns[i];
                    return false;
                }
                if (c == '.')
                {
                    if (dot)
                    {
                        error = "Invalid decimal";
                        return false;
                    }
                    dot = true;
                }
            }
        }
        else if (t.types[i] == "DATETIME")
        {
            if (!isDateTime(values[i]))
            {
                error = "Invalid DATETIME";
                return false;
            }
        }
    }

    Row row;
    row.values = values;
    row.expiryTime = std::time(nullptr) + 3600;

    int expiryColumn = findColumnIndex(t, "EXPIRES_AT");
    if (expiryColumn != -1)
    {
        double expiry = 0.0;
        if (!tryParseNumber(values[expiryColumn], expiry))
        {
            error = "Invalid expiration timestamp";
            return false;
        }
        row.expiryTime = static_cast<std::time_t>(expiry);
    }

    t.rows.push_back(row);
    queryCache.clear();

    // primary index on first column
    if (!values.empty())
        t.primaryIndex.insert(values[0], t.rows.size() - 1);

    std::cout << "Row inserted into: " << name << std::endl;
    return true;
}

// SELECT *
bool Database::selectAll(const std::string &name,
                         std::string &result,
                         std::string &error)
{
    std::lock_guard<std::mutex> lock(dbLock.mtx);

    std::string cacheKey = "SELECT_ALL_" + name;

    if (queryCache.exists(cacheKey))
    {
        result = queryCache.get(cacheKey);
        return true;
    }

    if (tables.find(name) == tables.end())
    {
        error = "Table does not exist";
        return false;
    }

    Table &t = tables[name];
    std::vector<std::vector<std::string>> rows;

    for (auto &row : t.rows)
    {
        if (isRowExpired(row))
            continue;

        rows.push_back(row.values);
    }

    result = serializeRows(rows);
    queryCache.put(cacheKey, result);
    return true;
}

// SELECT specific columns
bool Database::selectColumns(const std::string &name,
                             const std::vector<std::string> &cols,
                             std::string &result,
                             std::string &error)
{
    std::lock_guard<std::mutex> lock(dbLock.mtx);

    std::string cacheKey = "SELECT_COLS_" + name;
    for (auto &c : cols)
        cacheKey += "_" + c;

    if (queryCache.exists(cacheKey))
    {
        result = queryCache.get(cacheKey);
        return true;
    }

    if (tables.find(name) == tables.end())
    {
        error = "Table does not exist";
        return false;
    }

    Table &t = tables[name];
    std::vector<int> indices;
    std::vector<std::vector<std::string>> rows;

    for (auto &col : cols)
    {
        int index = findColumnIndex(t, col);
        if (index == -1)
        {
            error = "Column not found: " + col;
            return false;
        }
        indices.push_back(index);
    }

    for (auto &row : t.rows)
    {
        if (isRowExpired(row))
            continue;

        std::vector<std::string> selectedRow;
        for (int idx : indices)
            selectedRow.push_back(row.values[idx]);

        rows.push_back(selectedRow);
    }

    result = serializeRows(rows);
    queryCache.put(cacheKey, result);
    return true;
}

// WHERE by index
bool Database::selectWhere(const std::string &name,
                           int columnIndex,
                           const std::string &value,
                           std::string &result,
                           std::string &error)
{
    std::lock_guard<std::mutex> lock(dbLock.mtx);

    if (tables.find(name) == tables.end())
    {
        error = "Table does not exist";
        return false;
    }

    Table &t = tables[name];
    if (columnIndex < 0 ||
        columnIndex >= static_cast<int>(t.columns.size()))
    {
        error = "Column index out of range";
        return false;
    }

    std::vector<std::vector<std::string>> rows;

    for (auto &row : t.rows)
    {
        if (isRowExpired(row))
            continue;

        if (columnIndex < static_cast<int>(row.values.size()) &&
            row.values[columnIndex] == value)
            rows.push_back(row.values);
    }

    result = serializeRows(rows);
    return true;
}

// WHERE by column
bool Database::selectWhereColumn(const std::string &name,
                                 const std::string &columnName,
                                 const std::string &value,
                                 std::string &result,
                                 std::string &error)
{
    std::lock_guard<std::mutex> lock(dbLock.mtx);

    std::string cacheKey =
        "SELECT_WHERE_" + name + "_" + columnName + "_" + value;

    if (queryCache.exists(cacheKey))
    {
        result = queryCache.get(cacheKey);
        return true;
    }

    if (tables.find(name) == tables.end())
    {
        error = "Table does not exist";
        return false;
    }

    Table &t = tables[name];
    int colIndex = findColumnIndex(t, columnName);

    if (colIndex == -1)
    {
        error = "Column not found";
        return false;
    }

    std::vector<std::vector<std::string>> rows;

    // primary index fast path
    if (colIndex == 0 && t.primaryIndex.exists(value))
    {
        Row &row = t.rows[t.primaryIndex.get(value)];

        if (!isRowExpired(row))
            rows.push_back(row.values);

        result = serializeRows(rows);
        queryCache.put(cacheKey, result);
        return true;
    }

    for (auto &row : t.rows)
    {
        if (isRowExpired(row))
            continue;

        if (row.values[colIndex] == value)
            rows.push_back(row.values);
    }

    result = serializeRows(rows);
    queryCache.put(cacheKey, result);
    return true;
}

// SELECT cols + WHERE
bool Database::selectColumnsWhereColumn(
    const std::string &name,
    const std::vector<std::string> &cols,
    const std::string &columnName,
    const std::string &op,
    const std::string &value,
    std::string &result,
    std::string &error)
{
    std::lock_guard<std::mutex> lock(dbLock.mtx);

    std::string cacheKey =
        "SELECT_COLS_WHERE_" + name + "_" + columnName + "_" + value;

    for (auto &c : cols)
        cacheKey += "_" + c;

    if (queryCache.exists(cacheKey))
    {
        result = queryCache.get(cacheKey);
        return true;
    }

    if (tables.find(name) == tables.end())
    {
        error = "Table does not exist";
        return false;
    }

    Table &t = tables[name];
    int whereIndex = findColumnIndex(t, columnName);
    std::vector<int> indices;
    std::vector<std::vector<std::string>> rows;

    if (whereIndex == -1)
    {
        error = "Column not found";
        return false;
    }

    for (auto &col : cols)
    {
        int index = findColumnIndex(t, col);
        if (index == -1)
        {
            error = "Column not found: " + col;
            return false;
        }
        indices.push_back(index);
    }

    for (auto &row : t.rows)
    {
        if (isRowExpired(row))
            continue;

        std::string cell = row.values[whereIndex];
        if (compareValues(cell, op, value))
        {
            std::vector<std::string> selectedRow;
            for (int idx : indices)
                selectedRow.push_back(row.values[idx]);
            rows.push_back(selectedRow);
        }
    }

    result = serializeRows(rows);
    queryCache.put(cacheKey, result);
    return true;
}

bool Database::selectColumnsWhereColumn(
    const std::string &name,
    const std::vector<std::string> &selectedCols,
    const std::string &whereColumn,
    const std::string &whereValue,
    std::string &result,
    std::string &error)
{
    return selectColumnsWhereColumn(
        name,
        selectedCols,
        whereColumn,
        "=",
        whereValue,
        result,
        error);
}

// INNER JOIN
bool Database::innerJoin(const std::string &tableA,
                         const std::string &tableB,
                         const std::string &colA,
                         const std::string &colB,
                         const std::vector<std::string> &selectedCols,
                         const std::string &whereColumn,
                         const std::string &whereOp,
                         const std::string &whereValue,
                         std::string &result,
                         std::string &error)
{
    std::lock_guard<std::mutex> lock(dbLock.mtx);

    if (tables.find(tableA) == tables.end() ||
        tables.find(tableB) == tables.end())
    {
        error = "Table not found";
        return false;
    }

    Table &t1 = tables[tableA];
    Table &t2 = tables[tableB];

    int idxA = findColumnIndex(t1, colA);
    int idxB = findColumnIndex(t2, colB);

    if (idxA == -1 || idxB == -1)
    {
        error = "Join column not found";
        return false;
    }

    struct SelectedColumn
    {
        bool fromLeft;
        int index;
    };

    std::vector<SelectedColumn> selected;
    if (selectedCols.size() == 1 && selectedCols[0] == "*")
    {
        for (size_t i = 0; i < t1.columns.size(); ++i)
            selected.push_back({true, static_cast<int>(i)});
        for (size_t i = 0; i < t2.columns.size(); ++i)
            selected.push_back({false, static_cast<int>(i)});
    }
    else
    {
        for (const auto &col : selectedCols)
        {
            int leftIndex = findColumnIndex(t1, col);
            int rightIndex = findColumnIndex(t2, col);

            if (leftIndex != -1 && rightIndex != -1 &&
                col.find('.') == std::string::npos)
            {
                error = "Ambiguous column: " + col;
                return false;
            }
            if (leftIndex != -1)
            {
                selected.push_back({true, leftIndex});
                continue;
            }
            if (rightIndex != -1)
            {
                selected.push_back({false, rightIndex});
                continue;
            }

            error = "Column not found: " + col;
            return false;
        }
    }

    std::vector<std::vector<std::string>> rows;

    for (auto &row1 : t1.rows)
    {
        if (isRowExpired(row1))
            continue;

        std::string val = row1.values[idxA];

        if (idxB == 0 && t2.primaryIndex.exists(val))
        {
            Row &row2 = t2.rows[t2.primaryIndex.get(val)];

            if (isRowExpired(row2))
                continue;

            if (!whereColumn.empty())
            {
                int leftWhere = findColumnIndex(t1, whereColumn);
                int rightWhere = findColumnIndex(t2, whereColumn);
                std::string whereCell;

                if (leftWhere != -1 && rightWhere != -1 &&
                    whereColumn.find('.') == std::string::npos)
                {
                    error = "Ambiguous WHERE column: " + whereColumn;
                    return false;
                }
                if (leftWhere != -1)
                    whereCell = row1.values[leftWhere];
                else if (rightWhere != -1)
                    whereCell = row2.values[rightWhere];
                else
                {
                    error = "WHERE column not found";
                    return false;
                }

                if (!compareValues(whereCell, whereOp, whereValue))
                    continue;
            }

            std::vector<std::string> joinedRow;
            for (const auto &col : selected)
                joinedRow.push_back(col.fromLeft ? row1.values[col.index] : row2.values[col.index]);
            rows.push_back(joinedRow);
        }
        else
        {
            for (auto &row2 : t2.rows)
            {
                if (isRowExpired(row2))
                    continue;

                if (row1.values[idxA] == row2.values[idxB])
                {
                    if (!whereColumn.empty())
                    {
                        int leftWhere = findColumnIndex(t1, whereColumn);
                        int rightWhere = findColumnIndex(t2, whereColumn);
                        std::string whereCell;

                        if (leftWhere != -1 && rightWhere != -1 &&
                            whereColumn.find('.') == std::string::npos)
                        {
                            error = "Ambiguous WHERE column: " + whereColumn;
                            return false;
                        }
                        if (leftWhere != -1)
                            whereCell = row1.values[leftWhere];
                        else if (rightWhere != -1)
                            whereCell = row2.values[rightWhere];
                        else
                        {
                            error = "WHERE column not found";
                            return false;
                        }

                        if (!compareValues(whereCell, whereOp, whereValue))
                            continue;
                    }

                    std::vector<std::string> joinedRow;
                    for (const auto &col : selected)
                        joinedRow.push_back(col.fromLeft ? row1.values[col.index] : row2.values[col.index]);
                    rows.push_back(joinedRow);
                }
            }
        }
    }

    result = serializeRows(rows);
    return true;
}
