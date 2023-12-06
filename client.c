#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"

// test client 

// segment file into packets
struct packet {
    unsigned short seq_num;
    unsigned short ack_num;
    char last;
    char data[PAYLOAD_SIZE];
};

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
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

    // read file loop
    while (!feof(fp)) {
        size_t bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
        if (bytes_read < 0) {
            perror("Error reading file");
            break;
        }

    memset(&pkt, 0, sizeof(pkt)); // sets all of the bytes in pkt to 0
    pkt.seqnum = htons(seq_num) // unsigned seq_num int into network byte
    pkt.last = (feof(fp)) ? 1 : 0; // if end of file, set pkt.last = 1 else 0
    memcpy(pkt.data, buffer, bytes_read);

    // send packet
    if (sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size) < 0) {
    perror("Error sending packet");
    break;
    }   

    // handle ACK from server
    retry_count = 0
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(listen_sockfd, &read_fds);
        tv.tv_sec = 2;  // timeout
        tv.tv_usec = 0;
    }

    retval = select(listen_sockfd + 1, &read_fds, NULL, NULL, &tv);
        if (retval == -1) {
            perror("select()");
            break;
        } else if (retval) {
            if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size) > 0) {
                if (ntohs(ack_pkt.ack_num) == seq_num) {
                    // ACK received
                    // ntohs translates unsigned int to host byte. if host byte == network byte
                    break;
                }
            }
        } else {
            // Timeout: no data within specified time
            if (++retry_count > 3) { // retry limit
                printf("No ack for seq %d, stopping\n", seq_num);
                fclose(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            }
            printf("Timeout, resending packet seq %d\n", seq_num);
            if (sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size) < 0) {
                perror("Error resending packet");
                break;
            }
        }
    }
    seq_num++;
}
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

