#include "parser.h"
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>

// helper
static std::string toUpper(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

// trim + remove semicolons
std::string trim(const std::string &str)
{
    std::string cleaned = str;

    cleaned.erase(
        std::remove(cleaned.begin(), cleaned.end(), ';'),
        cleaned.end());

    size_t start = cleaned.find_first_not_of(" \n\r\t");
    size_t end = cleaned.find_last_not_of(" \n\r\t");

    if (start == std::string::npos)
        return "";

    return cleaned.substr(start, end - start + 1);
}

// query type detection
QueryType parseQueryType(const std::string &query)
{
    std::string q = toUpper(query);

    if (q.find("CREATE TABLE") != std::string::npos)
        return CREATE;
    else if (q.find("INSERT INTO") != std::string::npos)
        return INSERT;
    else if (q.find("INNER JOIN") != std::string::npos)
        return JOIN;
    else if (q.find("SELECT") != std::string::npos)
        return SELECT;
    else
        return UNKNOWN;
}

// extract table name
std::string extractTableName(const std::string &query)
{
    std::stringstream ss(query);
    std::string word, table;

    while (ss >> word)
    {
        std::string w = toUpper(word);
        if (w == "TABLE" || w == "INTO" || w == "FROM")
        {
            ss >> table;
            break;
        }
    }

    size_t parenPos = table.find('(');
    if (parenPos != std::string::npos)
        table = table.substr(0, parenPos);

    return toUpper(trim(table));
}

// CREATE columns
std::vector<std::string> extractColumns(const std::string &query)
{
    std::vector<std::string> columns;

    int start = query.find("(");
    int end = query.rfind(")");

    if (start == -1 || end == -1)
        return columns;

    std::string inside =
        query.substr(start + 1, end - start - 1);

    std::stringstream ss(inside);
    std::string colDef;

    while (getline(ss, colDef, ','))
    {
        std::stringstream cs(colDef);
        std::string col;
        cs >> col;

        columns.push_back(toUpper(trim(col)));
    }

    return columns;
}

// CREATE types
std::vector<std::string> extractTypes(const std::string &query)
{
    std::vector<std::string> types;

    int start = query.find("(");
    int end = query.rfind(")");

    if (start == -1 || end == -1)
        return types;

    std::string inside =
        query.substr(start + 1, end - start - 1);

    std::stringstream ss(inside);
    std::string colDef;

    while (getline(ss, colDef, ','))
    {
        std::stringstream cs(colDef);
        std::string col, type;
        cs >> col >> type;

        types.push_back(toUpper(trim(type)));
    }

    return types;
}

// INSERT values
std::vector<std::string> extractValues(const std::string &query)
{
    auto batches = extractValueBatches(query);
    if (batches.empty())
        return {};

    return batches.front();
}

std::vector<std::vector<std::string>> extractValueBatches(const std::string &query)
{
    std::vector<std::vector<std::string>> batches;

    int valuesPos = toUpper(query).find("VALUES");
    if (valuesPos == -1)
        return batches;

    std::vector<std::string> currentRow;
    std::string currentValue;
    bool inQuote = false;
    bool inTuple = false;

    std::string payload = query.substr(valuesPos + 6);

    for (char ch : payload)
    {
        if (ch == '\'')
        {
            inQuote = !inQuote;
            currentValue += ch;
            continue;
        }

        if (!inQuote)
        {
            if (ch == '(')
            {
                inTuple = true;
                currentRow.clear();
                currentValue.clear();
                continue;
            }

            if (ch == ',' && inTuple)
            {
                currentRow.push_back(trim(currentValue));
                currentValue.clear();
                continue;
            }

            if (ch == ')' && inTuple)
            {
                currentRow.push_back(trim(currentValue));

                for (auto &value : currentRow)
                {
                    if (value.size() >= 2 &&
                        value.front() == '\'' &&
                        value.back() == '\'')
                    {
                        value = value.substr(1, value.size() - 2);
                    }
                }

                batches.push_back(currentRow);
                currentRow.clear();
                currentValue.clear();
                inTuple = false;
                continue;
            }
        }

        if (inTuple)
            currentValue += ch;
    }

    return batches;
}

// SELECT columns
std::vector<std::string> extractSelectColumns(const std::string &query)
{
    std::vector<std::string> cols;

    int selectPos = toUpper(query).find("SELECT");
    int fromPos = toUpper(query).find("FROM");

    if (selectPos == -1 || fromPos == -1)
        return cols;

    std::string colPart =
        query.substr(selectPos + 6,
                     fromPos - (selectPos + 6));

    colPart = trim(colPart);

    std::stringstream ss(colPart);
    std::string col;

    while (getline(ss, col, ','))
    {
        cols.push_back(toUpper(trim(col)));
    }

    return cols;
}

// WHERE exists
bool hasWhere(const std::string &query)
{
    return toUpper(query).find("WHERE") != std::string::npos;
}

// old numeric index support
int extractWhereColumn(const std::string &query)
{
    std::stringstream ss(query);
    std::string word;

    while (ss >> word)
    {
        if (toUpper(word) == "WHERE")
        {
            int col;
            ss >> col;
            return col;
        }
    }

    return -1;
}

// WHERE column name
std::string extractWhereColumnName(const std::string &query)
{
    int wherePos = toUpper(query).find("WHERE");
    if (wherePos == -1)
        return "";

    std::string condition =
        trim(query.substr(wherePos + 5));

    std::vector<std::string> ops =
        {">=", "<=", ">", "<", "="};

    for (auto &op : ops)
    {
        int pos = condition.find(op);
        if (pos != -1)
        {
            std::string col =
                condition.substr(0, pos);

            // handle TABLE.COLUMN
            int dotPos = col.find(".");
            if (dotPos != -1)
                col = col.substr(dotPos + 1);

            return toUpper(trim(col));
        }
    }

    return "";
}

// WHERE operator
std::string extractWhereOperator(const std::string &query)
{
    int wherePos = toUpper(query).find("WHERE");
    if (wherePos == -1)
        return "";

    std::string condition =
        trim(query.substr(wherePos + 5));

    std::vector<std::string> ops =
        {">=", "<=", ">", "<", "="};

    for (auto &op : ops)
    {
        if (condition.find(op) != std::string::npos)
            return op;
    }

    return "";
}

// WHERE value
std::string extractWhereValue(const std::string &query)
{
    int wherePos = toUpper(query).find("WHERE");
    if (wherePos == -1)
        return "";

    std::string condition =
        trim(query.substr(wherePos + 5));

    std::vector<std::string> ops =
        {">=", "<=", ">", "<", "="};

    for (auto &op : ops)
    {
        int pos = condition.find(op);
        if (pos != -1)
        {
            std::string value =
                trim(condition.substr(pos + op.size()));

            if (value.size() >= 2 &&
                value.front() == '\'' &&
                value.back() == '\'')
            {
                value =
                    value.substr(1, value.size() - 2);
            }

            return value;
        }
    }

    return "";
}

// JOIN table
std::string extractJoinTable(const std::string &query)
{
    std::stringstream ss(query);
    std::string word, table;

    while (ss >> word)
    {
        if (toUpper(word) == "JOIN")
        {
            ss >> table;
            break;
        }
    }

    return toUpper(trim(table));
}

// JOIN left column
std::string extractJoinLeftColumn(const std::string &query)
{
    int onPos = toUpper(query).find("ON");
    int eqPos = query.find("=");

    std::string left =
        query.substr(onPos + 2,
                     eqPos - (onPos + 2));

    left = trim(left);

    int dotPos = left.find(".");
    if (dotPos != -1)
        left = left.substr(dotPos + 1);

    return toUpper(trim(left));
}

// JOIN right column
std::string extractJoinRightColumn(const std::string &query)
{
    int eqPos = query.find("=");

    std::string right =
        trim(query.substr(eqPos + 1));

    int wherePos = toUpper(right).find("WHERE");
    if (wherePos != -1)
        right = trim(right.substr(0, wherePos));

    int dotPos = right.find(".");
    if (dotPos != -1)
        right = right.substr(dotPos + 1);

    return toUpper(trim(right));
}
