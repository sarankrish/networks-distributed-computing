#include <unistd.h>
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

ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags){	
	int status=0;
	int count=0;
	printf("Length of file: %d \n",len);
	while (len>DATALEN){
		gbnhdr *data_packet = malloc(sizeof(*data_packet));
		data_packet->type=DATA;
		data_packet->seqnum = state.seqnum+1;
		data_packet->checksum = 0;
		memcpy(data_packet->data,buf,DATALEN);

		status = sendto(sockfd,data_packet,DATALEN,0,state.address,state.socklen);
		printf("Status:%d\n",status);
		if(status == -1){
			printf("ERROR: DATA packet %d send failed. Resending ...\n",data_packet->seqnum);
			return (-1);
		}
	    free(data_packet);
		printf("Packet %d successfully sent!",count+1);
		len-=DATALEN;
		buf+=DATALEN;
		count++;
	}
	gbnhdr *data_packet = malloc(sizeof(*data_packet));
	data_packet->type=DATA;
	data_packet->seqnum = state.seqnum+1;
	data_packet->checksum = 0;
	memcpy(data_packet->data,buf,len);

	status = sendto(sockfd,data_packet,DATALEN,0,state.address,state.socklen);
	printf("Status:%d\n",status);
	if(status == -1){
		printf("ERROR: DATA packet %d send failed. Resending ...\n",data_packet->seqnum);
		return (-1);
	}
	free(data_packet);
	printf("Packet %d successfully sent!\n",count+1);
	count++;

	/* Hint: Check the data length field 'len'.
	 *       If it is > DATALEN, you will have to split the data
	 *       up into multiple packets - you don't have to worry
	 *       about getting more than N * DATALEN.
	 */

	return SUCCESS;
}

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags){

	int retryCount = 1;
	int status = 0;

	gbnhdr *packet = malloc(sizeof(*packet));
	gbnhdr *ack_packet = malloc(sizeof(*ack_packet));

	while(retryCount <= MAX_RETRY_ATTEMPTS && state.state != CLOSED){

		status = recvfrom(sockfd, packet, sizeof(*packet), 0, state.address, &state.socklen);
		if(status != -1){
			if(packet->type == FIN){
				/* Sender has initiated connection teardown */
				printf("INFO: Connection teardown initiated ...\n");
				printf("INFO: FIN received successfully.\n");
				state.state = FIN_RCVD;
				/* Receiver responds with FINACK */
				ack_packet->type = FINACK;
				ack_packet->seqnum = 0;
				ack_packet->checksum = 0;
				status = sendto(sockfd, ack_packet, sizeof(*ack_packet), 0, state.address, state.socklen);
				if(status == -1){
					printf("ERROR: FINACK send failed.Retrying ...\n");
					state.state = ESTABLISHED;
					retryCount++;
				}else{
					printf("INFO: FINACK successfully sent.\n");
					state.state = FIN_SENT;
				}
				/* Receiver sends FIN */
				packet->type = FIN;
				packet->seqnum = 0;
				packet->checksum = 0;
				status = sendto(sockfd, packet, sizeof(*packet), 0, state.address, state.socklen);
				if(status == -1){
					printf("ERROR: FIN send failed.Retrying ...\n");
					state.state = FIN_SENT;
					retryCount++;
				}else{
					printf("INFO: FIN successfully sent.\n");
					state.state = FIN_SENT;
				}
			}else if(packet->type == FINACK){
				/* Sender has responded with FINACK */
				printf("INFO: FINACK received successfully.\n");
				/* Receiver terminates connection */
				state.state = CLOSED;
				close(sockfd);
				free(packet);
				free(ack_packet);
				printf("INFO: Connection terminated.\n");
				return 0;
			}else if(packet->type == DATA){
				/* Sender has sent a DATA packet */
				printf("INFO: DATA received successfully.\n");
				/* Receiver responds with DATAACK */
                if(state.seqnum == packet->seqnum){
                    printf("INFO: Received DATA packet is in sequence.\n");
                    state.seqnum = packet->seqnum + 1;
                    ack_packet->seqnum = state.seqnum;
                    ack_packet->checksum = 0;
                }else {
                    printf("INFO: DATA packet has the incorrect sequence number.\n");
                    ack_packet->seqnum = state.seqnum;
                    ack_packet->checksum = 0;
                }
                /* Sending ACK / duplicate ACK */
                if (sendto(sockfd, ack_packet, sizeof(*ack_packet), 0, state.address, state.socklen) == -1) {
                        printf("ERROR: ACK sending failed.\n");
                        state.state = CLOSED;
                        break;
                } else {
					if(state.seqnum == packet->seqnum)
						printf("INFO: ACK sent successfully.\n");						
					else
						printf("INFO: Duplicate ACK has been sent.\n");
                }
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

	gbnhdr *fin_packet = malloc(sizeof(*fin_packet));
	gbnhdr *fin_ack_packet = malloc(sizeof(*fin_ack_packet));

	while(retryCount <= MAX_RETRY_ATTEMPTS && state.state != CLOSED){

		if(state.state == ESTABLISHED){
			/* Sender to initiate connection teardown with FIN packet */
			fin_packet->type = FIN;
			fin_packet->seqnum = 0;
			fin_packet->checksum = 0;
			status = sendto(sockfd, fin_packet, sizeof(*fin_packet), 0, state.address, state.socklen);
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
		}else if(state.state == FIN_SENT){
			/* Receiver responds with FINACK */
			status = recvfrom(sockfd, fin_ack_packet, sizeof(*fin_ack_packet), 0, state.address, &state.socklen);	
			if(status != -1){
				if(fin_ack_packet->type == FINACK){
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
				printf("INFO: Waiting for FIN from receiver.\n");
				status = recvfrom(sockfd, fin_packet, sizeof(*fin_packet), 0, state.address, &state.socklen);
				if(status != -1){
					if(fin_packet->type == FIN){
						printf("INFO: FIN received successfully.\n");
						/* Sender responding with a FINACK */
						fin_ack_packet->type = FINACK;
						fin_ack_packet->seqnum = 0;
						fin_ack_packet->checksum = 0;
						status = sendto(sockfd, fin_ack_packet, sizeof(*fin_ack_packet), 0, state.address, state.socklen);
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
			syn_packet->checksum = 0;			
			status = sendto(sockfd, syn_packet, sizeof(*syn_packet), 0, server, socklen);
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
					ack_packet->checksum = 0;
					status = sendto(sockfd, ack_packet, sizeof(*ack_packet), 0, server, socklen);
					if(status != -1){
						state.state = ESTABLISHED;
						printf("INFO: ACK sent successfully.\n");
						printf("INFO: Connection established successfully.\n");
						free(ack_packet);
						state.address=server;
						state.socklen=socklen;
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
			syn_ack_packet->checksum = 0;
			status = sendto(sockfd, syn_ack_packet, sizeof(*syn_ack_packet), 0, client, *socklen);
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
