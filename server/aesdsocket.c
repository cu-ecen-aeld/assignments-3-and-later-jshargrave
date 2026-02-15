/*
    Author: Joseph Hargrave
    Date: 2/15/2026
*/

#include "aesdsocket.h"

int main()
{
    startup();
}

int main_loop(int socket_fd, struct sockaddr client_address, socklen_t client_address_len, struct addrinfo *res)
{
    // Main loop
    char *send_buffer = NULL;
    char *recv_buffer = NULL;
    char *recv_buffer_temp = NULL;
    size_t total_sent_bytes = 0;
    size_t total_file_size = 0; 
    size_t total_allocated_size = 0; 
    size_t total_buffer_size = 0; 
    ssize_t recv_bytes = 0;
    char temp[RECV_BUFFER_SIZE];
    bool found_end;
    int accept_fd = 0;
    char ip_string[INET_ADDRSTRLEN];

    


    current_state = WaitingForClient;
    while (keep_looping)
    {
        if (current_state == WaitingForClient)
        {
            accept_fd = waiting_for_client(socket_fd, client_address, client_address_len, ip_string, sizeof(ip_string));
        }
        else if (current_state == RecievingData)
        {
            recieving_data(accept_fd, recv_buffer, total_buffer_size, total_allocated_size, client_address, ip_string);
        }
        else if (current_state == SendingData)
        {
            sending_data(accept_fd, send_buffer, total_file_size, total_sent_bytes);
        }
    }
}

int startup()
{
    current_state = Startup;

    // Setup logger
    openlog(NULL, LOG_ODELAY, LOG_USER);
    print_and_log(LOG_INFO, "%s\n", "AESD Socket Starting!");

    // Variable Setup
    bool found_end_packet = false;
    bool client_connected = false;
    struct sockaddr client_address;
    socklen_t client_address_len = sizeof(client_address);
    struct addrinfo * res = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;

    // Get address info
    int getaddrinfo_return_value = getaddrinfo(NULL, SOCKET_PORT, &hints, &res);
    if (getaddrinfo_return_value != 0)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to get address info!");
        return 1;
    }

    // Socket file descriptor
    int socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to get socket file descriptor!");
        return 1;
    }

    // Socket bind
    int bind_return_value = bind(socket_fd, res->ai_addr, res->ai_addrlen);
    if (bind_return_value == -1)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to bind socket!");
        return 1;
    }

    // Socket listen
    int listen_return_value = listen(socket_fd, 10);
    if (listen_return_value == -1)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to listen on socket!");
        return 1;
    }

    main_loop(socket_fd, client_address, client_address_len, res);
}

int waiting_for_client(int socket_fd, struct sockaddr client_address, socklen_t client_address_len, char* ip_string, size_t ip_string_size)
{
    int accept_fd = accept(socket_fd, &client_address, &client_address_len);
    if (accept_fd == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to accept client connection!");
        return 1;
    }

    // Print client IP address
    get_client_ip_address(client_address, ip_string, ip_string_size);
    print_and_log(LOG_INFO, "Accepted connection from %s\n", ip_string);

    current_state = RecievingData;
    return accept_fd;
}

int recieving_data(int accept_fd, char *recv_buffer, size_t total_buffer_size, size_t total_allocated_size, struct sockaddr client_address, char* ip_string)
{
    char temp[RECV_BUFFER_SIZE];
    int recv_bytes = recv(accept_fd, temp, sizeof(temp), 0);

    if (recv_bytes == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to receive data from client!");
        keep_looping = false;
        return 1;
    }

    // Client closed connection    char ip_string[INET_ADDRSTRLEN];
    
    if (recv_bytes == 0)
    {
        print_and_log(LOG_INFO, "Closed connection from %s\n", ip_string);
        close(accept_fd);
        current_state = WaitingForClient;
        return 0;
    }

    print_and_log(LOG_DEBUG, "%s\n", temp);


    // Grow buffer if needed
    if (total_allocated_size < total_buffer_size + recv_bytes + 1)
    {
        total_allocated_size += RECV_BUFFER_SIZE;
        char* recv_buffer_temp = realloc(recv_buffer, total_allocated_size);
        if (!recv_buffer_temp)
        {
            keep_looping = false;
            return -1;
        }

        recv_buffer = recv_buffer_temp;
    }

    // Copy data to buffer
    memcpy(recv_buffer + total_buffer_size, temp, recv_bytes);
    total_buffer_size += recv_bytes;

    // End of packet received, write to file and reset buffer
    if (recv_buffer[total_buffer_size - 1] == '\n')
    {
        // Writing to file and resetting buffer
        write_to_file(FILE_PATH, recv_buffer, total_buffer_size);
        total_buffer_size = 0;
    }
}

int sending_data(int accept_fd, char *send_buffer, size_t total_file_size, size_t total_sent_bytes)
{
    // Read file contents to send buffer
    send_buffer = malloc(total_file_size + 1);
    if (!send_buffer)
    {
        keep_looping = false;
        return 1;
    }
    read_from_file(FILE_PATH, send_buffer, total_file_size + 1);

    // Send file contents to client
    while(total_sent_bytes < total_file_size)
    {
        ssize_t send_bytes = send(accept_fd, send_buffer + total_sent_bytes, total_file_size - total_sent_bytes, 0);
        if (send_bytes == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "Error: Failed to send data to client!");
            keep_looping = false;
            continue;   
        }
        total_sent_bytes += send_bytes;
    }

    // Free send buffer
    free(send_buffer);
}

int shutting_down(int socket_fd, char *recv_buffer, struct addrinfo *res)
{
    current_state = ShuttingDown;

    print_and_log(LOG_INFO, "%s\n", "AESD Socket Stopping!");

    // Free receive buffer
    free(recv_buffer);

    // Free address info
    freeaddrinfo(res);


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

void process_client_data(const char *data, ssize_t data_length)
{
    // Process the received data here
    // For example, you can print it to the console or log it
    printf("Received data: %.*s\n", (int)data_length, data);

    for(int i = 0; i < data_length; i++)
    {
        if (data[i] == '\n')
        {
            printf("Newline character found at index %d\n", i);
        }
    }
}

int write_to_file(const char *filename, const char *data, ssize_t data_length)
{
    // Open the file for writing
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to open file '%s' for writing!\n", filename);
        return 1;
    }

    // Write the data to the file
    ssize_t bytes_written = write(fd, data, data_length);
    if (bytes_written == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to write to file '%s'!\n", filename);
        return 1;
    }
    else
    {
        print_and_log(LOG_INFO, "Successfully wrote %zd bytes to file '%s'\n", bytes_written, filename);
        return 1;
    }

    // Close the file descriptor
    close(fd);
}

int read_from_file(const char *filename, char *buffer, size_t buffer_size)
{
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to open file '%s' for reading!\n", filename);
        return 1;
    }

    // Read the data from the file
    ssize_t bytes_read = read(fd, buffer, buffer_size - 1); // Leave space for null terminator
    if (bytes_read == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to read from file '%s'!\n", filename);
        return 1;
    }
    else
    {
        buffer[bytes_read] = '\0'; // Null-terminate the buffer
        print_and_log(LOG_INFO, "Successfully read %zd bytes from file '%s'\n", bytes_read, filename);
        return 1;
    }

    // Close the file descriptor
    close(fd);
}

void get_client_ip_address(struct sockaddr client_address, char* ip_string, size_t ip_string_size)
{
    struct sockaddr_in *addr_in_ptr = (struct sockaddr_in *)&client_address;
    inet_ntop(AF_INET, &addr_in_ptr->sin_addr, ip_string, ip_string_size);
}