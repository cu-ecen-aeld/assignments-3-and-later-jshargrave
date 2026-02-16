/*
    Author: Joseph Hargrave
    Date: 2/15/2026
*/

#include "aesdsocket.h"



int main()
{
    // Setup logger
    openlog(NULL, LOG_ODELAY, LOG_USER);
    print_and_log(LOG_INFO, "%s\n", "AESD Socket Starting!");

    // Variable Setup
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags    = AI_PASSIVE;   // bind()

    if (startup() != -1)
    {
        main_loop();
    }
    shutting_down();

    return 0;
}

int main_loop()
{
    // Main loop
    current_state = WaitingForClient;
    while (keep_looping)
    {
        if (current_state == WaitingForClient)
        {
            waiting_for_client();
        }
        else if (current_state == RecievingData)
        {
            recieving_data();
        }
        else if (current_state == SendingData)
        {
            sending_data();
        }
    }
    return 0;
}

int startup()
{
    current_state = Startup;

    // Get address info
    int getaddrinfo_return_value = getaddrinfo(NULL, SOCKET_PORT, &hints, &res);
    if (getaddrinfo_return_value != 0)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to get address info!");
        return -1;
    }

    // Socket file descriptor
    socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socket_fd == -1)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to get socket file descriptor!");
        return -1;
    }

    // Socket bind
    int bind_return_value = bind(socket_fd, res->ai_addr, res->ai_addrlen);
    if (bind_return_value == -1)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to bind socket!");
        return -1;
    }

    // Socket listen
    int listen_return_value = listen(socket_fd, 10);
    if (listen_return_value == -1)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to listen on socket!");
        return -1;
    }

    return 0;
}

int waiting_for_client()
{
    client_address_len = sizeof(client_address);
    client_fd = accept(socket_fd, (struct sockaddr *)&client_address, &client_address_len);
    if (client_fd == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to accept client connection!");
        return -1;
    }

    // Print client IP address
    get_client_ip_address(client_address, ip_string, sizeof(ip_string));
    print_and_log(LOG_INFO, "Accepted connection from %s\n", ip_string);

    current_state = RecievingData;
    return 0;
}

int recieving_data()
{
    char temp[RECV_BUFFER_SIZE];
    ssize_t recv_bytes = recv(client_fd, temp, sizeof(temp), 0);

    if (recv_bytes == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to receive data from client!");
        return -1;
    }

    // Client closed connection
    if (recv_bytes == 0)
    {
        print_and_log(LOG_INFO, "Closed connection from %s\n", ip_string);
        close(client_fd);
        current_state = WaitingForClient;
        return 0;
    }

    // Grow buffer if needed
    while (recv_allocated_size < recv_buffer_size + recv_bytes + 1)
    {
        recv_allocated_size += RECV_BUFFER_SIZE;
        char* recv_buffer_temp = realloc(recv_buffer, recv_allocated_size);
        if (!recv_buffer_temp)
        {
            keep_looping = false;
            return -1;
        }

        recv_buffer = recv_buffer_temp;
    }

    // Copy data to buffer
    memcpy(recv_buffer + recv_buffer_size, temp, recv_bytes);

    // If newline recieved, write to file, and shift everything forward in the buffer
    int count = 0;
    bool found_packet_end = false;
    size_t new_packet_start = 0;
    size_t remaining = 0;
    while (count < recv_bytes)
    {
        if (recv_buffer[recv_buffer_size + count] == PACKET_ENDING)
        {
            found_packet_end = true;
            write_to_file(FILE_PATH, recv_buffer, recv_buffer_size + count + 1);

            new_packet_start = recv_buffer_size + count + 1;
            remaining  = (size_t)recv_bytes - (count + 1);
            memmove(recv_buffer, recv_buffer + new_packet_start, remaining);
            break;
        }
        count++;
    }

    if (found_packet_end)
    {
        recv_buffer_size = remaining;
        current_state = SendingData;
    }
    else
    {
        recv_buffer_size += recv_bytes;
    }

    recv_buffer[recv_buffer_size] = '\0'; // Null terminate for printouts
    return 0;
}

int sending_data()
{
    send_bytes_sent = 0;
    send_buffer_size = get_file_size(FILE_PATH);
    if (send_buffer_size == -1)
    {
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to get file size!");
        return -1;
    }

    // Read file contents to send buffer
    send_buffer = malloc(send_buffer_size + 1);
    if (!send_buffer)
    {
        keep_looping = false;
        return -1;
    }
    int total_bytes_to_send = read_from_file(FILE_PATH, send_buffer, send_buffer_size + 1);

    while (send_bytes_sent < total_bytes_to_send)
    {
        ssize_t send_bytes = send(client_fd, send_buffer + send_bytes_sent, total_bytes_to_send - send_bytes_sent, 0);
        if (send_bytes == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "Error: Failed to send data to client!");
            free(send_buffer);
            return -1;
        }
        send_bytes_sent += send_bytes;
    }

    // Free send buffer
    free(send_buffer);
    send_buffer = NULL;

    current_state = RecievingData;

    return 0;
}

int shutting_down()
{
    current_state = ShuttingDown;

    print_and_log(LOG_INFO, "%s\n", "AESD Socket Stopping!");

    // Free receive buffer
    if (recv_buffer)
    {
        free(recv_buffer);
    }

    if (send_buffer)
    {
        free(send_buffer);
    }

    if (socket_fd != -1)
    {
        close(socket_fd);
    }

    if (client_fd != -1)
    {
        close(client_fd);
    }

    if (res)
    {
        freeaddrinfo(res);
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
        return -1;
    }

    // Write the data to the file
    ssize_t bytes_written = write(fd, data, data_length);
    if (bytes_written == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to write to file '%s'!\n", filename);
        close(fd);
        return -1;
    }
    else
    {
        print_and_log(LOG_INFO, "Successfully wrote %zd bytes to file '%s'\n", bytes_written, filename);
    }

    // Close the file descriptor
    close(fd);
    return bytes_written;
}

int read_from_file(const char *filename, char *buffer, size_t buffer_size)
{
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to open file '%s' for reading!\n", filename);
        return -1;
    }

    // Read the data from the file
    ssize_t bytes_read = read(fd, buffer, buffer_size - 1); // Leave space for null terminator
    if (bytes_read == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to read from file '%s'!\n", filename);
        close(fd);
        return -1;
    }
    else
    {
        buffer[bytes_read] = '\0'; // Null-terminate the buffer
        print_and_log(LOG_INFO, "Successfully read %zd bytes from file '%s'\n", bytes_read, filename);
    }

    // Close the file descriptor
    close(fd);
    return bytes_read;
}

int get_file_size(const char *filename)
{
    struct stat st;
    if (stat(FILE_PATH, &st) != 0)
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