#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#define START_OF_PACKET 0xFFFF
#define END_OF_PACKET 0xFFFF
#define CLIENT_ID 0xFF
#define MAX_RETRIES 3
#define TIMEOUT 3000

#define DATA 0XFFF1
#define ACK 0XFFF2
#define REJECT 0XFFF3

// Reject Subcodes
#define OUT_OF_SEQUENCE 0XFFF4
#define LENGTH_MISMATCH 0XFFF5
#define END_OF_PACKET_MISSING 0XFFF6
#define DUPLICATE_PACKET 0XFFF7

#define PORT 35556

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
    struct sockaddr_in client_address, server_address;
    int server_address_len = sizeof(server_address);
    int client_socket, packet_no = 0, attempt_no = 1, poll_result;
    char payload[255];
    char *server = "127.0.0.1";

    if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    memset((char *)&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // poll for ack_timer
    struct pollfd pfdsock;
    pfdsock.fd = client_socket;
    pfdsock.events = POLLIN | POLLERR | POLLHUP;

    // different test scenarios
    Packet data_packets[10];
    for (int i = 0; i < 10; i++)
    {
        data_packets[i].client_id = CLIENT_ID;
        data_packets[i].start = START_OF_PACKET;
        data_packets[i].end = END_OF_PACKET;
        data_packets[i].data = DATA;
        data_packets[i].segment_no = i;
        strncpy(data_packets[i].payload, payload, 255);
        data_packets[i].length = sizeof(data_packets[i].payload);
        if (i == 6)
        {
            data_packets[i].length = 0x03;
        }
        if (i == 7)
        {
            data_packets[i].end = 0xF00F;
        }
        if (i == 8)
        {
            data_packets[i] = data_packets[i - 1];
        }
        if (i == 9)
        {
            data_packets[i].segment_no = 0;
        }
    }

    ResponsePacket response;
    Packet packet;

    while (packet_no < 10)
    {
        // sending packet to server
        packet = data_packets[packet_no];
        printf("\n\nSending Packet No %d\n", packet.segment_no);
        // if (packet_no == 6)
        // {
        //     packet.length = 0X03;
        // }
        // else if (packet_no == 7)
        // {
        //     packet.end = 0XF00F;
        // }
        // else if (packet_no == 8)
        // {
        //     packet = data_packets[packet_no - 1];
        //         }
        // else if (packet_no == 9)
        // {
        //     packet.segment_no = 6;
        // }

        if (sendto(client_socket, &packet, sizeof(Packet), 0, (struct sockaddr *)&server_address, server_address_len) == -1)
        {
            perror("Error sending packets");
            exit(EXIT_FAILURE);
        }

        // loop to resend packets if failed.
        attempt_no = 1;
        while (attempt_no < 4)
        {

            poll_result = poll(&pfdsock, 1, 3000);

            if (poll_result == 0)
            {
                // timeout due to no response from the server
                printf("No response received from Server..Trying again..Retry %d out of 3\n", attempt_no);
                if (sendto(client_socket, &packet, sizeof(Packet), 0, (struct sockaddr *)&server_address, server_address_len) == -1)
                {
                    perror("Timeout error:\n");
                    exit(1);
                }
                attempt_no++;
            }
            else if (poll_result < 0)
            {
                // error
                perror("Poll error");
                return -1;
            }
            else
            {
                // response recieved
                int n = recvfrom(client_socket, &response, sizeof(ResponsePacket), 0, (struct sockaddr *)&server_address, &server_address_len);

                if (n < 0)
                {
                    perror("Client: Receive error:\n");
                    exit(1);
                }

                if (response.type == (short)ACK)
                {
                    // Displaying ACK message
                    printf(" ACK received for Packet %d\n", packet_no);
                    break;
                }
                else
                {
                    printf("\nReject Subcode: %hu\n", response.reject_sub_code);
                    // displaying error messages correspondingly
                    if (response.reject_sub_code == (short)OUT_OF_SEQUENCE)
                    {
                        printf("Out of sequence error, expected Segment No: %d\n", response.recvd_segment_no + 1);
                        // attempt_no = 4;
                        break;
                    }
                    else if (response.reject_sub_code == (short)LENGTH_MISMATCH)
                    {
                        printf("Length mismatch error. Expected length: %d\n", packet.length);
                        // attempt_no = 4;
                        break;
                    }
                    else if (response.reject_sub_code == (short)END_OF_PACKET_MISSING)
                    {
                        printf("Missing end of packet id error\n");
                        // attempt_no = 4;
                        break;
                    }
                    else if (response.reject_sub_code == (short)DUPLICATE_PACKET)
                    {
                        printf("Duplicate packet received error, expected Segment No: %d\n", response.recvd_segment_no + 1);
                        // attempt_no = 4;
                        break;
                    }
                    else
                    {
                        printf("Reject subcode not matching\n");
                    }
                }
            }
        }

        if (attempt_no >= 4)
        { // After 3 attempts, break from the loop
            printf("Server is not responding even after retries..Exiting the program!!");
            break;
        }
        else
        {
            packet_no = packet_no + 1;
        }
        printf("-------------------------------------------------\n");
    }
    close(client_socket);
    return 0;
}
