#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

constexpr int UDP_PORT = 6000;

int main()
{
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(UDP_PORT);

    if (bind(server_fd,
             reinterpret_cast<sockaddr*>(&server),
             sizeof(server)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    std::cout << "[UDP] Listening on port "
              << UDP_PORT << "\n";

    char buffer[1024]{};

    sockaddr_in client{};
    socklen_t client_len = sizeof(client);

    ssize_t bytes = recvfrom(
        server_fd,
        buffer,
        sizeof(buffer) - 1,
        0,
        reinterpret_cast<sockaddr*>(&client),
        &client_len);

    if (bytes < 0) {
        perror("recvfrom");
        close(server_fd);
        return 1;
    }

    buffer[bytes] = '\0';

    std::cout << "[UDP] Received: "
              << buffer << "\n";

    const std::string response = "UDP_ACK";

    sendto(
        server_fd,
        response.c_str(),
        response.size(),
        0,
        reinterpret_cast<sockaddr*>(&client),
        client_len);

    std::cout << "[UDP] ACK sent\n";

    close(server_fd);

    return 0;
}
