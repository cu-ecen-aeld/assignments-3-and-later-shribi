#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/queue.h>

bool SIGNAL_RECEIVED = false;

// file write mutex
pthread_mutex_t file_write_mutex;

struct thread_node {
    pthread_t thread_id;
    int client_sockfd;
    struct sockaddr_in client_addr;
    bool active;
    SLIST_ENTRY(thread_node) entries;
};
SLIST_HEAD(thread_list_head, thread_node);
struct thread_list_head thread_list = SLIST_HEAD_INITIALIZER(thread_list);

void signal_handler(int signum) {
    syslog(LOG_INFO, "Caught signal %d, exiting", signum);
    SIGNAL_RECEIVED = true;
}

void *time_logger(void *arg) {
    (void)arg; // unused parameter
    while (!SIGNAL_RECEIVED) {
        for (int i = 0; i < 10 && !SIGNAL_RECEIVED; i++) {
            sleep(1);
        }
        if (SIGNAL_RECEIVED) break;

        pthread_mutex_lock(&file_write_mutex);
        FILE *fp = fopen("/var/tmp/aesdsocketdata", "a");
        if (fp == NULL) {
            perror("fopen");
            pthread_mutex_unlock(&file_write_mutex);
            continue;
        }
        time_t now = time(NULL);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%a %b %d %Y %H:%M:%S %z\n", &tm_info);
    
        // logging time
        printf("Logging time: %s", timestamp);

        char line[128];
        snprintf(line, sizeof(line), "timestamp:%s\n", timestamp);
        fputs(line, fp);
        fclose(fp);
        pthread_mutex_unlock(&file_write_mutex);
    }
    return NULL;
}

void *client_handler(void *arg) {
    struct thread_node *client_node = (struct thread_node *)arg;
    int client_sockfd = client_node->client_sockfd;
    // Accumulates data across recv() calls as a null-terminated string until a newline-terminated packet is available
    size_t buf_size = 1024;
    char *buffer = malloc(buf_size);
    if (buffer == NULL) {
        perror("malloc");
        close(client_sockfd);
        return NULL;
    }
    buffer[0] = '\0';

    pthread_mutex_lock(&file_write_mutex);
    FILE *fp = fopen("/var/tmp/aesdsocketdata", "a");
    if (fp == NULL) {
        perror("fopen");
        free(buffer);
        close(client_sockfd);
        return NULL;
    }

    char recv_chunk[1024];
    ssize_t bytes_recvd;
    while ((bytes_recvd = recv(client_sockfd, recv_chunk, sizeof(recv_chunk) - 1, 0)) > 0) {
        recv_chunk[bytes_recvd] = '\0';
        size_t needed = strlen(buffer) + strlen(recv_chunk) + 1;
        if (needed > buf_size) {
            char *new_buffer = realloc(buffer, needed);
            if (new_buffer == NULL) {
                perror("realloc");
                break; // discard this over-length packet and drop the connection
            }
            buffer = new_buffer;
            buf_size = needed;
        }
        strcat(buffer, recv_chunk);

        // Write out every complete (newline-terminated) packet currently buffered
        char *newline_pos;
        while ((newline_pos = strchr(buffer, '\n')) != NULL) {
            size_t packet_len = newline_pos - buffer + 1;

            if (fwrite(buffer, 1, packet_len, fp) != packet_len) {
                perror("fwrite");
                break;
            }
            fflush(fp);

            // Stream the full file back in fixed-size chunks; it may be larger than available heap/RAM
            FILE *read_fp = fopen("/var/tmp/aesdsocketdata", "r");
            if (read_fp == NULL) {
                perror("fopen");
            } else {
                char send_chunk[1024];
                size_t n;
                while ((n = fread(send_chunk, 1, sizeof(send_chunk), read_fp)) > 0) {
                    size_t sent_total = 0;
                    while (sent_total < n) {
                        ssize_t sent = send(client_sockfd, send_chunk + sent_total, n - sent_total, 0);
                        if (sent < 0) {
                            perror("send");
                            break;
                        }
                        sent_total += (size_t)sent;
                    }
                }
                fclose(read_fp);
            }

            // Shift the leftover partial packet (if any) to the front, including its null terminator
            memmove(buffer, newline_pos + 1, strlen(newline_pos + 1) + 1);
        }
    }
    if (bytes_recvd < 0) {
        perror("recv");
        return NULL;
    }

    fclose(fp);
    pthread_mutex_unlock(&file_write_mutex);
    free(buffer);
    close(client_sockfd);
    syslog(LOG_INFO, "Closed connection from %s:%d", inet_ntoa(client_node->client_addr.sin_addr), ntohs(client_node->client_addr.sin_port));
    client_node->active = false;
    return NULL;
}

void cleanup_thread_list(void) {
    struct thread_node *first = SLIST_FIRST(&thread_list);
    while (first != NULL) {
        struct thread_node * next = SLIST_NEXT(first, entries);
        if (!first->active) {
            pthread_join(first->thread_id, NULL);
            SLIST_REMOVE(&thread_list, first, thread_node, entries);
            free(first);
        }
        first = next;
    }
}

int main(int argc, char *argv[]) {
    bool daemonize = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemonize = true;
    }

    // Create syslog logger 
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    // Ensure a clean slate in case a prior run left this behind (e.g. was killed with SIGKILL)
    remove("/var/tmp/aesdsocketdata");

    // Open a stream socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Bind the socker to 9000
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(9000);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (daemonize) {
        // Daemonize the process
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(sockfd);
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Parent process exits
            exit(EXIT_SUCCESS);
        }
        // Child process continues
        if (setsid() < 0) {
            perror("setsid");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }

    // Listen for incoming connections
    if (listen(sockfd, 10) < 0) {
        perror("listen");
        close(sockfd);
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Register to SIGINT and SIGTERM 
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    pthread_t time_logger_thread;
    if (pthread_create(&time_logger_thread, NULL, time_logger, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // Accept and serve connections one at a time, looping forever to handle each new client
    while (!SIGNAL_RECEIVED) {        
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd < 0) {
            perror("accept");
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Assign a dedicated thread to handle the client connection and store the ID in a linked list and monitor via a flag wo join gracefully
        struct thread_node * new_conn = malloc(sizeof(*new_conn));
        if (new_conn == NULL) {
            perror("malloc");
            close(client_sockfd);
            continue;
        }
        new_conn->client_sockfd = client_sockfd;
        new_conn->active = true;
        new_conn->client_addr = client_addr;

        if (pthread_create(&new_conn->thread_id, NULL, client_handler, (void *)new_conn) != 0) {
            perror("pthread_create");
            close(client_sockfd);
            continue;
        }

        SLIST_INSERT_HEAD(&thread_list, new_conn, entries);
        cleanup_thread_list();
    }

    cleanup_thread_list();

    syslog(LOG_INFO, "Exiting");
    closelog();
    close(sockfd);

    pthread_join(time_logger_thread, NULL);
    remove("/var/tmp/aesdsocketdata");
    printf("------------Server Exiting--------------\n");
    return 0;
}