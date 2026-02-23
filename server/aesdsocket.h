/*
    Author: Joseph Hargrave
    Date: 2/15/2026
*/

#ifndef AESD_SOCKET
#define AESD_SOCKET

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
#define RECV_BUFFER_GROW_SIZE 1024
#define RECV_BUFFER_TEMP_SIZE 1024
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
    ProcessingData,
    ClosedConnection
};

struct ThreadData{
    int client_fd;
    enum ThreadState thread_state;
    bool thread_complete_success;
};

// LINKED LIST
struct LinkedListItem
{
    pthread_t thread_id;
    char client_ip_string[INET_ADDRSTRLEN];
    SLIST_ENTRY(LinkedListItem) LinkedListItems;
};

SLIST_HEAD(LinkedListHead, LinkedListItem);

struct LinkedListHead linked_list;

// MUTEX
pthread_mutex_t file_mutex;

// Buffer structure
struct BufferData
{
    char* buffer;
    size_t size;
    size_t allocated;
};

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
    Main entry point into the program. Handles argument parsing and splits 
    depending on if the user passed the daemon arguemnt.
*/
int main(int argc, char *argv[]);

/*
    Main loop where the main thread waits for connections from clients, 
    starts threads, and joins threads. No more client connections are 
    accepted after catching exit signals.
*/
int main_loop();


void* main_loop_thread(void* thread_param);
int main_loop_fork();
int startup();
int check_for_client_connection(int* c_fd, char* ip_string, int ip_string_size);
int waiting_for_data(struct ThreadData *t_data, struct BufferData *rb_data, struct BufferData *rb_data_temp);
int sending_data(struct ThreadData *t_data, struct BufferData *rb_data, struct BufferData *rb_data_new);
size_t process_received_data(struct ThreadData *t_data, struct BufferData *rb_data, struct BufferData *rb_data_new);
int shutting_down();
void print_and_log(int level, const char *format, ...);
void process_client_data(const char *data, ssize_t data_length);
int open_file_for_read_write();
int write_to_file(int fd, const char *data, ssize_t data_length);
int read_from_file(int fd, char *buffer, size_t buffer_size);
int get_file_size(const char *filename);
void get_client_ip_address(struct sockaddr_storage client_address, char* ip_string, size_t ip_string_size);
static void signal_handler(int signal_number);
int join_threads();
bool all_threads_closed();
int create_thread(int c_fd, char* ip_string, size_t ip_string_size);
void thread_cleanup(struct BufferData* rb_data, struct BufferData* rb_data_buffer);
int release_file_mutex();
int get_file_mutex();

#endif