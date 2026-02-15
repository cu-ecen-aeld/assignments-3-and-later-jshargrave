
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

#define SOCKET_PORT "9000"
#define RECV_BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

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

void write_to_file(const char *filename, const char *data, ssize_t data_length)
{
    // Open the file for writing
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to open file '%s' for writing!\n", filename);
        return;
    }

    // Write the data to the file
    ssize_t bytes_written = write(fd, data, data_length);
    if (bytes_written == -1)
    {
        print_and_log(LOG_ERR, "Error: Failed to write to file '%s'!\n", filename);
    }
    else
    {
        print_and_log(LOG_INFO, "Successfully wrote %zd bytes to file '%s'\n", bytes_written, filename);
    }

    // Close the file descriptor
    close(fd);
}

int main()
{
    // Setup logger
    openlog(NULL, LOG_ODELAY, LOG_USER);

    print_and_log(LOG_INFO, "%s\n", "AESD Socket Starting!");

    // Variable Setup
    bool found_end_packet = false;
    bool keep_looping = true;
    bool client_connected = false;
    char recv_buffer[RECV_BUFFER_SIZE];
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

    

    int listen_return_value = listen(socket_fd, 10);
    if (listen_return_value == -1)
    {
        // Error!!!
        print_and_log(LOG_ERR, "%s\n", "Error: Failed to listen on socket!");
        return 1;
    }

    

    // Main loop
    int accept_fd;
    ssize_t recv_return_value;
    char ip_string[INET_ADDRSTRLEN];
    struct sockaddr_in *addr_in_ptr = NULL;
    while (keep_looping)
    {
        accept_fd = accept(socket_fd, &client_address, &client_address_len);
        if (accept_fd == -1)
        {
            print_and_log(LOG_ERR, "%s\n", "No client connected. Sleeping for 5 seconds");
            client_connected = false;
            sleep(5);
        }
        else
        {
            // One time print when cleint connects
            if (!client_connected)
            {
                addr_in_ptr = (struct sockaddr_in *)&client_address;
                inet_ntop(AF_INET, &addr_in_ptr->sin_addr, ip_string, sizeof(ip_string));
                print_and_log(LOG_INFO, "Accepted connection from %s\n", ip_string);
                client_connected = true;
            }

            // Start recieving data from client
            recv_return_value = recv(accept_fd, recv_buffer, RECV_BUFFER_SIZE, MSG_DONTWAIT);
            if (recv_return_value == -1)
            {
                print_and_log(LOG_ERR, "%s\n", "Error: Failed to receive data from client!");
            }
            else if (recv_return_value > 0)
            {
                process_client_data(recv_buffer, recv_return_value);

                if (found_end_packet)
                {
                    write_to_file(FILE_PATH, recv_buffer, recv_return_value);
                    found_end_packet = false;
                }
            }
        }
    }

    print_and_log(LOG_INFO, "%s\n", "AESD Socket Stopping!");

    // Free address info
    freeaddrinfo(res);


    closelog();

    return 0;
}