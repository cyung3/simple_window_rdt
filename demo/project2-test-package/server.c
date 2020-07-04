
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

enum state{ No_Connection = 0, Waiting = 1, Connected = 2, Closing = 3, Closed = 4 };
/*
Simplifications:
1. Do not need to implement checksum computation or verification
2. Assume there is no corruption, reordering,
  and duplications of packets in transmit
  Only unrealiablilty is packet loss
3. Do not need to estimate RTT or update RTO using RTT
  RTO is always fixed
4. No need to handle parallel connections
5. Do not need to realize congestion control
*/

/*
log2(25600) = log2(MAX_SEQUENCE_NUMBER) = 14.6438 => 15b
        -> pad with zero -> 16bits == 2B


TCP HEADER FORMAT
------------------------
Sequence number = 2B
Ack number = 2B
ACK flag = 2B
SYN flag = 2B
FIN flag = 2B
Padding = 2B
-------------------------
Total = 12B
*/

/*
  NEED TO IMPLEMENT:
  Seq and ack number checking
  ack_num = 0 when ACK = 0
  large file transfer
*/

typedef uint16_t Word;
const int MAX_SEQUENCE_NUMBER = 25600; 
const int MAX_UDP_SIZE = 524; //bytes - 512B for payload 12 for header
const int MAX_DATA_SIZE = 512;
const int WINDOW_SIZE = 10;

int fileNum = 1;
char fileNumStr[MAX_DATA_SIZE];
FILE* fp;
int status = No_Connection;
int cur_ack_num = 0;
int cur_seq_num = 0;

struct sockaddr_in servaddr;
struct sockaddr_in clientaddr;
socklen_t len;

unsigned char request_buf[MAX_UDP_SIZE] = {0};
unsigned char response_buf[MAX_UDP_SIZE] = {0};
unsigned char pipeline[12*WINDOW_SIZE] = {0};

int generate_ack(){
  return rand() % (MAX_SEQUENCE_NUMBER+1);
}

// This might not be needed, server only responds acks
// No data is sent
void build_ack(Word seq_num, Word ack_num, Word SYN, Word ACK, Word FIN, unsigned char* return_buf){
  memset(return_buf, 0, MAX_UDP_SIZE);
  uint16_t sn = htons(seq_num);
  uint16_t an = htons(ack_num);
  uint16_t sf = htons(SYN);
  uint16_t af = htons(ACK);
  uint16_t ff = htons(FIN);

  memcpy(return_buf, (unsigned char*)&sn, 2);
  memcpy(return_buf+2, (unsigned char*)&an, 2);
  memcpy(return_buf+4, (unsigned char*)&sf, 2);
  memcpy(return_buf+6, (unsigned char*)&af, 2);
  memcpy(return_buf+8, (unsigned char*)&ff, 2);
  //memset(&return_buf[10], 0, 2);
}

//not the same file
void open_file(char* mode){
  sprintf(fileNumStr, "%d.file", fileNum);
  fp = fopen(fileNumStr, mode);
  if(fp == NULL){
    perror("Error opening file\n");
  }
}
void log_file(unsigned char* data, size_t numBytes){
  int num = fwrite(data, sizeof(char), numBytes, fp);
  if(num < 0){
    perror("Error writing to file!");
  }
}

void process_request(int sockfd){
  //MOVE THESE VARIABLES OUTSIDE

  //char data[MAX_DATA_SIZE];
  Word seq_num = -1;
  Word ack_num = -1;
  Word SYN = 0;
  Word ACK = 0;
  Word FIN = 0;

  int numSegmentsInPipe = 0;
  size_t request_len;
  memset(response_buf,0 ,MAX_UDP_SIZE);
  memset(request_buf, 0, MAX_UDP_SIZE);
  len = sizeof(clientaddr);
  request_len = 0;
  
  if(status != Closing){
    request_len = recvfrom(sockfd, request_buf, MAX_UDP_SIZE, 0, (struct sockaddr*)&clientaddr, &len);
    if (request_len < 0){
      perror("Receive error\n");
      exit(1);
    }
    //ELSE IF request_len > 12
      //Parse request buffer
    if(request_len < 12){
      perror("Header not found\n");
      request_buf[request_len] = '\0';
      exit(1);}

  	memcpy((void*)&seq_num,request_buf, 2);
    memcpy((void*)&ack_num,request_buf+2,2);
    memcpy((void*)&SYN, request_buf+4, 2);
    memcpy((void*)&ACK, request_buf+6, 2);
    memcpy((void*)&FIN, request_buf+8, 2);

    seq_num = ntohs(seq_num);
    ack_num = ntohs(ack_num);
    SYN = ntohs(SYN);
    ACK = ntohs(ACK);
    FIN = ntohs(FIN);

    printf("RECV %d %d", seq_num, ack_num);
    if (SYN == 1)
    {
      printf(" SYN");
    }
    if(ACK == 1){
      printf(" ACK");
    }
    if (FIN == 1)
    {
      printf(" FIN");
    }
    printf("\n");
  }


    
    //SET ACK = 0 IF ACK FLAG IS FALSE
    //Process request based on current state
  switch(status){
    case No_Connection:
      if((SYN==1) && (ACK==0) && (FIN==0) && (request_len == 12)){

        if((seq_num < MAX_SEQUENCE_NUMBER) && (seq_num >= 0)){
          //prep ack_sending
          cur_ack_num = seq_num + 1; //or length of data in other states
        }else if (seq_num >= MAX_SEQUENCE_NUMBER){
          cur_ack_num = 0;
        }
        


        //randomly generate sequence number at beginning of connection for security purposes
        cur_seq_num = generate_ack();
        build_ack(cur_seq_num, cur_ack_num, 1, 1, 0, response_buf); //response_buf[12]

        status = Waiting;

        int r = sendto(sockfd, response_buf, 12, 0, (struct sockaddr*) &clientaddr, len);
        if(r < 0){
          perror("Error with sendto()");
          exit(1);
        }else{
          printf("SEND %d %d SYN ACK\n", cur_seq_num, cur_ack_num);
        }
        //Create new .file
        open_file("w");
        fclose(fp);

        //edge cases dont matter because we add one later anyways
        //if -1 will just be added to be 0 after

      }else{ perror("SYN!=1,ACK!=0,FIN!=0\n");}
      break;
    //Connection established already 
    // Needs to receive an ack for the syn-ack sent
    // can have a payload
    case Waiting: 
      if((SYN==0) && (ACK==1) && (FIN==0) && (request_len > 12)){
        if((cur_ack_num != seq_num) && (cur_seq_num+1 != ack_num)){
          return;
        }

        int datalength = request_len - 12;

        cur_ack_num = (seq_num + datalength) % (MAX_SEQUENCE_NUMBER+1);
        

        cur_seq_num = ack_num;
        
        numSegmentsInPipe=0;
        //log data
        open_file("a");
        log_file(request_buf+12, datalength);
        fclose(fp);


        build_ack(cur_seq_num, cur_ack_num, 0, 1, 0, response_buf);
        memcpy(pipeline, response_buf,12);
        numSegmentsInPipe++;
        int r = sendto(sockfd, response_buf, 12, 0, (struct sockaddr*) &clientaddr, len);
        if (r < 0) { perror("Error with sendto()"); exit(1);
        }else{ printf("SEND %d %d ACK\n", cur_seq_num, cur_ack_num); }
        status = Connected;
          
        //}
      }
      break;
    //After receiving ack and connection established, resume operations as normal
    case Connected:

      if((SYN==0) && (FIN==0) && (request_len >= 12)){

        int datalength = request_len - 12;

        if(ACK == 1){
          //if(numSegmentsInPipe == 10 || datalength < MAX_DATA_SIZE){
          cur_seq_num = (cur_seq_num + 1) % (MAX_SEQUENCE_NUMBER+1);
          numSegmentsInPipe = 0;
        }
        //idk about this
        else if((cur_ack_num+datalength != seq_num) && (ack_num != 0)){
          return;
        }

        open_file("a");
        log_file(request_buf+12, datalength);
        fclose(fp);

        build_ack(cur_seq_num, (cur_ack_num+datalength)%(MAX_SEQUENCE_NUMBER+1), 0, 1, 0, response_buf);
        memcpy((pipeline+(12*numSegmentsInPipe)), response_buf, 12);
        int r = sendto(sockfd, (pipeline+(12*numSegmentsInPipe)), 12, 0, (struct sockaddr*) &clientaddr, len);
        if (r < 0) { perror("Error with sendto()"); exit(1);
        }else{ printf("SEND %d %d ACK\n", cur_seq_num, cur_ack_num); }


        numSegmentsInPipe++;
        cur_ack_num = (cur_ack_num + datalength) % (MAX_SEQUENCE_NUMBER+1);


        
      }else if ((SYN==0) && (ACK==0) && (FIN==1)){
        /// FILL IN - 
        //send ACK FOR FIRST FIN MESSAGE
        // then move to closing
        //no payload
        //if((seq_num == cur_seq_num) && (ack_num == 0)){

        if((cur_ack_num != seq_num) && (ack_num != 0)){
          return;
        }

        if(request_len == 12){
          if((seq_num < MAX_SEQUENCE_NUMBER) && (seq_num >= 0)){
            cur_ack_num = seq_num + 1;
          }
          else{
            cur_ack_num = 0;
          }

        build_ack(cur_seq_num, cur_ack_num, 0, 1, 0, response_buf);
        int r = sendto(sockfd, response_buf, 12, 0, (struct sockaddr*) &clientaddr, len);
        if (r < 0){
          perror("Error with sendto()");
          exit(1);
        }else{ printf("SEND %d %d ACK\n", cur_seq_num, cur_ack_num); }
        fileNum++;
        status = Closing;
        }
      }

      
      break;
    case Closing:
      //NEED TO FIND A WAY TO CALL CLOSING WITH IN SAME ITERATION
      //if statements no neccessary
      //add if statement with flag for complete here later on
      build_ack(cur_seq_num, 0, 0, 0, 1, response_buf);
      int r = sendto(sockfd, response_buf, 12, 0, (struct sockaddr*) &clientaddr, len);
      if (r < 0)
      {
        perror("Error with sendto()");
        exit(1);
      }else{ printf("SEND %d 0 FIN\n", cur_seq_num); }

      status = Closed;

      break;
    case Closed:
      //printf("Closed\n");
      if((SYN==0) && (ACK==1) && (FIN==0)){
        if((cur_ack_num != seq_num) && (cur_seq_num+1 != ack_num)){
          perror("incorrect seq_num or ack_num\n");
          return;
        }

        //if((seq_num <= MAX_SEQUENCE_NUMBER) && (seq_num >= 0) && (request_len == 12)){
          //RESET BUFFERS
          //memset(clientaddr,0, sizeof(struct sockaddr_in));
          //memset(request_buf,0, MAX_UDP_SIZE);
          //memset(response_buf,0,MAX_UDP_SIZE);
          //memset(data,0,MAX_UDP_SIZE);
          //Set status to No_Connection after connection is closed
          status = No_Connection;
        //}
        
      }
      break;
    default: 
      perror("FSM error");
      exit(1);
  }
  
}


/*
Check_No_Connection
  receives all header data
  returns 1 if SYN is set to 1, ACK = 0, FIN = 0 
  seq_num in bounds, and ack_num = 0 (irrelavant)
  and there is no data payload
  else return 0


*/
/*int check_No_Connection(Word seq_num, Word ack_num, Word SYN, Word ACK, Word Fin, int request_buf_size){
  if(SYN+ACK+FIN == 1){
    if((seq_num <= MAX_SEQUENCE_NUMBER) && (seq_num >= 0) && (request_buf_size == 12)){
      status = Waiting;
      cur_ack_num = seq_num + 1; //or length of data in other states
      cur_seq_num = generate_ack();
      send_ack(cur_seq_num, cur_ack_num, 0, 1, 0);
      return 1;
    }
    return 0;
  }
  return 0;
}*/


int main(int argc, char* argv[]){
  if(argc != 2){
    perror("Invalid number of arguments\nUsage: ./server -<port>");
    exit(1);
    //errno
  }

  int sockfd;
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  // *** Initialize local listening socket address ***
  memset((void*)&servaddr, 0, sizeof(struct sockaddr_in));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(atoi(argv[1]));
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY allows to connect to any one of the host’s IP address

  // *** Socket Bind ***
  //printf("Trying to bind port\n");
  if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) < 0) {
    perror("bind");
    exit(1);
  }
  while(1){
    process_request(sockfd);
  }
}

