#include <winsock2.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

namespace
{
const std::string kResponseTerminator = "__FLEXQL_END__\n";
const std::string kRequestTerminator = "__FLEXQL_SQL_END__\n";

bool sendAll(SOCKET sock, const char *data, int length)
{
    int totalSent = 0;
    while (totalSent < length)
    {
        int sent = send(sock, data + totalSent, length - totalSent, 0);
        if (sent <= 0)
            return false;
        totalSent += sent;
    }
    return true;
}
}

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;

    WSAStartup(MAKEWORD(2,2), &wsa);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(9000);

    connect(sock, (struct sockaddr *)&server, sizeof(server));

    std::cout << "Connected to server.\n";

    while (true) {
        std::string query = "";
        std::string line;

        std::cout << "FlexQL> ";

        // keep reading until query contains ';' anywhere
        while (std::getline(std::cin, line)) {
            query += line + "\n";

            // IMPORTANT: semicolon anywhere in full query
            if (query.find(';') != std::string::npos) {
                break;
            }

            std::cout << "       > ";
        }

        if (query.empty()) continue;

        if (query.find("exit;") != std::string::npos) break;

        std::string payload = query + kRequestTerminator;
        if (!sendAll(sock, payload.c_str(), static_cast<int>(payload.size()))) {
            std::cout << "Failed to send query.\n";
            break;
        }

        std::string response;
        char buffer[4096];

        while (true) {
            int recv_size = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (recv_size <= 0) {
                std::cout << "Disconnected from server.\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }

            buffer[recv_size] = '\0';
            response += buffer;

            if (response.find(kResponseTerminator) != std::string::npos) {
                break;
            }
        }

        size_t terminatorPos = response.find(kResponseTerminator);
        if (terminatorPos != std::string::npos) {
            response.erase(terminatorPos);
        }

        if (response.rfind("ERROR|", 0) == 0) {
            std::cout << "Error: " << response.substr(6);
        } else if (response == "OK\n" || response == "OK") {
            std::cout << "OK\n";
        } else {
            std::cout << response;
            if (!response.empty() && response.back() != '\n') {
                std::cout << "\n";
            }
        }
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}
