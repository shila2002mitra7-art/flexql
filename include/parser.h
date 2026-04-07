#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>

enum QueryType
{
    CREATE,
    INSERT,
    SELECT,
    JOIN,
    UNKNOWN
};

QueryType parseQueryType(const std::string &query);

std::string trim(const std::string &str);

std::string extractTableName(const std::string &query);
std::vector<std::string> extractValues(const std::string &query);

std::vector<std::string> extractColumns(const std::string &query);
std::vector<std::string> extractTypes(const std::string &query);

std::vector<std::string> extractSelectColumns(const std::string &query);

// WHERE
bool hasWhere(const std::string &query);
int extractWhereColumn(const std::string &query);
std::string extractWhereColumnName(const std::string &query);
std::string extractWhereOperator(const std::string& query);
std::string extractWhereValue(const std::string &query);

// JOIN
std::string extractJoinTable(const std::string &query);
std::string extractJoinLeftColumn(const std::string &query);
std::string extractJoinRightColumn(const std::string &query);

#endif