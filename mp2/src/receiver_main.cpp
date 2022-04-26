/* 
 * File:   receiver_main.c
 * Author: 
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <unordered_map>

#define MAXBUFLEN 1472
#define MSS MAXBUFLEN-sizeof(unsigned int)*2 //1464

using std::unordered_map;

typedef struct packet{
    unsigned int seq_num;
    unsigned int data_size;
    char data[MSS]; 
}Packet;

struct sockaddr_in si_me, si_other;
socklen_t addrlen;
int s;
FILE *oFile;
unordered_map<unsigned int, Packet*> buffer; 

unsigned int base_seq ;
unsigned int last_acked_seq ;

void intHandler(int sig) {
    fclose(oFile);
    close(s);
    exit(1);
}

void diep(const char *s) {
    perror(s);
    exit(1);
}

void appendToFile(Packet* pkt){
    size_t ret = fwrite(pkt->data,sizeof(char),pkt->data_size,oFile);
    if(ret!=pkt->data_size)
        diep("fwrite");
}

void init(unsigned short int myUDPport, char* destinationFile){
    addrlen = sizeof(si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");

    oFile = fopen(destinationFile,"wb");
    if(oFile == NULL)
        diep("open file");
    fclose(oFile);
    oFile = fopen(destinationFile,"ab");
    if(oFile == NULL)
        diep("open file");
    buffer = unordered_map<unsigned int, Packet*>(); 
}

void handshake(){
    // handshake
    if(recvfrom(s, &base_seq, sizeof(base_seq) , 0,(struct sockaddr *)&si_me, &addrlen) == -1){
        diep("recv handshake");
    }
    if(sendto(s, &base_seq, sizeof(base_seq), 0, (struct sockaddr *)&si_me, addrlen) == -1){
        diep("send error");
    }
    printf("sent ack for handshake : %d\n", base_seq);
    last_acked_seq=base_seq;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    init(myUDPport, destinationFile);
    handshake();
    
	/* Now receive data and send acknowledgements */
    while(1){
        Packet* pkt = new Packet;  
        printf("listener: waiting to recvfrom...\n");
        size_t numbytes;
        if ((numbytes = recvfrom(s, pkt, MAXBUFLEN , 0,
            (struct sockaddr *)&si_me, &addrlen)) == -1) {
            perror("recvfrom");
            exit(1);
        }else if (numbytes == 0) break;
        // TODO: if pkt->data_size != numbytes

        printf("listener: packet seq num is: \"%d\"\n", pkt->seq_num);
        printf("listener: packet is %d bytes long\n", pkt->data_size);
        //printf("listener: packet contains \"%s\"\n", pkt->data);

        unsigned int cur_seq = pkt->seq_num;
        //sleep(1);

        // deal with the end of file signal
        if(cur_seq == base_seq) {
            // close connection    
            break;    
        }
        // send ACK of received pkt to the sender
        if(cur_seq != last_acked_seq + 1) {
            if (cur_seq > last_acked_seq + 1){
                // append to the buffer if not already in
                if(buffer.find(cur_seq)==buffer.end()){
                    buffer[cur_seq] = pkt;
                }
            }

            cur_seq = last_acked_seq;
            if(sendto(s, &cur_seq, sizeof(cur_seq), 0, (struct sockaddr *)&si_me, addrlen) == -1){
                perror("send error");
            }
            printf("sent ack\n");
            continue;
        }

        // if the incoming pkt is the expected pkt
        last_acked_seq++;
        if(sendto(s, &cur_seq, sizeof(cur_seq), 0, (struct sockaddr *)&si_me, addrlen) == -1){
            perror("send error");
        }
        printf("sent ack\n");
        appendToFile(pkt);
        // check if the incoming pkt fills the gap in the buffer
        while (buffer.find(last_acked_seq+1)!=buffer.end()){
            last_acked_seq++;
            cur_seq++;
            if(sendto(s, &cur_seq, sizeof(cur_seq), 0, (struct sockaddr *)&si_me, addrlen) == -1){
                perror("send error");
            }
            printf("sent ack\n");
            appendToFile(buffer[cur_seq]);
            buffer.erase(cur_seq);
        }
    }
    
    fclose(oFile);
    close(s);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }
    signal(SIGINT, intHandler);

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

