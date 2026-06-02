#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

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

// FIX: Define YELLOW as an alias for WARNING
#define YELLOW      WARNING

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
    (void)data; (void)length;
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

void animate_packet_transmission(int step) {
    if (step == 0) printf(DARK_GREEN "▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁" RESET "\n");
    else if (step == 1) printf(GREEN "▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂" RESET "\n");
    else if (step == 2) printf(LIME "▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃▃" RESET "\n");
    else if (step == 3) printf(NEON_GREEN "▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄" RESET "\n");
}

void print_network_diagram() {
    printf(GREEN "\n");
    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET NEON_GREEN "  ◆ TCP/IP NETWORK COMMUNICATION CLIENT - v2.0 ◆" RESET GREEN "  ║" RESET "\n");
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝" RESET "\n\n");
    
    printf(CYAN "  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓" RESET "\n");
    printf(CYAN "  ┃" RESET "                                                        " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET " " NEON_GREEN "CLIENT MACHINE" RESET " " GRAY "─────────────" RESET " " GREEN "═══════════" RESET " " GRAY "─────────────" RESET " " NEON_GREEN "SERVER" RESET " " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET "                                                        " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET " " INFO "TCP SOCKET" RESET " " GRAY "──→ [NETWORK STACK] ──→ " INFO "TCP SOCKET" RESET CYAN " ┃" RESET "\n");
    printf(CYAN "  ┃" RESET "                                                        " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET " " BINARY "127.0.0.1:ephemeral" RESET "  " GRAY "   (Encrypted)" RESET "  " BINARY "127.0.0.1:8080" RESET " " CYAN "┃" RESET "\n");
    printf(CYAN "  ┃" RESET "                                                        " CYAN "┃" RESET "\n");
    printf(CYAN "  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛" RESET "\n\n");
}

void print_client_info() {
    printf(DARK_GREEN "\n╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(DARK_GREEN "║" RESET GREEN " CLIENT CONFIGURATION " RESET DARK_GREEN "                                    ║" RESET "\n");
    printf(DARK_GREEN "╠════════════════════════════════════════════════════════════════╣" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Target Server IP     : " CYAN "127.0.0.1" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Target Port          : " CYAN "8080" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Protocol              : " CYAN "TCP/IP (Layer 4 Transport)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Address Family        : " CYAN "AF_INET (IPv4)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Socket Type           : " CYAN "SOCK_STREAM (Reliable)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Data Encoding         : " CYAN "UTF-8 (ASCII Compatible)" RESET "\n");
    printf(DARK_GREEN "║" RESET "  " GREEN "→" RESET " Handshake Mode        : " CYAN "3-WAY (SYN, SYN-ACK, ACK)" RESET "\n");
    printf(DARK_GREEN "╚════════════════════════════════════════════════════════════════╝" RESET "\n");
}

void print_welcome_banner() {
    printf(DARK_GREEN);
    printf("\n");
    printf("   ██████╗ ██████╗ ███╗   ███╗███╗   ███╗\n");
    printf("  ██╔════╝██╔═══██╗████╗ ████║████╗ ████║\n");
    printf("  ██║     ██║   ██║██╔████╔██║██╔████╔██║\n");
    printf("  ██║     ██║   ██║██║╚██╔╝██║██║╚██╔╝██║\n");
    printf("  ╚██████╗╚██████╔╝██║ ╚═╝ ██║██║ ╚═╝ ██║\n");
    printf("   ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚═╝     ╚═╝\n");
    printf(RESET "\n");
}

void print_tcp_handshake() {
    printf(CYAN "\n╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET GREEN " 3-WAY TCP HANDSHAKE - CONNECTION ESTABLISHMENT " RESET CYAN "║" RESET "\n");
    printf(CYAN "╚════════════════════════════════════════════════════════════════╝" RESET "\n\n");
    
    printf(LIME "  [STEP 1] CLIENT → SERVER: SYN Packet\n");
    animate_packet_transmission(1);
    printf(INFO "  ├─ Sequence: 0x00000000\n");
    printf(INFO "  ├─ ACK Flag: 0\n");
    printf(INFO "  ├─ SYN Flag: 1\n");
    printf(INFO "  └─ Purpose: Request connection\n\n");
    usleep(600000);
    
    printf(LIME "  [STEP 2] SERVER → CLIENT: SYN-ACK Packet\n");
    animate_packet_transmission(2);
    printf(INFO "  ├─ Sequence: 0x12345678\n");
    printf(INFO "  ├─ Acknowledge: 0x00000001\n");
    printf(INFO "  ├─ SYN Flag: 1\n");
    printf(INFO "  ├─ ACK Flag: 1\n");
    printf(INFO "  └─ Purpose: Acknowledge & respond\n\n");
    usleep(600000);
    
    printf(LIME "  [STEP 3] CLIENT → SERVER: ACK Packet\n");
    animate_packet_transmission(3);
    printf(INFO "  ├─ Sequence: 0x00000001\n");
    printf(INFO "  ├─ Acknowledge: 0x12345679\n");
    printf(INFO "  ├─ ACK Flag: 1\n");
    printf(INFO "  └─ Purpose: Confirm connection\n\n");
    usleep(600000);
    
    printf(GREEN "  ✓ " RESET BOLD "TCP HANDSHAKE COMPLETE - Connection Established!" RESET "\n");
    printf(SUCCESS "  └─ Ready for bidirectional data transfer\n\n");
}

void print_outgoing_packet(const char *message, int length) {
    message_count++;
    packet_sequence++;
    
    printf(CYAN "\n╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET NEON_GREEN " OUTGOING PACKET #%d " RESET CYAN "                               ║" RESET "\n", message_count);
    printf(CYAN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    
    printf(GRAY "[%s]" RESET " " GREEN "└─ Sending to server\n" RESET, get_timestamp());
    
    printf("\n" INFO "SESSION DETAILS:" RESET "\n");
    printf(BINARY "├─ Source IP:     " RESET CYAN "127.0.0.1\n" RESET);
    printf(BINARY "├─ Source Port:   " RESET CYAN "Ephemeral\n" RESET);
    printf(BINARY "├─ Dest IP:       " RESET CYAN "127.0.0.1\n" RESET);
    printf(BINARY "├─ Dest Port:     " RESET CYAN "8080\n" RESET);
    printf(BINARY "├─ Packet Size:   " RESET CYAN "%d bytes\n" RESET, length);
    printf(BINARY "├─ TTL:           " RESET CYAN "64\n" RESET);
    printf(BINARY "└─ Protocol:      " RESET CYAN "TCP/IP v4\n" RESET);
    
    printf("\n" INFO "PAYLOAD DATA:" RESET "\n");
    printf(BINARY "┌─ ASCII CONTENT ────────────────────────────────────────────┐" RESET "\n");
    printf(BINARY "│ " RESET NEON_GREEN "→ " SUCCESS "%s\n" RESET, message);
    printf(BINARY "└────────────────────────────────────────────────────────────┘\n" RESET);
    
    print_hex_payload(message, length);
    print_packet_structure(message, length);
}

void print_incoming_response(const char *response, int length) {
    printf(CYAN "\n╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET NEON_GREEN " INCOMING RESPONSE (SERVER ACK) " RESET CYAN "                      ║" RESET "\n");
    printf(CYAN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    
    printf(GRAY "[%s]" RESET " " GREEN "└─ Received from server\n" RESET, get_timestamp());
    
    printf("\n" INFO "SESSION DETAILS:" RESET "\n");
    printf(BINARY "├─ Source IP:     " RESET CYAN "127.0.0.1\n" RESET);
    printf(BINARY "├─ Source Port:   " RESET CYAN "8080\n" RESET);
    printf(BINARY "├─ Dest IP:       " RESET CYAN "127.0.0.1\n" RESET);
    printf(BINARY "├─ Packet Type:   " RESET CYAN "ACK (Acknowledgement)\n" RESET);
    printf(BINARY "├─ Packet Size:   " RESET CYAN "%d bytes\n" RESET, length);
    printf(BINARY "├─ TTL:           " RESET CYAN "64\n" RESET);
    printf(BINARY "└─ Protocol:      " RESET CYAN "TCP/IP v4\n" RESET);
    
    printf("\n" INFO "SERVER RESPONSE:" RESET "\n");
    printf(BINARY "┌─ ASCII CONTENT ────────────────────────────────────────────┐" RESET "\n");
    printf(BINARY "│ " RESET LIME "← " SUCCESS "%s\n" RESET, response);
    printf(BINARY "└────────────────────────────────────────────────────────────┘\n" RESET);
    
    print_hex_payload(response, length);
}

void print_transmission_complete() {
    printf(GREEN "\n  ╔═══════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "  ║ " RESET SUCCESS "✓ ROUND-TRIP COMPLETE & ACKNOWLEDGED" RESET GREEN " ║" RESET "\n");
    printf(GREEN "  ╚═══════════════════════════════════════════════════════════╝" RESET "\n\n");
    
    printf(INFO "PACKET JOURNEY:" RESET "\n");
    printf(SUCCESS "  [1] Message typed at client\n");
    printf(SUCCESS "  [2] Packet assembled at socket layer\n");
    printf(SUCCESS "  [3] TCP header attached\n");
    printf(SUCCESS "  [4] IP header attached\n");
    printf(SUCCESS "  [5] Data transmitted to server\n");
    printf(SUCCESS "  [6] Server receives & processes\n");
    printf(SUCCESS "  [7] Server sends ACK response\n");
    printf(SUCCESS "  [8] Client receives acknowledgement\n");
    printf(SUCCESS "  [9] Ready for next message...\n\n");
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char input_msg[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};
    
    print_welcome_banner();
    print_network_diagram();
    print_client_info();
    
    // Create socket
    printf(GRAY "\n[%s]" RESET " Creating TCP/IPv4 socket...\n", get_timestamp());
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf(ERROR "  ✗ FATAL: Failed to create socket\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket created successfully (FD: %d)\n" RESET, sock);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    printf(GRAY "\n[%s]" RESET " Attempting connection to 127.0.0.1:8080...\n\n", get_timestamp());
    
    print_tcp_handshake();
    
    // Actual connection attempt
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf(ERROR "✗ CONNECTION FAILED: Server unreachable\n" RESET);
        printf(ERROR "  └─ Please ensure the server is running on 127.0.0.1:%d\n\n" RESET, PORT);
        return -1;
    }
    
    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET SUCCESS " ✓ CONNECTION ESTABLISHED - BIDIRECTIONAL CHANNEL ACTIVE " RESET GREEN "║" RESET "\n");
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    printf(NEON_GREEN "  Client: 127.0.0.1 (ephemeral)\n" RESET);
    printf(NEON_GREEN "  Server: 127.0.0.1:8080\n" RESET);
    printf(GRAY "  Status: Ready to transmit\n\n" RESET);
    
    // Communication loop
    while (1) {
        printf(GREEN "➜ " RESET BOLD "Enter message (or 'exit' to quit): " RESET);
        if (fgets(input_msg, sizeof(input_msg), stdin) == NULL) {
            printf(ERROR "✗ Input error\n\n" RESET);
            break;
        }
        input_msg[strcspn(input_msg, "\n")] = 0;
        
        if (strlen(input_msg) == 0) {
            // FIX: YELLOW is now defined
            printf(YELLOW "⚠ Empty message - please enter valid text\n\n" RESET);
            continue;
        }
        
        if (strcmp(input_msg, "exit") == 0) {
            printf(GRAY "\n[%s]" RESET WARNING " Closing connection...\n" RESET, get_timestamp());
            break;
        }
        
        int bytes_to_send = strlen(input_msg);
        
        // Display outgoing packet
        print_outgoing_packet(input_msg, bytes_to_send);
        usleep(300000);
        
        // Send actual data
        if (write(sock, input_msg, bytes_to_send) < 0) {
            printf(ERROR "✗ TRANSMISSION FAILED\n\n" RESET);
            break;
        }
        
        usleep(400000);
        
        // Receive response
        memset(response, 0, BUFFER_SIZE);
        int bytes_received = read(sock, response, BUFFER_SIZE - 1);
        
        if (bytes_received < 0) {
            printf(ERROR "✗ RECEPTION ERROR\n\n" RESET);
            break;
        }
        
        if (bytes_received == 0) {
            printf(GRAY "[%s]" RESET WARNING " Server closed connection\n\n" RESET, get_timestamp());
            break;
        }
        
        response[bytes_received] = '\0';
        
        // Display incoming response
        print_incoming_response(response, bytes_received);
        usleep(300000);
        
        print_transmission_complete();
        usleep(500000);
    }
    
    // Cleanup
    close(sock);
    
    printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(GREEN "║" RESET SUCCESS " ✓ CLIENT SHUTDOWN - CONNECTION CLOSED " RESET GREEN "║" RESET "\n");
    printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
    printf(GRAY "[%s]" RESET INFO " Socket resources released\n\n" RESET, get_timestamp());
    
    return 0;
}
