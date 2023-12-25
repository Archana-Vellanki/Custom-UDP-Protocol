#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#define START_OF_PACKET 0xFFFF
#define END_OF_PACKET 0xFFFF
#define CLIENT_ID 0XFF
#define ACC_PER 0XFFF8
#define LENGTH 0XFF

#define ACCESS_OK 0XFFFB
#define NOT_EXIST 0XFFFA
#define NOT_PAID 0XFFF9

#define TWO_G 02
#define THREE_G 03
#define FOUR_G 04
#define FIVE_G 05

#define PORT 35000

typedef struct
{
    uint16_t start_of_packet;
    uint8_t client_id;
    uint16_t acc_per;
    uint8_t segment_no;
    uint8_t length;
    uint8_t technology;
    uint32_t source_subscriber_no;
    uint16_t end_of_packet;
} AccessRequestPacket;

typedef struct
{
    uint16_t start_of_packet;
    uint8_t client_id;
    uint16_t response_code;
    uint8_t segment_no;
    uint8_t length;
    uint8_t technology;
    uint32_t source_subscriber_no;
    uint16_t end_of_packet;
} ServerResponsePacket;

void initializeAccessRequestPacket(AccessRequestPacket *packet, uint8_t technology, uint32_t source_subscriber_no)
{
    // initializes the access request packet with the given values.
    packet->start_of_packet = START_OF_PACKET;
    packet->end_of_packet = END_OF_PACKET;
    packet->client_id = CLIENT_ID;
    packet->acc_per = ACC_PER;
    packet->segment_no = 1;
    packet->length = sizeof(packet);
    packet->technology = technology;
    packet->source_subscriber_no = source_subscriber_no;
}

int main()
{

    // setup the socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("Error creating socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t server_addr_len = sizeof(server_addr);

    // poll for timer check
    struct pollfd pfdsock;
    pfdsock.fd = sock;
    pfdsock.events = POLLIN;

    // Declaring the request packet
    AccessRequestPacket request_packet;
    memset(&request_packet, 0, sizeof(request_packet));

    // Declaring the response packet
    ServerResponsePacket response_packet;
    memset(&response_packet, 0, sizeof(response_packet));

    int i = 1, attempt_no = 1, size = 4, poll_result;

    while (i <= size)
    {
        switch (i)
        {
            // Each case handles the scenarios mentioned
        case 1:
            // Subscriber permitted to access the network
            initializeAccessRequestPacket(&request_packet, FOUR_G, 4085546805);
            break;
        case 2:
            // Subscriber does not exist in the database
            initializeAccessRequestPacket(&request_packet, FOUR_G, 4086808821);
            break;
        case 3:
            // Subscriber exists but technology does not match
            initializeAccessRequestPacket(&request_packet, FIVE_G, 4085546805);
            break;
        case 4:
            // Subscriber has not paid
            initializeAccessRequestPacket(&request_packet, THREE_G, 4086668821);
            break;
        }
        printf("*****************************************************\n");
        printf("Sending access request for number %u. Attempt 1 out of 3\n", request_packet.source_subscriber_no);

        if (sendto(sock, &request_packet, sizeof(request_packet), 0, (struct sockaddr *)&server_addr, server_addr_len) == -1)
        {
            perror("Sendto error:\n");
            exit(1);
        }

        attempt_no = 1;
        // ack_timer implementation
        while (attempt_no <= 3)
        {
            // poll for 3 seconds, and resend up to 2 times
            poll_result = poll(&pfdsock, 1, 3000);

            if (poll_result == 0)
            {
                // timeout, must resend
                attempt_no++;
                printf("\n No response on access request for %u.\nRetransmitting. Attempt %d out of 3\n", request_packet.source_subscriber_no, attempt_no);
                if (sendto(sock, &request_packet, sizeof(request_packet), 0, (struct sockaddr *)&server_addr, server_addr_len) == -1)
                {
                    perror(" Sendto error:\n");
                    exit(1);
                }
            }
            else if (poll_result == -1)
            {
                // error
                perror("Poll error:\n");
                return poll_result;
            }
            else
            {
                ssize_t recv_size = recvfrom(sock, &response_packet, sizeof(response_packet), 0, (struct sockaddr *)&server_addr, &server_addr_len);
                if (recv_size < 0)
                {
                    perror("Error receiving data");
                    return -1;
                }
                printf("Response: ");
                switch (response_packet.response_code)
                {
                case NOT_PAID:
                    printf("Subscriber has not paid\n");
                    break;
                case NOT_EXIST:
                    printf("Subscriber does not exist in the database\n");
                    break;
                case ACCESS_OK:
                    printf("Subscriber permitted to access the network\n");
                    break;
                default:
                    printf("Unknown response code received\n");
                    break;
                }
                break;
            }
        }
        if (attempt_no > 3)
        {
            printf("Server Does Not respond.\n");
            break;
        }
        i++;
    }

    close(sock);

    return 0;
}

// int sendPacketToServer(int sockfd, const void *packet, size_t packet_size, const struct sockaddr *server_addr, socklen_t server_addr_len)
// {
//     ssize_t send_size = sendto(sockfd, packet, packet_size, 0, server_addr, server_addr_len);
//     if (send_size < 0)
//     {
//         perror("Error sending data");
//         return -1;
//     }
//     printf("Request sent to server\n");
//     return 0;
// }
// initializeAccessRequestPacket(&request_packet, CLIENT_ID, FOUR_G, 4085546805);

// sendPacketToServer(sock, &request_packet, sizeof(request_packet), (struct sockaddr *)&server_addr, sizeof(server_addr));

// receivePacketFromServer(sock, &response_packet, sizeof(response_packet), (struct sockaddr *)&server_addr, &server_addr_len);

// initializeAccessRequestPacket(&request_packet, CLIENT_ID, FOUR_G, 4086808821);
// sendPacketToServer(sock, &request_packet, sizeof(request_packet), (struct sockaddr *)&server_addr, sizeof(server_addr));
// receivePacketFromServer(sock, &response_packet, sizeof(response_packet), (struct sockaddr *)&server_addr, &server_addr_len);

// initializeAccessRequestPacket(&request_packet, CLIENT_ID, FIVE_G, 4085546805);
// sendPacketToServer(sock, &request_packet, sizeof(request_packet), (struct sockaddr *)&server_addr, sizeof(server_addr));
// receivePacketFromServer(sock, &response_packet, sizeof(response_packet), (struct sockaddr *)&server_addr, &server_addr_len);

// initializeAccessRequestPacket(&request_packet, CLIENT_ID, THREE_G, 4086668821);
// sendPacketToServer(sock, &request_packet, sizeof(request_packet), (struct sockaddr *)&server_addr, sizeof(server_addr));
// receivePacketFromServer(sock, &response_packet, sizeof(response_packet), (struct sockaddr *)&server_addr, &server_addr_len);
