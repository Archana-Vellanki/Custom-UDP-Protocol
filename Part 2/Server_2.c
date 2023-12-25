// Archana Vellanki
// SCU ID: 7700009833

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

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

typedef struct
{
    uint32_t subscriber_number;
    uint8_t technology;
    uint8_t paid;
} SubscriberRecord;

int readSubscriberDatabase(const char *filename, SubscriberRecord *records, int max_records)
{
    // This function reads the subscriber database file and generates the records for all the users.
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return -1;
    }

    char line[256];
    int num_records = 0;

    // Read records from the file
    while (fgets(line, sizeof(line), file) != NULL && num_records < max_records)
    {
        int result = sscanf(line, "%u %hhu %hhu",
                            &records[num_records].subscriber_number,
                            &records[num_records].technology,
                            &records[num_records].paid);

        if (result == 3)
        {
            num_records++;
        }
        else
        {
            fprintf(stderr, "Error parsing line: %s", line);
        }
    }

    printf("Number of records:%d\n", num_records);
    fclose(file);

    return num_records;
}

void searchAndInitializeResponse(const AccessRequestPacket *request_packet, ServerResponsePacket *response_packet, const SubscriberRecord *subscriberDatabase, int num_records)
{
    // Searches the subscriber records for the client's records and initiates the response with corresponding message.
    response_packet->start_of_packet = START_OF_PACKET;
    response_packet->end_of_packet = END_OF_PACKET;
    response_packet->client_id = CLIENT_ID;
    response_packet->length = sizeof(response_packet);
    response_packet->segment_no = 1;
    response_packet->technology = request_packet->technology;
    response_packet->source_subscriber_no = request_packet->source_subscriber_no;

    for (int i = 0; i < num_records; ++i)
    {
        if (subscriberDatabase[i].subscriber_number == request_packet->source_subscriber_no &&
            subscriberDatabase[i].technology == request_packet->technology)
        {
            // Subscriber found in the database
            if (subscriberDatabase[i].paid)
            {
                response_packet->response_code = ACCESS_OK; // Subscriber permitted to access the network
                printf("Subscriber permitted to access the network\n");
            }
            else
            {
                response_packet->response_code = NOT_PAID; // Subscriber has not paid
                printf("Subscriber has not paid\n");
            }

            break; // Subscriber found
        }
        else if (subscriberDatabase[i].subscriber_number == request_packet->source_subscriber_no &&
                 subscriberDatabase[i].technology != request_packet->technology)
        {
            response_packet->response_code = NOT_EXIST; // Subscriber exists but technology doesnt match
            printf("Subscriber exists but technology doesnt match\n");
        }
        else
        {
            response_packet->response_code = NOT_EXIST; // Subscriber does not exist in the database
            printf("Subscriber does not exist in the database\n");
        }
    }
}

int main()
{
    SubscriberRecord records[3];
    int num_records = readSubscriberDatabase("Verification_Database.txt", records, 3);

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Error creating socket");
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address and port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding socket");
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    while (1)
    {
        AccessRequestPacket request_packet;
        memset(&request_packet, 0, sizeof(request_packet));

        socklen_t client_addr_len = sizeof(client_addr);

        // Receive request from client
        ssize_t recv_size = recvfrom(sockfd, &request_packet, sizeof(request_packet), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (recv_size < 0)
        {
            perror("Error receiving data");
            continue;
        }
        printf("Received a request from the user %u \nTechnology:%u\n", request_packet.source_subscriber_no, request_packet.technology);

        // Validate request and generate response
        ServerResponsePacket response_packet;
        memset(&response_packet, 0, sizeof(response_packet));

        searchAndInitializeResponse(&request_packet, &response_packet, records, num_records);

        // Send response to client
        ssize_t send_size = sendto(sockfd, &response_packet, sizeof(response_packet), 0, (struct sockaddr *)&client_addr, client_addr_len);
        if (send_size < 0)
        {
            perror("Error sending response");
        }

        printf("Response sent to client\n");
        printf("*****************************************\n\n");
    }

    return 0;
}
