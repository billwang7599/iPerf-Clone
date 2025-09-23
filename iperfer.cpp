#include <cmath>
#include <iostream>
#include <string>
#include <unistd.h>      // For close
#include <netdb.h>
#include <chrono>
#include <cstring>

std::string end_message = "FIN";
const int CHUNK_SIZE_BYTES = 1000;

int send_all(int sockfd, const char *data, size_t len) {
    // send data
    size_t total_sent = 0;
    while (total_sent < len) {
        int bytes_sent = send(sockfd, data + total_sent, len - total_sent, 0);
        if (bytes_sent == -1) {
            std::cerr << "send_all failed" << std::endl;
            return -1; // Error
        }
        total_sent += bytes_sent;
    }
    return 0; // Success
}

int server(std::string port) {
    int port_number = std::stoi(port);
    struct addrinfo hints, *res;
    int sockfd, newfd;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;


    if (port_number < 1024 || port_number > 65535) {
        std::cerr << "Error: port number must be in the range of [1024, 65535]" << std::endl;
        return 1; // Indicates failure
    }

    // get address information
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // defaults to IPv4
    hints.ai_socktype = SOCK_STREAM; // defaults to TCP
    hints.ai_flags = AI_PASSIVE; // defaults to own machine

    if (getaddrinfo(nullptr, port.c_str(), &hints, &res) != 0) {
        std::cerr << "Error getting address info" << std::endl;
        return -1;
    }

    // get socket information
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    // binds socket to address
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("Error binding socket");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    if (listen(sockfd, 10) == -1) {
        perror("Error listening on socket");
        close(sockfd);
        return -1;
    }

    addr_size = sizeof(their_addr);

    char buffer[CHUNK_SIZE_BYTES];
    long long total_bytes_received = 0; // Use a larger type for a potentially large total
    int bytes_in_this_chunk;

    newfd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size); // blocking function, waits for next connection
    if (newfd == -1) {
        std::cerr << "Connection error" << std::endl;
    }


    // Loop until the connection is closed (recv returns 0) or an error occurs (returns -1)
    std::string data_stream;

    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    while ((bytes_in_this_chunk = recv(newfd, buffer, CHUNK_SIZE_BYTES, 0)) > 0) {

        // 1. Add the number of bytes from this specific call to the running total.
        total_bytes_received += bytes_in_this_chunk;
        data_stream.append(buffer, bytes_in_this_chunk);
        size_t fin_pos = data_stream.find(end_message);
        if (fin_pos != std::string::npos) {
            // std::cout << "'FIN' detected in data stream." << std::endl;
            break; // Exit the loop!
        }

    }
    std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> duration_s = end_time - start_time;


    std::string message = "'FIN' received, shutting down connection";
    if (send_all(newfd, message.c_str(), message.size()) == -1) {
        std::cerr << "send_all from server failed" << std::endl;
        return -1; // Error
    }

    if (shutdown(newfd, SHUT_WR) == -1) { // shutting down connection
        std::cerr << "shutdown failed" << std::endl;
    }

    if (duration_s.count() != 0) {
        double kb_received = round(total_bytes_received * 10 / 1024.0)/10;
        double mbps = round(kb_received * 80 / 1000 / duration_s.count()) / 10;
        std::cout << "Received=" << kb_received << " KB, Rate=" << mbps << " Mbps" << std::endl;
    }
    close(newfd);

    close(sockfd);
    return 0;
}

int client(char* hostname, char* port, int time_s) {
    if (std::stoi(port) < 1024 || std::stoi(port) > 65535) {
        std::cerr << "Error: port number must be in the range of [1024, 65535]" << std::endl;
        return 1; // Indicates failure
    }

    if (time_s < 1) {
        std::cerr << "Error: time must be greater than 0" << std::endl;
        return 1; // Indicates failure
    }

    struct addrinfo hints, *res;
    int sockfd;

    // get address information
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // defaults to IPv4
    hints.ai_socktype = SOCK_STREAM; // defaults to TCP

    if (getaddrinfo(hostname, port, &hints, &res) != 0) {
        std::cerr << "Error getting address info" << std::endl;
        return -1;
    }

    // get socket information
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("Error connecting to server");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    // send data to server
    char package[CHUNK_SIZE_BYTES];
    memset(package, '0', CHUNK_SIZE_BYTES);

    std::chrono::duration<double> duration_s;
    long int bytes_sent = 0;
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    while (true) {
        if (duration_s.count() >= time_s) {
            break;
        }

        int flag = send_all(sockfd, package, CHUNK_SIZE_BYTES);
        if (flag == -1) {
            std::cerr << "Failed to send data chunk." << std::endl;
            close(sockfd);
            freeaddrinfo(res);
            return -1;
        } else if (flag == -2) {
            std::cerr << "Socket buffer is full." << std::endl;
        }
        bytes_sent += CHUNK_SIZE_BYTES;
        std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
        duration_s = end_time - start_time;
    }

    // send end message
    if (send_all(sockfd, end_message.c_str(), end_message.size()) == -1) {
        std::cerr << "Failed to send end message." << std::endl;
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    // receive end message
    char received_buffer[CHUNK_SIZE_BYTES];
    int bytes_received = recv(sockfd, received_buffer, sizeof(received_buffer), 0);
    if (bytes_received == -1) {
        std::cerr << "Error receiving end message." << std::endl;
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }
    std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
    // calculate new RTT rate given new time
    duration_s = end_time - start_time;

    close(sockfd);
    freeaddrinfo(res);

    double kb_sent = round(10 * bytes_sent / 1024.0) / 10;
    double rate_mbps = round(kb_sent * 10 / duration_s.count() / 1000 * 8) / 10;
    std::cout << "Sent=" << kb_sent << " KB, ";
    std::cout << "Rate= " << rate_mbps << " Mbps" << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    std::string error = "Error: missing or extra arguments";
    // --- Server Mode Check ---
    if (argc == 4) {
        // Expected format: ./iPerfer -s -p <port>
        if (std::string(argv[1]) == "-s" && std::string(argv[2]) == "-p") {
            return server(argv[3]);
        } else {
            std::cerr << error << std::endl;
            return 1;
        }
    }
    // --- Client Mode Check ---
    else if (argc == 8) {
        // Expected format: ./iPerfer -c -h <host> -p <port> -t <time>
        if (std::string(argv[1]) == "-c" &&
            std::string(argv[2]) == "-h" &&
            std::string(argv[4]) == "-p" &&
            std::string(argv[6]) == "-t") {

            try {
                // Safely convert time argument to integer
                return client(argv[3], argv[5], std::stoi(argv[7]));
            } catch (const std::exception& e) {
                std::cerr << error << std::endl;
                return 1;
            }
        } else {
            std::cerr << error << std::endl;
            return 1;
        }
    }
    // --- Incorrect Number of Arguments ---
    else {
        std::cerr << error << std::endl;
        return 1;
    }

    return 0; // Should not be reached
}
