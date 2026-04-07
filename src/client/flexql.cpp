#include "flexql.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")

namespace
{
const std::string kResponseTerminator = "__FLEXQL_END__\n";
const std::string kRequestTerminator = "__FLEXQL_SQL_END__\n";
}

struct FlexQL
{
    SOCKET sock;
};

// helper split
static std::vector<std::string> split(
    const std::string &line,
    char delim)
{
    std::stringstream ss(line);
    std::string item;
    std::vector<std::string> result;

    while (std::getline(ss, item, delim))
        result.push_back(item);

    return result;
}

int flexql_open(const char *host, int port, FlexQL **db)
{
    if (!db)
        return FLEXQL_ERROR;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return FLEXQL_ERROR;

    *db = new FlexQL();

    (*db)->sock = socket(AF_INET, SOCK_STREAM, 0);

    if ((*db)->sock == INVALID_SOCKET)
        return FLEXQL_ERROR;

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (strcmp(host, "localhost") == 0)
        host = "127.0.0.1";

    server.sin_addr.s_addr = inet_addr(host);

    if (connect((*db)->sock,
                (sockaddr *)&server,
                sizeof(server)) < 0)
    {
        delete *db;
        return FLEXQL_ERROR;
    }

    return FLEXQL_OK;
}

int flexql_close(FlexQL *db)
{
    if (!db)
        return FLEXQL_ERROR;

    closesocket(db->sock);
    delete db;
    WSACleanup();

    return FLEXQL_OK;
}

int flexql_exec(
    FlexQL *db,
    const char *sql,
    FlexQLCallback callback,
    void *arg,
    char **errmsg)
{
    if (!db || !sql)
    {
        if (errmsg)
            *errmsg = _strdup("Invalid DB handle or SQL");
        return FLEXQL_ERROR;
    }

    std::string payload = std::string(sql) + kRequestTerminator;
    int sent = send(db->sock, payload.c_str(), static_cast<int>(payload.size()), 0);

    if (sent <= 0)
    {
        if (errmsg)
            *errmsg = _strdup("Failed to send SQL");
        return FLEXQL_ERROR;
    }

    // receive full response safely
    std::string result;
    char buffer[4096];

    while (true)
    {
        int recv_size = recv(
            db->sock,
            buffer,
            sizeof(buffer) - 1,
            0);

        if (recv_size <= 0)
            break;

        buffer[recv_size] = '\0';
        result += buffer;

        if (result.find(kResponseTerminator) != std::string::npos)
            break;
    }

    size_t terminatorPos = result.find(kResponseTerminator);
    if (terminatorPos == std::string::npos)
    {
        if (errmsg)
            *errmsg = _strdup("Incomplete response from server");
        return FLEXQL_ERROR;
    }

    result.erase(terminatorPos);

    if (result.empty())
        result = "OK\n";

    if (result.rfind("ERROR|", 0) == 0)
    {
        if (errmsg)
            *errmsg = _strdup(result.substr(6).c_str());
        return FLEXQL_ERROR;
    }

    // no callback means success only
    if (!callback)
        return FLEXQL_OK;

    std::stringstream ss(result);
    std::string row;

    while (std::getline(ss, row))
    {
        if (row.empty() || row == "OK")
            continue;

        std::vector<std::string> cols = split(row, '|');

        std::vector<char *> values;
        std::vector<char *> names;

        for (size_t i = 0; i < cols.size(); i++)
        {
            values.push_back(
                const_cast<char *>(cols[i].c_str()));

            std::string colName =
                "COL" + std::to_string(i);

            names.push_back(_strdup(colName.c_str()));
        }

        int rc = callback(
            arg,
            (int)values.size(),
            values.data(),
            names.data());

        for (char *p : names)
            free(p);

        // callback abort support
        if (rc == 1)
            break;
    }

    return FLEXQL_OK;
}

void flexql_free(void *ptr)
{
    if (ptr)
        free(ptr);
}
