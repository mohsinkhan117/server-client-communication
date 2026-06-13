#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/types.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_TIMESTAMP 32

// ============== ANSI Color Codes ==============
#define RESET "\x1b[0m"
#define BOLD "\x1b[1m"
#define DIM "\x1b[2m"

#define BLACK "\x1b[30m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"

#define DARK_GREEN "\x1b[38;5;22m"
#define BRIGHT_GREEN "\x1b[38;5;46m"
#define LIME "\x1b[38;5;118m"

#define SUCCESS GREEN
#define ERROR RED
#define WARNING YELLOW
#define INFO CYAN
#define BINARY LIME

// ============== Global Variables ==============
volatile int packet_count = 0;
volatile int byte_count_sent = 0;
volatile int byte_count_received = 0;

char *get_timestamp_short()
{
    static char timestamp[16];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    return timestamp;
}

char *get_timestamp_precise()
{
    static char timestamp[MAX_TIMESTAMP];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    return timestamp;
}

void print_hex_dump(const char *data, int length)
{
    printf(BINARY "│ " RESET);
    for (int i = 0; i < length && i < 256; i++)
    {
        printf(BINARY "%02X " RESET, (unsigned char)data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < length)
        {
            printf("\n" BINARY "│ " RESET);
        }
    }
    printf(BINARY "\n" RESET);
}

void print_ascii_dump(const char *data, int length)
{
    printf(BINARY "│ " RESET);
    for (int i = 0; i < length && i < 256; i++)
    {
        unsigned char c = (unsigned char)data[i];
        if (c >= 32 && c < 127)
        {
            printf("%c", c);
        }
        else
        {
            printf(".");
        }
        if ((i + 1) % 16 == 0 && i + 1 < length)
        {
            printf("\n" BINARY "│ " RESET);
        }
    }
    printf("\n");
}

void display_socket_info(int sock_fd, const char *remote_ip)
{
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);

    getsockname(sock_fd, (struct sockaddr *)&local_addr, &addr_len);

    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, INET_ADDRSTRLEN);

    printf(CYAN "┌─ SOCKET [FD: %d] ───────────────────────────────────────────┐" RESET "\n", sock_fd);
    printf(CYAN "│" RESET " Local  : %s:%d\n", local_ip, ntohs(local_addr.sin_port));
    printf(CYAN "│" RESET " Remote : %s:%d\n", remote_ip, PORT);
    printf(CYAN "│" RESET " State  : ESTABLISHED (TCP_ESTABLISHED)\n");
    printf(CYAN "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

void display_outgoing_packet(int packet_num, const char *message, int length)
{
    packet_count++;
    byte_count_sent += length;

    printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET BRIGHT_GREEN " OUTGOING DATA PACKET #%d " RESET CYAN "                       ║" RESET "\n", packet_num);
    printf(CYAN "╚══════════════════════════════════════════════════════════════╝" RESET "\n\n");

    printf(YELLOW "[%s] " RESET GREEN "└─ Sending to server\n" RESET, get_timestamp_short());

    printf("\n" INFO "┌─ PACKET METADATA ───────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│ Sequence #       : %d\n", packet_num);
    printf(INFO "│ Source IP        : 127.0.0.1\n");
    printf(INFO "│ Source Port      : Ephemeral\n");
    printf(INFO "│ Dest IP          : 127.0.0.1\n");
    printf(INFO "│ Dest Port        : 8080 (0x1F90)\n");
    printf(INFO "│ Payload Size     : %d bytes\n", length);
    printf(INFO "│ Flags            : [SYN=0, ACK=0, PSH=1, FIN=0]\n");
    printf(INFO "└─────────────────────────────────────────────────────────────┘" RESET "\n\n");

    printf(BINARY "┌─ HEX DUMP (Binary Representation) ───────────────────────────┐" RESET "\n");
    print_hex_dump(message, length);
    printf(BINARY "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");

    printf(BINARY "┌─ ASCII DUMP ─────────────────────────────────────────────────┐" RESET "\n");
    print_ascii_dump(message, length);
    printf(BINARY "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

void display_incoming_packet(const char *response, int length)
{
    byte_count_received += length;

    printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET BRIGHT_GREEN " INCOMING RESPONSE PACKET (SERVER ACK) " RESET CYAN "           ║" RESET "\n");
    printf(CYAN "╚══════════════════════════════════════════════════════════════╝" RESET "\n\n");

    printf(YELLOW "[%s] " RESET GREEN "└─ Received from server\n" RESET, get_timestamp_short());

    printf("\n" INFO "┌─ RESPONSE METADATA ────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│ Source IP        : 127.0.0.1\n");
    printf(INFO "│ Source Port      : 8080 (0x1F90)\n");
    printf(INFO "│ Dest IP          : 127.0.0.1\n");
    printf(INFO "│ Payload Size     : %d bytes\n", length);
    printf(INFO "│ Flags            : [ACK=1, PSH=1]\n");
    printf(INFO "└─────────────────────────────────────────────────────────────┘" RESET "\n\n");

    printf(BINARY "┌─ HEX DUMP ───────────────────────────────────────────────────┐" RESET "\n");
    print_hex_dump(response, length);
    printf(BINARY "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

void display_stats()
{
    printf(MAGENTA "\n╔═ CLIENT STATISTICS ═══════════════════════════════════════╗" RESET "\n");
    printf(MAGENTA "║" RESET " Packets Sent     : %d\n", packet_count);
    printf(MAGENTA "║" RESET " Bytes Sent       : %d\n", byte_count_sent);
    printf(MAGENTA "║" RESET " Bytes Received   : %d\n", byte_count_received);
    printf(MAGENTA "║" RESET " Total I/O        : %d bytes\n", byte_count_sent + byte_count_received);
    printf(MAGENTA "╚════════════════════════════════════════════════════════════╝" RESET "\n\n");
}

void print_banner()
{
    printf(DARK_GREEN);
    printf("\n");
    printf("   ██████╗ ██╗     ██╗███████╗███╗   ██╗████████╗     ██╗   ██╗███████╗\n");
    printf("  ██╔════╝ ██║     ██║██╔════╝████╗  ██║╚══██╔══╝     ██║   ██║╚════██║\n");
    printf("  ██║      ██║     ██║█████╗  ██╔██╗ ██║   ██║        ██║   ██║     ██║\n");
    printf("  ██║      ██║     ██║██╔══╝  ██║╚██╗██║   ██║        ╚██╗ ██╔╝     ██║\n");
    printf("  ╚██████╗ ███████╗██║███████╗██║ ╚████║   ██║         ╚████╔╝  ███████║\n");
    printf("   ╚═════╝ ╚══════╝╚═╝╚══════╝╚═╝  ╚═══╝   ╚═╝          ╚═══╝   ╚══════╝\n");
    printf(RESET "\n");
    printf(CYAN "  Multi-Instance TCP/IP Client with Real-time Binary Visualization\n");
    printf(CYAN "  Version 3.0 | Enhanced for Production Environments\n\n" RESET);
}

int main(int argc, char *argv[])
{
    print_banner();

    int sock = 0;
    struct sockaddr_in serv_addr;
    char input_msg[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};

    // Parse command line arguments
    const char *server_ip = "127.0.0.1";
    if (argc > 1)
    {
        server_ip = argv[1];
    }

    printf(CYAN "┌─ CLIENT CONFIGURATION ─────────────────────────────────────┐" RESET "\n");
    printf(CYAN "│" RESET " Target Server   : %s\n", server_ip);
    printf(CYAN "│" RESET " Target Port     : %d\n", PORT);
    printf(CYAN "│" RESET " Protocol        : TCP/IP (IPv4)\n");
    printf(CYAN "│" RESET " Timestamp       : %s\n", get_timestamp_precise());
    printf(CYAN "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");

    // Create socket
    printf(YELLOW "[%s] " RESET "Creating TCP socket...\n", get_timestamp_short());
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        printf(ERROR "  ✗ Socket creation failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket created (FD: %d)\n" RESET, sock);

    // Setup server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Connect to server
    printf(YELLOW "\n[%s] " RESET "Attempting connection to %s:%d...\n\n", get_timestamp_short(), server_ip, PORT);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf(ERROR "✗ Connection failed!\n" RESET);
        printf(ERROR "  └─ Ensure server is running: ./server_enhanced\n\n" RESET);
        return -1;
    }

    // Display connection established
    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET SUCCESS " ✓ CONNECTED TO SERVER [FD: %d] " RESET GREEN "                  ║" RESET "\n", sock);
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    printf(BRIGHT_GREEN "  Connected: %s:%d\n" RESET, server_ip, PORT);
    printf(CYAN "  Status: Bidirectional communication active\n\n" RESET);

    display_socket_info(sock, server_ip);

    // Communication loop
    int msg_counter = 1;
    while (1)
    {
        printf(GREEN "➜ " RESET BOLD "Enter message (or 'exit' to quit): " RESET);
        fflush(stdout);

        if (fgets(input_msg, sizeof(input_msg), stdin) == NULL)
        {
            printf(ERROR "✗ Input error\n\n" RESET);
            break;
        }

        // Remove newline
        input_msg[strcspn(input_msg, "\n")] = 0;

        // Check for empty input
        if (strlen(input_msg) == 0)
        {
            printf(WARNING "⚠ Empty message - please enter valid text\n\n" RESET);
            continue;
        }

        // Check for exit command
        if (strcmp(input_msg, "exit") == 0)
        {
            printf(YELLOW "\n[%s] " RESET "Closing connection...\n\n" RESET, get_timestamp_short());
            break;
        }

        // Check for stats command
        if (strcmp(input_msg, "stats") == 0)
        {
            display_stats();
            continue;
        }

        int bytes_to_send = strlen(input_msg);

        // Display outgoing packet
        display_outgoing_packet(msg_counter, input_msg, bytes_to_send);
        usleep(300000);

        // Send data
        if (write(sock, input_msg, bytes_to_send) < 0)
        {
            printf(ERROR "✗ Transmission failed\n\n" RESET);
            break;
        }

        printf(SUCCESS "  ✓ Packet transmitted successfully\n" RESET);
        usleep(400000);

        // Receive response
        memset(response, 0, BUFFER_SIZE);
        int bytes_received = read(sock, response, BUFFER_SIZE - 1);

        if (bytes_received < 0)
        {
            printf(ERROR "✗ Reception error\n\n" RESET);
            break;
        }

        if (bytes_received == 0)
        {
            printf(YELLOW "[%s] " RESET WARNING "Server closed connection\n\n" RESET, get_timestamp_short());
            break;
        }

        response[bytes_received] = '\0';

        // Display incoming response
        display_incoming_packet(response, bytes_received);
        usleep(300000);

        printf(GREEN "  ╔═══════════════════════════════════════════════════════════╗" RESET "\n");
        printf(GREEN "  ║ " RESET SUCCESS "✓ ROUND-TRIP COMPLETE & ACKNOWLEDGED" RESET GREEN " ║" RESET "\n");
        printf(GREEN "  ╚═══════════════════════════════════════════════════════════╝" RESET "\n\n");

        msg_counter++;
        usleep(500000);
    }

    // Cleanup
    close(sock);

    display_stats();

    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET SUCCESS " ✓ CLIENT SHUTDOWN - CONNECTION CLOSED " RESET GREEN "║" RESET "\n");
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    printf(YELLOW "[%s] " RESET "Socket resources released\n\n" RESET, get_timestamp_short());

    return 0;
}