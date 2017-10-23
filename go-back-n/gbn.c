#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include "gbn.h"

state_t state;

uint16_t checksum(uint16_t *buf, int nwords)
{
	uint32_t sum;

	for (sum = 0; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

void handle_alarm(int sig){
	/*Signal handler for when a data packet times out.
	 * Reset current packet to the beginning of window
	 * thus resending all paeckets in the window */
	printf("RETRY: %d\n",state.retry);
	if(state.retry<=5){
		state.seq_curr=state.seq_base+1;
		state.retry++;
		state.win_size=1;
		state.seq_max=state.seq_base+state.win_size-1;
	}
	else{
		printf("ERROR: Exceeded retry limit. Check for connection error!\n");
		return;
	}
	signal(SIGALRM,handle_alarm);
	alarm(3);
	return;
}

ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags){
	state.win_size=1;
	state.retry=0;
	state.seq_base=1;
	state.seq_max=state.seq_base+state.win_size-1;
	int status=-1;
	printf("INFO: Length of file: %d bytes\n",len);
	/*Calculate number of packets*/
	int packet_num;
	if(len%DATALEN==0)packet_num=len/DATALEN;
	else packet_num=len/DATALEN+1;
	/*if current packet is in the window but not at the beginning, don't reset alarm.
	 *Only reset alarm when the window has moved.*/
	bool reset_alrm=true;
	state.seq_curr=1;

    printf("PACKET COUNT: %d \n",packet_num);
	while(state.seq_curr<=packet_num+1){
		/*install alarm handler*/
		signal(SIGALRM,handle_alarm);
		if (state.seq_curr==N+1)state.seq_curr=1;
		state.seq_max=state.seq_base+state.win_size-1;
		gbnhdr *data_packet=malloc(sizeof(*data_packet));
		data_packet->type=DATA;
		data_packet->seqnum=state.seq_curr;
		data_packet->checksum= (uint16_t) 0;
		memcpy(data_packet->data,buf+(state.seq_curr-1)*DATALEN,DATALEN);

		state.seq_curr++;
		if(reset_alrm==true){
			alarm(3);
			reset_alrm=false;
		}

		data_packet->checksum = checksum(data_packet, sizeof(*data_packet) / sizeof(uint16_t));
		printf("INFO: Checksum of packet: %d\n",data_packet->checksum);

		if(state.seq_curr-1==packet_num && len%DATALEN>0)
			status=maybe_sendto(sockfd,data_packet,5+len%DATALEN,0,state.address,state.socklen);
		else if(state.seq_curr-1<packet_num)
			status=maybe_sendto(sockfd,data_packet,sizeof(*data_packet),0,state.address,state.socklen);

		if(status==-1){
			printf("ERROR: DATA packet %d send failed.\n",state.seq_curr-1);
		}
		else{
			 printf("INFO: DATA packet %d sent successfully.\n",state.seq_curr-1);
			 state.retry=0;
		}

		/*Reached end of window, wait for the correct ACK*/
		while(state.seq_curr-1==state.seq_max){
			if(state.retry<=5){
				if (state.seq_base>packet_num) return (0);
				gbnhdr *packet = malloc(sizeof(*packet));
				status = recvfrom(sockfd, packet, sizeof(*packet), 0, state.address, &state.socklen);
				if(status!=-1 ){
					if(packet->type==DATAACK){
						printf("INFO: ACK received requesting packet %d\n",packet->seqnum);
						if(packet->seqnum>state.seq_base){
							alarm(0);
							state.seq_max=packet->seqnum+state.win_size-1;
							state.seq_base=packet->seqnum;
							reset_alrm=true;
							state.win_size=2;
						}
						break;
					}
				}
			}else return(-1);
		}
		
		
	}
	return(0);
}

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags){

	int retryCount = 1;
	int status = 0;
	bool is_seq = false;

	gbnhdr *packet = malloc(sizeof(*packet));
	gbnhdr *ack_packet = malloc(sizeof(*ack_packet));

	while(retryCount <= MAX_RETRY_ATTEMPTS && state.state != CLOSED){

		status = recvfrom(sockfd, packet, sizeof(*packet), 0, state.address, &state.socklen);
		if(status != -1){
			uint16_t checksum_actual = packet->checksum;
			packet->checksum = (uint16_t) 0;
			uint16_t checksum_expected  = checksum(packet, sizeof(*packet) / sizeof(uint16_t));	
			printf("INFO: Expected checksum value : %d . Actual checksum value : %d\n",checksum_expected,checksum_actual);
			if(packet->type == DATA){
				/* Sender has sent a DATA packet */
				printf("INFO: DATA received successfully.\n");
				/* Receiver responds with DATAACK */
				ack_packet->type=DATAACK;
				
				printf("INFO: Packet seqnum : %d State seqnum: %d\n",packet->seqnum,state.seqnum);
                if(state.seqnum == packet->seqnum){
					printf("INFO: Received DATA packet is in sequence.\n");
					memcpy(buf,packet->data,sizeof(packet->data));
                    state.seqnum = packet->seqnum + 1;
                    ack_packet->seqnum = state.seqnum;
					ack_packet->checksum = 0;
					is_seq = true;
                }else {
					printf("INFO: DATA packet has the incorrect sequence number.\n");
					ack_packet->type=DATAACK;
                    ack_packet->seqnum = state.seqnum;
					ack_packet->checksum = 0;
					is_seq = false;
                }
                /* Sending ACK / duplicate ACK */
                if (maybe_sendto(sockfd, ack_packet, sizeof(*ack_packet), 0, state.address, state.socklen) == -1) {
                        printf("ERROR: ACK sending failed.\n");
                        state.state = CLOSED;
                        break;
                } else {
					if(is_seq){
						printf("INFO: ACK sent successfully.\n");
						printf("STATUS: %d\n",status);
						return status-5;
					}else
						printf("INFO: Duplicate ACK has been sent.\n");
				}
				/*if(*packet->data!=NULL){
					printf("SIZE: %d \n",sizeof(packet->data));
					return sizeof(packet->data);
				}
				else{
					/*Listen for the next packet,determine if having reached end of file.
					* If have received all data, wait for FIN*/ /*
					status=recvfrom(sockfd, packet, sizeof(*packet), 0, state.address, &state.socklen);
					if(status!=-1 && packet->type==FIN ){
						
					}
					else return sizeof(packet->data);
				}*/
				
			}
			else if (packet->type==FIN){
				state.state=FIN_INIT;
				return 0;
			}
		}else{
				printf("ERROR: Packet wasn't received correctly. Retrying ...\n");
				state.state = ESTABLISHED;
				retryCount++;
		}
		

	}
	free(packet);
	free(ack_packet);
	return(-1);
}

int gbn_close(int sockfd){

	int retryCount = 1;
	int status = 0;

	gbnhdr *fin_init_packet=malloc(sizeof(*fin_init_packet));
	gbnhdr *fin_packet = malloc(sizeof(*fin_packet));
	gbnhdr *fin_ack_packet = malloc(sizeof(*fin_ack_packet));

	while(retryCount <= MAX_RETRY_ATTEMPTS && state.state != CLOSED){

		if(state.state == ESTABLISHED){
			/* Sender to initiate connection teardown with FIN packet */
			fin_packet->type = FIN;
			fin_packet->seqnum = 0;
			fin_packet->checksum = (uint16_t) 0;
			fin_packet->checksum = checksum(fin_packet, sizeof(*fin_packet) / sizeof(uint16_t));
			status = maybe_sendto(sockfd, fin_packet, sizeof(*fin_packet), 0, state.address, state.socklen);
			if(status == -1){
				printf("ERROR: FIN send failed.Retrying ...\n");
				state.state = ESTABLISHED;
				retryCount++;
			}else{
				printf("INFO: Connection teardown initiated ...\n");
				printf("INFO: FIN successfully sent.\n");
				state.state = FIN_SENT;
				retryCount = 1;
			}
		}else if(state.state==FIN_INIT){
			printf("INFO: Sender initiated FIN.\n");
			fin_ack_packet->type = FINACK;
			fin_ack_packet->seqnum = 0;
			fin_ack_packet->checksum = (uint16_t) 0;
			fin_ack_packet->checksum = checksum(fin_ack_packet, sizeof(*fin_ack_packet) / sizeof(uint16_t));						
			status = maybe_sendto(sockfd, fin_ack_packet, sizeof(*fin_ack_packet), 0, state.address, state.socklen);
			if(status == -1){
				printf("ERROR: FINACK send failed.Retrying ...\n");
				state.state = FIN_INIT;
				retryCount++;
			}else{
				printf("INFO: FINACK successfully sent.\n");			
				state.state = FIN_ACKED;
				retryCount=1;
			}
		}else if(state.state==FIN_ACKED){
			/* receiver sends sender FIN packet */
			fin_packet->type = FIN;
			fin_packet->seqnum = 0;
			fin_packet->checksum = (uint16_t) 0;
			fin_packet->checksum = checksum(fin_packet, sizeof(*fin_packet) / sizeof(uint16_t));
			status = maybe_sendto(sockfd, fin_packet, sizeof(*fin_packet), 0, state.address, state.socklen);
			if(status == -1){
				printf("ERROR: FIN send failed.Retrying ...\n");
				state.state = FIN_ACKED;
				retryCount++;
			}else{
				printf("INFO: FIN successfully sent.\n");
				state.state = FIN_CONF;
				retryCount = 1;
			}
		}else if(state.state==FIN_CONF){
			/*Receiver waits for FINACK from Sender*/
			status = recvfrom(sockfd, fin_ack_packet, sizeof(*fin_ack_packet), 0, state.address, &state.socklen);	
			if(status != -1){
				if(fin_ack_packet->type == FINACK){
					/*Sender gets FINACK from receiver*/
					printf("INFO: FINACK received successfully.\n");
					printf("INFO: Connection terminated.\n");						
					state.state = CLOSED;
					close(sockfd);
					free(fin_packet);
					free(fin_ack_packet);
					free(fin_init_packet);
					return 0;
				}
			}else{
					printf("ERROR: FINACK wasn't received. Retrying ...\n");
					state.state = FIN_CONF;
					retryCount++;
			}

		}else if(state.state == FIN_SENT){
			/* Sender waits for FINACK from receiver */
			status = recvfrom(sockfd, fin_ack_packet, sizeof(*fin_ack_packet), 0, state.address, &state.socklen);	
			if(status != -1){
				if(fin_ack_packet->type == FINACK){
					/*Sender gets FINACK from receiver*/
					printf("INFO: FINACK received successfully.\n");
					state.state = FIN_RCVD;
					retryCount = 1;
				}
			}else{
					printf("ERROR: FINACK wasn't received. Retrying ...\n");
					state.state = FIN_SENT;
					retryCount++;
			}
		}else if(state.state == FIN_RCVD){
				/* Receiver sends a FIN */
				status = recvfrom(sockfd, fin_packet, sizeof(*fin_packet), 0, state.address, &state.socklen);
				if(status != -1){
					if(fin_packet->type == FIN){
						printf("INFO: FIN received successfully.\n");
						/* Sender responding with a FINACK */
						fin_ack_packet->type = FINACK;
						fin_ack_packet->seqnum = 0;
						fin_ack_packet->checksum = (uint16_t) 0;
						fin_ack_packet->checksum = checksum(fin_ack_packet, sizeof(*fin_ack_packet) / sizeof(uint16_t));						
						status = maybe_sendto(sockfd, fin_ack_packet, sizeof(*fin_ack_packet), 0, state.address, state.socklen);
						if(status == -1){
							printf("ERROR: FINACK send failed.Retrying ...\n");
							state.state = FIN_RCVD;
							retryCount++;
						}else{
							printf("INFO: FINACK successfully sent.\n");
							printf("INFO: Connection terminated.\n");						
							state.state = CLOSED;
							close(sockfd);
							free(fin_packet);
							free(fin_ack_packet);
							free(fin_init_packet);
							return 0;
						}
					}else{
						printf("ERROR: FIN wasn't received. Retrying ...\n");
						state.state = FIN_RCVD;
						retryCount++;
					}
				}
		}
	}

	free(fin_packet);
	free(fin_ack_packet);
	if(retryCount <= MAX_RETRY_ATTEMPTS && state.state == CLOSED)	
		return SUCCESS;

	printf("ERROR: Maximum retries reached.\n");
	return(-1);
}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

	/* TCP 3-way handshake - Client*/
	state.state = CLOSED;
	int retryCount = 1;
	int status = 0;
	

	while(retryCount <= MAX_RETRY_ATTEMPTS && state.state != ESTABLISHED){
		if(state.state != SYN_SENT){
			/* SYN to be sent to the receiver */
			gbnhdr *syn_packet = malloc(sizeof(*syn_packet));
			syn_packet->type = SYN;
			syn_packet->seqnum = 0;
			syn_packet->checksum = (uint16_t) 0;
			syn_packet->checksum = checksum(syn_packet, sizeof(*syn_packet) / sizeof(uint16_t));						
			status = maybe_sendto(sockfd, syn_packet, sizeof(*syn_packet), 0, server, socklen);
			if(status == -1){
				printf("ERROR: SYN send failed.Retrying ...\n");
				state.state = CLOSED;
				retryCount++;
			}
			printf("INFO: SYN send succeeded.\n");
			state.state = SYN_SENT;
			free(syn_packet);
		}else if(state.state == SYN_SENT){
			/* SYN_ACK received from the receiver */
			gbnhdr *syn_ack_packet = malloc(sizeof(*syn_ack_packet));

			status = recvfrom(sockfd, syn_ack_packet, sizeof(*syn_ack_packet), 0, server, &socklen);
			
			if(status != -1){

				if(syn_ack_packet->type == SYNACK){
					printf("INFO: SYNACK received successfully.\n");
					/* ACK to be sent to the receiver */
					gbnhdr *ack_packet = malloc(sizeof(*ack_packet));
					ack_packet->type = ACK;
					ack_packet->seqnum = 1;
					ack_packet->checksum = (uint16_t) 0;
					ack_packet->checksum = checksum(ack_packet, sizeof(*ack_packet) / sizeof(uint16_t));
					status = maybe_sendto(sockfd, ack_packet, sizeof(*ack_packet), 0, server, socklen);
					if(status != -1){
						state.state = ESTABLISHED;
						state.seqnum=ack_packet->seqnum;
						printf("INFO: ACK sent successfully.\n");
						printf("INFO: Connection established successfully.\n");
						free(ack_packet);
						state.address=server;
						state.socklen=socklen;
						state.seqnum = 1;
						return SUCCESS;
					}else{
						printf("ERROR: ACK send failed.Retrying ...\n");
						retryCount++;
						free(ack_packet);
						continue;
					}
				}else if(syn_ack_packet->type == RST){
					state.state = CLOSED;
					printf("INFO: Connection refused by server.\n");
					free(syn_ack_packet);
					return SUCCESS;
				}
				

			}else {
				printf("ERROR: SYNACK wasn't received correctly.Retrying ...\n");
				retryCount++;
				continue;
				free(syn_ack_packet);
			}
		}
	}
	printf("ERROR: Maximum retries reached. Exiting.\n");	
	return(-1);
}

int gbn_listen(int sockfd, int backlog){

	/*return listen(sockfd, backlog); */
	return 0;

}

int gbn_bind(int sockfd, const struct sockaddr *server, socklen_t socklen){

	return bind(sockfd, server, socklen);

}	

int gbn_socket(int domain, int type, int protocol){
		
	/*----- Randomizing the seed. This is used by the rand() function -----*/
	srand((unsigned)time(0));
	int sockfd = socket(domain, type, protocol);

	return(sockfd);
}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen){

	/* TCP 3-way handshake - Server*/
	state.state = LISTEN;
	int retryCount = 1;
	int status = 0;

	while(retryCount <= MAX_RETRY_ATTEMPTS && state.state != ESTABLISHED){
		/* SYN received from the sender */
		if(state.state != SYN_RCVD){
			gbnhdr *syn_packet = malloc(sizeof(*syn_packet));
			status = recvfrom(sockfd, syn_packet, sizeof(*syn_packet), 0, client, socklen);
			if(status != -1){
				if(syn_packet->type == SYN){
					state.state = SYN_RCVD;
					printf("INFO: SYN received successfully.\n");
					free(syn_packet);
				}			
			}else {
				printf("ERROR: SYNACK wasn't received correctly.Retrying ...\n");
				retryCount++;
				state.state = LISTEN;
				free(syn_packet);
				continue;
			}
		}else if(state.state == SYN_RCVD){
			/* SYNACK to be sent to the sender */
			gbnhdr *syn_ack_packet = malloc(sizeof(*syn_ack_packet));
			syn_ack_packet->type = SYNACK;
			syn_ack_packet->seqnum = 1;
			syn_ack_packet->checksum = (uint16_t) 0;
			syn_ack_packet->checksum = checksum(syn_ack_packet, sizeof(*syn_ack_packet) / sizeof(uint16_t));
			status = maybe_sendto(sockfd, syn_ack_packet, sizeof(*syn_ack_packet), 0, client, *socklen);
			if(status != -1){
				printf("INFO: SYN_ACK sent successfully.\n");
				free(syn_ack_packet);
				/* ACK to be sent to the sender */
				gbnhdr *ack_packet = malloc(sizeof(*ack_packet));
				status = recvfrom(sockfd, ack_packet, sizeof(*ack_packet), 0, client, socklen);
				if(status != -1){
					if(ack_packet->type == ACK){
						state.state = ESTABLISHED;
						printf("INFO: ACK received successfully.\n");
						printf("INFO: Connection established successfully.\n");
						state.address = client;
						state.socklen = *socklen;
						state.seqnum = 1;
						free(ack_packet);
						return sockfd;
					}			
				}else {
					printf("ERROR: SYNACK wasn't received correctly.Retrying ...\n");
					retryCount++;
					state.state = LISTEN;
					free(ack_packet);
					continue;
				}

			}else{
				printf("ERROR: SYN_ACK send failed.Retrying ...\n");
				retryCount++;
				free(syn_ack_packet);
				continue;
			}

		}
	}
		

	return(-1);
}

ssize_t maybe_sendto(int  s, const void *buf, size_t len, int flags, \
                     const struct sockaddr *to, socklen_t tolen){

	char *buffer = malloc(len);
	memcpy(buffer, buf, len);
	
	
	/*----- Packet not lost -----*/
	if (rand() > LOSS_PROB*RAND_MAX){
		/*----- Packet corrupted -----*/
		if (rand() < CORR_PROB*RAND_MAX){
			
			/*----- Selecting a random byte inside the packet -----*/
			int index = (int)((len-1)*rand()/(RAND_MAX + 1.0));

			/*----- Inverting a bit -----*/
			char c = buffer[index];
			if (c & 0x01)
				c &= 0xFE;
			else
				c |= 0x01;
			buffer[index] = c;
		}

		/*----- Sending the packet -----*/
		int retval = sendto(s, buffer, len, flags, to, tolen);
		free(buffer);
		return retval;
	}
	/*----- Packet lost -----*/
	else
		return(len);  /* Simulate a success */
}
