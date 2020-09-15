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
    if(argc != 2)
    {
      fprintf(stderr, "Wrong number of command line arguments. Usage: ./server <PORT>\n");
      exit(1);
    }
    // Get the port number
    int port = atoi(argv[1]);

    // *** Initialize socket for listening ***
    int sockfd;
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // *** Initialize local listening socket address ***
    struct sockaddr_in my_addr;
    struct sockaddr_in client_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY allows to connect to any one of the host’s IP address

    // *** Socket Bind ***
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }
    
    if(fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl");
        exit(1);
    }

    struct Packet pack;
    int count = 1;
    char printout[512];
    struct Packet window[10];
    int seqs[10];
    
    while(1) {
        memset((char *)&pack, 0, RECV_SIZE);
        socklen_t len = sizeof(client_addr);
        int length = 0;
        int curr_ind = 0;
        int last_seq = 0;
        int curr = -1;
        for (int i = 0; i < 10; i++)
        {
            seqs[i] = -1;
        }
        do
        {
            length = recvfrom(sockfd, (char*)&pack, RECV_SIZE, 0, (struct sockaddr *) &client_addr, &len);
        } while (length <= 0);
        
        
        if (pack.syn_f != 1) {
            continue;
        }
        else {
            sprintf(printout, "RECV %d %d SYN\n", pack.seq, pack.ack);
            write(STDOUT_FILENO, printout, strlen(printout));
            
        }
        int temp_ack = pack.ack;
        int temp_seq = pack.seq;
        curr = pack.seq + 1;
        if (curr > MAX_SEQ) {
            curr = 1;
        }

        // Send SYN-ACK
        srand(time(0));
        int seq_num =  rand() % (MAX_SEQ + 1);
        pack.ack = temp_seq + 1;
        if (pack.ack > MAX_SEQ) {
            pack.ack = 0;
        }
        pack.ack_f = 1;
        pack.fin_f = 0;
        pack.length = 0;
        pack.seq = seq_num;
        pack.syn_f = 1;
        if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &client_addr, len) < 0) {
            perror("send");
            exit(1);
        }
        else
        {
            sprintf(printout, "SEND %d %d SYN ACK\n", pack.seq, pack.ack);
            write(STDOUT_FILENO, printout, strlen(printout));
            memset(printout, 0, 512);
        }
        last_seq = pack.seq + 1;
        if (last_seq > MAX_SEQ) {
            last_seq = 0;
        }

        char filename[1024];
        sprintf(filename, "%d", count);
        strcat(filename, ".file");
        // Open a file
        int filefd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0777);
        if (filefd < 0) {
            perror("open\n");
            continue;
        }

        int first = 0;

        // Check if there are dup syn
        while (1) {
            struct Packet next;
            length = recvfrom(sockfd, (char*)&next, RECV_SIZE, 0, (struct sockaddr *) &client_addr, &len);
            if (length <= 0) {
                continue;
            }
            else if (next.syn_f == 1) {
                sprintf(printout, "RECV %d %d SYN\n", next.seq, next.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &client_addr, len) < 0) {
                    perror("send");
                    exit(1);
                }
                else
                {
                    sprintf(printout, "SEND %d %d SYN DUP-ACK\n", pack.seq, pack.ack);
                    write(STDOUT_FILENO, printout, strlen(printout));
                    memset(printout, 0, 512);
                }
            }
            else {
                memcpy(&pack, &next, RECV_SIZE);
                break;
            }
        }

        while (1)
        {
            // Receive packets
            if (first == 0) {
                first = 1;
            }
            else
            {
                length = recvfrom(sockfd, (char*)&pack, RECV_SIZE, 0, (struct sockaddr *) &client_addr, &len);
            }
            if (length <= 0) {
                continue;
            }
            if (pack.ack_f == 1){
                sprintf(printout, "RECV %d %d ACK\n", pack.seq, pack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                memset(printout, 0, 512);
            }
            else if (pack.fin_f == 1)
            {
                sprintf(printout, "RECV %d %d FIN\n", pack.seq, pack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                memset(printout, 0, 512);
            }
            
            else {
                sprintf(printout, "RECV %d %d\n", pack.seq, pack.ack);
                write(STDOUT_FILENO, printout, strlen(printout));
                memset(printout, 0, 512);
            }

            temp_ack = pack.ack;
            temp_seq = pack.seq;
            struct Packet cont;
            memcpy(&cont, &pack, RECV_SIZE);
            

            if (pack.fin_f == 0) {
                // Send ack
                pack.ack = temp_seq + pack.length;
                if (pack.ack > MAX_SEQ) {
                    pack.ack -= (MAX_SEQ + 1);
                }
                pack.ack_f = 1;
                pack.fin_f = 0;
                pack.length = 0;
                pack.seq = last_seq;
                pack.syn_f = 0;
                if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &client_addr, len) < 0) {
                    perror("send");
                    exit(1);
                }
                else
                {
                    sprintf(printout, "SEND %d %d ACK\n", pack.seq, pack.ack);
                    write(STDOUT_FILENO, printout, strlen(printout));
                    memset(printout, 0, 512);
                }
                last_seq = pack.seq;
                // Check whether within range

                if (within_range(cont.seq, curr) == 1)
                {
                    int ind = (cont.seq - curr) / 512;
                    if (ind < 0) {
                        ind = (cont.seq + MAX_SEQ + 1 - curr) / 512;
                    }
                    // Calculate real index
                    
                    int real_index = curr_ind + ind;
                    if (real_index >= 10)
                    {
                        real_index -= 10;
                    }
                    
                    // Copy to buffer
                    memcpy(window + real_index, &cont, RECV_SIZE);
                    seqs[real_index] = cont.seq;

                    // Write to the file
                    while (seqs[curr_ind] != -1)
                    {
                        if (write(filefd, window[curr_ind].data, window[curr_ind].length) < 0) {
                            perror("write\n");
                            break;
                        }
                        curr = window[curr_ind].seq + window[curr_ind].length;
                        if (curr > MAX_SEQ) {
                            curr -= (MAX_SEQ + 1);
                        }
                        seqs[curr_ind] = -1;
                        curr_ind++;
                        if (curr_ind > 9)
                        {
                            curr_ind -= 10;
                        }
                    }
                    
                }
            }
            else {
                // Send ack
                pack.ack = temp_seq + 1;
                if (pack.ack > MAX_SEQ) {
                    pack.ack -= (MAX_SEQ + 1);
                }
                pack.ack_f = 1;
                pack.fin_f = 0;
                pack.length = 0;
                pack.seq = last_seq;
                pack.syn_f = 0;
                if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &client_addr, len) < 0) {
                    perror("send");
                    exit(1);
                }
                else
                {
                    sprintf(printout, "SEND %d %d ACK\n", pack.seq, pack.ack);
                    write(STDOUT_FILENO, printout, strlen(printout));
                    memset(printout, 0, 512);
                }
                
                double start_time, end;
                struct timeval start, now;

                // Send fin
                pack.ack = 0;
                pack.ack_f = 0;
                pack.fin_f = 1;
                pack.length = 0;
                pack.seq = last_seq;
                pack.syn_f = 0;
                if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &client_addr, len) < 0) {
                    perror("send");
                    exit(1);
                }
                else
                {
                    sprintf(printout, "SEND %d %d FIN\n", pack.seq, pack.ack);
                    write(STDOUT_FILENO, printout, strlen(printout));
                    memset(printout, 0, 512);
                }
                gettimeofday(&start, NULL);
                start_time = (double) start.tv_sec + (double) start.tv_usec/1000000;
                end = start_time;
                // Recv ack
                length = 0;
                int acked = 0;
                struct Packet fin;
                memcpy(&fin, &pack, RECV_SIZE);
                do {
                    gettimeofday(&now, NULL);
                    end = (double) now.tv_sec + (double) now.tv_usec/1000000;
                    /*
                    sprintf(printout, "%.5f", end - start_time);
                    write(STDOUT_FILENO, printout, strlen(printout));
                    */
                    if (end - start_time > 0.5 && acked == 0)
                    {
                        // Print timeout
                        sprintf(printout, "TIMEOUT %d\n", fin.seq);
                        write(STDOUT_FILENO, printout, strlen(printout));
                        // Resend
                        if (sendto(sockfd, (void *)&fin, fin.length + 12, 0, (struct sockaddr*) &client_addr, len) < 0) {
                            perror("send");
                            exit(1);
                        }
                        else
                        {
                            sprintf(printout, "RESEND %d %d FIN\n", fin.seq, fin.ack);
                            write(STDOUT_FILENO, printout, strlen(printout));
                            gettimeofday(&start, NULL);
                            start_time = (double) start.tv_sec + (double) start.tv_usec/1000000;
                        }
                    }
                    length = recvfrom(sockfd, (char*)&pack, RECV_SIZE, 0, (struct sockaddr *) &client_addr, &len);
                    if (length > 0 && pack.fin_f == 1)
                    {
                        sprintf(printout, "RECV %d %d FIN\n", pack.seq, pack.ack);
                        write(STDOUT_FILENO, printout, strlen(printout));
                        memset(printout, 0, 512);
                        // Send ack
                        pack.ack = temp_seq + 1;
                        if (pack.ack > MAX_SEQ) {
                            pack.ack -= (MAX_SEQ + 1);
                        }
                        pack.ack_f = 1;
                        pack.fin_f = 0;
                        pack.length = 0;
                        pack.seq = last_seq;
                        pack.syn_f = 0;
                        if (sendto(sockfd, (void *)&pack, length + 12, 0, (struct sockaddr*) &client_addr, len) < 0) {
                            perror("send");
                            exit(1);
                        }
                        else
                        {
                            sprintf(printout, "SEND %d %d DUP-ACK\n", pack.seq, pack.ack);
                            write(STDOUT_FILENO, printout, strlen(printout));
                            memset(printout, 0, 512);
                            acked = 1;
                        }
                        length = -1;
                    }
                    else if (pack.ack_f == 1) {
                        break;
                    }
                } while (length <= 0);
                if (pack.ack_f == 1) {
                    sprintf(printout, "RECV %d %d ACK\n", pack.seq, pack.ack);
                    write(STDOUT_FILENO, printout, strlen(printout));
                    memset(printout, 0, 512);
                    acked = 1;
                    break;
                }
            }
        }
        

        count++;
        close(filefd);
    }
    exit(0);
}