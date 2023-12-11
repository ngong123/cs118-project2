#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    sockaddr_in server_addr, client_addr_from, client_addr_to;
    packet buffer[MAX_SEQUENCE];
    socklen_t addr_size = sizeof(client_addr_from);
    unsigned short expected_seq_num = 0;
    packet recv_pkt, ack_pkt;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); 
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int optval = 1;
    setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");
    if (fp == NULL) {
        perror("Error opening output file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Receive file from the client and save it as output.txt

    // nathan: SR logic. MAX_SEQUENCE = 2048 which is the max size of the sliding window. 
    // nathan: cwnd & buffer variable init - for SR 
    bool received[MAX_SEQUENCE] = {false};
    while (true) {
        ssize_t recv_len = recvfrom(listen_sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr *)&client_addr_from, &addr_size);
        if (recv_len > 0) {
            if (!recv_pkt.ack) { // ignore packets that are not data packets
                int wrapped_seqnum = recv_pkt.seqnum % MAX_SEQUENCE;
                printf("Received Packet: seqnum=%d, wrapped_seqnum=%d\n", recv_pkt.seqnum, wrapped_seqnum);
                if (!received[wrapped_seqnum]) { // Ignore duplicate packets
                    buffer[wrapped_seqnum] = recv_pkt;
                    received[wrapped_seqnum] = true;
                    printRecv(&recv_pkt);
                }

                // Sending ACK for every packet received regardless if it's a duplicate
                build_packet(&ack_pkt, 0, recv_pkt.seqnum, 0, 1, 0, NULL);
                sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size);
                printf("Sent ACK: acknum=%d\n", recv_pkt.seqnum);
                
                // Process and write in order packets
                while (received[expected_seq_num]) {
                    fwrite(buffer[expected_seq_num].payload, 1, buffer[expected_seq_num].length, fp);
                    fflush(fp);
                    printf("Processed Packet: expected_seq_num=%d\n", expected_seq_num);
                    received[expected_seq_num] = false;
                    if (buffer[expected_seq_num].last) {
                        // last packet received, clean up and exit
                        fclose(fp);
                        close(listen_sockfd);
                        close(send_sockfd);
                        return 0;
                    }
                    expected_seq_num = (expected_seq_num + 1) % MAX_SEQUENCE;
                }
            }
        } else if (recv_len == 0) {
            // close connection
            break;
        } else {
            // An error occurred
            perror("recvfrom failed");
            break;
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}