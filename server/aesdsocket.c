/*
    Author: Joseph Hargrave
    Date: 2/15/2026
*/

#include "aesdsocket.h"



int main(int argc, char *argv[])
{
    if (argc >= 2 && strcmp(argv[1], "-d") == 0)
    {
        daemon_enabled = true;
    }

    if (startup() != -1)
    {
        if (daemon_enabled)
        {
            main_loop_fork();
        }
        else
        {
            main_loop();
        }
    }

    shutting_down();

    return 0;
}

int main_loop()
{
    // Main loop
    int client_fd;
    char ip_string[INET_ADDRSTRLEN];

    current_state = Waiting;
    bool caught_signal_prev = false;

    // Time
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    time_t next_timestamp = tp.tv_sec + 10;

    while (keep_looping)
    {
        // Print when we catch the signal
        if (caught_signal && !caught_signal_prev)
        {
            caught_signal_prev = caught_signal;
            print_and_log(LOG_INFO, "%s\n", "Caught signal, exiting");
        }

        // Write time stamp if 10 sec passed
        clock_gettime(CLOCK_MONOTONIC, &tp);
        if (tp.tv_sec >= next_timestamp)
        {
            next_timestamp = tp.tv_sec + 10;
            write_time_stamp();
        }

        if (current_state == Waiting)
        {
            // Join any closed threads
            join_threads();

            // If signal caught and all threads closed exit main loop
            if (all_threads_closed() && caught_signal)
            {
                keep_looping = false;
            }

            // Don't accept any more connections once the signal is caught
            if (!caught_signal)
            {
                check_for_client_connection(&client_fd, &ip_string[0], sizeof(ip_string));
            }
        }
        else if (current_state == CreatingThread)
        {
            create_thread(client_fd, &ip_string[0], sizeof(ip_string));
            current_state = Waiting;
        }
    }
    return 0;
}

int write_time_stamp()
{
    print_and_log(LOG_INFO, "Writing timestamp!\n");

    // Get the local time and store in char array
    time_t epoch_time;
    struct tm *local_time;
    char time_buffer[TIME_BUFFER_SIZE];
    time(&epoch_time);
    local_time = localtime(&epoch_time);
    size_t strftime_rv = strftime(&time_buffer[0], sizeof(time_buffer), TIME_FORMAT, local_time);
    if (strftime_rv == 0)
    {
        print_and_log(LOG_ERR, "Error: Failed to format the time!\n");
        return -1;
    }

    // Get mutex
    if (get_file_mutex() == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to get mutex!\n");
        return -1;
    }

    int f_fd = open_file_for_read_write();
    if (f_fd == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to open file '%s'!\n", FILE_PATH);
        release_file_mutex();
        return -1;
    }



    if (write_to_file(f_fd, time_buffer, strlen(time_buffer)) == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to write to file '%s'!\n", FILE_PATH);
        close(f_fd);
        release_file_mutex();
        return -1;
    }

    if (close(f_fd) == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to close file '%s'!\n", FILE_PATH);
        release_file_mutex();
        return -1;
    }

    if (release_file_mutex() == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to release mutex!\n");
        return -1;
    }

    return 0;
}


void* main_loop_thread(void* thread_param)
{
    struct ThreadData *thread_data_ptr = (struct ThreadData *)thread_param;

    bool keep_looping_thread = true;

    struct BufferData *receive_buffer_data = malloc(sizeof(struct BufferData));
    receive_buffer_data->buffer = NULL;
    receive_buffer_data->allocated = 0;
    receive_buffer_data->size = 0;

    struct BufferData *receive_buffer_data_new = malloc(sizeof(struct BufferData));
    receive_buffer_data_new->buffer = malloc(RECV_BUFFER_TEMP_SIZE);
    receive_buffer_data_new->allocated = RECV_BUFFER_TEMP_SIZE;
    receive_buffer_data_new->size = 0;

    while(keep_looping_thread)
    {
        // Exit all threads if signal received
        if (caught_signal)
        {
            keep_looping_thread = false;
        }

        if (thread_data_ptr->thread_state == WaitingForData)
        {
            if (waiting_for_data(thread_data_ptr, receive_buffer_data, receive_buffer_data_new) == -1)
            {
                thread_data_ptr->thread_state = ClosedConnection;
            }
        }
        else if (thread_data_ptr->thread_state == ProcessingData)
        {
            if (process_received_data(thread_data_ptr, receive_buffer_data, receive_buffer_data_new) == -1)
            {
                thread_data_ptr->thread_state = ClosedConnection;
            }
        }
        else if (thread_data_ptr->thread_state == SendingData)
        {
            if (sending_data(thread_data_ptr, receive_buffer_data, receive_buffer_data_new) == -1)
            {
                thread_data_ptr->thread_state = ClosedConnection;
            }
        }
        else if (thread_data_ptr->thread_state == ClosedConnection)
        {
            keep_looping_thread = false;
        }
    }

    // Cleanup local thread variables
    thread_cleanup(receive_buffer_data, receive_buffer_data_new);

    return thread_data_ptr;
}

int create_thread(int c_fd, char* ip_string, size_t ip_string_size)
{
    // Setup linked list and thread data values
    struct LinkedListItem *linked_list_item_ptr = NULL;
    struct ThreadData * thread_data_ptr = NULL;

    linked_list_item_ptr = malloc(sizeof(struct LinkedListItem));
    if (linked_list_item_ptr == NULL)
    {
        // Memory not allocated
        print_and_log(LOG_ERR, "Error: Failed to allocate memory for linked list item!\n");
        return -1;
    }
    
    void* memcpy_rv = memcpy(linked_list_item_ptr->client_ip_string, ip_string, ip_string_size);
    if (memcpy_rv == NULL)
    {
        // Memory not allocated
        print_and_log(LOG_ERR, "Error: Failed to copy ip string into linked list item!\n");
        return -1;
    }

    thread_data_ptr = malloc(sizeof(struct ThreadData));
    if (thread_data_ptr == NULL)
    {
        // Memory not allocated
        print_and_log(LOG_ERR, "Error: Failed to allocate memory for thread data!\n");
        return -1;
    }

    thread_data_ptr->client_fd = c_fd;
    thread_data_ptr->thread_state = WaitingForData;


    int pthread_create_rv = pthread_create(&linked_list_item_ptr->thread_id, NULL, main_loop_thread, thread_data_ptr);
    if (pthread_create_rv != 0)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to create thread!\n");
        return -1;
    }

    SLIST_INSERT_HEAD(&linked_list, linked_list_item_ptr, LinkedListItems);

    return 0;
}

int join_threads()
{
    struct LinkedListItem* linked_list_item_ptr = NULL;
    struct LinkedListItem* linked_list_item_ptr_temp = NULL;
    struct ThreadData *thread_data_ptr;
    void* thread_return = NULL;

    SLIST_FOREACH_SAFE(linked_list_item_ptr, &linked_list, LinkedListItems, linked_list_item_ptr_temp)
    {
        int join_rv = pthread_tryjoin_np(linked_list_item_ptr->thread_id, &thread_return);
        
        // Check if thread joined
        if (join_rv == 0)
        {
            print_and_log(LOG_INFO, "Closed connection from %s\n", linked_list_item_ptr->client_ip_string);
            
            thread_data_ptr = (struct ThreadData *)thread_return;
            close(thread_data_ptr->client_fd);

            // Remove Linked List item
            SLIST_REMOVE(&linked_list, linked_list_item_ptr, LinkedListItem, LinkedListItems);

            // Cleanup
            if (linked_list_item_ptr != NULL)
            {
                free(linked_list_item_ptr);
                linked_list_item_ptr = NULL;
            }

            if (thread_data_ptr != NULL)
            {
                free(thread_data_ptr);
                thread_data_ptr = NULL;
            }
        }
    }

    return 0;
}

bool all_threads_closed()
{
    if (SLIST_EMPTY(&linked_list))
    {
        return true;
    }
    else
    {
        return false;
    }
}


int main_loop_fork()
{
    pid_t fork_pid = fork();
    if (fork_pid == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to create child!");
        return -1;
    }
    else if (fork_pid == 0)
    {
        // Child logic
        int setsid_return_value = setsid();
        if (setsid_return_value == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "Error: setsid failed in child!");
            return -1;
        }

        int chdir_return_value = chdir("/");
        if (chdir_return_value == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "Error: chdir failed in child!");
            return -1;
        }

        // Redirect output
        int fd = open("/dev/null", O_RDWR);
        if (fd == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "Error: Failed to get file descriptor for /dev/null in child!");
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "Error: Failed to redirect STDIN_FILENO to /dev/null in child!");
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "Error: Failed to redirect STDOUT_FILENO to /dev/null in child!");
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "Error: Failed to redirect STDERR_FILENO to /dev/null in child!");
            return -1;
        }

        close(fd);

        // Start main loop
        main_loop();
        return 0;
    }
    else
    {
        // Parent logic
        exit(0);
    }
    return 0;
}

int startup()
{
    current_state = Startup;

    // Linked List init
    SLIST_INIT(&linked_list);

    // Mutex
    pthread_mutex_init(&file_mutex, NULL);

    // Setup logger
    openlog(NULL, LOG_ODELAY, LOG_USER);
    print_and_log(LOG_INFO, "%s\n", "AESD Socket Starting!");

    // Variable Setup - Anything that couldn't be done in .h file
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags    = AI_PASSIVE;   // bind()

    // Signals
    struct sigaction signal_action;
    memset(&signal_action, 0, sizeof(signal_action));
    signal_action.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &signal_action, NULL) != 0)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to register SIGTERM signal handler!");
        return -1;
    }

    if (sigaction(SIGINT, &signal_action, NULL) != 0)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to register SIGINT signal handler!");
        return -1;
    }

    // Get address info
    int getaddrinfo_return_value = getaddrinfo(NULL, SOCKET_PORT, &hints, &res);
    if (getaddrinfo_return_value != 0)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to get address info!");
        return -1;
    }

    // Socket file descriptor
    socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socket_fd == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to get socket file descriptor!");
        return -1;
    }

    // Socket bind
    int bind_return_value = bind(socket_fd, res->ai_addr, res->ai_addrlen);
    if (bind_return_value == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to bind socket!");
        return -1;
    }

    // Socket listen
    int listen_return_value = listen(socket_fd, 10);
    if (listen_return_value == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to listen on socket!");
        return -1;
    }

    // Change socket to NonBlocking
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to get socket flags!");
        return -1;
    }

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to set socket non-blocking!");
        return -1;
    }

    return 0;
}

int check_for_client_connection(int* c_fd, char* ip_string, int ip_string_size)
{
    client_address_len = sizeof(client_address);
    *c_fd = accept(socket_fd, (struct sockaddr *)&client_address, &client_address_len);
    if (*c_fd == -1)
    {
        // If signal triggerd this fail don't print this error
        if (errno == EINTR && caught_signal)
        {
            return 0;
        }
        return -1;
    }

    // Print client IP address
    get_client_ip_address(client_address, ip_string, ip_string_size);
    print_and_log(LOG_INFO, "Accepted connection from %s\n", ip_string);

    current_state = CreatingThread;
    return 0;
}

int waiting_for_data(struct ThreadData *t_data, struct BufferData *rb_data, struct BufferData *rb_data_new)
{
    // Get data from client
    ssize_t received_bytes = recv(t_data->client_fd, rb_data_new->buffer, rb_data_new->allocated, 0);

    if (received_bytes == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to receive data from client!");
        return -1;
    }

    // Client closed connection
    if (received_bytes == 0)
    {
        t_data->thread_state = ClosedConnection;
        return 0;
    }

    rb_data_new->size = received_bytes;

    // Grow buffer if needed
    while (rb_data->allocated < rb_data->size + rb_data_new->size)
    {
        rb_data->allocated += RECV_BUFFER_GROW_SIZE;
        char* r_buffer_new = realloc(rb_data->buffer, rb_data->allocated);
        if (!r_buffer_new)
        {
            return -1;
        }

        rb_data->buffer = r_buffer_new;
    }

    // Ready to proccess data
    t_data->thread_state = ProcessingData;
    return 0;
}

size_t process_received_data(struct ThreadData *t_data, struct BufferData *rb_data, struct BufferData *rb_data_new)
{
    size_t index = 0;
    bool found_end = false;
    while (index < rb_data_new->size)
    {
        // If we receive a complete packet then break out of loop
        if (rb_data_new->buffer[index] == PACKET_ENDING)
        {
            found_end = true;
            break;
        }
        index++;
    }

    // If we found end byte then transition to sending data, otherwise wait for additional data
    if (found_end)
    {
        // Copy data up to new packet into main buffer
        memcpy(rb_data->buffer + rb_data->size, rb_data_new->buffer, index + 1);
        rb_data->size += index + 1;

        // Re-size new buffer data
        memmove(rb_data_new->buffer, rb_data_new->buffer + (index + 1), rb_data_new->size - (index + 1));
        rb_data_new->size -= index + 1;

        // Send data
        t_data->thread_state = SendingData;
    }
    else
    {
        // Copy all data into main buffer
        memcpy(rb_data->buffer + rb_data->size, rb_data_new->buffer, rb_data_new->size);
        rb_data->size += rb_data_new->size;

        // Resize new buffer data
        rb_data_new->size = 0;

        // Wait for more data
        t_data->thread_state = WaitingForData;
    }

    return 0;
}

int sending_data(struct ThreadData *t_data, struct BufferData *rb_data, struct BufferData *rb_data_temp)
{
    // Get mutex
    if (get_file_mutex() == -1)
    {
        return -1;
    }

    int f_fd = open_file_for_read_write();
    if (f_fd == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to open file '%s'!\n", FILE_PATH);
        release_file_mutex();
        return -1;
    }

    if (write_to_file(f_fd, rb_data->buffer, rb_data->size) == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to write to file '%s'!\n", FILE_PATH);
        close(f_fd);
        release_file_mutex();
        return -1;
    }

    // Reset receive buffer
    rb_data->size = 0;

    // Reset file to start
    lseek(f_fd, 0, SEEK_SET);

    // Read all bytes from file and send back to client
    char send_buffer[SEND_BUFFER_SIZE];
    ssize_t bytes_read;
    ssize_t sent_bytes;
    ssize_t total_bytes_sent;
    while ((bytes_read = read_from_file(f_fd, send_buffer, sizeof(send_buffer))) > 0)
    {
        total_bytes_sent = 0;

        // Send loop
        while (total_bytes_sent < bytes_read)
        {
            sent_bytes = send(t_data->client_fd, send_buffer + total_bytes_sent, bytes_read - total_bytes_sent, 0);
            if (sent_bytes == -1)
            {
                print_and_log(LOG_ERR, "%s\n", "Error: Failed to send data to client!");
                close(f_fd);
                release_file_mutex();
                return -1;
            }
            total_bytes_sent += sent_bytes;
        }
    }

    if (bytes_read == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to read from file '%s'!\n", FILE_PATH);
        close(f_fd);
        release_file_mutex();
        return -1;
    }

    if (close(f_fd) == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to close file '%s'!\n", FILE_PATH);
        release_file_mutex();
        return -1;
    }

    if (release_file_mutex() == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to release mutex!\n");
        return -1;
    }

    // Transition back to processing if there is still more data to process
    // Otherwise wait for more data from client
    if (rb_data_temp->size > 0)
    {
        t_data->thread_state = ProcessingData;
    }
    else
    {
        t_data->thread_state = WaitingForData;
    }

    return 0;
}

int get_file_mutex()
{
    // Get mutex
    int rc = pthread_mutex_lock(&file_mutex);
    if (rc != 0)
    {
        print_and_log(LOG_ERR, "Error: Failed to get file mutex!\n");
        return -1;
    }
    return 0;
}

int release_file_mutex()
{
    // Release mutex
    int rc = pthread_mutex_unlock(&file_mutex);
    if (rc != 0)
    {
        print_and_log(LOG_ERR, "Error: Failed to release file mutex!\n");
        return -1;
    }
    return 0;
}

int open_file_for_read_write()
{
    // Open the file for writing
    int fd = open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to open file '%s' for writing!\n", FILE_PATH);
        return -1;
    }

    return fd;
}

void thread_cleanup(struct BufferData* rb_data, struct BufferData* rb_data_new)
{
    // Free buffers
    if (rb_data)
    {
        if (rb_data->buffer)
        {
            free(rb_data->buffer);
            rb_data->buffer = NULL;
        }
        free(rb_data);
        rb_data = NULL;
    }

    if (rb_data_new)
    {
        if (rb_data_new->buffer)
        {
            free(rb_data_new->buffer);
            rb_data_new->buffer = NULL;
        }
        free(rb_data_new);
        rb_data_new = NULL;
    }
}

int shutting_down()
{
    current_state = ShuttingDown;

    print_and_log(LOG_INFO, "%s\n", "AESD Socket Stopping!");

    if (res)
    {
        freeaddrinfo(res);
        res = NULL;
    }

    // Close sockets
    if (socket_fd != -1)
    {
        close(socket_fd);
        socket_fd = -1;
    }

    // Delete tmp file
    int unlink_return_value = unlink(FILE_PATH);
    if (unlink_return_value == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to delete temp file '%s'!\n", FILE_PATH);
    }

    closelog();

    return 0;
}

void print_and_log(int level, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(format, args);       // stdout
    va_end(args);

    va_start(args, format);
    vsyslog(level, format, args); // syslog
    va_end(args);
}

int write_to_file(int fd, const char *data, ssize_t data_length)
{
    // Write the data to the file
    ssize_t bytes_written = write(fd, data, data_length);
    if (bytes_written == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to write to file!\n");
        return -1;
    }

    if (bytes_written != data_length)
    {
        print_and_log(LOG_ERR, "Error: Did not write all bytes to file!\n");
        return -1;
    }

    // Close the file descriptor
    return bytes_written;
}

int read_from_file(int fd, char *buffer, size_t buffer_size)
{
    // Read the data from the file
    ssize_t bytes_read = read(fd, buffer, buffer_size - 1); // Leave space for null terminator
    if (bytes_read == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to read from file!\n");
        return -1;
    }
    else
    {
        buffer[bytes_read] = '\0'; // Null-terminate the buffer
        print_and_log(LOG_INFO, "Successfully read %zd bytes from file!\n", bytes_read);
    }

    // Close the file descriptor
    return bytes_read;
}

int get_file_size(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) != 0)
    {
        print_and_log(LOG_ERR, "Error: stat failed for '%s'\n", filename);
        return -1;
    }

    return (int)st.st_size;
}

void get_client_ip_address(struct sockaddr_storage client_address, char* ip_string, size_t ip_string_size)
{
    struct sockaddr_in *addr_in_ptr = (struct sockaddr_in *)&client_address;
    inet_ntop(AF_INET, &addr_in_ptr->sin_addr, ip_string, ip_string_size);
}

static void signal_handler(int signal_number)
{
    if (signal_number == SIGINT || signal_number == SIGTERM)
    {
        caught_signal = 1;
    }
}