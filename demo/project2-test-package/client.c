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
#include <time.h>


void send_data();
int establish_conn();



enum state{ No_Connection = 0, Waiting = 1, Connected = 2, Closing = 3, Closed = 4 };
typedef uint16_t Word;

const int WINDOW_SIZE = 10;
const int MAX_SEQUENCE_NUMBER = 25600; 
const int MAX_UDP_SIZE = 524; //bytes - 512B for payload 12 for header
const int MAX_DATA_SIZE = 512;
void process_request(int client_fd);
int status = No_Connection;
Word cur_ack_num = 0;
Word cur_seq_num = 0;
FILE * fp;
unsigned int port;
unsigned int sockfd;
char* hostname;
char* filename;
unsigned char response_buf[MAX_UDP_SIZE];
unsigned char request_buf[MAX_UDP_SIZE];
unsigned char pipeline[MAX_UDP_SIZE*WINDOW_SIZE] = {0};
struct sockaddr_in servaddr;
//struct hostent *server;
socklen_t len;
Word seq_num,ack_num,SYN,ACK,FIN;
int save_before_close_seq=0;
int save_seq=0; 
int save_ack=0;

void build_ack(Word seq_num, Word ack_num, Word SYN, Word ACK, Word FIN, unsigned char* return_buf){
	memset(return_buf, 0, MAX_UDP_SIZE);
	uint16_t sn = htons(seq_num);
  	uint16_t an = htons(ack_num);
  	uint16_t sf = htons(SYN);
  	uint16_t af = htons(ACK);
  	uint16_t ff = htons(FIN);

	memcpy(return_buf, (unsigned char*)&sn, 2);
	memcpy(return_buf+2, (unsigned char*) &an, 2);
  	memcpy(return_buf+4, (unsigned char*)&sf, 2);
  	memcpy(return_buf+6, (unsigned char*)&af, 2);
  	memcpy(return_buf+8, (unsigned char*)&ff, 2);


	

	//memset(&return_buf[10], 0, 2);
}

void recv_ack(Word* seq_num, Word* ack_num, Word* SYN, Word* ACK, Word* FIN){
	//Word seq_num,ack_num,SYN,ACK,FIN;

	memset(request_buf, 0, MAX_UDP_SIZE);
	int request_len = recvfrom(sockfd, request_buf, 12, 0, (struct sockaddr*)&servaddr, &len);
	//Server will always send messages that are of length 12
	if (request_len < 0){
		perror("Socket receive error");
		exit(1);
	}else if(request_len == 12){
		// obtained response

	  	memcpy((void*)seq_num,request_buf, 2);
	    memcpy((void*)ack_num,request_buf+2,2);
	    memcpy((void*)SYN, request_buf+4, 2);
	    memcpy((void*)ACK, request_buf+6, 2);
	    memcpy((void*)FIN, request_buf+8, 2);

	    *seq_num = ntohs(*seq_num);
	    *ack_num = ntohs(*ack_num);
	    *SYN = ntohs(*SYN);
	    *ACK = ntohs(*ACK);
	    *FIN = ntohs(*FIN);
    	
	    printf("RECV %d %d", *seq_num, *ack_num);
	    if (*SYN == 1)
	    {
	      printf(" SYN");
	    }
	    if(*ACK == 1){
	      printf(" ACK");
	    }
	    if (*FIN == 1)
	    {
	      printf(" FIN");
	    }
	    printf("\n");

	}
	else{
		perror("Packet not of correct length\n");
	}
}


void send_packet(Word seq_num, Word ack_num, Word SYN, Word ACK, Word FIN, int numBytes, char* data){
	build_ack(seq_num, ack_num, SYN, ACK, FIN, response_buf);
	if(data!=NULL){
		memcpy((void*)(response_buf+12), data, (numBytes-12));
	}

	int ret = sendto(sockfd, (void*)response_buf, numBytes, 0, (struct sockaddr*)&servaddr, len);
	if(ret < 0){
		perror("Error with sendto()");
        //printf("return val: %d", ret);
        exit(1);
	}else{
		printf("SEND %d %d", seq_num, ack_num);
		if (SYN == 1){
			printf(" SYN");
		}
		if (ACK == 1){
			printf(" ACK");
		}
		if (FIN == 1){
			printf(" FIN");
		}
		printf("\n");
	}

}

void loop(){
	int ok = 0;
	Word seq_num,ack_num,SYN,ACK,FIN;
	switch(status){
	    //check ack num = 0 for ack = 0
	    case No_Connection:
	    	ok = establish_conn();
	    	if(ok == 1){
	    		status = Waiting;
	    	}else{
	    		perror("Failed to establish connection.\n");
	    	}
	    break;
	    //Connection established already 
	    // Needs to receive an ack for the syn-ack sent
	    // can have a payload
	    case Waiting: 
	      status = Connected;
	    break;

	    case Connected:
	     	send_data();     	
	     	save_before_close_seq = (cur_ack_num-1)%(MAX_SEQUENCE_NUMBER+1);
	     	status = Closing;
	    break;


	    case Closing:
	    	//SENDING FIN
	    	//printf("Closing\n");
	    	send_packet(cur_seq_num, 0, 0, 0, 1, 12, NULL);

	    	recv_ack(&seq_num, &ack_num, &SYN, &ACK, &FIN);
	    	//printf("sn %d==%d, an %d==%d sf %d af %d ff %d\n", seq_num,save_before_close_seq, ack_num,cur_seq_num+1,SYN,ACK,FIN);
    		if((ACK == 1) && (ack_num == (cur_seq_num+1)%(MAX_SEQUENCE_NUMBER+1)) && (seq_num == save_before_close_seq) && (FIN == 0) && (SYN == 0)){
	    		//saveack = cur_ack_num;
	    		save_seq = seq_num;
	    		save_ack = ack_num;
	    		status = Closed;
    		}else{
    			//resend

    		}
	    	
	      	
	    break;

	    case Closed:
	      	recv_ack(&seq_num, &ack_num, &SYN, &ACK, &FIN);
	      	//START 2s TIMER


			if(FIN == 1 && seq_num == save_seq && ack_num == 0 && SYN==0 && ACK==0){
				cur_ack_num = (seq_num+1)%(MAX_SEQUENCE_NUMBER+1);
				send_packet(save_ack, (seq_num+1)%(MAX_SEQUENCE_NUMBER+1), 0, 1, 0, 12, NULL);
				status = No_Connection;
			}

	    break;

	    default: 
	      perror("FSM error");
	      exit(1);
	}

}
  


void open_file(){
	fp = fopen(filename, "r");
	if( fp == NULL ){
		perror("No such file.\n");
		exit(1);	
	}
}

/*
void close_conn(){
	//send initial FIN packet
	//printf("Closing connection\n");
	send_packet(cur_seq_num, 0, 0, 0, 1, 12, NULL);
	//printf("Send client FIN\n");
	int finok;
	//if recv ack
	int ok = recv_ack(seq_num, ack_num, 0, 1, 0);
	//printf("Recv server ACK to client FIN\n");
	if (ok > 0){
		int saveseq = cur_seq_num;
		//and recv fin from server
		finok = recv_ack(cur_ack_num, 0, 0, 0, 1);
		//printf("Recv server FIN\n");
		if (finok > 0){
			//send back ack
			send_packet(saveseq, cur_ack_num, 0, 1, 0, 12, NULL);
			//printf("Sending ACK to server FIN\n");
		}else{
			//resend
		}
	}
	else{
		//resend
	}
}
*/


void send_data(){
	int bytesRead = 0;
	char data_buf[MAX_DATA_SIZE];
	Word seq_num,ack_num,SYN,ACK,FIN;
	Word pseq_num, pack_num, pSYN, pACK, pFIN;
	open_file();
	if(fp != NULL){
		//while file still not finished
		fseek(fp, 0L, SEEK_END);
		double file_size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		
		/*
			rounds are 10 packets each
			pipeline arr holds 10 segments
				first packet send has ack num and ACK flag set others have ACK = 0
					- server should grab the first ack num sent and use for 9 other packets
					When receive packet, process 
		*/
		{
			int numSegments = file_size/MAX_DATA_SIZE;
			
			int curSegment = 0;
			
			unsigned char* datagram = pipeline;
			for(int round = 0; round < numSegments; round=round+10){
				int numSegmentsInPipe = 0;
				//edge cases?
				//if a multiple of 512 data
				for(;curSegment < 10; curSegment++){
					//read file
					datagram = pipeline+(MAX_UDP_SIZE*curSegment);
					bytesRead = fread(data_buf, 1, MAX_DATA_SIZE, fp);
					if(bytesRead < 0){
						perror("error reading file\n");
					}else if (bytesRead == 0){
						break;
					}
					else{
						//copy to pipeline to be sent
						//if 0th segment, ACK = 1
						if(curSegment == 0){
							send_packet(cur_seq_num, cur_ack_num, 0, 1, 0, (bytesRead+12), data_buf);
							//offset cur_seq_num by amount of bytes.
							cur_seq_num = (cur_seq_num + bytesRead)%(MAX_SEQUENCE_NUMBER+1);
						}else{
							//these packets are not acking any packets sent by the 
							send_packet(cur_seq_num, 0, 0, 0, 0, (bytesRead+12), data_buf);
							cur_seq_num = (cur_seq_num + bytesRead)%(MAX_SEQUENCE_NUMBER+1);
						}
						memcpy(datagram, response_buf, (bytesRead+12));
						numSegmentsInPipe++;

						// if(bytesRead < MAX_DATA_SIZE){
						// 	break;
						// }
					}

				}
				numSegmentsInPipe = curSegment;
				curSegment = 0;
				//recvfrom blocks
				for(;curSegment < numSegmentsInPipe; curSegment++){
					datagram = pipeline+(MAX_UDP_SIZE*curSegment);

					recv_ack(&seq_num, &ack_num, &SYN, &ACK, &FIN);

					//Parse pipeline
					memcpy((void*)&pseq_num,datagram, 2);
		    		memcpy((void*)&pack_num,datagram+2,2);
		    		memcpy((void*)&pSYN, datagram+4, 2);
		    		memcpy((void*)&pACK, datagram+6, 2);
		    		memcpy((void*)&pFIN, datagram+8, 2);

		    		//process data
					if(ACK == 1){
						if(cur_seq_num == ack_num && cur_ack_num == seq_num){
								cur_seq_num = ack_num;
								cur_ack_num = (seq_num + 1)%(MAX_SEQUENCE_NUMBER+1);
						}
					}//else but we assume no corruption
				}
				curSegment = 0;

			}
		}
		//AFTER SENDING AND RECEIVING, UPDATE cur_ack_num and cur_seq_num
		// {

		// 				//wait to receive ack
		// 	recv_ack(&seq_num, &ack_num, &SYN, &ACK, &FIN);
		// 	if(SYN == 0 && ACK == 1 && FIN == 0 && ack_num == cur_seq_num+bytesRead && seq_num == cur_ack_num){
		// 			//update numbers
		// 		if((seq_num < MAX_SEQUENCE_NUMBER) && (seq_num >= 0)){
		//           	cur_ack_num = seq_num + 1; //or length of data in other states
		//         }else if (seq_num >= MAX_SEQUENCE_NUMBER){
		//           	cur_ack_num = 0;
		//         }
		//         cur_seq_num = ack_num;
		//     }
		// }


		fclose(fp);	
	}else{
		perror("Exiting...\n");
		exit(1);
	}
}


int establish_conn(){
	cur_ack_num = 0;
	cur_seq_num = rand() % (MAX_SEQUENCE_NUMBER+1);

	send_packet(cur_seq_num, cur_ack_num, 1, 0, 0, 12, NULL);

	//wait for acknowledgement
	Word seq_num, ack_num, SYN, ACK, FIN;
	recv_ack(&seq_num, &ack_num, &SYN, &ACK, &FIN);	
	if((SYN == 1) && (ACK == 1) && (FIN == 0) && (ack_num == cur_seq_num+1)){
        if((seq_num < (MAX_SEQUENCE_NUMBER)) && (seq_num >= 0)){
          //prep ack_sending
          cur_ack_num = seq_num + 1; //or length of data in other states
        }else if (seq_num >= MAX_SEQUENCE_NUMBER){
          cur_ack_num = 0;
        }
        cur_seq_num = ack_num;
    	return 1;
    }
    return 0;
}


int main(int argc, char* argv[]){
	if(argc != 4){
		perror("Invalid number of arguments\nUsage: ./client -<HOST-OR-IP> <PORT> <FILENAME>");
    	exit(1);
	}
	srand(time(NULL));
	hostname = argv[1];
	port = atoi(argv[2]);
	filename = argv[3];

	//Create Socket and set as UDP socket
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0))< 0){
		perror("Cannot create socket");
		exit(1);
	}

	if (strcmp(hostname, "localhost")==0){
		hostname = "127.0.0.1";
	}
	// server = gethostbyname(hostname);
	// if(server == NULL){
	// 	perror("No such given host.\n");
	// 	exit(0);
	// }

	memset(&servaddr, 0, sizeof(struct sockaddr_in));

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, hostname, &servaddr.sin_addr);
	len = sizeof(servaddr);
	
	do{
		loop();
	}while(status != No_Connection);
	


	close(sockfd);
}