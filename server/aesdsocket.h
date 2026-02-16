/*
    Author: Joseph Hargrave
    Date: 2/15/2026
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
    None,
    Startup,
    WaitingForClient,
    RecievingData,
    SendingData,
    ShuttingDown
};

// Variable Setup
enum State current_state;
bool keep_looping = true;
int socket_fd = 0;
int client_fd = 0;
struct addrinfo * res = NULL;
struct sockaddr_storage client_address;
socklen_t client_address_len = sizeof(client_address);
char ip_string[INET_ADDRSTRLEN];
struct addrinfo hints;

char *send_buffer = NULL;
char *recv_buffer = NULL;
size_t recv_allocated_size = 0; 
size_t recv_buffer_size = 0; 
size_t send_buffer_size = 0; 
size_t send_bytes_sent = 0; 


int main();
int main_loop();
int startup();
int waiting_for_client();
int recieving_data();
int sending_data();
int shutting_down();
void print_and_log(int level, const char *format, ...);
void process_client_data(const char *data, ssize_t data_length);
int write_to_file(const char *filename, const char *data, ssize_t data_length);
int read_from_file(const char *filename, char *buffer, size_t buffer_size);
int get_file_size(const char *filename);
void get_client_ip_address(struct sockaddr_storage client_address, char* ip_string, size_t ip_string_size);



