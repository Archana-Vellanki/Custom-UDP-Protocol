#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#define START_OF_PACKET 0xFFFF
#define END_OF_PACKET 0xFFFF
#define CLIENT_ID 0xFF
#define TIMEOUT 3000
#define PORT 35556

#define DATA 0XFFF1
#define ACK 0XFFF2
#define REJECT 0XFFF3

#define OUT_OF_SEQUENCE 0XFFF4
#define LENGTH_MISMATCH 0XFFF5
#define END_OF_PACKET_MISSING 0XFFF6
#define DUPLICATE_PACKET 0XFFF7

typedef struct
{
    short start;
    char client_id;
    char length;
    short data;
    char segment_no;
    char payload[255];
    short end;
} Packet;

typedef struct
{
    short start;
    char client_id;
    short type;
    char recvd_segment_no;
    short reject_sub_code;
    short end;
} ResponsePacket;

int main()
{
    // socket setup
    int server_socket, current_segment_number;
    int prev_segment_number = -1;
    struct sockaddr_in server_address, client_address;
    memset((char *)&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    socklen_t client_address_len = sizeof(client_address);

    Packet received_packet;
    ResponsePacket response;

    if ((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Error when creating socket");
        exit(EXIT_FAILURE);
    }

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        // binding the socket to the server
        perror("Error binding the socket");
        exit(EXIT_FAILURE);
    }

    printf("Server socket binded successfully..\n");

    while (1)
    {
        // continuously listening as what usually server does
        ssize_t bytes_received = recvfrom(server_socket, &received_packet, sizeof(Packet), 0,
                                          (struct sockaddr *)&client_address, &client_address_len);

        if (bytes_received == -1)
        {
            perror("Error receiving packet");
            exit(EXIT_FAILURE);
        }

        response.start = START_OF_PACKET;
        response.end = END_OF_PACKET;
        response.client_id = CLIENT_ID;
        response.recvd_segment_no = received_packet.segment_no;
        current_segment_number = received_packet.segment_no;

        // Handling error messages for each scenario
        if (prev_segment_number == current_segment_number)
        {
            response.type = REJECT;
            response.reject_sub_code = DUPLICATE_PACKET;
            printf("Duplicate packets error. Receieved segment no %d again \n", current_segment_number);
            // printf("Reject_sub_code in packet:%hu\nThe definition of subcode:%hu\n", response.reject_sub_code, DUPLICATE_PACKET);
            prev_segment_number = current_segment_number;
        }
        else if ((prev_segment_number + 1) != current_segment_number)
        {
            response.type = REJECT;
            response.reject_sub_code = OUT_OF_SEQUENCE;
            printf("Out of sequence packet error. Received segment no %d instead of %d\n", current_segment_number, prev_segment_number + 1);
            // printf("Reject_sub_code in packet:%hu\nThe definition of subcode:%hu\n", response.reject_sub_code, OUT_OF_SEQUENCE);
        }
        else if (received_packet.end != (short)END_OF_PACKET)
        {
            response.type = REJECT;
            response.reject_sub_code = END_OF_PACKET_MISSING;
            printf("Received Packet %d has incorrect end packet ID \n", current_segment_number);
            // printf("Reject_sub_code in packet:%hu\nThe definition of subcode:%hu\n", response.reject_sub_code, END_OF_PACKET_MISSING);
            prev_segment_number = current_segment_number;
        }
        else if (received_packet.length != (char)sizeof(received_packet.payload))
        {
            // error in length from frame length data and payload size
            response.type = REJECT;
            response.reject_sub_code = LENGTH_MISMATCH;
            printf("Received Packet %d has a length mismatch\n", current_segment_number);
            // printf("Reject_sub_code in packet:%hu\nThe definition of subcode:%hu\n", response.reject_sub_code, LENGTH_MISMATCH);
            prev_segment_number = current_segment_number;
        }
        else
        {
            // handling ACK
            response.type = ACK;
            response.reject_sub_code = 0;
            printf("Acknowledgement sent for packet %d\n", current_segment_number);
            prev_segment_number = current_segment_number;
        }
        // printf("Reject_sub_code:%hu in the server\nThe definition of subcodes:%hu\n", response.reject_sub_code, END_OF_PACKET);
        printf("-------------------------------------------------\n\n");
        sendto(server_socket, &response, sizeof(response), 0, (struct sockaddr *)&client_address, client_address_len);
    }
}
