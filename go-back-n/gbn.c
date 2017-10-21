#include <unistd.h>
#include "gbn.h"

state_t s;

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
	/* TODO: Your code here. */
	int status=0;
	int count=0;
	printf("Length of file: %d \n",len);
	while (len>DATALEN){
		gbnhdr *data_packet = malloc(sizeof(*data_packet));
		data_packet->type=DATA;
		data_packet->seqnum = s.seqnum+1;
		data_packet->checksum = 0;
		memcpy(data_packet->data,buf,DATALEN);

		status = sendto(sockfd,data_packet,DATALEN,0,s.server,s.socklen);
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
	data_packet->seqnum = s.seqnum+1;
	data_packet->checksum = 0;
	memcpy(data_packet->data,buf,len);

	status = sendto(sockfd,data_packet,DATALEN,0,s.server,s.socklen);
	printf("Status:%d\n",status);
	if(status == -1){
		printf("ERROR: DATA packet %d send failed. Resending ...\n",data_packet->seqnum);
		return (-1);
	}
	free(data_packet);
	printf("Packet %d successfully sent!",count+1);
	count++;


	/* Hint: sCheck the data length field 'len'.
	 *       If it is > DATALEN, you will have to split the data
	 *       up into multiple packets - you don't have to worry
	 *       about getting more than N * DATALEN.
	 */

	return(-1);
}

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags){

	/* TODO: Your code here. */

	return(-1);
}

int gbn_close(int sockfd){
	
	s.state = CLOSED;
	int retryCount = 1;
	int status = 0;

	while(retryCount <= MAX_RETRY_ATTEMPTS && s.state != ESTABLISHED){
		if(s.state != SYN_SENT){


	
		
	
	
	}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

	/* TCP 3-way handshake - Client*/
	s.state = CLOSED;
	int retryCount = 1;
	int status = 0;

	while(retryCount <= MAX_RETRY_ATTEMPTS && s.state != ESTABLISHED){
		if(s.state != SYN_SENT){
			/* SYN to be sent to the receiver */
			gbnhdr *syn_packet = malloc(sizeof(*syn_packet));
			syn_packet->type = SYN;
			syn_packet->seqnum = 0;
			syn_packet->checksum = 0;			
			status = sendto(sockfd, syn_packet, sizeof(*syn_packet), 0, server, socklen);
			if(status == -1){
				printf("ERROR: SYN send failed.Retrying ...\n");
				s.state = CLOSED;
				retryCount++;
			}
			printf("INFO: SYN send succeeded.\n");
			s.state = SYN_SENT;
			free(syn_packet);
		}else if(s.state == SYN_SENT){
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
						s.state = ESTABLISHED;
						printf("INFO: ACK sent successfully.\n");
						printf("INFO: Connection established successfully.\n");
						free(ack_packet);
						s.server=server;
						s.socklen=socklen;
						return SUCCESS;
					}else{
						printf("ERROR: ACK send failed.Retrying ...\n");
						retryCount++;
						free(ack_packet);
						continue;
					}
				}else if(syn_ack_packet->type == RST){
					s.state = CLOSED;
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
	s.state = LISTEN;
	int retryCount = 1;
	int status = 0;

	while(retryCount <= MAX_RETRY_ATTEMPTS && s.state != ESTABLISHED){
		/* SYN received from the sender */
		if(s.state != SYN_RCVD){
			gbnhdr *syn_packet = malloc(sizeof(*syn_packet));
			status = recvfrom(sockfd, syn_packet, sizeof(*syn_packet), 0, client, socklen);
			if(status != -1){
				if(syn_packet->type == SYN){
					s.state = SYN_RCVD;
					printf("INFO: SYN received successfully.\n");
					free(syn_packet);
				}			
			}else {
				printf("ERROR: SYNACK wasn't received correctly.Retrying ...\n");
				retryCount++;
				s.state = LISTEN;
				free(syn_packet);
				continue;
			}
		}else if(s.state == SYN_RCVD){
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
						s.state = ESTABLISHED;
						printf("INFO: ACK received successfully.\n");
						printf("INFO: Connection established successfully.\n");
						s.address = *client;
                        s.socklen = *socklen;
						free(ack_packet);
						return sockfd;
					}			
				}else {
					printf("ERROR: SYNACK wasn't received correctly.Retrying ...\n");
					retryCount++;
					s.state = LISTEN;
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
