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
    while (keep_looping)
    {
        // Print when we catch the signal
        if (caught_signal && !caught_signal_prev)
        {
            caught_signal_prev = caught_signal;
            print_and_log(LOG_INFO, "%s\n", "Caught signal, exiting");
        }

        if (current_state == Waiting)
        {
            // Join any closed threads
            join_closed_thread();

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

void* main_loop_thread(void* thread_param)
{
    struct thread_data *thread_data_ptr = (struct thread_data *)thread_param;

    enum ThreadState thread_state = WaitingForData;
    bool keep_looping_thread = true;

    char *recv_buffer = NULL;
    size_t recv_buffer_size = 0;
    size_t recv_buffer_allocated_size = 0;


    while(keep_looping_thread)
    {
        if (thread_state == WaitingForData)
        {
            waiting_for_data(&thread_state, thread_data_ptr->client_fd, recv_buffer, &recv_buffer_size, &recv_buffer_allocated_size);
        }
        else if (thread_state == ClosedConnection)
        {
            keep_looping_thread = false;
        }
    }

    thread_cleanup(thread_data_ptr->client_fd, recv_buffer);

    return 0;
}

int create_thread(int c_fd, char* ip_string, size_t ip_string_size)
{
    // Setup linked list values
    struct LinkedListItem *linked_list_item_ptr = NULL;

    linked_list_item_ptr = malloc(sizeof(struct LinkedListItem));
    if (linked_list_item_ptr == NULL)
    {
        // Memory not allocated
        print_and_log(LOG_ERR, "Error: Failed to allocate memory for linked list item!/n");
        return -1;
    }
    
    linked_list_item_ptr->t_data.client_fd = c_fd;
    void* memcpy_rv = memcpy(linked_list_item_ptr->client_ip_string, ip_string, ip_string_size);
    if (memcpy_rv == NULL)
    {
        // Memory not allocated
        print_and_log(LOG_ERR, "Error: Failed to copy ip string into linked list item!/n");
        return -1;
    }

    int pthread_create_rv = pthread_create(&linked_list_item_ptr->thread_id, NULL, main_loop_thread, &linked_list_item_ptr->t_data);
    if (pthread_create_rv != 0)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to create thread!/n");
        return -1;
    }

    SLIST_INSERT_HEAD(&linked_list, linked_list_item_ptr, LinkedListItems);

    return 0;
}

int join_closed_thread()
{
    struct LinkedListItem* linked_list_item_ptr = NULL;
    struct LinkedListItem* linked_list_item_ptr_temp = NULL;
    void* thread_return = NULL;

    SLIST_FOREACH_SAFE(linked_list_item_ptr, &linked_list, LinkedListItems, linked_list_item_ptr_temp)
    {
        int join_rv = pthread_tryjoin_np(linked_list_item_ptr->thread_id, thread_return);
        
        // Check if thread joined
        if (join_rv == 0)
        {
            print_and_log(LOG_INFO, "Closed connection from %s\n", linked_list_item_ptr->client_ip_string);

            // Cleanup
            free(linked_list_item_ptr);
            linked_list_item_ptr = NULL;
            SLIST_REMOVE(&linked_list, linked_list_item_ptr, LinkedListItem, LinkedListItems);
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
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to accept client connection!");
        return -1;
    }

    // Print client IP address
    get_client_ip_address(client_address, ip_string, ip_string_size);
    print_and_log(LOG_INFO, "Accepted connection from %s\n", ip_string);

    current_state = CreatingThread;
    return 0;
}

int waiting_for_data(enum ThreadState *t_state, int c_fd, char* r_buffer, size_t* r_buffer_size, size_t* r_buffer_allocated_size)
{
    char recv_buffer_temp[RECV_BUFFER_SIZE];
    ssize_t recv_bytes = recv(c_fd, recv_buffer_temp, sizeof(recv_buffer_temp), 0);

    if (recv_bytes == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to receive data from client!");
        return -1;
    }

    // Client closed connection
    if (recv_bytes == 0)
    {
        close(c_fd);
        *t_state = ClosedConnection;
        return 0;
    }
    else
    {
        process_recieved_data(t_state, c_fd, r_buffer, r_buffer_size, r_buffer_allocated_size, &recv_buffer_temp[0], recv_bytes);
    }

    return 0;
}

int process_recieved_data(enum ThreadState *t_state, int c_fd, char* r_buffer, size_t* r_buffer_size, size_t* r_buffer_allocated_size, char* r_buffer_temp, size_t r_bytes)
{
    // Grow buffer if needed
    while (*r_buffer_allocated_size < *r_buffer_size + r_bytes + 1)
    {
        *r_buffer_allocated_size += RECV_BUFFER_SIZE;
        char* r_buffer_new = realloc(r_buffer, *r_buffer_allocated_size);
        if (!r_buffer_new)
        {
            return -1;
        }

        r_buffer = r_buffer_new;
    }

    // Copy data to buffer
    memcpy(r_buffer + *r_buffer_size, r_buffer_temp, r_bytes);

    // If newline recieved, write to file, and shift everything forward in the buffer
    int count = 0;
    bool found_packet_end = false;
    size_t new_packet_start = 0;
    size_t remaining = 0;
    while (count < r_bytes)
    {
        // Actions when we recieve complete packet
        if (r_buffer[*r_buffer_size + count] == PACKET_ENDING)
        {
            found_packet_end = true;

            if (get_file_mutex() == -1)
            {
                return -1;
            }

            int f_fd = open_file_for_read_write();
            if (f_fd == -1)
            {
                release_file_mutex();
                return -1;
            }

            if (write_to_file(f_fd, r_buffer, *r_buffer_size + count + 1) == -1)
            {
                close(f_fd);
                release_file_mutex();
                return -1;
            }
            
            if (sending_data(t_state, c_fd, f_fd) == -1)
            {
                close(f_fd);
                release_file_mutex();
                return -1;
            }

            if (close(f_fd) == -1)
            {
                release_file_mutex();
                return -1;
            }

            if (release_file_mutex() == -1)
            {
                return -1;
            }

            new_packet_start = *r_buffer_size + count + 1;
            remaining  = (size_t)r_bytes - (count + 1);
            memmove(r_buffer, r_buffer + new_packet_start, remaining);
            break;
        }
        count++;
    }

    if (found_packet_end)
    {
        *r_buffer_size = remaining;
    }
    else
    {
        *r_buffer_size += r_bytes;
    }

    r_buffer[*r_buffer_size] = '\0'; // Null terminate for printouts
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
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    return fd;
}

int sending_data(enum ThreadState *t_state, int c_fd, int f_fd)
{
    // Read the data from the file
    char send_buffer[SEND_BUFFER_SIZE];
    ssize_t bytes_read;
    ssize_t sent_bytes;
    ssize_t total_bytes_sent;
    while ((bytes_read = read_from_file(f_fd, send_buffer, sizeof(send_buffer))) > 0)
    {
        total_bytes_sent = 0;

        while (total_bytes_sent < bytes_read)
        {
            sent_bytes = send(c_fd, send_buffer + total_bytes_sent, bytes_read - total_bytes_sent, 0);
            if (sent_bytes == -1)
            {
                print_and_log(LOG_ERR, "%s\n", "Error: Failed to send data to client!");
                return -1;
            }
            total_bytes_sent += sent_bytes;
        }
    }

    if (bytes_read == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to read from file '%s'!\n", FILE_PATH);
        return -1;
    }

    *t_state = WaitingForData;

    return 0;
}

void thread_cleanup(int c_fd, char* r_buffer)
{
    if (c_fd != -1)
    {
        close(c_fd);
        c_fd = -1;
    }

    // Free buffers
    if (r_buffer)
    {
        free(r_buffer);
        r_buffer = NULL;
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
        print_and_log(LOG_ERR, "Error: Failed to read from file '%s'!\n");
        return -1;
    }
    else
    {
        buffer[bytes_read] = '\0'; // Null-terminate the buffer
        print_and_log(LOG_INFO, "Successfully read %zd bytes from file '%s'\n", bytes_read);
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