#include <iostream>
#include <string>
#include <vector>

#include "flexql.h"

namespace
{
struct RowCollector
{
    std::vector<std::string> rows;
};

int collectRows(void *data, int argc, char **argv, char **columnNames)
{
    (void)columnNames;
    RowCollector *collector = static_cast<RowCollector *>(data);
    std::string row;

    for (int i = 0; i < argc; ++i)
    {
        if (i > 0)
            row += "|";
        row += argv[i] ? argv[i] : "NULL";
    }

    collector->rows.push_back(row);
    return 0;
}

bool execOnly(FlexQL *db, const std::string &sql, const std::string &label)
{
    char *errMsg = nullptr;
    int rc = flexql_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != FLEXQL_OK)
    {
        std::cout << "[FAIL] " << label << " -> "
                  << (errMsg ? errMsg : "unknown error") << "\n";
        if (errMsg)
            flexql_free(errMsg);
        return false;
    }

    std::cout << "[PASS] " << label << "\n";
    return true;
}

bool execQuery(FlexQL *db,
               const std::string &sql,
               const std::string &label,
               const std::vector<std::string> &expectedRows)
{
    RowCollector collector;
    char *errMsg = nullptr;
    int rc = flexql_exec(db, sql.c_str(), collectRows, &collector, &errMsg);

    if (rc != FLEXQL_OK)
    {
        std::cout << "[FAIL] " << label << " -> "
                  << (errMsg ? errMsg : "unknown error") << "\n";
        if (errMsg)
            flexql_free(errMsg);
        return false;
    }

    if (collector.rows != expectedRows)
    {
        std::cout << "[FAIL] " << label << "\n";
        std::cout << "Expected:\n";
        for (const auto &row : expectedRows)
            std::cout << "  " << row << "\n";
        std::cout << "Actual:\n";
        for (const auto &row : collector.rows)
            std::cout << "  " << row << "\n";
        return false;
    }

    std::cout << "[PASS] " << label << "\n";
    return true;
}
}

int main()
{
    FlexQL *db = nullptr;
    if (flexql_open("127.0.0.1", 9000, &db) != FLEXQL_OK)
    {
        std::cout << "Cannot connect to FlexQL\n";
        return 1;
    }

    bool ok = true;

    ok &= execOnly(
        db,
        "CREATE TABLE CLIENT_USERS(ID DECIMAL, NAME VARCHAR(64), BALANCE DECIMAL, EXPIRES_AT DECIMAL);",
        "CREATE TABLE");
    ok &= execOnly(
        db,
        "INSERT INTO CLIENT_USERS VALUES (1, 'Alice', 1200, 1893456000);",
        "INSERT row 1");
    ok &= execOnly(
        db,
        "INSERT INTO CLIENT_USERS VALUES (2, 'Bob', 450, 1893456000);",
        "INSERT row 2");
    ok &= execQuery(
        db,
        "SELECT * FROM CLIENT_USERS;",
        "SELECT *",
        {"1|Alice|1200|1893456000", "2|Bob|450|1893456000"});
    ok &= execQuery(
        db,
        "SELECT NAME, BALANCE FROM CLIENT_USERS;",
        "SELECT specific columns",
        {"Alice|1200", "Bob|450"});
    ok &= execQuery(
        db,
        "SELECT NAME FROM CLIENT_USERS WHERE BALANCE > 1000;",
        "WHERE clause",
        {"Alice"});

    ok &= execOnly(
        db,
        "CREATE TABLE CLIENT_ORDERS(ORDER_ID DECIMAL, USER_ID DECIMAL, AMOUNT DECIMAL, EXPIRES_AT DECIMAL);",
        "CREATE TABLE for join");
    ok &= execOnly(
        db,
        "INSERT INTO CLIENT_ORDERS VALUES (101, 1, 50, 1893456000);",
        "INSERT order 101");
    ok &= execOnly(
        db,
        "INSERT INTO CLIENT_ORDERS VALUES (102, 2, 150, 1893456000);",
        "INSERT order 102");
    ok &= execQuery(
        db,
        "SELECT CLIENT_USERS.NAME, CLIENT_ORDERS.AMOUNT "
        "FROM CLIENT_USERS INNER JOIN CLIENT_ORDERS "
        "ON CLIENT_USERS.ID = CLIENT_ORDERS.USER_ID;",
        "INNER JOIN",
        {"Alice|50", "Bob|150"});

    flexql_close(db);
    return ok ? 0 : 1;
}
