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
#include <sys/time.h>
#include<time.h>

#define RECV_SIZE 524
#define MAX_SEQ 25600

struct Packet
{
    uint16_t seq;
    uint16_t ack;
    uint16_t length;
    uint16_t ack_f;
    uint16_t syn_f;
    uint16_t fin_f;
    char data[512];
};

int within_range (int comp, int curr) {
    int max = curr + 512 * 10;
    int min = curr;
    if (max > MAX_SEQ) {
        int min = max - (MAX_SEQ + 1);
        int max = min;
        if (comp >= min && comp < max) {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        if (comp >= min && comp < max)
        {
            return 1;
        }
        else {
            return 0;
        }
    }
}

int main(int argc, char** argv) {
    
    if(argc != 4)
    {
      fprintf(stderr, "Wrong number of command line arguments. Usage: ./client <HOSTNAME-OR-IP> <PORT> <FILENAME>\n");
      exit(1);
    }
    // Get the port number
    char* hostname = argv[1];
    int port = atoi(argv[2]);
    char* filename = argv[3];
    char printout[512];
    struct Packet window[10];
    double window_time[10];
    for (int i = 0; i < 10; i++) {
        window_time[i] = -1;
    }
    struct timeval start, now;
    int done = 0;
    int last_seq = 0;
    int num_empty = 10;
    double start_time, end;

    int sockfd;
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // *** Initialize the server socket address ***
    struct sockaddr_in server_addr; // server socket address struct
    server_addr.sin_family = AF_INET; // protocol family
    server_addr.sin_port = htons(port); // port number
    struct hostent *host_name = gethostbyname(hostname); // get IP from host name
    server_addr.sin_addr = *((struct in_addr *)host_name->h_addr); // set IP address
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero)); // make the rest bytes zero

    if(fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl");
        exit(1);
    }

    struct Packet syn;
    memset(&syn, 0, RECV_SIZE);
    // Random Sequence number
    srand(time(0));
    int seq_num =  rand() % (MAX_SEQ + 1);
    // SYN packet
    syn.ack = 0;
    syn.ack_f = 0;
    syn.fin_f = 0;
    syn.length = 0;
    syn.seq = seq_num;
    syn.syn_f = 1;
    socklen_t len = sizeof(server_addr);
    if (sendto(sockfd, (void *)&syn, 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
        perror("send");
        exit(1);
    }
    else
    {
        sprintf(printout, "SEND %d %d SYN\n", syn.seq, syn.ack);
        write(STDOUT_FILENO, printout, strlen(printout));
    }
    
    // Receive SYN-ACK 
    gettimeofday(&start, NULL);
    start_time = (double) start.tv_sec + (double) start.tv_usec/1000000;
    end = start_time;
    struct Packet pack;
    memset(&pack, 0, sizeof(pack));
    int length = 0;
    do
    {
        // Time out, resend
        gettimeofday(&now, NULL);
        end = (double) now.tv_sec + (double) now.tv_usec/1000000;
        if (end - start_time > 0.5)
        {
            // Print timeout
            sprintf(printout, "TIMEOUT %d\n", syn.seq);
            write(STDOUT_FILENO, printout, strlen(printout));
            // Resend
            if (sendto(sockfd, (void *)&syn, syn.length + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
                perror("send");
                exit(1);
            }
            else
            {
                sprintf(printout, "RESEND %d %d SYN\n", syn.seq, syn.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                gettimeofday(&start, NULL);
                start_time = (double) start.tv_sec + (double) start.tv_usec/1000000;
            }
        }
        length = recvfrom(sockfd, (char*)&pack, RECV_SIZE, 0, (struct sockaddr *) &server_addr, &len);
    } while (length <= 0);
    if(pack.ack_f != 1 && pack.syn_f != 1) {
        perror("syn");
        exit(1);
    }
    else
    {
        sprintf(printout, "RECV %d %d SYN ACK\n", pack.seq, pack.ack);
        write(STDOUT_FILENO, printout, strlen(printout));
    }

    // Read from file

    int filefd = open(filename, O_RDONLY);
    pack.ack_f = 1;
    pack.fin_f = 0;
    int temp_ack = pack.ack;
    int temp_seq = pack.seq;
    pack.seq = temp_ack;
    pack.ack = temp_seq + 1;
    if (pack.ack > MAX_SEQ) {
        pack.ack = 0;
    }
    pack.syn_f = 0;
    int rlength = read(filefd, (void*) pack.data, 512);
    if (rlength < 0) {
        perror("read");
        exit(1);
    }
    if (rlength < 512) {
        done = 1;
    }
    pack.length = rlength;
    last_seq = pack.seq;
    // write(STDOUT_FILENO, (void*) pack.data, length);
    int curr = pack.seq;

    // Send to server
    if (sendto(sockfd, (void *)&pack, rlength + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
        perror("send");
        exit(1);
    }
    else
    {
        sprintf(printout, "SEND %d %d ACK\n", pack.seq, pack.ack);
        write(STDOUT_FILENO, printout, strlen(printout));
        gettimeofday(&start, NULL);
        window_time[0] = (double) start.tv_sec + (double) start.tv_usec/1000000;
        memcpy(window, &pack, RECV_SIZE);
        num_empty--;
    }
    // Copy to array
    memset(&pack, 0, sizeof(pack));

    // Send the rest part
    while (1) {
        for (int i = 0; i < 10; i++)
        {
            if (window_time[i] != -1) {
                // Check whether timeout
                gettimeofday(&now, NULL);
                end = (double) now.tv_sec + (double) now.tv_usec/1000000;
                if (end - window_time[i] > 0.5) {
                    // Print timeout
                    sprintf(printout, "TIMEOUT %d\n", window[i].seq);
                    write(STDOUT_FILENO, printout, strlen(printout));
                    // Resend
                    if (sendto(sockfd, (void *)&window[i], window[i].length + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
                        perror("send");
                        exit(1);
                    }
                    else
                    {
                        if (window[i].ack_f == 1)
                        {
                            sprintf(printout, "RESEND %d %d ACK\n", window[i].seq, window[i].ack);
                            write(STDOUT_FILENO, printout, strlen(printout));
                        }
                        else
                        {
                            sprintf(printout, "RESEND %d %d\n", window[i].seq, window[i].ack);
                            write(STDOUT_FILENO, printout, strlen(printout));
                        }
                        gettimeofday(&start, NULL);
                        window_time[i] = (double) start.tv_sec + (double) start.tv_usec/1000000;
                    }
                    
                }
                continue;
            }
            if (done == 1) {
                continue;
            }
            if (within_range(last_seq + rlength, curr) == 1 && window_time[i] == -1)
            {
                // Prepare packs
                pack.ack_f = 0;
                pack.fin_f = 0;
                pack.seq = last_seq + rlength;
                pack.ack = 0;
                if (pack.seq > MAX_SEQ) {
                    pack.seq = pack.seq - MAX_SEQ - 1;
                }
                last_seq = pack.seq;
                pack.syn_f = 0;
                rlength = read(filefd, (void*) pack.data, 512);
                if (rlength < 0) {
                    perror("read");
                    exit(1);
                }
                if (rlength == 0) {
                    done = 1;
                    break;
                }
                pack.length = rlength;
                // write(STDOUT_FILENO, (void*) pack.data, length);

                // Send to server
                if (sendto(sockfd, (void *)&pack, rlength + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
                    perror("send");
                    exit(1);
                }
                else
                {
                    sprintf(printout, "SEND %d %d\n", pack.seq, pack.ack);
                    write(STDOUT_FILENO, printout, strlen(printout));
                    gettimeofday(&start, NULL);
                    window_time[i] = (double) start.tv_sec + (double) start.tv_usec/1000000;
                    memcpy(window + i, &pack, RECV_SIZE);
                    num_empty--;
                }
                memset(&pack, 0, sizeof(pack));
                if (rlength < 512) {
                    done = 1;
                    break;
                }
            }
        }
        // Recv ack
        // Exit loop
        /*
        char nmem[2];
        sprintf(nmem, "%d", done);
        write(STDOUT_FILENO, nmem, 5);
        */
        if (done == 1 && num_empty == 10) {
            break;
        }
        length = recvfrom(sockfd, (char*)&pack, RECV_SIZE, 0, (struct sockaddr *) &server_addr, &len);
        if (length <= 0) {
            continue;
        }
        else
        {
            sprintf(printout, "RECV %d %d ACK\n", pack.seq, pack.ack);
            write(STDOUT_FILENO, printout, strlen(printout));
            int j = 0;
            int min = pack.ack + 9 * 512;
            for (; j < 10; j++) {
                if (window_time[j] != -1){
                    int expected_ack = window[j].seq + window[j].length;
                    if (expected_ack > MAX_SEQ) {
                        expected_ack -= (MAX_SEQ + 1);
                    }
                    if (expected_ack == pack.ack)
                    {
                        /*
                        char nmem[3];
                        sprintf(nmem, "%d", num_empty);
                        write(STDOUT_FILENO, nmem, 3);
                        */
                        window_time[j] = -1;
                        num_empty++;
                    }
                }
            }
            for (j = 0; j < 10; j++) {
                if (window_time[j] != -1 && (window[j].seq < curr) && ((window[j].seq + MAX_SEQ + 1) < min))
                {
                    min = window[j].seq + MAX_SEQ + 1;
                }
                else if (window_time[j] != -1 && window[j].seq < min) {
                    min = window[j].seq;
                }
            }
            int assu_ack = curr + 512;
            if (assu_ack > MAX_SEQ){
                assu_ack -= (MAX_SEQ + 1);
            }
            if (pack.ack == assu_ack) {
                curr = min;
                if(curr > MAX_SEQ) {
                    curr -= (MAX_SEQ + 1);
                }
                
            }
        }
    }
 

    // Fin
    // Send fin
    pack.ack_f = 0;
    pack.fin_f = 1;
    temp_ack = pack.ack;
    temp_seq = pack.seq;
    pack.seq = temp_ack;
    pack.ack = 0;
    pack.length = 0;
    pack.syn_f = 0;
    if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
        perror("send");
        exit(1);
    }
    else
    {
        sprintf(printout, "SEND %d %d FIN\n", pack.seq, pack.ack);
        write(STDOUT_FILENO, printout, strlen(printout));
    }
    int expected_ack = pack.seq + 1;
    if (expected_ack > MAX_SEQ) {
        expected_ack = 0;
    }
    struct Packet finpack;
    memcpy(&finpack, &pack, RECV_SIZE);

    int un_ack = 1;
    // Recv ack
    // REmemset(&pack, 0, sizeof(pack));
    gettimeofday(&start, NULL);
    start_time = (double) start.tv_sec + (double) start.tv_usec/1000000;
    int fin_sent = start_time;
    end = start_time;
    struct Packet temp;
    do
    {
        // Time out, resend
        gettimeofday(&now, NULL);
        end = (double) now.tv_sec + (double) now.tv_usec/1000000;
        if (end - start_time > 0.5)
        {
            // Print timeout
            sprintf(printout, "TIMEOUT %d\n", pack.seq);
            write(STDOUT_FILENO, printout, strlen(printout));
            // Resend
            if (sendto(sockfd, (void *)&pack, pack.length + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
                perror("send");
                exit(1);
            }
            else
            {
                sprintf(printout, "RESEND %d %d FIN\n", pack.seq, pack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                gettimeofday(&start, NULL);
                start_time = (double) start.tv_sec + (double) start.tv_usec/1000000;
            }
        }
        length = recvfrom(sockfd, (char*)&temp, RECV_SIZE, 0, (struct sockaddr *) &server_addr, &len);
        if (length > 0 && (temp.ack_f != 1)) {
            sprintf(printout, "RECV %d %d FIN\n", temp.seq, temp.ack);
            write(STDOUT_FILENO, printout, strlen(printout));
            break;
        }
    } while (length <= 0);

    memcpy(&pack, &temp, RECV_SIZE);
    if (pack.ack_f == 1) {
        sprintf(printout, "RECV %d %d ACK\n", pack.seq, pack.ack);
        write(STDOUT_FILENO, printout, strlen(printout));
        temp_ack = pack.ack;
        un_ack = 0;

        // Recv fin
        memset(&pack, 0, sizeof(pack));
        do
        {
            length = recvfrom(sockfd, (char*)&pack, RECV_SIZE, 0, (struct sockaddr *) &server_addr, &len);
            if (pack.fin_f == 1 && length > 0){
                sprintf(printout, "RECV %d %d FIN\n", pack.seq, pack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
            }
            else if (length > 0) {
                sprintf(printout, "RECV %d %d ACK\n", pack.seq, pack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
            }
        } while (length <= 0 || pack.fin_f != 1);
    }


    // Send ack
    pack.ack_f = 1;
    pack.fin_f = 0;
    temp_seq = pack.seq + 1;
    if (temp_seq > MAX_SEQ) {
        temp_seq = 0;
    }
    pack.seq = temp_ack;
    pack.ack = temp_seq;
    pack.length = 0;
    pack.syn_f = 0;
    if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
        perror("send");
        exit(1);
    }
    else
    {
        sprintf(printout, "SEND %d %d ACK\n", pack.seq, pack.ack);
        write(STDOUT_FILENO, printout, strlen(printout));
    }

    // Wait for 2 seconds
    gettimeofday(&start, NULL);
    start_time = (double) start.tv_sec + (double) start.tv_usec/1000000;
    end = start_time;

    int first = 1;
    while ((end - start_time) < 2.0)
    {
        if (recvfrom(sockfd, (char*)&pack, RECV_SIZE, 0, (struct sockaddr *) &server_addr, &len) > 0) {
            if (pack.fin_f == 1) {
                sprintf(printout, "RECV %d %d FIN\n", pack.seq, pack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
                    perror("send");
                    exit(1);
                }
                else
                {
                    sprintf(printout, "SEND %d %d DUP-ACK\n", pack.seq, pack.ack);
                    write(STDOUT_FILENO, printout, strlen(printout));
                }
            }
            else {
                sprintf(printout, "RECV %d %d ACK\n", pack.seq, pack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                un_ack = 0;
            }
        }
        gettimeofday(&now, NULL);
        end = (double) now.tv_sec + (double) now.tv_usec/1000000;
        if ((end - fin_sent) > 0.5 && un_ack == 1 && first == 1) {
            if (sendto(sockfd, (void *)&finpack, finpack.length + 12, 0, (struct sockaddr*) &server_addr, len) < 0) {
                perror("send");
                exit(1);
            }
            else
            {
                sprintf(printout, "RESEND %d %d FIN\n", finpack.seq, finpack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                gettimeofday(&start, NULL);
                fin_sent = (double) start.tv_sec + (double) start.tv_usec/1000000;
                end = fin_sent;
            }
            first = 0;
        }
    } 

    close(filefd);
    exit(0);
}