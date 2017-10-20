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

	/* Hint: Check the data length field 'len'.
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

	/* TODO: Your code here. */

	return(-1);
}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

	/* TCP 3-way handshake - Client*/
	s.state = CLOSED;
	int retryCount = 0;
	int status = 0;

	while(retryCount <= MAX_RETRY_ATTEMPTS && s.state != ESTABLISHED){
		if(s.state != SYN_SENT){
			/* Need malloc or take risk with memory leak ? */
			/* SYN to be sent to the receiver */
			gbnhdr syn_packet;
			syn_packet.type = SYN;
			syn_packet.seqnum = 0;
			syn_packet.checksum = 0;			
			status = sendto(sockfd, &syn_packet, sizeof(syn_packet), 0, server, socklen);
			if(status == 1){
				printf("ERROR: SYN send failed.Retrying ...\n");
				s.state = CLOSED;
				retryCount++;
			}
			printf("INFO: SYN send succeeded.\n");
			s.state = SYN_SENT;
		}else if(s.state == SYN_SENT){
			/* SYN_ACK received from the receiver */
			gbnhdr syn_ack_packet;

			status = recvfrom(sockfd, &syn_ack_packet, sizeof(syn_ack_packet), 0, server, socklen);
			
			if(status != -1){

				if(syn_ack_packet.type == SYNACK){
					printf("INFO: SYNACK received successfully.\n");
					/* ACK to be sent to the receiver */
					gbnhdr ack_packet;
					ack_packet.type = ACK;
					ack_packet.seqnum = 1;
					ack_packet.checksum = 0;
					status = sendto(sockfd, &ack_packet, sizeof(ack_packet), 0, server, socklen);
					if(status != -1){
						s.state = ESTABLISHED;
						printf("INFO: ACK sent successfully.\n");
						printf("INFO: Connection established successfully.\n");
						return SUCCESS;
					}else{
						printf("ERROR: ACK send failed.Retrying ...\n");
						retryCount++;
						continue;
					}
				}else if(syn_ack_packet.type == RST){
					s.state = CLOSED;
					printf("INFO: Connection refused by server.\n");
					return SUCCESS;
				}
				

			}else {
				printf("ERROR: SYNACK wasn't received correctly.Retrying ...\n");
				retryCount++;
				continue;
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
	
	printf("Inside gbn_socket");

	int sockfd = socket(domain, type, protocol);

	return(sockfd);
}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen){

	/* TODO: Your code here. */

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
