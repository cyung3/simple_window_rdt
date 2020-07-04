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


enum state{ No_Connection = 0, Waiting = 1, Connected = 2, Closing = 3, Closed = 4 };
typedef uint16_t Word;

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
struct sockaddr_in servaddr;
//struct hostent *server;
socklen_t len;
Word seq_num,ack_num,SYN,ACK,FIN;

void build_ack(Word seq_num, Word ack_num, Word SYN, Word ACK, Word FIN, unsigned char* return_buf){
	memset(return_buf, 0, MAX_UDP_SIZE);
	uint16_t sn = htons(seq_num);
  	uint16_t an = htons(ack_num);
  	uint16_t sf = htons(SYN);
  	uint16_t af = htons(ACK);
  	uint16_t ff = htons(FIN);

	memcpy(return_buf, (unsigned char*)&sn, 2);
	//memcpy(return_buf+1, &(seq_num+1), 1);
	//if(ACK == 0){
		memcpy(return_buf+2, (unsigned char*) &an, 2);//}
  	//memcpy(return_buf+3, &(ack_num+1), 1);
  	memcpy(return_buf+4, (unsigned char*)&sf, 2);
  	memcpy(return_buf+6, (unsigned char*)&af, 2);
  	memcpy(return_buf+8, (unsigned char*)&ff, 2);
  	//printf("seq_num = %02X%02X, ack_num = %hu, SYN=%hu,ACK=%hu,FIN=%hu\n", return_buf[0], return_buf[1], return_buf[2],return_buf[4], return_buf[6], return_buf[8]);

	

	//memset(&return_buf[10], 0, 2);
}

int recv_ack(Word seq_num_check, Word ack_num_check, Word SYN_check, Word ACK_check, Word FIN_check){
	Word seq_num,ack_num,SYN,ACK,FIN;

	memset(request_buf, 0, MAX_UDP_SIZE);
	int request_len = recvfrom(sockfd, request_buf, 12, 0, (struct sockaddr*)&servaddr, &len);
	//Server will always send messages that are of length 12
	if (request_len < 0){
		perror("Socket receive error");
		exit(1);
	}else if(request_len == 12){
		// obtained response

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

    	//Check flags are correct
    	if((SYN == SYN_check) && (ACK == ACK_check) && (FIN == FIN_check)){
    		//seq_num check to implement
    		//if ((seq_num == seq_num_check) && (ack_num == ack_num_check))
    		//{
		        if((seq_num < MAX_SEQUENCE_NUMBER) && (seq_num >= 0)){
		          //prep ack_sending
		          cur_ack_num = seq_num + 1; //or length of data in other states
		        }else if (seq_num >= MAX_SEQUENCE_NUMBER){
		          cur_ack_num = 0;
		        }
		        if(ACK == 1){
		        	cur_seq_num = ack_num;
		    	}
	          	return 1;
        //   	}
        //   	else if((SYN == SYN_check) && (ACK==ACK_check) && (SYN == 1) && (ACK == 1))
        //   	{
        //   		//if SYN ACK are both 1
        //   		if(seq_num == seq_num_check){
	       //   		if((seq_num < MAX_SEQUENCE_NUMBER) && (seq_num >= 0)){
			     //      //prep ack_sending
			     //      cur_ack_num = seq_num + 1; //or length of data in other states
			     //    }else if (seq_num >= MAX_SEQUENCE_NUMBER){
			     //      cur_ack_num = 0;
			     //    }
			     //    if(ACK == 1){
			     //    	cur_seq_num = ack_num;
			    	// }
		      //     	return 1;
	       //    	}
	       //    	return 0;
        //   	}
        //   	else{return 0;}
          	
          	//memset(response_buf, 0, MAX_UDP_SIZE);
          	//build_ack(cur_seq_num, cur_ack_num, 0, 1, 0, response_buf);
    	}else{ return 0;}
	}
	else{
		perror("Packet not of correct length\n");
		return 0;
	}
}

void send_packet(Word seq_num, Word ack_num, Word SYN, Word ACK, Word FIN, int numBytes, char* data){
	build_ack(seq_num, ack_num, SYN, ACK, FIN, response_buf);
	if(data!=NULL){
		memcpy((void*)(response_buf+12), data, (numBytes-12));
	}
	// for (int i = 12; i < numBytes; i++){
	// 		printf("%c", response_buf[i]);
	// }printf("\n");
	//printf("SeqNum = %hi, AckNum = %hi, SYN = %hi, ACK = %hi, FIN = %hi\n", response_buf[0], response_buf[2],response_buf[4], response_buf[6], response_buf[8]);
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

void open_file(){
	fp = fopen(filename, "r");
	if( fp == NULL ){
		perror("No such file.\n");	
	}else{
		printf("Open file\n");
	}
}

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

void send_data(){
	//printf("Sending data\n");
	int bytesRead = 0;
	int ok;
	char data_buf[MAX_DATA_SIZE];
	open_file();
	if(fp != NULL){
		//while file still not finished
		int numSegments = 0;
		while((bytesRead = fread(data_buf, 1, MAX_DATA_SIZE, fp)) > 0){
			send_packet(cur_seq_num, cur_ack_num, 0, 1, 0, (bytesRead+12), data_buf);
			numSegments++;
			//wait to receive ack
			
		
			ok = recv_ack(cur_seq_num, cur_ack_num, 0, 1, 0);
			if(ok <= 0){
				perror("Error receiving message\n");
			}
		}
		fclose(fp);	
	}else{
		perror("Exiting...\n");
		exit(1);
	}

	
}


int establish_conn(){
	cur_ack_num = 0;
	cur_seq_num = rand() % MAX_SEQUENCE_NUMBER;
	//printf("Ack_num = %hu, Seq_num = %hu\n", cur_ack_num, cur_seq_num);

	send_packet(cur_seq_num, cur_ack_num, 1, 0, 0, 12, NULL);

	//wait for acknowledgement
	//printf("Sent first message, blocking and waiting for connection\n");
	return recv_ack(cur_seq_num, cur_ack_num, 1, 1, 0);	
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
	if(establish_conn() > 0){
		send_data();
		close_conn();
		//printf("Finished\n");
	}else{

	}
	


	close(sockfd);
}