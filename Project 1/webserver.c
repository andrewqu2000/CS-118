#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define RECV_SIZE 8192


int main()
{
    // *** Initialize socket for listening ***
    int sockfd;
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // *** Initialize local listening socket address ***
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(5678);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY allows to connect to any one of the host’s IP address

    // *** Socket Bind ***
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    // *** Socket Listen ***
    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(1);
    }

    // *** Accept and Read&Write **
    int client_fd;
    struct sockaddr_in client_addr; // client address
    unsigned int sin_size;
    // https://stackoverflow.com/questions/2659952/maximum-length-of-http-get-request
    char recv_buf[8192];
    char filename[8192];
    char filetype;
    int recv_buf_size;

    while (1) { // main accept() loop
        sin_size = sizeof(struct sockaddr_in);
        if ((client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }
        printf("server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));
        memset(recv_buf, 0, RECV_SIZE);
        memset(filename, 0, RECV_SIZE);
        filetype = '\0';
        if ((recv_buf_size = read(client_fd, recv_buf, RECV_SIZE)) < 0) {
            perror("read\n");
            close(client_fd);
            continue;
        }
        printf("%s", recv_buf);

        // Get the filename and type
        int cur_name_off = 0;
        int cur_req_off = 5;
        // Extract the name
        while (1) {
            if (recv_buf[cur_req_off] == ' ') {
                filetype = 'b';
                filename[cur_name_off++] = '\0';
                break;
            }
            if (recv_buf[cur_req_off] == '.') {
                filename[cur_name_off++] = '.';
                filename[cur_name_off++] = '\0';
                cur_req_off++;
                break;
            }
            filename[cur_name_off++] = recv_buf[cur_req_off];
            cur_req_off++;
        }
        // Extract the type
        if (filetype != 'b') {
            if (strncmp(recv_buf + cur_req_off, "html", 4) == 0) {
                strcat(filename, "html");
                filetype = 'h';
            }
            else if (strncmp(recv_buf + cur_req_off, "txt", 3) == 0) {
                strcat(filename, "txt");
                filetype = 't';
            }
            else if (strncmp(recv_buf + cur_req_off, "jpg", 3) == 0) {
                strcat(filename, "jpg");
                filetype = 'j';
            }
            else if (strncmp(recv_buf + cur_req_off, "png", 3) == 0) {
                strcat(filename, "png");
                filetype = 'p';
            }
        }

        // Open the requested file
        int open_file = open(filename, O_RDONLY);
        if (open_file < 0)
        {
            perror("open");
            close(client_fd);
            continue;
        }

        // Write header
        char header[200];
        memset(header, 0, 200);
        strcpy(header, "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: ");
        // https://www.geeksforgeeks.org/switch-statement-cc/
        switch (filetype)
        {
            case 'b':
                strcat(header, "application/octet-stream\r\n\r\n");
                break;
            // https://www.lifewire.com/file-extensions-and-mime-types-3469109
            case 'j':
                strcat(header, "image/jpeg\r\n\r\n");
                break;
            // https://www.developershome.com/wap/detection/detection.asp?page=httpHeaders
            case 'p':
                strcat(header, "image/png\r\n\r\n");
                break;
            case 'h':
                strcat(header, "text/html\r\n\r\n");
                break;
            case 't':
                strcat(header, "text/plain\r\n\r\n");
                break;
        }
        /*
        if (write(client_fd, header, strlen(header) < 0)) {
            perror("write");
            close(client_fd);
            continue;
        }
        */

        char send_buf[1048576];
        char send_cont[1048776];
        memset(send_buf, 0, 1048576);
        memset(send_cont, 0, 1048776);
        int send_size = 0;
        int sent_size = 0;

        // Read file and send out
        send_size = read(open_file, send_buf, 1048576);
        // https://stackoverflow.com/questions/39599060/how-to-concatenate-two-strings-with-many-null-characters
        strcat(send_cont, header);
        memcpy(send_cont + strlen(header), send_buf, send_size);

        if (send_size < 0)
        {
            perror("read from file");
            continue;
        }
        // Write to socket
        else
        {
            sent_size = write(client_fd, send_cont, send_size + strlen(header));
            if (sent_size < 0)
            {
                perror("write to buffer");
                continue;
            }
        }
        
        close(client_fd);
    }
    close(sockfd);
    return 0;
}
