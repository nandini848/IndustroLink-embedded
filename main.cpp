#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

constexpr int PORT = 5000;
constexpr int BUFFER_SIZE = 1024;

std::atomic<bool> running{true};
std::mutex io_mutex;

void device_simulator(const std::string& device_name,
                      const std::string& sensor_type,
                      double base_value)
{
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) <= 0) {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cerr << "[DEVICE] Invalid controller address\n";
        return;
    }

    while (running) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);

        if (sock < 0) {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::cerr << "[DEVICE] socket() failed: "
                      << std::strerror(errno) << "\n";
            return;
        }

        if (connect(sock,
                    reinterpret_cast<sockaddr*>(&server),
                    sizeof(server)) == 0) {

            {
                std::lock_guard<std::mutex> lock(io_mutex);
                std::cout << "[DEVICE] " << device_name
                          << " connected to controller\n";
            }

            for (int i = 0; i < 5 && running; ++i) {
                double value = base_value + i * 0.5;

                std::string message =
                    device_name + "," +
                    sensor_type + "," +
                    std::to_string(value) + "\n";

                ssize_t sent = send(sock,
                                    message.c_str(),
                                    message.size(),
                                    0);

                if (sent < 0) {
                    std::lock_guard<std::mutex> lock(io_mutex);
                    std::cerr << "[DEVICE] send() failed: "
                              << std::strerror(errno) << "\n";
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(io_mutex);
                    std::cout << "[TELEMETRY] " << message;
                }

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(500));
            }

            close(sock);
            break;
        }

        close(sock);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(500));
    }
}

void controller()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cerr << "[CONTROLLER] socket() failed: "
                  << std::strerror(errno) << "\n";
        running = false;
        return;
    }

    int opt = 1;

    if (setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0) {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cerr << "[CONTROLLER] setsockopt() failed: "
                  << std::strerror(errno) << "\n";
        close(server_fd);
        running = false;
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd,
             reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) < 0) {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cerr << "[CONTROLLER] bind() failed: "
                  << std::strerror(errno) << "\n";
        close(server_fd);
        running = false;
        return;
    }

    if (listen(server_fd, 5) < 0) {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cerr << "[CONTROLLER] listen() failed: "
                  << std::strerror(errno) << "\n";
        close(server_fd);
        running = false;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cout << "[CONTROLLER] Listening on TCP port "
                  << PORT << "\n";
    }

    for (int device = 0; device < 2 && running; ++device) {
        int client = accept(server_fd, nullptr, nullptr);

        if (client < 0) {
            if (running) {
                std::lock_guard<std::mutex> lock(io_mutex);
                std::cerr << "[CONTROLLER] accept() failed: "
                          << std::strerror(errno) << "\n";
            }
            continue;
        }

        char buffer[BUFFER_SIZE]{};
        ssize_t bytes_received;

        while ((bytes_received = recv(client,
                                      buffer,
                                      sizeof(buffer) - 1,
                                      0)) > 0) {

            buffer[bytes_received] = '\0';

            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << "[CONTROLLER] Received: "
                      << buffer;
        }

        close(client);
    }

    close(server_fd);
    running = false;
}

int main()
{
    std::cout << "IndustroLink Controller-Device Framework\n";
    std::cout << "TCP/IP + Multithreading + Telemetry\n\n";

    running = true;

    std::thread controller_thread(controller);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(200));

    std::thread temperature_device(
        device_simulator,
        "TEMP-01",
        "temperature",
        25.0);

    std::thread pressure_device(
        device_simulator,
        "PRESS-01",
        "pressure",
        100.0);

    temperature_device.join();
    pressure_device.join();

    running = false;

    controller_thread.join();

    std::cout << "\nIndustroLink communication test completed.\n";

    return 0;
}
