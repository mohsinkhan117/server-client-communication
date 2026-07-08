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
#include <ifaddrs.h>
#include <ctype.h>

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10
#define MAX_TIMESTAMP 32

// ============== ANSI Color Codes ==============
#define RESET       "\x1b[0m"
#define BOLD        "\x1b[1m"
#define DIM         "\x1b[2m"
#define UNDERLINE   "\x1b[4m"

#define BLACK       "\x1b[30m"
#define RED         "\x1b[31m"
#define GREEN       "\x1b[32m"
#define YELLOW      "\x1b[33m"
#define BLUE        "\x1b[34m"
#define MAGENTA     "\x1b[35m"
#define CYAN        "\x1b[36m"
#define WHITE       "\x1b[37m"
#define GRAY "\x1b[90m"

#define DARK_GREEN  "\x1b[38;5;22m"
#define BRIGHT_GREEN "\x1b[38;5;46m"
#define LIME        "\x1b[38;5;118m"
#define NEON_GREEN  "\x1b[38;5;190m"

#define SUCCESS     GREEN
#define ERROR       RED
#define WARNING     YELLOW
#define INFO        CYAN
#define BINARY      LIME

// ============== Data Structures ==============
typedef struct {
    int socket_fd;
    int client_id;
    struct sockaddr_in address;
    pthread_t thread_id;
    time_t connected_time;
    long bytes_sent;
    long bytes_received;
    long last_bytes_sent;
    long last_bytes_received;
    time_t last_sample_time;
    int active;
    char client_ip[INET_ADDRSTRLEN];
    uint16_t client_port;
    char local_ip[INET_ADDRSTRLEN];
    uint16_t local_port;
    pthread_mutex_t write_lock; // serializes writes to this socket between
                                // the auto-ACK reply and operator messages
} Client;

typedef struct {
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

int g_port = DEFAULT_PORT;
char g_bind_ip[INET_ADDRSTRLEN] = "0.0.0.0";   // what we bind to (may be a wildcard)
char g_display_ip[INET_ADDRSTRLEN] = "0.0.0.0"; // the real address we show in the UI
int g_server_fd = -1;                          // listening socket, so the console
                                                // thread can unblock accept() on /quit
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER; // keeps multiple threads'
                                                         // output from interleaving

// ============== Utility Functions ==============
char* get_timestamp_precise() {
    static char timestamp[MAX_TIMESTAMP];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    return timestamp;
}

char* get_timestamp_short() {
    static char timestamp[16];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    return timestamp;
}

// Finds the first non-loopback IPv4 address on the machine, so the UI
// never has to fall back to a hardcoded 127.0.0.1 when bound to 0.0.0.0.
int get_local_ip(char *buf, size_t buflen) {
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;

    if (getifaddrs(&ifaddr) == -1) {
        return 0;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &sa->sin_addr, buf, buflen);
        found = 1;
        break;
    }

    freeifaddrs(ifaddr);
    return found;
}

// Reads /proc/stat twice with a short sample window to get real,
// instantaneous CPU utilization instead of a fake/simulated number.
double get_cpu_usage_percent() {
    unsigned long long u1, n1, s1, i1, io1, irq1, sirq1, st1;
    unsigned long long u2, n2, s2, i2, io2, irq2, sirq2, st2;

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return -1.0;
    if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u1, &n1, &s1, &i1, &io1, &irq1, &sirq1, &st1) != 8) {
        fclose(fp);
        return -1.0;
    }
    fclose(fp);

    usleep(150000); // 150ms sample window

    fp = fopen("/proc/stat", "r");
    if (!fp) return -1.0;
    if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u2, &n2, &s2, &i2, &io2, &irq2, &sirq2, &st2) != 8) {
        fclose(fp);
        return -1.0;
    }
    fclose(fp);

    unsigned long long idle1 = i1 + io1;
    unsigned long long idle2 = i2 + io2;
    unsigned long long total1 = u1 + n1 + s1 + i1 + io1 + irq1 + sirq1 + st1;
    unsigned long long total2 = u2 + n2 + s2 + i2 + io2 + irq2 + sirq2 + st2;

    unsigned long long total_delta = total2 - total1;
    unsigned long long idle_delta = idle2 - idle1;

    if (total_delta == 0) return 0.0;
    return (double)(total_delta - idle_delta) * 100.0 / (double)total_delta;
}

void get_load_average(double *one, double *five, double *fifteen) {
    FILE *fp = fopen("/proc/loadavg", "r");
    *one = *five = *fifteen = 0.0;
    if (!fp) return;
    fscanf(fp, "%lf %lf %lf", one, five, fifteen);
    fclose(fp);
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

// ============== Process & System Info Display ==============
void display_system_info() {
    FILE *meminfo = fopen("/proc/meminfo", "r");
    unsigned long mem_total = 0, mem_available = 0;

    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            if (sscanf(line, "MemTotal: %lu", &mem_total) == 1) continue;
            if (sscanf(line, "MemAvailable: %lu", &mem_available) == 1) continue;
        }
        fclose(meminfo);
    }

    FILE *uptime = fopen("/proc/uptime", "r");
    double up_seconds = 0;
    if (uptime) {
        fscanf(uptime, "%lf", &up_seconds);
        fclose(uptime);
    }

    double load1, load5, load15;
    get_load_average(&load1, &load5, &load15);

    double cpu_pct = get_cpu_usage_percent();
    double mem_used_pct = mem_total > 0 ? (double)(mem_total - mem_available) * 100.0 / (double)mem_total : 0.0;

    long up_days = (long)up_seconds / 86400;
    long up_hours = ((long)up_seconds % 86400) / 3600;
    long up_mins = ((long)up_seconds % 3600) / 60;

    printf(INFO "┌─ SYSTEM STATUS ─────────────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│" RESET " Timestamp    : %s\n", get_timestamp_precise());
    printf(INFO "│" RESET " CPU Usage    : %.1f%%\n", cpu_pct < 0 ? 0.0 : cpu_pct);
    printf(INFO "│" RESET " Load Average : %.2f, %.2f, %.2f (1m, 5m, 15m)\n", load1, load5, load15);
    printf(INFO "│" RESET " Memory       : %.1f%% used (%lu / %lu MB)\n",
           mem_used_pct, (mem_total - mem_available) / 1024, mem_total / 1024);
    printf(INFO "│" RESET " System Uptime: %ldd %ldh %ldm\n", up_days, up_hours, up_mins);
    printf(INFO "└─────────────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

void display_server_process_info(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *stat_file = fopen(path, "r");
    if (!stat_file) return;

    long vsize, rss, utime, stime;
    int threads;
    char comm[256];

    // Field layout of /proc/[pid]/stat (1-indexed): 14=utime 15=stime
    // 20=num_threads 23=vsize 24=rss. The previous version skipped one
    // extra field and ended up reading rsslim (often "unlimited") into
    // rss, which showed up as a bogus astronomically large number.
    fscanf(stat_file, "%*d %255s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld %*ld %*ld %*d %*d %d %*u %*u %ld %ld",
           comm, &utime, &stime, &threads, &vsize, &rss);
    fclose(stat_file);

    long clk_tck = sysconf(_SC_CLK_TCK);
    double cpu_seconds = clk_tck > 0 ? (double)(utime + stime) / (double)clk_tck : 0.0;

    printf(BRIGHT_GREEN "┌─ SERVER PROCESS [PID: %d] ──────────────────────────────────┐" RESET "\n", pid);
    printf(BRIGHT_GREEN "│" RESET " Process Name : %s\n", comm);
    printf(BRIGHT_GREEN "│" RESET " Threads      : %d\n", threads);
    printf(BRIGHT_GREEN "│" RESET " Virtual Mem  : %ld bytes (%.2f MB)\n", vsize, vsize / 1024.0 / 1024.0);
    printf(BRIGHT_GREEN "│" RESET " RSS Memory   : %ld pages (%.2f MB)\n", rss, rss * 4.0 / 1024.0);
    printf(BRIGHT_GREEN "│" RESET " CPU Time     : %.2fs (user+sys)\n", cpu_seconds);
    printf(BRIGHT_GREEN "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

// ============== Client Management ==============
void display_socket_info(int sock_fd, int client_id, char *out_local_ip, uint16_t *out_local_port) {
    struct sockaddr_in local_addr, peer_addr;
    socklen_t addr_len = sizeof(local_addr);

    getsockname(sock_fd, (struct sockaddr *)&local_addr, &addr_len);
    getpeername(sock_fd, (struct sockaddr *)&peer_addr, &addr_len);

    char local_ip[INET_ADDRSTRLEN], peer_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, INET_ADDRSTRLEN);

    if (out_local_ip) strncpy(out_local_ip, local_ip, INET_ADDRSTRLEN);
    if (out_local_port) *out_local_port = ntohs(local_addr.sin_port);

    printf(CYAN "┌─ SOCKET #%d [FD: %d] ───────────────────────────────────┐" RESET "\n", client_id, sock_fd);
    printf(CYAN "│" RESET " Local  : %s:%d\n", local_ip, ntohs(local_addr.sin_port));
    printf(CYAN "│" RESET " Remote : %s:%d\n", peer_ip, ntohs(peer_addr.sin_port));
    printf(CYAN "│" RESET " State  : ESTABLISHED (TCP_ESTABLISHED)\n");
    printf(CYAN "└─────────────────────────────────────────────────────────┘" RESET "\n");
}

void display_clients_list() {
    pthread_mutex_lock(&print_lock);
    pthread_mutex_lock(&server_state.lock);

    time_t now = time(NULL);

    printf(MAGENTA "\n╔═ CONNECTED CLIENTS ═══════════════════════════════════════════════════╗" RESET "\n");
    printf(MAGENTA "║" RESET " ID  │ IP Address      │ Port  │ Connected     │ I/O            │ Rate" RESET MAGENTA " ║" RESET "\n");
    printf(MAGENTA "╠════╪═════════════════╪═══════╪═══════════════╪════════════════╪══════════╣" RESET "\n");

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server_state.clients[i].active) {
            Client *c = &server_state.clients[i];
            int connected_secs = (int)(now - c->connected_time);

            double elapsed = difftime(now, c->last_sample_time);
            double up_rate = 0.0, down_rate = 0.0;
            if (elapsed > 0) {
                up_rate = (double)(c->bytes_sent - c->last_bytes_sent) / elapsed;
                down_rate = (double)(c->bytes_received - c->last_bytes_received) / elapsed;
            }
            c->last_bytes_sent = c->bytes_sent;
            c->last_bytes_received = c->bytes_received;
            c->last_sample_time = now;

            printf(MAGENTA "║" RESET " %2d  │ %-15s │ %5d │ %3dh %2dm %2ds │ %6ldB↑ %6ldB↓ │ %5.1f B/s" RESET MAGENTA " ║" RESET "\n",
                   c->client_id,
                   c->client_ip,
                   c->client_port,
                   connected_secs / 3600,
                   (connected_secs % 3600) / 60,
                   connected_secs % 60,
                   c->bytes_sent,
                   c->bytes_received,
                   up_rate + down_rate);
        }
    }

    printf(MAGENTA "╠═══════════════════════════════════════════════════════════════════════╣" RESET "\n");
    printf(MAGENTA "║" RESET " Total: %d | Active: %d | Packets: %ld | Total I/O: %ld bytes" RESET MAGENTA "               ║" RESET "\n",
        server_state.total_clients,
        server_state.active_clients,
        server_state.total_packets,
        server_state.total_bytes_exchanged);
    printf(MAGENTA "╚═══════════════════════════════════════════════════════════════════════╝" RESET "\n\n");

    pthread_mutex_unlock(&server_state.lock);
    pthread_mutex_unlock(&print_lock);
}

// ============== Packet Display Functions ==============
void display_incoming_packet(int client_id, const char *client_ip, uint16_t client_port,
                            const char *dest_ip, uint16_t dest_port,
                            const char *buffer, int bytes_read) {
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
    printf(INFO "│ Dest IP          : %s\n", dest_ip);
    printf(INFO "│ Dest Port        : %d (0x%04X)\n", dest_port, dest_port);
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

void display_outgoing_packet(int client_id, const char *src_ip, uint16_t src_port,
                              const char *buffer, int length) {
    printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗" RESET "\n");
    printf(CYAN "║" RESET BRIGHT_GREEN " OUTGOING RESPONSE PACKET [CLIENT #%d] " RESET CYAN "            ║" RESET "\n", client_id);
    printf(CYAN "╚══════════════════════════════════════════════════════════════╝" RESET "\n\n");

    printf(GRAY "[%s] " RESET GREEN "└─ Sending to client\n" RESET, get_timestamp_short());

    printf("\n" INFO "┌─ RESPONSE METADATA ────────────────────────────────────────┐" RESET "\n");
    printf(INFO "│ Packet Type      : ACK (Data Acknowledgement)\n");
    printf(INFO "│ Source IP        : %s\n", src_ip);
    printf(INFO "│ Source Port      : %d (0x%04X)\n", src_port, src_port);
    printf(INFO "│ Payload Size     : %d bytes\n", length);
    printf(INFO "│ Flags            : [ACK=1, PSH=1]\n");
    printf(INFO "└─────────────────────────────────────────────────────────────┘" RESET "\n\n");

    printf(BINARY "┌─ HEX DUMP ───────────────────────────────────────────────────┐" RESET "\n");
    print_hex_dump(buffer, length);
    printf(BINARY "└──────────────────────────────────────────────────────────────┘" RESET "\n\n");
}

// ============== Client Handler Thread ==============
void* handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    char local_ip[INET_ADDRSTRLEN];
    uint16_t local_port = 0;

    display_socket_info(client->socket_fd, client->client_id, local_ip, &local_port);
    strncpy(client->local_ip, local_ip, INET_ADDRSTRLEN);
    client->local_port = local_port;

    while (server_running && client->active) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client->socket_fd, buffer, BUFFER_SIZE - 1);

        if (bytes_read <= 0) {
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

        pthread_mutex_lock(&print_lock);

        // Display incoming packet
        display_incoming_packet(client->client_id, client->client_ip, client->client_port,
                                 client->local_ip, client->local_port, buffer, bytes_read);

        // Generate response with timestamp and server info
        int response_len = snprintf(response, sizeof(response),
                                   "[%s] ACK<- Server received (%d bytes): %.256s",
                                   get_timestamp_short(), bytes_read, buffer);

        usleep(300000);

        // Display outgoing packet
        display_outgoing_packet(client->client_id, client->local_ip, client->local_port, response, response_len);

        // Send response (write_lock keeps this from colliding with an
        // operator-typed message going out on the same socket)
        pthread_mutex_lock(&client->write_lock);
        int written = write(client->socket_fd, response, response_len);
        pthread_mutex_unlock(&client->write_lock);

        if (written < 0) {
            printf(ERROR "[%s] Failed to send response to client #%d\n\n" RESET,
                   get_timestamp_short(), client->client_id);
            pthread_mutex_unlock(&print_lock);
            break;
        }

        pthread_mutex_lock(&server_state.lock);
        client->bytes_sent += response_len;
        server_state.total_bytes_exchanged += response_len;
        pthread_mutex_unlock(&server_state.lock);

        printf(SUCCESS "\n  ✓ Packet transmitted successfully\n" RESET);
        printf(INFO "  └─ Round-trip complete (waiting for next message, or an operator reply...)\n\n" RESET);

        pthread_mutex_unlock(&print_lock);

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

// ============== Operator Console (server -> client messaging) ==============
void print_console_help() {
    pthread_mutex_lock(&print_lock);
    printf(INFO "\n┌─ SERVER CONSOLE COMMANDS ───────────────────────────────────┐" RESET "\n");
    printf(INFO "│" RESET " @<id> <message>   Send a message to client <id>\n");
    printf(INFO "│" RESET " @all <message>    Broadcast a message to every client\n");
    printf(INFO "│" RESET " /list             Show connected clients\n");
    printf(INFO "│" RESET " /help             Show this help\n");
    printf(INFO "│" RESET " /quit             Shut down the server\n");
    printf(INFO "└─────────────────────────────────────────────────────────────┘" RESET "\n\n");
    pthread_mutex_unlock(&print_lock);
}

// Sends an operator-typed message to one connected client. Returns bytes
// written, or -1 if that client isn't active / the write failed.
int send_to_client(int client_index, const char *msg) {
    pthread_mutex_lock(&server_state.lock);
    Client *c = &server_state.clients[client_index];
    int active = c->active;
    int fd = c->socket_fd;
    int client_id = c->client_id;
    pthread_mutex_unlock(&server_state.lock);

    if (!active) return -1;

    char formatted[BUFFER_SIZE];
    int len = snprintf(formatted, sizeof(formatted), "[%s] MSG<- Server operator: %.4000s",
                        get_timestamp_short(), msg);

    pthread_mutex_lock(&c->write_lock);
    int result = write(fd, formatted, len);
    pthread_mutex_unlock(&c->write_lock);

    pthread_mutex_lock(&print_lock);
    if (result > 0) {
        pthread_mutex_lock(&server_state.lock);
        c->bytes_sent += result;
        server_state.total_bytes_exchanged += result;
        pthread_mutex_unlock(&server_state.lock);
        printf(SUCCESS "  ✓ Sent %d bytes to client #%d: \"%s\"\n\n" RESET, result, client_id, msg);
    } else {
        printf(ERROR "  ✗ Failed to send to client #%d\n\n" RESET, client_id);
    }
    pthread_mutex_unlock(&print_lock);

    return result;
}

void* console_thread(void *arg) {
    (void)arg;
    char line[BUFFER_SIZE];

    print_console_help();

    while (server_running) {
        printf(BOLD "server> " RESET);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) break;
        line[strcspn(line, "\n")] = 0;

        if (strlen(line) == 0) continue;

        if (strcmp(line, "/help") == 0) {
            print_console_help();
            continue;
        }

        if (strcmp(line, "/list") == 0) {
            display_clients_list();
            continue;
        }

        if (strcmp(line, "/quit") == 0) {
            pthread_mutex_lock(&print_lock);
            printf(WARNING "\nShutting down server...\n\n" RESET);
            pthread_mutex_unlock(&print_lock);
            server_running = 0;
            if (g_server_fd >= 0) shutdown(g_server_fd, SHUT_RDWR);
            break;
        }

        if (line[0] == '@') {
            char *space = strchr(line, ' ');
            if (!space || strlen(space + 1) == 0) {
                printf(WARNING "Usage: @<id|all> <message>\n\n" RESET);
                continue;
            }

            char target[32];
            size_t idlen = (size_t)(space - (line + 1));
            if (idlen >= sizeof(target)) idlen = sizeof(target) - 1;
            strncpy(target, line + 1, idlen);
            target[idlen] = '\0';
            const char *msg = space + 1;

            if (strcmp(target, "all") == 0) {
                int ids[MAX_CLIENTS], count = 0;
                pthread_mutex_lock(&server_state.lock);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (server_state.clients[i].active) ids[count++] = i;
                }
                pthread_mutex_unlock(&server_state.lock);

                if (count == 0) {
                    printf(WARNING "⚠ No active clients to send to\n\n" RESET);
                } else {
                    for (int k = 0; k < count; k++) send_to_client(ids[k], msg);
                }
            } else {
                char *endptr;
                long id = strtol(target, &endptr, 10);
                if (*endptr != '\0' || id < 1 || id > MAX_CLIENTS) {
                    printf(ERROR "Invalid client id: %s\n\n" RESET, target);
                    continue;
                }
                send_to_client((int)(id - 1), msg);
            }
            continue;
        }

        printf(WARNING "Unknown command. Type /help for options.\n\n" RESET);
    }

    return NULL;
}

// ============== Banner & Initialization ==============
void print_banner() {
    printf(DARK_GREEN);
    printf("\n");
    printf("  ███████╗███████╗██████╗ ██╗   ██╗███████╗██████╗    \n");
    printf("  ██╔════╝██╔════╝██╔══██╗██║   ██║██╔════╝██╔══██╗   \n");
    printf("  ███████╗█████╗  ██████╔╝██║   ██║█████╗  ██████╔╝   \n");
    printf("  ╚════██║██╔══╝  ██╔══██╗╚██╗ ██╔╝██╔══╝  ██╔══██╗   \n");
    printf("  ███████║███████╗██║  ██║ ╚████╔╝ ███████╗██║  ██║   \n");
    printf("  ╚══════╝╚══════╝╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚═╝  ╚═╝   \n");
    printf(RESET "\n");
    printf(CYAN "  Multi-Client TCP/IP Server with Real-time Binary Visualization\n\n" RESET);
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [bind_ip] [port]\n", prog_name);
    printf("  bind_ip  IPv4 address to bind to (default: 0.0.0.0, all interfaces)\n");
    printf("  port     TCP port to listen on (default: %d)\n\n", DEFAULT_PORT);
}

// ============== Main Server ==============
int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    pid_t server_pid = getpid();

    // ---- Parse bind IP / port from the terminal ----
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc > 1) {
        strncpy(g_bind_ip, argv[1], INET_ADDRSTRLEN - 1);
    }
    if (argc > 2) {
        g_port = atoi(argv[2]);
        if (g_port <= 0 || g_port > 65535) {
            fprintf(stderr, ERROR "Invalid port: %s\n" RESET, argv[2]);
            print_usage(argv[0]);
            return -1;
        }
    }

    print_banner();
    display_system_info();
    display_server_process_info(server_pid);

    // Initialize mutexes
    pthread_mutex_init(&server_state.lock, NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_mutex_init(&server_state.clients[i].write_lock, NULL);
    }

    // Create socket
    printf(GRAY "[%s] " RESET "Creating TCP/IPv4 socket...\n", get_timestamp_short());
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf(ERROR "  ✗ Socket creation failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket created (FD: %d)\n" RESET, server_fd);

    // Set socket options
    printf(GRAY "[%s] " RESET "Configuring socket options...\n", get_timestamp_short());
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        printf(ERROR "  ✗ setsockopt failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Socket options configured\n" RESET);

    // Setup address structure
    address.sin_family = AF_INET;
    address.sin_port = htons(g_port);
    if (strcmp(g_bind_ip, "0.0.0.0") == 0) {
        address.sin_addr.s_addr = INADDR_ANY;
        // Resolve the machine's real address for display purposes only.
        if (!get_local_ip(g_display_ip, sizeof(g_display_ip))) {
            strncpy(g_display_ip, "0.0.0.0", sizeof(g_display_ip));
        }
    } else {
        if (inet_pton(AF_INET, g_bind_ip, &address.sin_addr) != 1) {
            fprintf(stderr, ERROR "Invalid bind IP: %s\n" RESET, g_bind_ip);
            print_usage(argv[0]);
            return -1;
        }
        strncpy(g_display_ip, g_bind_ip, sizeof(g_display_ip));
    }

    // Bind socket
    printf(GRAY "[%s] " RESET "Binding to %s:%d...\n", get_timestamp_short(), g_bind_ip, g_port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf(ERROR "  ✗ Bind failed - Port already in use or address unavailable\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Bound to %s:%d\n" RESET, g_bind_ip, g_port);

    // Listen
    printf(GRAY "[%s] " RESET "Entering listening state...\n", get_timestamp_short());
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        printf(ERROR "  ✗ Listen failed\n" RESET);
        return -1;
    }
    printf(SUCCESS "  ✓ Listening for incoming connections\n" RESET);

    g_server_fd = server_fd;

    printf(WARNING "\n[%s] " RESET "Awaiting client connections on %s:%d...\n\n" RESET,
           get_timestamp_short(), g_display_ip, g_port);

    // Operator console: lets you type real messages to send to connected
    // clients (@<id> <msg>, @all <msg>) instead of only the automatic ACK.
    pthread_t console_tid;
    if (pthread_create(&console_tid, NULL, console_thread, NULL) != 0) {
        printf(WARNING "  ⚠ Could not start operator console (server will still auto-ACK clients)\n\n" RESET);
    } else {
        pthread_detach(console_tid);
    }

    // Main acceptance loop
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_fd < 0) {
            if (server_running) {
                printf(ERROR "Accept error\n" RESET);
            }
            continue;
        }

        // Find available client slot
        pthread_mutex_lock(&server_state.lock);
        int client_index = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!server_state.clients[i].active) {
                client_index = i;
                break;
            }
        }
        pthread_mutex_unlock(&server_state.lock);

        if (client_index == -1) {
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
        client->last_sample_time = client->connected_time;
        client->bytes_sent = 0;
        client->bytes_received = 0;
        client->last_bytes_sent = 0;
        client->last_bytes_received = 0;
        client->active = 1;

        inet_ntop(AF_INET, &client_addr.sin_addr, client->client_ip, INET_ADDRSTRLEN);
        client->client_port = ntohs(client_addr.sin_port);

        pthread_mutex_lock(&server_state.lock);
        server_state.total_clients++;
        server_state.active_clients++;
        pthread_mutex_unlock(&server_state.lock);

        // Print connection established
        pthread_mutex_lock(&print_lock);
        printf(GREEN "╔════════════════════════════════════════════════════════════════╗" RESET "\n");
        printf(GREEN "║" RESET SUCCESS " ✓ CLIENT #%d CONNECTED [FD: %d] " RESET GREEN "                  ║" RESET "\n",
               client->client_id, client_fd);
        printf(GREEN "╚════════════════════════════════════════════════════════════════╝\n" RESET);
        printf(NEON_GREEN "  Client: %s:%d\n" RESET, client->client_ip, client->client_port);
        printf(NEON_GREEN "  Server: %s:%d\n" RESET, g_display_ip, g_port);
        printf(GRAY "  Status: Bidirectional communication active (type @%d <msg> in the server console to chat)\n\n" RESET,
               client->client_id);
        pthread_mutex_unlock(&print_lock);

        // Create thread for this client
        if (pthread_create(&client->thread_id, NULL, handle_client, client) != 0) {
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
