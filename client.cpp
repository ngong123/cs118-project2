#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <algorithm>
#include <vector>

#include "utils.h"

bool timed_out_helper(struct timespec *packet_send_time) {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    short elapsed_time_ms = (current_time.tv_sec - packet_send_time->tv_sec) * 1000;
    elapsed_time_ms += (current_time.tv_nsec - packet_send_time->tv_nsec) / 1000000;

    return elapsed_time_ms > TIMEOUT;
}


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
    FD_ZERO(&read_fds);
    FD_SET(listen_sockfd, &read_fds);
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 40000;

    // Initialize vectors and variables
    std::vector<packet> packetvec(MAX_SEQUENCE);
    std::vector<time_t> timeouts(MAX_SEQUENCE);
    struct timespec send_time[MAX_SEQUENCE];  // Replace MAX_SEQ_NUM with the actual size
    bool acked[MAX_SEQUENCE] = {false};
    int duplicateAcks[MAX_SEQUENCE] = {0};
    double timeoutSeconds = 1.0;
    int cwnd = 1;         // Congestion window
    int ssthresh = 20;    // slow start threshold
    int window_base = 0;  // Window base
    int seq_number = 0; // current seq number
    bool fast_recovery = false;

    while (!feof(fp))
    {
        printf("Main Loop Start: seq_num=%d, window_base=%d, cwnd=%d\n", seq_num, window_base, cwnd);
        // Only read new data if there is space in the window and it's not already sent
        while (!acked[seq_num] && seq_num < window_base + cwnd) {
            // FOR LOOP 1: SEND NEW PACKETS IN WINDOW VECTOR AND HANDLE ACKS
            unsigned i = 0;
            unsigned short curr_seq_num = window_base + i;
            size_t read_bytes = fread(buffer, 1, PAYLOAD_SIZE, fp);
            bool last_packet = (read_bytes < PAYLOAD_SIZE || feof(fp));
            build_packet(&packetvec[i], curr_seq_num, 0, last_packet, 0, read_bytes, buffer);

            // Send the packet - first time transmission
            sendto(send_sockfd, &packetvec[i], sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
            clock_gettime(CLOCK_MONOTONIC, &send_time[curr_seq_num]);
            printSend(&packetvec[i], 0); 

            // wait for ACK
            FD_SET(listen_sockfd, &read_fds);
            tv.tv_sec = TIMEOUT; 
            tv.tv_usec = 40000;
    
                // Use select to check for incoming data
            if (select(listen_sockfd + 1, &read_fds, NULL, NULL, &tv) > 0) {
                ssize_t recv_len = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size);

                if (recv_len > 0 && ack_pkt.ack && ack_pkt.acknum == curr_seq_num) {
                    acked[curr_seq_num] = true;
                    window_base = ack_pkt.acknum + 1;
                    if (ssthresh >= cwnd) { cwnd += 1.0; } // Slow start
                    else { cwnd += 1.0 / cwnd; } // Congestion avoidance
                }
            }
            // no data received
            else {
                acked[curr_seq_num] = false;
                duplicateAcks[curr_seq_num]++; // increment duplicate ACKs array
            }

            // Update sequence number
            seq_num = (seq_num + 1) % MAX_SEQUENCE;
            i = (i + 1) % MAX_SEQUENCE;
                    
            }
        
            // --Retransmit timed out packets --
        // FOR LOOP 2: CHECK TIMEOUTS; KEEP TIMEOUT COUNT; RETRASMIT timed out packets
        for (unsigned short i = 0; i+window_base < WINDOW_SIZE - seq_num; i++)
        {
            seq_num = window_base + i;
            if (!acked[seq_num] && timed_out_helper(&send_time[seq_num]))  {
                if (duplicateAcks[seq_num] < 3) {
                    clock_gettime(CLOCK_MONOTONIC, &send_time[seq_num]);
                    sendto(send_sockfd, &packetvec[i], sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
                    printSend(&packetvec[i], 1); // Resend
                    fast_recovery = true;
                }
                if (fast_recovery) {
                    cwnd = ((int)cwnd / 2 > 2) ? cwnd / 2 : 2;
                    seq_num = window_base + cwnd - 1;
                    fast_recovery = false;
                }

            }
        }
        // FAST RETRANSMIT - 3 dup ACK
        if (duplicateAcks[seq_num]== 3) {
            ssthresh = std::max(cwnd/2, 2);
            cwnd = ssthresh + 3;
            sendto(send_sockfd, &packetvec[seq_num], sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
        }
        // FAST RECOVERY transition phase - upon each additioanl (4th, 5th) duplicate ACK
        else if (duplicateAcks[seq_num] > 3) {
            cwnd += 1;
        }
    }




        // // Retransmit timed-out packets
        // if (!acked[seq_num])
        // for (int i = window_base; i < next_seq_num; ++i) {
        //     struct timespec current_time;
        //     clock_gettime(CLOCK_MONOTONIC, &current_time);
        //     if ((current_time.tv_sec - timeoutvec[i].tv_sec) > timeoutSeconds) {
        //         // Retransmission logic
        //         printSend(&packetvec[i], 1); // Resend
        //         clock_gettime(CLOCK_MONOTONIC, &timeoutvec[i]); // Update timeout
        //     }
        // }

        // FOR LOOP 2: SEND NEW PACKETS IN WINDOW VECTOR
        // for (unsigned short i = 0; i < WINDOW_SIZE - packetvec.size(); i++) {
        //     if (!acked[i] && (i == seq_num)) {
        //         // logic for sending new packets
        //         printSend(&packetvec[i], 0);  // Indicate firsttime transmission
        //     }
        //     // handle ACKs
        //     if (select(listen_sockfd + 1, &read_fds, NULL, NULL, &tv) > 0) {
        //         if (ack_pkt.ack && ack_pkt.acknum == seq_num) {
        //             acked[i] = true;
        //         }
        //     } else {
        //         retry_count++;
        //     }
        // }
        // seq_num = (seq_num + 1) % MAX_SEQUENCE;
    // }



        // while (next_seq_num < window_base + cwnd && next_seq_num < MAX_SEQUENCE)
        // {
        //     size_t read_bytes = fread(buffer, 1, PAYLOAD_SIZE, fp);
        //     if (read_bytes == 0)
        //         break;
        //     bool last_packet = (read_bytes < PAYLOAD_SIZE || feof(fp));
        //     build_packet(&packetvec[next_seq_num], next_seq_num, 0, last_packet, 0, read_bytes, buffer);
        //     printSend(&packetvec[next_seq_num], 0); // First time send
        //     time_t timeoutvec[next_seq_num];
        //     next_seq_num++;
        // }

    //     // Handle ACKs
    //     for (unsigned short i = 0; i < WINDOW_SIZE - packetvec.size(); i++) {
    //         if (select(listen_sockfd + 1, &read_fds, NULL, NULL, &tv) > 0)
    //         {
    //             if (ack_pkt.ack && ack_pkt.acknum >= window_base)
    //             {
    //                 window_base = ack_pkt.acknum + 1;
    //                 // update cwnd here
    //             }
    //         }
    //         seq_num = (seq_num + 1) % MAX_SEQUENCE;
    //     }
    // }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;

    
}

