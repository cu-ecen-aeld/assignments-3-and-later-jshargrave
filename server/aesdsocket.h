/*
    Author: Joseph Hargrave
    Date: 2/15/2026
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>
#include <syslog.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>

#define SOCKET_PORT "9000"
#define RECV_BUFFER_SIZE 1024
#define PACKET_BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define PACKET_ENDING '\n'

enum State { 
    Startup,
    WaitingForClient,
    RecievingData,
    SendingData,
    ShuttingDown
};

enum State current_state;
bool keep_looping = true;


int main();
int main_loop();
int startup();
int waiting_for_client(int socket_fd, struct sockaddr client_address, socklen_t client_address_len, char* ip_string, size_t ip_string_size);
int recieving_data(int accept_fd, char **recv_buffer, size_t *total_buffer_size, size_t *total_allocated_size, char* ip_string);
int sending_data(int accept_fd, char *send_buffer, size_t total_file_size, size_t total_sent_bytes);
int shutting_down(int socket_fd, char *recv_buffer, struct addrinfo *res);
void print_and_log(int level, const char *format, ...);
void process_client_data(const char *data, ssize_t data_length);
int write_to_file(const char *filename, const char *data, ssize_t data_length);
int read_from_file(const char *filename, char *buffer, size_t buffer_size);
int get_file_size(const char *filename);
void get_client_ip_address(struct sockaddr client_address, char* ip_string, size_t ip_string_size);



