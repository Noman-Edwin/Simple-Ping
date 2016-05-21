#include "unp.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define BUFSIZE 1500
char    *host;
pid_t    pid;
int	sockfd;
int	nsent;
int 	datalen=56;
char	sendbuf[BUFSIZE];
struct sockaddr  *sasend;
struct addrinfo *ai;

uint16_t
in_cksum(uint16_t *add, int len)
{
	int	nleft = len;
	uint32_t sum = 0;
	uint16_t *w = add;
	uint16_t answer =  0;

	while (nleft > 1){
	sum += *w++;
	nleft -= 2;	
	}
	if ( nleft == 1){
 	*(unsigned char *)(&answer) = *(unsigned char *)w;
	sum+=answer;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;
	return (answer);
	 err_quit("cksum");
}



void
send_a()
{
	int	m;
	int	len;
	struct	icmp *icmp;

	icmp = (struct icmp *) sendbuf;
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_id = pid;
	icmp->icmp_seq = nsent++;
	memset(icmp->icmp_data, 0xa5, datalen);
	gettimeofday((struct timeval *) icmp->icmp_data, NULL);

	len = 8 + datalen;
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = in_cksum((u_short *) icmp, len);
	
	if((m=sendto(sockfd, sendbuf, len, 0, ai->ai_addr, ai->ai_addrlen))== -1)
		err_quit("sendto error");
}
	
void
tv_sub(struct timeval *out, struct timeval *in)
{
	if ( (out->tv_usec -= in->tv_usec ) < 0){
		--out->tv_sec;
		out->tv_usec +=1000000;
		}
	out->tv_sec -= in ->tv_sec;
}


void
proc(char *ptr, ssize_t len, struct msghdr *msg, struct timeval *tvrecv)
{
	int	hlenl, icmplen;
	double  rtt;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend;
	
	ip = (struct ip *) ptr;
	hlenl = (ip->ip_hl << 2);
	if (ip->ip_p != IPPROTO_ICMP)
		return;
	icmp = (struct icmp *)(ptr + hlenl);
	if ( (icmplen =len -hlenl) < 8)	
 		return ;
	if (icmp->icmp_type == ICMP_ECHOREPLY)	{
		if (icmp->icmp_id != pid)
			return;
		if(icmplen <  16)
			return;
		
		tvsend = (struct timeval *) icmp->icmp_data;
		tv_sub(tvrecv, tvsend);
		rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec/1000.0;
		
		printf("%d bytes from %s: seq=%u,ttl=%d, rtt=%.3f ms\n",
			  icmplen, host,icmp->icmp_seq, ip->ip_ttl,rtt);
		}
	
}


void
sig_alrm(int signo)
{
	send_a();
	alarm(1);
	return;
}

void
readloop()
{
	int size;
	char recvbuf[BUFSIZE];
	char controlbuf[BUFSIZE];
	struct iovec iov;
	struct msghdr msg;
	struct timeval  tval;
	ssize_t nn;


	sockfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
	setuid(getuid());
		
	size=60 * 1024;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size,sizeof(size));
	
	sig_alrm(SIGALRM);

	iov.iov_base = recvbuf;
	iov.iov_len = sizeof(recvbuf);
	msg.msg_name = calloc(1,ai->ai_addrlen);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = controlbuf;
	for ( ; ; ){
		msg.msg_namelen = ai->ai_addrlen;
		msg.msg_controllen = sizeof(controlbuf);
		nn = recvmsg (sockfd, &msg, 0);
		if (nn < 0){
		   if (errno == EINTR)
			continue;
		   else
			err_sys("recvmsg error");
			 }
		
		gettimeofday(&tval, NULL);
		proc(recvbuf, nn, &msg, &tval);

	}
	
}

int
main(int argc, char **argv)
{
	ssize_t	n;
	struct  addrinfo *res;
	if (argc!=2) 
		err_quit("argument not enougth");
	
	host = argv[1];
	pid = getpid() & 0xffff;
	
	signal(SIGALRM,sig_alrm);
	
	if (n = getaddrinfo(host,NULL,0,&res) != 0)
		return 0;
	ai = host_serv(host,NULL,0,0);
	readloop();

	exit(0);
}
