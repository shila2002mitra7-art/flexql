#include <winsock2.h>
#include <iostream>
#include <string>
#include <thread>
#include <cctype>

#include "parser.h"
#include "storage.h"

#pragma comment(lib, "ws2_32.lib")

Database db;

namespace
{
const std::string kResponseTerminator = "__FLEXQL_END__\n";
const std::string kRequestTerminator = "__FLEXQL_SQL_END__\n";
}

// each client gets its own thread
void handleClient(SOCKET client_socket)
{
    char buffer[4097];
    std::string pendingQuery;

    std::cout << "Client connected!\n";

    while (true)
    {
        int recv_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (recv_size <= 0)
            break;

        buffer[recv_size] = '\0';
        pendingQuery += buffer;

        size_t terminatorPos = std::string::npos;
        while ((terminatorPos = pendingQuery.find(kRequestTerminator)) != std::string::npos)
        {
            std::string query = trim(pendingQuery.substr(0, terminatorPos));
            pendingQuery.erase(0, terminatorPos + kRequestTerminator.size());

            if (query.empty())
                continue;

            QueryType type = parseQueryType(query);
            std::string table = extractTableName(query);
            std::string response;
            std::string error;
            bool success = false;

            // CREATE TABLE
            if (type == CREATE)
            {
                auto columns = extractColumns(query);
                auto types = extractTypes(query);

                success = db.createTable(table, columns, types, error);
            }

            // INSERT
            else if (type == INSERT)
            {
                auto batches = extractValueBatches(query);
                success = db.insertRows(table, batches, error);
            }

            // JOIN
            else if (type == JOIN)
            {
                std::string tableA = extractTableName(query);
                std::string tableB = extractJoinTable(query);

                std::string colA = extractJoinLeftColumn(query);
                std::string colB = extractJoinRightColumn(query);
                auto selectedCols = extractSelectColumns(query);
                std::string whereCol;
                std::string whereOp;
                std::string whereValue;

                if (hasWhere(query))
                {
                    whereCol = extractWhereColumnName(query);
                    whereOp = extractWhereOperator(query);
                    whereValue = extractWhereValue(query);
                }

                success = db.innerJoin(
                    tableA,
                    tableB,
                    colA,
                    colB,
                    selectedCols,
                    whereCol,
                    whereOp,
                    whereValue,
                    response,
                    error);
            }

            // SELECT
            else if (type == SELECT)
            {
                auto selectedCols = extractSelectColumns(query);

                // SELECT *
                if (selectedCols.size() == 1 && selectedCols[0] == "*")
                {
                    if (hasWhere(query))
                    {
                        std::string whereCol = extractWhereColumnName(query);
                        std::string val = extractWhereValue(query);

                        if (!whereCol.empty() && isdigit(whereCol[0]))
                        {
                            int col = std::stoi(whereCol);
                            success = db.selectWhere(table, col, val, response, error);
                        }
                        else
                        {
                            success = db.selectWhereColumn(table, whereCol, val, response, error);
                        }
                    }
                    else
                    {
                        success = db.selectAll(table, response, error);
                    }
                }

                // SELECT columns
                else
                {
                    if (hasWhere(query))
                    {
                        std::string whereCol = extractWhereColumnName(query);
                        std::string val = extractWhereValue(query);

                        std::string op = extractWhereOperator(query);

                        success = db.selectColumnsWhereColumn(
                            table,
                            selectedCols,
                            whereCol,
                            op,
                            val,
                            response,
                            error);
                    }
                    else
                    {
                        success = db.selectColumns(table, selectedCols, response, error);
                    }
                }
            }

            else
            {
                error = "Unknown query";
            }

            std::string payload;
            if (!success)
            {
                payload = "ERROR|" + error + "\n";
            }
            else if (response.empty())
            {
                payload = "OK\n";
            }
            else
            {
                payload = response;
            }

            payload += kResponseTerminator;
            send(client_socket, payload.c_str(), static_cast<int>(payload.size()), 0);
        }
    }

    closesocket(client_socket);
    std::cout << "Client disconnected\n";
}

int main()
{
    WSADATA wsa;
    SOCKET server_socket;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);

    WSAStartup(MAKEWORD(2, 2), &wsa);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(9000);

    bind(server_socket, (struct sockaddr *)&server, sizeof(server));

    listen(server_socket, 10);
    std::cout << "Server started on port 9000...\n";

    // accept clients forever
    while (true)
    {
        SOCKET client_socket = accept(
            server_socket,
            (struct sockaddr *)&client,
            &c);

        std::thread clientThread(handleClient, client_socket);
        clientThread.detach();
    }

    closesocket(server_socket);
    WSACleanup();

    return 0;
}
