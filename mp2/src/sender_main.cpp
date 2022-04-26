/* 
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <netdb.h>
#include <string>
#include <deque>
#include <climits>

using std::deque;
using std::pair;

#define MAXBUFLEN 1472
#define MSS MAXBUFLEN-sizeof(unsigned int)*2

#define TIMEOUT_VAL_MICROSEC 25*1000

typedef struct packet{
    unsigned int seq_num;
    unsigned int data_size;
    char data[MSS]; 
}Packet;

enum States {SEND_NEW,RESEND_OLD,WAIT_ACK};
enum Congestion_state {EXPO, ADDITIVE};

struct sockaddr_in si_other;
int s;
struct addrinfo hints,*servinfo;
socklen_t addrlen=sizeof(si_other);
bool working = true;
bool allSent = false;
States state = SEND_NEW;
Congestion_state congestion_state = EXPO;
unsigned int ssthresh = 64; 

struct timeval tp;

void diep(const char *s) {
    perror(s);
    exit(1);
}

FILE *fp;
unsigned int init_seq;
//unsigned int last_acked_seq;
unsigned int last_sent_seq;
unsigned int window_size = 2;
unsigned int window_init_seq;
unsigned long long int bytesToTransfer_;
int sent_acc = 0;
pair<unsigned int, int> last_recv_ack_cnt;

deque<Packet*> in_flight_pkt;

int timeOutRecv(void* buf, size_t exp_data_size){
    int ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tp, sizeof(tp));
    if(ret == -1){
        diep("set timeout failed");
    }
    printf("timeout set, waiting for reply\n");
    int numbytes = recvfrom(s, buf, exp_data_size , 0,NULL, NULL);
            
    if(numbytes == -1 && errno == EAGAIN){
        // timeout detected
        return -1;
    }else return numbytes;
}

void handshake(){
    srand(time(NULL));
    init_seq = rand()%10 + 1;

    // handshake
    if (sendto(s, &init_seq, sizeof(unsigned int), 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
	        perror("sendto failed");
    }
    last_sent_seq = init_seq;
    printf("sent init seq:%d\n",init_seq);
    unsigned int ack;
    if(timeOutRecv(&ack,sizeof(unsigned int))<0){
        diep("handshake timeout");
    }
    printf("received handshake ack:%d\n",ack);
    // TODO
    //last_acked_seq = init_seq;
    last_recv_ack_cnt = std::make_pair(init_seq,1);
    window_init_seq = init_seq+1;
}

void init(char* hostname, unsigned short int hostUDPport, char* filename,unsigned long long int bytesToTransfer){
    tp.tv_sec = 0;
    tp.tv_usec = TIMEOUT_VAL_MICROSEC;

    bytesToTransfer_=bytesToTransfer;
    in_flight_pkt = deque<Packet*>();

    //Open the file
    //FILE *fp;
    printf("file to send:%s\n",filename);
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */
	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(hostname, std::to_string(hostUDPport).c_str(), &hints, &servinfo) != 0) {
        fprintf(stderr, "failed to getaddrinfo\n");
        return;
    }
    
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");
    /*
    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }*/
}

// send all pkts within the window
void send_new_pkt(){
    printf("send new pkt:\n");
    if(bytesToTransfer_ <= sent_acc ) {
        state = WAIT_ACK;
        allSent = true;
        return;
    }
    // calculate how many pkts to send
    int in_flight_num = in_flight_pkt.size();
    int toBeSent = window_size-in_flight_num;

    for(int i=0;i<toBeSent;i++){
        Packet* pkt = new Packet;
        pkt->seq_num = last_sent_seq + 1; 
        pkt->data_size = bytesToTransfer_-sent_acc > MSS? MSS : (bytesToTransfer_-sent_acc);
        fread(pkt->data,sizeof(char),pkt->data_size,fp);

        if (sendto(s, pkt, pkt->data_size+sizeof(unsigned int)*2, 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
	        perror("sendto failed");
        }
        printf("sent data seq:%d\n",pkt->seq_num);

        in_flight_pkt.push_back(pkt);
        sent_acc += pkt->data_size;
        last_sent_seq++;
        if(bytesToTransfer_ <= sent_acc ) {
            allSent = true;
            break;
        }
    }
    state=WAIT_ACK;
}

void resend_pkt(){
    Packet* first_inflight_pkt = in_flight_pkt.front();
    if (sendto(s, first_inflight_pkt, first_inflight_pkt->data_size+sizeof(unsigned int)*2, 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
	    perror("sendto failed");
    }
    printf("sent data seq:%d\n",first_inflight_pkt->seq_num);
    state=WAIT_ACK;
}

void tahoe(){
    if (congestion_state == EXPO) ssthresh = window_size / 2;
    window_size = 1;
    window_init_seq = in_flight_pkt.front()->seq_num;
    // TODO: then grows exponentially
    congestion_state = EXPO;
}

void dupACKs(){
    window_size = window_size/2;
    window_init_seq = in_flight_pkt.front()->seq_num;
    // TODO: then grows addtively
    congestion_state = ADDITIVE;
}

void AIMD(){
    window_init_seq = window_init_seq + window_size;
    window_size++;
}

void fastRecovery(){
    window_init_seq = window_init_seq + window_size;
    if (2 * window_size <= ssthresh){
        window_size *= 2;
    }else{
        if (window_size >= ssthresh) window_size++;
        else window_size = ssthresh;
    }
}

void waitACK(){
    unsigned int ack;
    if(timeOutRecv(&ack,sizeof(unsigned int))<0){
        state = RESEND_OLD;
        printf("timeout detected\n");
        // timeout detected
        tahoe();
        return;
    }
    printf("ack received: %d\n",ack);

    if ( ack == last_recv_ack_cnt.first){
        last_recv_ack_cnt.second++;
        if(last_recv_ack_cnt.second == 3){
            // DUP acks detected
            dupACKs();
        }
    }else {
        last_recv_ack_cnt.first=ack;
        last_recv_ack_cnt.second=1;
    }

    Packet* first_inflight_pkt = in_flight_pkt.front();
    if(ack > first_inflight_pkt->seq_num) state = RESEND_OLD;
    else if (ack < first_inflight_pkt->seq_num){
        state = WAIT_ACK;
    }else{
        // get acked for the correct pkt
        in_flight_pkt.pop_front();
        if(!allSent) state = SEND_NEW;
        else{
            if(in_flight_pkt.size()==0) working = false;
        }
        if(window_size == ack - window_init_seq + 1){
            printf("window size gonna change, current window size: %d\n",window_size);
            switch(congestion_state){
                case EXPO: fastRecovery(); break;
                case ADDITIVE: AIMD();
            }
            printf("window size changed, current window size: %d\n",window_size);
        }
    }
}

void closeConnection(){
    // signal the end of transmission
    Packet* pkt = new Packet;
    pkt->seq_num = init_seq;
    pkt->data_size = 0;
    if (sendto(s, pkt, pkt->data_size+sizeof(unsigned int)*2, 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
	    perror("sendto failed");
    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    init(hostname, hostUDPport, filename,bytesToTransfer);
    handshake();
    printf("start sending file...\n");
    while(working){
        switch(state){
            case SEND_NEW: 
                send_new_pkt();
                break;
            case RESEND_OLD: 
                resend_pkt();
                break;
            case WAIT_ACK:
                // read all acks and deal with the acks
                waitACK();
                break;
        }
    }

	/* Send data and receive acknowledgements on s
    int acc = 0;
    while(acc<bytesToTransfer){
        // make pkt
        Packet* pkt = new Packet;
        pkt->seq_num = last_acked_seq + 1; 
        pkt->data_size = bytesToTransfer-acc > MSS? MSS : (bytesToTransfer-acc);
        fread(pkt->data,sizeof(char),pkt->data_size,fp);

        if (sendto(s, pkt, pkt->data_size+sizeof(unsigned int)*2, 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
	        perror("sendto failed");
        }
        printf("sent data seq:%d\n",pkt->seq_num);

        // wait for ack from receiver
        while(1){
            unsigned int acked_seq;
            int ret=timeOutRecv(&acked_seq,sizeof(unsigned int));
            
            if(ret == -1){
                // time out detected
                if (sendto(s, pkt, pkt->data_size+sizeof(unsigned int)*2, 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
	                perror("sendto failed");
                }
                printf("sent data seq:%d\n",pkt->seq_num);
                continue;
            }
            
            if (ret != sizeof(unsigned int)) {
                perror("cannot decode server reply");
            }

            if(acked_seq == pkt->seq_num){
                acc += pkt->data_size;
                last_acked_seq++;
                break; 
            }else{
                if (sendto(s, pkt, pkt->data_size+sizeof(unsigned int)*2, 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
	                perror("sendto failed");
                }
                printf("sent data seq:%d\n",pkt->seq_num);
            }
        }
    }  */
    closeConnection();
    printf("Closing the socket\n");
    close(s);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


