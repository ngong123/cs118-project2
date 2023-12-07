#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    timeval tv;
    packet pkt;
    packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;

    // read filename from command line argument
    if (argc != 2) {
        std::printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    fd_set read_fds;
    bool ack_received;
    int retry_count;
    
    while (!feof(fp)) {
        size_t read_bytes = fread(buffer, 1, PAYLOAD_SIZE, fp);
        bool last_packet = (read_bytes < PAYLOAD_SIZE || feof(fp));
        build_packet(&pkt, seq_num, 0, last_packet, 0, read_bytes, buffer);

        ack_received = false;
        retry_count = 0;
        while (!ack_received && retry_count < 100) {
            sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
            printSend(&pkt, 0);
            FD_ZERO(&read_fds);
            FD_SET(listen_sockfd, &read_fds);
            tv.tv_sec = TIMEOUT;
            tv.tv_usec = 0;

            if (select(listen_sockfd + 1, &read_fds, NULL, NULL, &tv) > 0) {
                recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size);
                if (ack_pkt.ack && ack_pkt.acknum == seq_num) {
                    ack_received = true;
                }
            } else {
                retry_count++;
            }
        }

        if (!ack_received) {
            std::printf("Failed to receive ACK for packet %d\n", seq_num);
            break; // Exit if ACK not received after MAX_RETRIES
        }

        seq_num = (seq_num + 1) % MAX_SEQUENCE;
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

