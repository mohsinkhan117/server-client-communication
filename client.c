#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096
#define MAX_TIMESTAMP 32

// ============== ANSI Color Codes ==============
#define RESET       "\x1b[0m"
#define BOLD        "\x1b[1m"
#define DIM         "\x1b[2m"

#define BLACK       "\x1b[30m"
#define RED         "\x1b[31m"
#define GREEN       "\x1b[32m"
#define YELLOW      "\x1b[33m"
#define BLUE        "\x1b[34m"
#define MAGENTA     "\x1b[35m"
#define CYAN        "\x1b[36m"

#define DARK_GREEN  "\x1b[38;5;22m"
#define BRIGHT_GREEN "\x1b[38;5;46m"
#define LIME        "\x1b[38;5;118m"

#define SUCCESS     GREEN
#define ERROR       RED
#define WARNING     YELLOW
#define INFO        CYAN
#define BINARY      LIME

// ============== Global Variables ==============
volatile int packet_count = 0;
volatile long byte_count_sent = 0;
volatile long byte_count_received = 0;
struct timeval session_start;

char g_server_ip[INET6_ADDRSTRLEN] = "127.0.0.1";
int g_port = DEFAULT_PORT;
char g_local_ip[INET_ADDRSTRLEN] = "0.0.0.0";
uint16_t g_local_port = 0;

char* get_timestamp_short() {
    static char timestamp[16];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    return timestamp;
}

char* get_timestamp_precise() {
    static char timestamp[MAX_TIMESTAMP];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    return timestamp;
}

void print_hex_dump(const char *data, int length) {
    printf(BINARY "│ " RESET);
    for (int i = 0; i < length && i < 256; i++) {
        printf(BINARY "%02X " RESET, (unsigned char)data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < length) {
            printf("\n" BINARY "│ " RESET);
        }
    }
    printf(BINARY "\n" RESET);
}

void print_ascii_dump(const char *data, int length) {
    printf(BINARY "│ " RESET);
    for (int i = 0; i < length && i < 256; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c >= 32 && c < 127) {
            printf("%c", c);
        } else {
            printf(".");
        }
        if ((i + 1) % 16 == 0 && i + 1 < length) {
            printf("\n" BINARY "│ " RESET);
        }
    }
    printf("\n");
}

void display_socket_info(int sock_fd) {
    struct sockaddr_in local_addr, peer_addr;
    socklen_t addr_len = sizeof(local_addr);

    getsockname(sock_fd, (struct sockaddr *)&local_addr, &addr_len);
    getpeername(sock_fd, (struct sockaddr *)&peer_addr, &addr_len);

    char local_ip[INET_ADDRSTRLEN], peer_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, INET_ADDRSTRLEN);

    strncpy(g_local_ip, local_ip, INET_ADDRSTRLEN);
    g_local_port = ntohs(local_addr.sin_port);

    printf(CYAN "┌─ SOCKET [FD: %d] ───────────────────────────────────────────┐" RESET "\n", sock_fd);
    printf(CYAN "│" RESET " Local  : %s:%d\n", local_ip, g_local_port);
    printf(CYAN "│" RESET " Remote : %s:%d\n", peer_ip, ntohs(peer_addr.sin_port));
    printf(CYAN "│" RESET " State  : ESTABLISHED (TCP_ESTABLISHED)\n");
    printf(CYAN "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

void display_outgoing_packet(int packet_num, const char *message, int length) {
    packet_count++;
    byte_count_sent += length;

    printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET BRIGHT_GREEN " OUTGOING DATA PACKET #%d " RESET CYAN "                       ║" RESET "\n", packet_num);
    printf(CYAN "╚══════════════════════════════════════════════════════════════╝" RESET "\n\n");

    printf(YELLOW "[%s] " RESET GREEN "└─ Sending to server\n" RESET, get_timestamp_short());

    printf("\n" INFO "┌─ PACKET METADATA ───────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│ Sequence #       : %d\n", packet_num);
    printf(INFO "│ Source IP        : %s\n", g_local_ip);
    printf(INFO "│ Source Port      : %d\n", g_local_port);
    printf(INFO "│ Dest IP          : %s\n", g_server_ip);
    printf(INFO "│ Dest Port        : %d (0x%04X)\n", g_port, g_port);
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

void display_incoming_packet(const char *response, int length) {
    byte_count_received += length;

    printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET BRIGHT_GREEN " INCOMING RESPONSE PACKET (SERVER ACK) " RESET CYAN "           ║" RESET "\n");
    printf(CYAN "╚══════════════════════════════════════════════════════════════╝" RESET "\n\n");

    printf(YELLOW "[%s] " RESET GREEN "└─ Received from server\n" RESET, get_timestamp_short());

    printf("\n" INFO "┌─ RESPONSE METADATA ────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│ Source IP        : %s\n", g_server_ip);
    printf(INFO "│ Source Port      : %d (0x%04X)\n", g_port, g_port);
    printf(INFO "│ Dest IP          : %s\n", g_local_ip);
    printf(INFO "│ Payload Size     : %d bytes\n", length);
    printf(INFO "│ Flags            : [ACK=1, PSH=1]\n");
    printf(INFO "└─────────────────────────────────────────────────────────────┘" RESET "\n\n");

    printf(BINARY "┌─ HEX DUMP ───────────────────────────────────────────────────┐" RESET "\n");
    print_hex_dump(response, length);
    printf(BINARY "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

void display_stats() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = (now.tv_sec - session_start.tv_sec) +
                      (now.tv_usec - session_start.tv_usec) / 1000000.0;
    if (elapsed <= 0) elapsed = 0.001;

    double avg_throughput = (byte_count_sent + byte_count_received) / elapsed;

    printf(MAGENTA "\n╔═ CLIENT STATISTICS ═══════════════════════════════════════╗" RESET "\n");
    printf(MAGENTA "║" RESET " Session Duration : %.1f seconds\n", elapsed);
    printf(MAGENTA "║" RESET " Packets Sent     : %d\n", packet_count);
    printf(MAGENTA "║" RESET " Bytes Sent       : %ld\n", byte_count_sent);
    printf(MAGENTA "║" RESET " Bytes Received   : %ld\n", byte_count_received);
    printf(MAGENTA "║" RESET " Total I/O        : %ld bytes\n", byte_count_sent + byte_count_received);
    printf(MAGENTA "║" RESET " Avg Throughput   : %.2f B/s\n", avg_throughput);
    printf(MAGENTA "╚════════════════════════════════════════════════════════════╝" RESET "\n\n");
}

void print_banner() {
    printf(DARK_GREEN);
    printf("\n");
    printf("   ██████╗ ██╗     ██╗███████╗███╗   ██╗████████╗  \n");
    printf("  ██╔════╝ ██║     ██║██╔════╝████╗  ██║╚══██╔══╝  \n");
    printf("  ██║      ██║     ██║█████╗  ██╔██╗ ██║   ██║     \n");
    printf("  ██║      ██║     ██║██╔══╝  ██║╚██╗██║   ██║     \n");
    printf("  ╚██████╗ ███████╗██║███████╗██║ ╚████║   ██║     \n");
    printf("   ╚═════╝ ╚══════╝╚═╝╚══════╝╚═╝  ╚═══╝   ╚═╝     \n");
    printf(RESET "\n");
    printf(CYAN "  Multi-Instance TCP/IP Client with Real-time Binary Visualization\n\n" RESET);
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [server_ip] [port]\n", prog_name);
    printf("  server_ip  IPv4 address of the server to connect to\n");
    printf("             (if omitted, you'll be prompted for it)\n");
    printf("  port       TCP port on the server (default: %d)\n\n", DEFAULT_PORT);
}

// Strips a trailing newline/CR left behind by fgets().
void strip_newline(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

// Interactively asks for the server IP and re-prompts until it's a
// syntactically valid IPv4 address. This is what makes the client usable
// from a different machine on the network instead of always talking to
// itself over loopback.
void prompt_for_server_ip(char *out_ip, size_t out_len) {
    char line[128];
    struct in_addr tmp;

    while (1) {
        printf(BOLD "Enter server IP address to connect to: " RESET);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf(ERROR "\n✗ Input error while reading server IP\n" RESET);
            exit(1);
        }
        strip_newline(line);

        if (strlen(line) == 0) {
            printf(WARNING "⚠ Please enter an IP address (e.g. 192.168.1.20)\n" RESET);
            continue;
        }

        if (inet_pton(AF_INET, line, &tmp) != 1) {
            printf(ERROR "✗ '%s' is not a valid IPv4 address. Try again.\n" RESET, line);
            continue;
        }

        strncpy(out_ip, line, out_len - 1);
        out_ip[out_len - 1] = '\0';
        return;
    }
}

// Interactively asks for the port, allowing Enter to accept the default.
int prompt_for_port(int default_port) {
    char line[32];

    printf(BOLD "Enter server port " RESET "(press Enter for default %d): ", default_port);
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL) {
        return default_port;
    }
    strip_newline(line);

    if (strlen(line) == 0) {
        return default_port;
    }

    int port = atoi(line);
    if (port <= 0 || port > 65535) {
        printf(WARNING "⚠ Invalid port '%s', using default %d instead\n" RESET, line, default_port);
        return default_port;
    }
    return port;
}

int main(int argc, char *argv[]) {
    print_banner();

    int sock = 0;
    struct sockaddr_in serv_addr;
    char input_msg[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};

    // ---- Get server IP / port, either from the terminal args or by asking ----
    // This client is meant to run on a *different* machine from the server,
    // so it never silently falls back to loopback (127.0.0.1) unless that's
    // literally what you type in.
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc > 1) {
        struct in_addr tmp;
        if (inet_pton(AF_INET, argv[1], &tmp) != 1) {
            fprintf(stderr, ERROR "Invalid server IP argument: %s\n" RESET, argv[1]);
            print_usage(argv[0]);
            return -1;
        }
        strncpy(g_server_ip, argv[1], sizeof(g_server_ip) - 1);
    } else {
        prompt_for_server_ip(g_server_ip, sizeof(g_server_ip));
    }

    if (argc > 2) {
        g_port = atoi(argv[2]);
        if (g_port <= 0 || g_port > 65535) {
            fprintf(stderr, ERROR "Invalid port: %s\n" RESET, argv[2]);
            print_usage(argv[0]);
            return -1;
        }
    } else {
        g_port = prompt_for_port(DEFAULT_PORT);
    }

    printf("\n");

    printf(CYAN "┌─ CLIENT CONFIGURATION ─────────────────────────────────────┐" RESET "\n");
    printf(CYAN "│" RESET " Target Server   : %s\n", g_server_ip);
    printf(CYAN "│" RESET " Target Port     : %d\n", g_port);
    printf(CYAN "│" RESET " Protocol        : TCP/IP (IPv4)\n");
    printf(CYAN "│" RESET " Timestamp       : %s\n", get_timestamp_precise());
    printf(CYAN "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");

    // Create socket
    printf(YELLOW "[%s] " RESET "Creating TCP socket...\n", get_timestamp_short());
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf(ERROR "  ✗ Socket creation failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket created (FD: %d)\n" RESET, sock);

    // Setup server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(g_port);
    if (inet_pton(AF_INET, g_server_ip, &serv_addr.sin_addr) != 1) {
        printf(ERROR "  ✗ Invalid server IP address: %s\n" RESET, g_server_ip);
        return -1;
    }

    // Connect to server
    printf(YELLOW "\n[%s] " RESET "Attempting connection to %s:%d...\n\n", get_timestamp_short(), g_server_ip, g_port);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf(ERROR "✗ Connection failed!\n" RESET);
        printf(ERROR "  └─ Ensure the server is running and reachable at %s:%d\n\n" RESET, g_server_ip, g_port);
        return -1;
    }

    gettimeofday(&session_start, NULL);

    // Display connection established
    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET SUCCESS " ✓ CONNECTED TO SERVER [FD: %d] " RESET GREEN "                  ║" RESET "\n", sock);
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    printf(BRIGHT_GREEN "  Connected: %s:%d\n" RESET, g_server_ip, g_port);
    printf(CYAN "  Status: Bidirectional communication active\n\n" RESET);

    display_socket_info(sock);

    // Communication loop
    int msg_counter = 1;
    while (1) {
        printf(GREEN "➜ " RESET BOLD "Enter message (or 'exit' to quit): " RESET);
        fflush(stdout);

        if (fgets(input_msg, sizeof(input_msg), stdin) == NULL) {
            printf(ERROR "✗ Input error\n\n" RESET);
            break;
        }

        // Remove newline
        input_msg[strcspn(input_msg, "\n")] = 0;

        // Check for empty input
        if (strlen(input_msg) == 0) {
            printf(WARNING "⚠ Empty message - please enter valid text\n\n" RESET);
            continue;
        }

        // Check for exit command
        if (strcmp(input_msg, "exit") == 0) {
            printf(YELLOW "\n[%s] " RESET "Closing connection...\n\n" RESET, get_timestamp_short());
            break;
        }

        // Check for stats command
        if (strcmp(input_msg, "stats") == 0) {
            display_stats();
            continue;
        }

        int bytes_to_send = strlen(input_msg);

        // Display outgoing packet
        display_outgoing_packet(msg_counter, input_msg, bytes_to_send);
        usleep(300000);

        // Send data
        if (write(sock, input_msg, bytes_to_send) < 0) {
            printf(ERROR "✗ Transmission failed\n\n" RESET);
            break;
        }

        printf(SUCCESS "  ✓ Packet transmitted successfully\n" RESET);
        usleep(400000);

        // Receive response
        memset(response, 0, BUFFER_SIZE);
        int bytes_received = read(sock, response, BUFFER_SIZE - 1);

        if (bytes_received < 0) {
            printf(ERROR "✗ Reception error\n\n" RESET);
            break;
        }

        if (bytes_received == 0) {
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
