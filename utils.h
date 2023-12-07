#ifndef UTILS_H
#define UTILS_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Constants
const char* SERVER_IP = "127.0.0.1";
const char* LOCAL_HOST = "127.0.0.1";
const int SERVER_PORT_TO = 5002;
const int CLIENT_PORT = 6001;
const int SERVER_PORT = 6002;
const int CLIENT_PORT_TO = 5001;
const int PAYLOAD_SIZE = 1024;
const int WINDOW_SIZE = 5;
const int TIMEOUT = 0;
const int MAX_SEQUENCE = 1024;

// Packet Layout
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char ack;
    char last;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Utility function to build a packet
void build_packet(packet* pkt, unsigned short seqnum, unsigned short acknum, char last, char ack, unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->ack = ack;
    pkt->last = last;
    pkt->length = length;
    std::memcpy(pkt->payload, payload, length);
}

// Utility function to print a packet
void printRecv(const packet* pkt) {
    std::printf("RECV %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
}

void printSend(const packet* pkt, int resend) {
    if (resend)
        std::printf("RESEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
    else
        std::printf("SEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
}

#endif // UTILS_H
