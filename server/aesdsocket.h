/*
    Author: Joseph Hargrave
    Date: 2/15/2026
*/

#define _GNU_SOURCE
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
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "queue.h"

#define SOCKET_PORT "9000"
#define RECV_BUFFER_SIZE 1024
#define SEND_BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define PACKET_ENDING '\n'

// STATES
enum State { 
    None,
    Startup,
    CreatingThread,
    ShuttingDown,
    Waiting
};

// THREAD
enum ThreadState
{
    SendingData,
    WaitingForData,
    ClosedConnection
};

struct thread_data{
    int client_fd;
    bool thread_complete_success;
};

// LINKED LIST
struct LinkedListItem
{
    pthread_t thread_id;
    char client_ip_string[INET_ADDRSTRLEN];
    struct thread_data t_data;
    SLIST_ENTRY(LinkedListItem) LinkedListItems;
};

SLIST_HEAD(LinkedListHead, LinkedListItem);

struct LinkedListHead linked_list;

// MUTEX
pthread_mutex_t file_mutex;

// Variable Setup
enum State current_state = None;
bool keep_looping = true;
int socket_fd = 0;
struct addrinfo * res = NULL;
struct addrinfo hints;

struct sockaddr_storage client_address;
socklen_t client_address_len = sizeof(client_address);

// Signal flag
static volatile sig_atomic_t caught_signal = 0;

// Argument Flags
bool daemon_enabled = false;



/*

*/
int main();

/*

*/
int main_loop();


void* main_loop_thread(void* thread_param);
int main_loop_fork();
int startup();
int check_for_client_connection(int* c_fd, char* ip_string, int ip_string_size);
int waiting_for_data(enum ThreadState *t_state, int c_fd, char* r_buffer, size_t* r_buffer_size, size_t* r_buffer_allocated_size);
int sending_data(enum ThreadState *t_state, int c_fd, int f_fd);
int shutting_down();
void print_and_log(int level, const char *format, ...);
void process_client_data(const char *data, ssize_t data_length);
int process_recieved_data(enum ThreadState *t_state, int c_fd, char* r_buffer, size_t* r_buffer_size, size_t* r_buffer_allocated_size, char* r_buffer_temp, size_t r_bytes);
int open_file_for_read_write();
int write_to_file(int fd, const char *data, ssize_t data_length);
int read_from_file(int fd, char *buffer, size_t buffer_size);
int get_file_size(const char *filename);
void get_client_ip_address(struct sockaddr_storage client_address, char* ip_string, size_t ip_string_size);
static void signal_handler(int signal_number);
int join_closed_thread();
bool all_threads_closed();
int create_thread(int c_fd, char* ip_string, size_t ip_string_size);
void thread_cleanup(int c_fd, char* r_buffer);
int release_file_mutex();
int get_file_mutex();