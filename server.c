#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/stat.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10
#define MAX_TIMESTAMP 32

// ============== ANSI Color Codes ==============
#define RESET "\x1b[0m"
#define BOLD "\x1b[1m"
#define DIM "\x1b[2m"
#define UNDERLINE "\x1b[4m"

#define BLACK "\x1b[30m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"
#define GRAY "\x1b[90m"

#define DARK_GREEN "\x1b[38;5;22m"
#define BRIGHT_GREEN "\x1b[38;5;46m"
#define LIME "\x1b[38;5;118m"
#define NEON_GREEN "\x1b[38;5;190m"

#define SUCCESS GREEN
#define ERROR RED
#define WARNING YELLOW
#define INFO CYAN
#define BINARY LIME

// ============== Data Structures ==============
typedef struct
{
    int socket_fd;
    int client_id;
    struct sockaddr_in address;
    pthread_t thread_id;
    time_t connected_time;
    long bytes_sent;
    long bytes_received;
    int active;
    char client_ip[INET_ADDRSTRLEN];
    uint16_t client_port;
} Client;

typedef struct
{
    int total_clients;
    int active_clients;
    long total_bytes_exchanged;
    long total_packets;
    Client clients[MAX_CLIENTS];
    pthread_mutex_t lock;
} ServerState;

// ============== Global Variables ==============
ServerState server_state = {0};
int server_running = 1;
volatile long packet_sequence = 0;

// ============== Utility Functions ==============
char *get_timestamp_precise()
{
    static char timestamp[MAX_TIMESTAMP];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    return timestamp;
}

char *get_timestamp_short()
{
    static char timestamp[16];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    return timestamp;
}

void print_binary_representation(unsigned char byte)
{
    for (int i = 7; i >= 0; i--)
    {
        printf("%d", (byte >> i) & 1);
    }
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

// ============== Process & System Info Display ==============
void display_system_info()
{
    FILE *meminfo = fopen("/proc/meminfo", "r");
    unsigned long mem_total = 0, mem_available = 0;

    if (meminfo)
    {
        char line[256];
        while (fgets(line, sizeof(line), meminfo))
        {
            if (sscanf(line, "MemTotal: %lu", &mem_total) == 1)
                continue;
            if (sscanf(line, "MemAvailable: %lu", &mem_available) == 1)
                continue;
        }
        fclose(meminfo);
    }

    FILE *uptime = fopen("/proc/uptime", "r");
    double up_seconds = 0;
    if (uptime)
    {
        fscanf(uptime, "%lf", &up_seconds);
        fclose(uptime);
    }

    printf(INFO "┌─ SYSTEM STATUS ─────────────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│" RESET " Timestamp    : %s\n", get_timestamp_precise());
    printf(INFO "│" RESET " Memory Total : %lu KB | Available: %lu KB\n", mem_total, mem_available);
    printf(INFO "│" RESET " Uptime       : %.0f seconds\n", up_seconds);
    printf(INFO "└─────────────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

void display_server_process_info(pid_t pid)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *stat_file = fopen(path, "r");
    if (!stat_file)
        return;

    long vsize, rss, utime, stime;
    int threads;
    char comm[256];

    fscanf(stat_file, "%*d %255s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld %*ld %*ld %*d %*d %d %*u %*u %ld %*u %ld",
           comm, &utime, &stime, &threads, &vsize, &rss);
    fclose(stat_file);

    printf(BRIGHT_GREEN "┌─ SERVER PROCESS [PID: %d] ──────────────────────────────────┐" RESET "\n", pid);
    printf(BRIGHT_GREEN "│" RESET " Process Name : %s\n", comm);
    printf(BRIGHT_GREEN "│" RESET " Threads      : %d\n", threads);
    printf(BRIGHT_GREEN "│" RESET " Virtual Mem  : %ld bytes (%.2f MB)\n", vsize, vsize / 1024.0 / 1024.0);
    printf(BRIGHT_GREEN "│" RESET " RSS Memory   : %ld pages (%.2f MB)\n", rss, rss * 4.0 / 1024.0);
    printf(BRIGHT_GREEN "│" RESET " CPU Time     : U:%ld S:%ld\n", utime, stime);
    printf(BRIGHT_GREEN "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

// ============== Client Management ==============
void display_socket_info(int sock_fd, int client_id)
{
    struct sockaddr_in local_addr, peer_addr;
    socklen_t addr_len = sizeof(local_addr);

    getsockname(sock_fd, (struct sockaddr *)&local_addr, &addr_len);
    getpeername(sock_fd, (struct sockaddr *)&peer_addr, &addr_len);

    char local_ip[INET_ADDRSTRLEN], peer_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, INET_ADDRSTRLEN);

    printf(CYAN "┌─ SOCKET #%d [FD: %d] ───────────────────────────────────┐" RESET "\n", client_id, sock_fd);
    printf(CYAN "│" RESET " Local  : %s:%d\n", local_ip, ntohs(local_addr.sin_port));
    printf(CYAN "│" RESET " Remote : %s:%d\n", peer_ip, ntohs(peer_addr.sin_port));
    printf(CYAN "│" RESET " State  : ESTABLISHED (TCP_ESTABLISHED)\n");
    printf(CYAN "└─────────────────────────────────────────────────────────┘" RESET "\n");
}

void display_clients_list()
{
    pthread_mutex_lock(&server_state.lock);

    printf(MAGENTA "\n╔═ CONNECTED CLIENTS ════════════════════════════════════════╗" RESET "\n");
    printf(MAGENTA "║" RESET " ID  │ IP Address      │ Port  │ Connected     │ I/O" RESET MAGENTA " ║" RESET "\n");
    printf(MAGENTA "╠════╪═════════════════╪═══════╪═══════════════╪════════╣" RESET "\n");

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server_state.clients[i].active)
        {
            time_t now = time(NULL);
            int connected_secs = (int)(now - server_state.clients[i].connected_time);

            printf(MAGENTA "║" RESET " %2d  │ %-15s │ %5d │ %3dh %2dm %2ds │ %luB↑ %luB↓" RESET MAGENTA " ║" RESET "\n",
                   server_state.clients[i].client_id,
                   server_state.clients[i].client_ip,
                   server_state.clients[i].client_port,
                   connected_secs / 3600,
                   (connected_secs % 3600) / 60,
                   connected_secs % 60,
                   server_state.clients[i].bytes_sent,
                   server_state.clients[i].bytes_received);
        }
    }

    printf(MAGENTA "╠════════════════════════════════════════════════════════════╣" RESET "\n");
    printf(MAGENTA "║" RESET " Total: %d | Active: %d | Packets: %ld | Total I/O: %ld bytes" RESET MAGENTA " ║" RESET "\n",
           server_state.total_clients,
           server_state.active_clients,
           server_state.total_packets,
           server_state.total_bytes_exchanged);
    printf(MAGENTA "╚════════════════════════════════════════════════════════════╝" RESET "\n\n");

    pthread_mutex_unlock(&server_state.lock);
}

// ============== Packet Display Functions ==============
void display_incoming_packet(int client_id, const char *client_ip, uint16_t client_port,
                             const char *buffer, int bytes_read)
{
    packet_sequence++;

    printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET BRIGHT_GREEN " INCOMING DATA PACKET #%ld [CLIENT #%d] " RESET CYAN "              ║" RESET "\n",
           packet_sequence, client_id);
    printf(CYAN "╚══════════════════════════════════════════════════════════════╝" RESET "\n\n");

    printf(GRAY "[%s] " RESET GREEN "└─ Received from client\n" RESET, get_timestamp_short());

    printf("\n" INFO "┌─ PACKET METADATA ───────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│ Sequence #       : %ld\n", packet_sequence);
    printf(INFO "│ Source IP        : %s\n", client_ip);
    printf(INFO "│ Source Port      : %d\n", client_port);
    printf(INFO "│ Dest IP          : 127.0.0.1\n");
    printf(INFO "│ Dest Port        : 8080 (0x1F90)\n");
    printf(INFO "│ Payload Size     : %d bytes\n", bytes_read);
    printf(INFO "│ Flags            : [SYN=0, ACK=1, PSH=1, FIN=0]\n");
    printf(INFO "└─────────────────────────────────────────────────────────────┘" RESET "\n\n");

    printf(BINARY "┌─ HEX DUMP (Binary Representation) ───────────────────────────┐" RESET "\n");
    print_hex_dump(buffer, bytes_read);
    printf(BINARY "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");

    printf(BINARY "┌─ ASCII DUMP ─────────────────────────────────────────────────┐" RESET "\n");
    print_ascii_dump(buffer, bytes_read);
    printf(BINARY "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

void display_outgoing_packet(int client_id, const char *buffer, int length)
{
    printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET BRIGHT_GREEN " OUTGOING RESPONSE PACKET [CLIENT #%d] " RESET CYAN "            ║" RESET "\n", client_id);
    printf(CYAN "╚══════════════════════════════════════════════════════════════╝" RESET "\n\n");

    printf(GRAY "[%s] " RESET GREEN "└─ Sending to client\n" RESET, get_timestamp_short());

    printf("\n" INFO "┌─ RESPONSE METADATA ────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│ Packet Type      : ACK (Data Acknowledgement)\n");
    printf(INFO "│ Source IP        : 127.0.0.1\n");
    printf(INFO "│ Source Port      : 8080 (0x1F90)\n");
    printf(INFO "│ Payload Size     : %d bytes\n", length);
    printf(INFO "│ Flags            : [ACK=1, PSH=1]\n");
    printf(INFO "└─────────────────────────────────────────────────────────────┘" RESET "\n\n");

    printf(BINARY "┌─ HEX DUMP ───────────────────────────────────────────────────┐" RESET "\n");
    print_hex_dump(buffer, length);
    printf(BINARY "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

// ============== Client Handler Thread ==============
void *handle_client(void *arg)
{
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    display_socket_info(client->socket_fd, client->client_id);

    while (server_running && client->active)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client->socket_fd, buffer, BUFFER_SIZE - 1);

        if (bytes_read <= 0)
        {
            printf(WARNING "\n[%s] Client #%d disconnected (bytes_read=%d)\n\n" RESET,
                   get_timestamp_short(), client->client_id, bytes_read);
            break;
        }

        buffer[bytes_read] = '\0';

        pthread_mutex_lock(&server_state.lock);
        client->bytes_received += bytes_read;
        server_state.total_bytes_exchanged += bytes_read;
        server_state.total_packets++;
        pthread_mutex_unlock(&server_state.lock);

        // Display incoming packet
        display_incoming_packet(client->client_id, client->client_ip, client->client_port, buffer, bytes_read);

        // Generate response with timestamp and server info
        int response_len = snprintf(response, sizeof(response),
                                    "[%s] ACK→ Server received (%d bytes): %.256s",
                                    get_timestamp_short(), bytes_read, buffer);

        usleep(300000);

        // Display outgoing packet
        display_outgoing_packet(client->client_id, response, response_len);

        // Send response
        if (write(client->socket_fd, response, response_len) < 0)
        {
            printf(ERROR "[%s] Failed to send response to client #%d\n\n" RESET,
                   get_timestamp_short(), client->client_id);
            break;
        }

        pthread_mutex_lock(&server_state.lock);
        client->bytes_sent += response_len;
        server_state.total_bytes_exchanged += response_len;
        pthread_mutex_unlock(&server_state.lock);

        printf(SUCCESS "\n  ✓ Packet transmitted successfully\n" RESET);
        printf(INFO "  └─ Round-trip complete (waiting for next message...)\n\n" RESET);

        usleep(500000);
    }

    pthread_mutex_lock(&server_state.lock);
    client->active = 0;
    server_state.active_clients--;
    pthread_mutex_unlock(&server_state.lock);

    close(client->socket_fd);

    printf(GRAY "\n[%s] " RESET WARNING "Client #%d connection terminated. Cleaned up resources.\n\n" RESET,
           get_timestamp_short(), client->client_id);

    return NULL;
}

// ============== Banner & Initialization ==============
void print_banner()
{
    printf(DARK_GREEN);
    printf("\n");
    printf("  ███████╗███████╗██████╗ ██╗   ██╗███████╗██████╗     ██╗   ██╗██████╗ \n");
    printf("  ██╔════╝██╔════╝██╔══██╗██║   ██║██╔════╝██╔══██╗    ██║   ██║╚════██╗\n");
    printf("  ███████╗█████╗  ██████╔╝██║   ██║█████╗  ██████╔╝    ██║   ██║ █████╔╝\n");
    printf("  ╚════██║██╔══╝  ██╔══██╗╚██╗ ██╔╝██╔══╝  ██╔══██╗    ██║   ██║██╔═══╝ \n");
    printf("  ███████║███████╗██║  ██║ ╚████╔╝ ███████╗██║  ██║    ╚██████╔╝███████╗\n");
    printf("  ╚══════╝╚══════╝╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚═╝  ╚═╝     ╚═════╝ ╚══════╝\n");
    printf(RESET "\n");
    printf(CYAN "  Multi-Client TCP/IP Server with Real-time Binary Visualization\n");
    printf(CYAN "  Version 3.0 | Enhanced for Production Environments\n\n" RESET);
}

// ============== Main Server ==============
int main(int argc, char *argv[])
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    pid_t server_pid = getpid();

    print_banner();
    display_system_info();
    display_server_process_info(server_pid);

    // Initialize mutex
    pthread_mutex_init(&server_state.lock, NULL);

    // Create socket
    printf(GRAY "[%s] " RESET "Creating TCP/IPv4 socket...\n", get_timestamp_short());
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        printf(ERROR "  ✗ Socket creation failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket created (FD: %d)\n" RESET, server_fd);

    // Set socket options
    printf(GRAY "[%s] " RESET "Configuring socket options...\n", get_timestamp_short());
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        printf(ERROR "  ✗ setsockopt failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket options configured\n" RESET);

    // Setup address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    printf(GRAY "[%s] " RESET "Binding to port %d...\n", get_timestamp_short(), PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        printf(ERROR "  ✗ Bind failed - Port already in use\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Bound to 0.0.0.0:%d\n" RESET, PORT);

    // Listen
    printf(GRAY "[%s] " RESET "Entering listening state...\n", get_timestamp_short());
    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        printf(ERROR "  ✗ Listen failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Listening for incoming connections\n" RESET);

    printf(WARNING "\n[%s] " RESET "Awaiting client connections on port %d...\n\n" RESET,
           get_timestamp_short(), PORT);

    // Main acceptance loop
    while (server_running)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_fd < 0)
        {
            if (server_running)
            {
                printf(ERROR "Accept error\n" RESET);
            }
            continue;
        }

        // Find available client slot
        pthread_mutex_lock(&server_state.lock);
        int client_index = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (!server_state.clients[i].active)
            {
                client_index = i;
                break;
            }
        }
        pthread_mutex_unlock(&server_state.lock);

        if (client_index == -1)
        {
            printf(WARNING "[%s] Maximum clients reached, rejecting connection\n\n" RESET,
                   get_timestamp_short());
            close(client_fd);
            continue;
        }

        // Setup client structure
        Client *client = &server_state.clients[client_index];
        client->socket_fd = client_fd;
        client->client_id = client_index + 1;
        client->address = client_addr;
        client->connected_time = time(NULL);
        client->bytes_sent = 0;
        client->bytes_received = 0;
        client->active = 1;

        inet_ntop(AF_INET, &client_addr.sin_addr, client->client_ip, INET_ADDRSTRLEN);
        client->client_port = ntohs(client_addr.sin_port);

        pthread_mutex_lock(&server_state.lock);
        server_state.total_clients++;
        server_state.active_clients++;
        pthread_mutex_unlock(&server_state.lock);

        // Print connection established
        printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
        printf(GREEN "║" RESET SUCCESS " ✓ CLIENT #%d CONNECTED [FD: %d] " RESET GREEN "                  ║" RESET "\n",
               client->client_id, client_fd);
        printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
        printf(NEON_GREEN "  Client: %s:%d\n" RESET, client->client_ip, client->client_port);
        printf(NEON_GREEN "  Server: 127.0.0.1:8080\n" RESET);
        printf(GRAY "  Status: Bidirectional communication active\n\n" RESET);

        // Create thread for this client
        if (pthread_create(&client->thread_id, NULL, handle_client, client) != 0)
        {
            printf(ERROR "Failed to create thread for client\n" RESET);
            close(client_fd);
            client->active = 0;
            continue;
        }

        // Display clients list
        display_clients_list();
    }

    close(server_fd);
    pthread_mutex_destroy(&server_state.lock);

    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET SUCCESS " ✓ SERVER SHUTDOWN - ALL CONNECTIONS CLOSED " RESET GREEN "║" RESET "\n");
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);

    return 0;
}