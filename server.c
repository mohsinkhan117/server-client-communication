#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// ANSI Color Codes - Professional Green Hacker Theme
#define RESET       "\x1b[0m"
#define BOLD        "\x1b[1m"
#define DIM         "\x1b[2m"
#define ITALIC      "\x1b[3m"

#define BLACK       "\x1b[30m"
#define DARK_GREEN  "\x1b[38;5;22m"
#define GREEN       "\x1b[38;5;46m"
#define LIME        "\x1b[38;5;118m"
#define NEON_GREEN  "\x1b[38;5;190m"
#define DARK_GRAY   "\x1b[38;5;235m"
#define GRAY        "\x1b[38;5;244m"
#define CYAN        "\x1b[38;5;51m"

#define SUCCESS     GREEN
#define ERROR       "\x1b[38;5;196m"
#define WARNING     NEON_GREEN
#define INFO        CYAN
#define BINARY      LIME

int message_count = 0;
int packet_sequence = 0;

char* get_timestamp() {
    static char timestamp[20];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    return timestamp;
}

void print_binary_byte(unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
}

void print_hex_payload(const char *data, int length) {
    printf(BINARY "┌─ PAYLOAD (HEX) ────────────────────────────────────────────┐" RESET "\n");
    printf(BINARY "│ " RESET);
    int bytes_per_line = 16;
    for (int i = 0; i < length && i < 256; i++) {
        printf(BINARY "%02X " RESET, (unsigned char)data[i]);
        if ((i + 1) % bytes_per_line == 0 && i + 1 < length) {
            printf("\n" BINARY "│ " RESET);
        }
    }
    printf(BINARY "\n└────────────────────────────────────────────────────────────┘" RESET "\n");
}

void print_packet_structure(const char *data, int length) {
    (void)data; (void)length; // suppress unused parameter warnings
    printf(BINARY "\n╔═══════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(BINARY "║" RESET "              PACKET STRUCTURE - BINARY VISUALIZATION" BINARY "              ║" RESET "\n");
    printf(BINARY "╚═══════════════════════════════════════════════════════════════╝" RESET "\n\n");
    
    printf(INFO "[TCP/IP LAYER 4 - TRANSPORT LAYER]" RESET "\n");
    printf(BINARY "├─ Source Port:      " RESET CYAN "Ephemeral" RESET " (0x0000) " BINARY "→ " RESET);
    print_binary_byte(0x00); printf(" "); print_binary_byte(0x00); printf("\n");
    
    printf(BINARY "├─ Dest Port:        " RESET CYAN "8080" RESET " (0x1F90) " BINARY "→ " RESET);
    print_binary_byte(0x1F); printf(" "); print_binary_byte(0x90); printf("\n");
    
    printf(BINARY "├─ Sequence #:       " RESET "%d" BINARY " (0x%08X)\n" RESET, packet_sequence, packet_sequence);
    printf(BINARY "├─ Window Size:      " RESET "65535" BINARY " (0xFFFF)\n" RESET);
    printf(BINARY "├─ Flags:            " RESET "[ACK|PUSH]" BINARY " → 011000\n" RESET);
    printf(BINARY "└─ Checksum:         " RESET "VALID" BINARY " ✓\n\n" RESET);
    
    printf(INFO "[IP LAYER 3 - NETWORK LAYER]" RESET "\n");
    printf(BINARY "├─ Version:          " RESET "IPv4" BINARY " (0x4)\n" RESET);
    printf(BINARY "├─ IHL:              " RESET "5 (20 bytes)" BINARY "\n" RESET);
    printf(BINARY "├─ TTL:              " RESET "64 hops" BINARY "\n" RESET);
    printf(BINARY "├─ Protocol:         " RESET "TCP (6)" BINARY "\n" RESET);
    printf(BINARY "├─ Source IP:        " RESET "127.0.0.1" BINARY "\n" RESET);
    printf(BINARY "└─ Dest IP:          " RESET "127.0.0.1" BINARY "\n\n" RESET);
    
    printf(INFO "[DATALINK LAYER 2 - ETHERNET]" RESET "\n");
    printf(BINARY "├─ Frame Type:       " RESET "Ethernet II" BINARY "\n" RESET);
    printf(BINARY "├─ Source MAC:       " RESET "00:00:00:00:00:02" BINARY "\n" RESET);
    printf(BINARY "└─ Dest MAC:         " RESET "00:00:00:00:00:01" BINARY "\n\n" RESET);
}

void animate_packet_transmission(const char *direction, int step) {
    (void)direction;
    if (step == 0) printf(DARK_GREEN "▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁" RESET "\n");
    else if (step == 1) printf(GREEN "▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂" RESET "\n");
    else if (step == 2) printf(LIME "▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃" RESET "\n");
    else if (step == 3) printf(NEON_GREEN "▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄" RESET "\n");
}

void print_network_diagram() {
    printf(GREEN "\n");
    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET NEON_GREEN "  ◆ TCP/IP NETWORK COMMUNICATION SERVER - v2.0 ◆" RESET GREEN "  ║" RESET "\n");
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝" RESET "\n\n");
    
    printf(CYAN "  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓" RESET "\n");
    printf(CYAN "  ┃" RESET "                                                        " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET " " NEON_GREEN "CLIENT MACHINE" RESET " " GRAY "─────────────" RESET " " GREEN "═══════════" RESET " " GRAY "─────────────" RESET " " NEON_GREEN "SERVER" RESET " " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET "                                                        " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET " " INFO "TCP SOCKET" RESET " " GRAY "──→ [NETWORK STACK] ──→ " INFO "TCP SOCKET" RESET CYAN " ┃" RESET "\n");
    printf(CYAN "  ┃" RESET "                                                        " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET " " BINARY "172.16.x.x:ephemeral" RESET "  " GRAY "   (Encrypted)" RESET "  " BINARY "127.0.0.1:8080" RESET " " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET "                                                        " CYAN "┃" RESET "\n");
    printf(CYAN "  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛" RESET "\n\n");
}

void print_server_info(int port) {
    printf(DARK_GREEN "\n╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(DARK_GREEN "║" RESET GREEN " SERVER CONFIGURATION " RESET DARK_GREEN "                                    ║" RESET "\n");
    printf(DARK_GREEN "╠════════════════════════════════════════════════════════════════╣" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Listening Port        : " CYAN "%d" RESET "\n", port);
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Protocol              : " CYAN "TCP/IP (Layer 4 Transport)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Address Family        : " CYAN "AF_INET (IPv4)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Socket Type           : " CYAN "SOCK_STREAM (Reliable)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Max Connections       : " CYAN "1 (Sequential)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Data Encoding         : " CYAN "UTF-8 (ASCII Compatible)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Status                : " SUCCESS "● ACTIVE" RESET "\n");
    printf(DARK_GREEN "╚════════════════════════════════════════════════════════════════╝" RESET "\n");
}

void print_welcome_banner() {
    printf(DARK_GREEN);
    printf("\n");
    printf("  ██╗    ██╗███████╗██╗     ██████╗ ███████╗███╗   ███╗███████╗\n");
    printf("  ██║    ██║██╔════╝██║     ██╔══██╗██╔════╝████╗ ████║██╔════╝\n");
    printf("  ██║ █╗ ██║█████╗  ██║     ██║  ██║█████╗  ██╔████╔██║█████╗  \n");
    printf("  ██║███╗██║██╔══╝  ██║     ██║  ██║██╔══╝  ██║╚██╔╝██║██╔══╝  \n");
    printf("  ╚███╔███╔╝███████╗███████╗██████╔╝███████╗██║ ╚═╝ ██║███████╗\n");
    printf("   ╚══╝╚══╝ ╚══════╝╚══════╝╚═════╝ ╚══════╝╚═╝     ╚═╝╚══════╝\n");
    printf(RESET "\n");
}

void print_tcp_handshake() {
    printf(CYAN "\n╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET GREEN " 3-WAY TCP HANDSHAKE - CONNECTION ESTABLISHMENT " RESET CYAN "║" RESET "\n");
    printf(CYAN "╚════════════════════════════════════════════════════════════════╝" RESET "\n\n");
    
    printf(LIME "  [STEP 1] CLIENT → SERVER: SYN Packet\n");
    animate_packet_transmission("→", 1);
    printf(INFO "  ├─ Sequence: 0x00000000\n");
    printf(INFO "  ├─ ACK Flag: 0\n");
    printf(INFO "  ├─ SYN Flag: 1\n");
    printf(INFO "  └─ Purpose: Request connection\n\n");
    usleep(600000);
    
    printf(LIME "  [STEP 2] SERVER → CLIENT: SYN-ACK Packet\n");
    animate_packet_transmission("←", 2);
    printf(INFO "  ├─ Sequence: 0x12345678\n");
    printf(INFO "  ├─ Acknowledge: 0x00000001\n");
    printf(INFO "  ├─ SYN Flag: 1\n");
    printf(INFO "  ├─ ACK Flag: 1\n");
    printf(INFO "  └─ Purpose: Acknowledge & respond\n\n");
    usleep(600000);
    
    printf(LIME "  [STEP 3] CLIENT → SERVER: ACK Packet\n");
    animate_packet_transmission("→", 3);
    printf(INFO "  ├─ Sequence: 0x00000001\n");
    printf(INFO "  ├─ Acknowledge: 0x12345679\n");
    printf(INFO "  ├─ ACK Flag: 1\n");
    printf(INFO "  └─ Purpose: Confirm connection\n\n");
    usleep(600000);
    
    printf(GREEN "  ✓ " RESET BOLD "TCP HANDSHAKE COMPLETE - Connection Established!" RESET "\n");
    printf(SUCCESS "  └─ Ready for bidirectional data transfer\n\n");
}

void print_incoming_packet(const char *buffer, int bytes_read, struct sockaddr_in *address) {
    message_count++;
    packet_sequence++;
    
    printf(CYAN "\n╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET NEON_GREEN " INCOMING PACKET #%d " RESET CYAN "                               ║" RESET "\n", message_count);
    printf(CYAN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    
    printf(GRAY "[%s]" RESET " " GREEN "└─ Received from client\n" RESET, get_timestamp());
    
    printf("\n" INFO "SESSION DETAILS:" RESET "\n");
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address->sin_addr, client_ip, INET_ADDRSTRLEN);
    printf(BINARY "├─ Source IP:     " RESET CYAN "%s\n" RESET, client_ip);
    printf(BINARY "├─ Source Port:   " RESET CYAN "%d\n" RESET, ntohs(address->sin_port));
    printf(BINARY "├─ Dest IP:       " RESET CYAN "127.0.0.1\n" RESET);
    printf(BINARY "├─ Dest Port:     " RESET CYAN "8080\n" RESET);
    printf(BINARY "├─ Packet Size:   " RESET CYAN "%d bytes\n" RESET, bytes_read);
    printf(BINARY "├─ TTL:           " RESET CYAN "64\n" RESET);
    printf(BINARY "└─ Protocol:      " RESET CYAN "TCP/IP v4\n" RESET);
    
    printf("\n" INFO "PAYLOAD DATA:" RESET "\n");
    printf(BINARY "┌─ ASCII CONTENT ────────────────────────────────────────────┐" RESET "\n");
    printf(BINARY "│ " RESET GREEN "→ " SUCCESS "%s\n" RESET, buffer);
    printf(BINARY "└────────────────────────────────────────────────────────────┘\n" RESET);
    
    print_hex_payload(buffer, bytes_read);
    print_packet_structure(buffer, bytes_read);
}

void print_outgoing_packet(const char *response, int length) {
    printf(CYAN "\n╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET NEON_GREEN " OUTGOING PACKET (SERVER RESPONSE) " RESET CYAN "                    ║" RESET "\n");
    printf(CYAN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    
    printf(GRAY "[%s]" RESET " " GREEN "└─ Sending to client (ACK + Echo)\n" RESET, get_timestamp());
    
    printf("\n" INFO "SESSION DETAILS:" RESET "\n");
    printf(BINARY "├─ Source IP:     " RESET CYAN "127.0.0.1\n" RESET);
    printf(BINARY "├─ Source Port:   " RESET CYAN "8080\n" RESET);
    printf(BINARY "├─ Dest IP:       " RESET CYAN "127.0.0.1 (Client)\n" RESET);
    printf(BINARY "├─ Packet Size:   " RESET CYAN "%d bytes\n" RESET, length);
    printf(BINARY "├─ Type:          " RESET CYAN "ACK (Data Acknowledgement)\n" RESET);
    printf(BINARY "└─ Protocol:      " RESET CYAN "TCP/IP v4\n" RESET);
    
    printf("\n" INFO "RESPONSE PAYLOAD:" RESET "\n");
    printf(BINARY "┌─ ASCII CONTENT ────────────────────────────────────────────┐" RESET "\n");
    printf(BINARY "│ " RESET NEON_GREEN "← " SUCCESS "ECHO: %s\n" RESET, response);
    printf(BINARY "└────────────────────────────────────────────────────────────┘\n" RESET);
    
    print_hex_payload(response, length);
}

void print_transmission_complete() {
    printf(GREEN "\n  ╔═══════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "  ║ " RESET SUCCESS "✓ TRANSMISSION COMPLETE & ACKNOWLEDGED" RESET GREEN " ║" RESET "\n");
    printf(GREEN "  ╚═══════════════════════════════════════════════════════════╝" RESET "\n\n");
    
    printf(INFO "PACKET JOURNEY:" RESET "\n");
    printf(SUCCESS "  [1] Data received at server socket\n");
    printf(SUCCESS "  [2] Packet parsed & inspected\n");
    printf(SUCCESS "  [3] Payload extracted & logged\n");
    printf(SUCCESS "  [4] Echo response generated\n");
    printf(SUCCESS "  [5] ACK packet prepared\n");
    printf(SUCCESS "  [6] Response transmitted to client\n");
    printf(SUCCESS "  [7] Waiting for next packet...\n\n");
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    
    print_welcome_banner();
    print_network_diagram();
    print_server_info(PORT);
    
    // Create socket
    printf(GRAY "\n[%s]" RESET " Creating TCP/IPv4 socket...\n", get_timestamp());
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf(ERROR "  ✗ FATAL: Socket creation failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket created successfully (FD: %d)\n" RESET, server_fd);
    
    // Set socket options
    printf(GRAY "[%s]" RESET " Configuring socket options...\n", get_timestamp());
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        printf(ERROR "  ✗ FATAL: setsockopt failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket options configured\n" RESET);
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind
    printf(GRAY "[%s]" RESET " Binding socket to port %d...\n", get_timestamp(), PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf(ERROR "  ✗ FATAL: Bind failed - Port may already be in use\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket bound to 0.0.0.0:%d\n" RESET, PORT);
    
    // Listen
    printf(GRAY "[%s]" RESET " Entering listening state...\n", get_timestamp());
    if (listen(server_fd, 1) < 0) {
        printf(ERROR "  ✗ FATAL: Listen failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Server listening for incoming connections\n" RESET);
    
    printf(WARNING "\n[%s] Awaiting client connection on port %d...\n\n" RESET, get_timestamp(), PORT);
    
    print_tcp_handshake();
    
    // Accept connection
    printf(INFO "[%s]" RESET " Accepting connection from client...\n\n", get_timestamp());
    client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (client_fd < 0) {
        printf(ERROR "✗ FATAL: Accept failed\n" RESET);
        return -1;
    }
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET SUCCESS " ✓ CLIENT CONNECTED - BIDIRECTIONAL CHANNEL ACTIVE " RESET GREEN "║" RESET "\n");
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    printf(NEON_GREEN "  Client: %s:%d\n" RESET, client_ip, ntohs(address.sin_port));
    printf(NEON_GREEN "  Server: 127.0.0.1:8080\n" RESET);
    printf(GRAY "  Status: Ready to receive & respond\n\n" RESET);
    
    // Communication loop
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_read < 0) {
            printf(ERROR "✗ READ ERROR\n\n" RESET);
            break;
        }
        
        if (bytes_read == 0) {
            printf(GRAY "\n[%s]" RESET WARNING " Client disconnected gracefully\n\n" RESET, get_timestamp());
            break;
        }
        
        buffer[bytes_read] = '\0';
        
        // Display incoming packet analysis
        print_incoming_packet(buffer, bytes_read, &address);
        usleep(300000);
        
        // Generate echo response - FIX: prevent truncation
        char response[BUFFER_SIZE];
        int safe_len = (bytes_read > (BUFFER_SIZE - 24)) ? (BUFFER_SIZE - 24) : bytes_read;
        snprintf(response, sizeof(response), "[ACK] Server received: %.*s", safe_len, buffer);
        
        // Display outgoing packet
        print_outgoing_packet(response, strlen(response));
        usleep(300000);
        
        // Send response
        if (write(client_fd, response, strlen(response)) < 0) {
            printf(ERROR "✗ WRITE ERROR\n\n" RESET);
            break;
        }
        
        print_transmission_complete();
        usleep(500000);
    }
    
    // Cleanup
    close(client_fd);
    close(server_fd);
    
    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET SUCCESS " ✓ SERVER SHUTDOWN - ALL CONNECTIONS CLOSED " RESET GREEN "║" RESET "\n");
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    printf(GRAY "[%s]" RESET INFO " Server resources released\n\n" RESET, get_timestamp());
    
    return 0;
}
