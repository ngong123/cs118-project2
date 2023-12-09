#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <algorithm>

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
    
    // nathan: SR and AIMD variables
    const int MAX_CWND = 1024;
    int cwnd = 1; // INITIAL CWND WINDOW SIZE INITIALIZED TO 1. 
    int ssthresh = 64; // initial cwnd, initial slow start threshold
    bool acked[MAX_SEQUENCE] = {false};
    struct timespec send_time[MAX_SEQUENCE];
    struct timespec current_time;
    int window_base = 0;
    bool slowStart = true;

    bool retransmit[MAX_SEQUENCE] = {false};
    int ack_count[MAX_SEQUENCE] = {0};

    // nathan: init cwnd, send packets in cwnd
    // while (!feof(fp) || window_base < seq_num) {
    //     // send packets in the congestion window
    //     for (int i = window_base; i < window_base + cwnd && i < seq_num + MAX_SEQUENCE; i++) {
    //         int wrapped_i = i % MAX_SEQUENCE;
    //         if (!acked[wrapped_i]) {
    //             if (retransmit[wrapped_i]) {  // Retransmission logic
    //                 // Logic to resend the packet at position wrapped_i
    //                 // Ensure that the packet data is still valid for retransmission
    //                 // ...
    //                 printSend(&pkt, 1);  // Indicate retransmission
    //                 retransmit[wrapped_i] = false;  // Reset retransmit flag
    //             } else if (i < seq_num) {
    //                 // Do nothing, already sent and waiting for ACK
    //             } else {
    //                 size_t read_bytes = fread(buffer, 1, PAYLOAD_SIZE, fp);
    //                 bool last_packet = (read_bytes < PAYLOAD_SIZE || feof(fp));
    //                 build_packet(&pkt, wrapped_i, 0, last_packet, 0, read_bytes, buffer);
    //                 sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
    //                 printSend(&pkt, 0);
    //                 clock_gettime(CLOCK_MONOTONIC, &send_time[wrapped_i]);
    //                 acked[wrapped_i] = false;
    //                 seq_num = (seq_num + 1) % MAX_SEQUENCE;  // Wrap around sequence number
    //             }
    //         }
    //     }

    while (!feof(fp)) {
    // send packets in the congestion window
    for (int i = window_base; i != (window_base + cwnd) % MAX_SEQUENCE; i = (i + 1) % MAX_SEQUENCE) {
        int wrapped_i = i % MAX_SEQUENCE;
        if (!acked[wrapped_i]) {
            if (retransmit[wrapped_i]) {  // Retransmission logic
                // Resend the packet
                fseek(fp, wrapped_i * PAYLOAD_SIZE, SEEK_SET); // Move the file pointer to the start of the missed packet
                size_t read_bytes = fread(buffer, 1, PAYLOAD_SIZE, fp); // Read the packet data again
                bool last_packet = (read_bytes < PAYLOAD_SIZE || feof(fp));
                build_packet(&pkt, wrapped_i, 0, last_packet, 0, read_bytes, buffer);
                sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
                printSend(&pkt, 1);  // Indicate retransmission
                clock_gettime(CLOCK_MONOTONIC, &send_time[wrapped_i]); // Reset the timer for this packet

                retransmit[wrapped_i] = false;  // Reset retransmit flag
            } else if (i != seq_num) {
                // Do nothing, already sent and waiting for ACK
            } else {
                size_t read_bytes = fread(buffer, 1, PAYLOAD_SIZE, fp);
                bool last_packet = (read_bytes < PAYLOAD_SIZE || feof(fp));
                build_packet(&pkt, wrapped_i, 0, last_packet, 0, read_bytes, buffer);
                sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
                printSend(&pkt, 0);
                clock_gettime(CLOCK_MONOTONIC, &send_time[wrapped_i]);
                acked[wrapped_i] = false;
                seq_num = (seq_num + 1) % MAX_SEQUENCE;  // Wrap around sequence number
            }
        }
    }

        // nathan: check for ACKs
        FD_ZERO(&read_fds);
        FD_SET(listen_sockfd, &read_fds);
        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 40000;

        // nathan: retransmission logic of lost packets
        if (select(listen_sockfd + 1, &read_fds, NULL, NULL, &tv) > 0) {
            recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size);
            int ack_index = ack_pkt.acknum % MAX_SEQUENCE;
            if (ack_pkt.ack && ack_index >= window_base && ack_index < MAX_SEQUENCE) {
                ack_count[ack_index]++;
                if (ack_count[ack_index] == 1) { // First ACK for this packet
                    acked[ack_index] = true;
                    if (ack_index == window_base) {
                        while (window_base < MAX_SEQUENCE && acked[window_base]) { // Update window base
                            window_base = (window_base + 1) % MAX_SEQUENCE;
                        }
                    }
                // maintain ack_count. When three duplicate ACKs are received, trigger fast retransmit, and reset sstresh and cwnd
                } else if (ack_count[ack_index] == 3) { // 3 duplicate ACKs received
                    retransmit[ack_index] = true;
                    // FAST RECOVERY Adjust cwnd and ssthresh for fast recovery
                    ssthresh = std::max(1, cwnd / 2);
                    cwnd = ssthresh + 3; // inflate the window by the number of duplicate ACKs
                }
            }
        }


        // nathan: timeout logic check for timeouts
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        for (int i = window_base; i != seq_num; i = (i + 1) % MAX_SEQUENCE) {
            int wrapped_i = i % MAX_SEQUENCE;
            if (!acked[wrapped_i]) {
                double time_diff = (current_time.tv_sec - send_time[wrapped_i].tv_sec) * 1000.0;
                time_diff += (current_time.tv_nsec - send_time[wrapped_i].tv_nsec) / 1000000.0;
                if (time_diff > TIMEOUT) {
                    // mark retransmission and adjust cwnd and ssthresh
                    retransmit[wrapped_i] = true;
                    ssthresh = std::max(1, cwnd / 2);
                    cwnd = 1;
                    slowStart = true;

                    // Resend the packet
                    fseek(fp, wrapped_i * PAYLOAD_SIZE, SEEK_SET); // Move the file pointer to the start of the missed packet
                    size_t read_bytes = fread(buffer, 1, PAYLOAD_SIZE, fp); // Read the packet data again
                    bool last_packet = (read_bytes < PAYLOAD_SIZE || feof(fp));
                    build_packet(&pkt, wrapped_i, 0, last_packet, 0, read_bytes, buffer);
                    sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
                    printSend(&pkt, 1);  // Indicate retransmission
                    clock_gettime(CLOCK_MONOTONIC, &send_time[wrapped_i]); // Reset the timer for this packet

                    retransmit[wrapped_i] = false;  // Reset retransmit flag after retransmission
                    break;  // Break after handling the first timeout
                }
            }
        }


        // nathan: implementing AIMD congestion control logic
        if (window_base == seq_num) {  // all packets in the window have been ACKed
            if (slowStart) {
                cwnd = std::min(cwnd * 2, ssthresh);
                if (cwnd >= ssthresh) {
                    slowStart = false;
                }
            } else {
                // Congestion Avoidance
                cwnd = std::min(cwnd + 1, MAX_CWND);
            }
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}