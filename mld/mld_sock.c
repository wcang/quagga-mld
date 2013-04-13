#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/icmp6.h>
#include "mld.h"
#include "mld_sock.h"
#include <stdlib.h>
#include <sys/uio.h>

int icmp6_sock_init()
{
  int sockfd;
  struct icmp6_filter filter;

  sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  
  if (sockfd == -1) {
    printf("Failed to create raw socket for ICMP: %s\n", strerror(errno));
    exit(1);
  }

  ICMP6_FILTER_SETBLOCKALL(&filter);
  ICMP6_FILTER_SETBLOCK(MLD_TYPE_DONE, &filter);
  ICMP6_FILTER_SETBLOCK(MLD_TYPE_QUERY, &filter);
  ICMP6_FILTER_SETBLOCK(MLD_TYPE_REPORT, &filter);

  if (setsockopt(sockfd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof(filter)) != 0) {
    printf("Failed to set ICMP filter: %s\n", strerror(errno));
    exit(1);
  }

  return sockfd;
}


int icmp6_send(int sockfd, int oif_index, struct in6_addr * src, 
    struct in6_addr * dest, unsigned char * msg, unsigned int msg_len)
{
  struct iovec iov;
  struct msghdr hdr;
  struct in6_pktinfo * pkt_info;
  unsigned char cmsghdr[CMSG_SPACE(sizeof(struct in6_pktinfo))];
  struct cmsghdr * chdr;
  struct sockaddr_in6 sin6;

  memset(&hdr, 0, sizeof(hdr));
  /* setup destination address */
  memset(&sin6, 0, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  memcpy(&sin6.sin6_addr, dest, sizeof(*dest));
  hdr.msg_name = &sin6;
  hdr.msg_namelen = sizeof(sin6);
  /* the data itself */
  iov.iov_base = msg; 
  iov.iov_len = msg_len;
  hdr.msg_iov = &iov;
  hdr.msg_iovlen = 1;
  /* control message to control output interface and src address to be used */
  chdr = (struct cmsghdr * ) cmsghdr;
  chdr->cmsg_level = IPPROTO_IPV6;
  chdr->cmsg_type = IPV6_PKTINFO;
  chdr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
  pkt_info = (struct in6_pktinfo *) CMSG_DATA(chdr);
  pkt_info->ipi6_ifindex = oif_index;

  if (src)
    memcpy(&pkt_info->ipi6_addr, src, sizeof(*src)); 
  else
    memset(&pkt_info->ipi6_addr, 0, sizeof(*src));

  hdr.msg_control = cmsghdr;
  hdr.msg_controllen = sizeof(cmsghdr);

  return sendmsg(sockfd, &hdr, 0);
}


/* msg_len should be the raw size of the msg buffer itself when passed in,
 * msg_len will contain the the size of actual data stored within msg
 * when the function returns
 */
int icmp6_recv(int sockfd, int * iif_index, struct in6_addr * src, 
    struct in6_addr * dest, unsigned char * msg, unsigned int * msg_len)
{
  int ret;
  struct iovec iov;
  struct msghdr hdr;
  struct in6_pktinfo * pkt_info;
  unsigned char cmsghdr[CMSG_SPACE(sizeof(struct in6_pktinfo))];
  struct cmsghdr * chdr;
  struct sockaddr_in6 sin6;

  memset(&hdr, 0, sizeof(hdr));
  /* setup destination address */
  memset(&sin6, 0, sizeof(sin6));
  hdr.msg_name = &sin6;
  hdr.msg_namelen = sizeof(sin6);
  /* the data itself */
  iov.iov_base = msg; 
  iov.iov_len = *msg_len;
  hdr.msg_iov = &iov;
  hdr.msg_iovlen = 1;
  /* control message to control output interface and src address to be used */
  chdr = (struct cmsghdr * ) cmsghdr;
  chdr->cmsg_level = IPPROTO_IPV6;
  chdr->cmsg_type = IPV6_PKTINFO;
  chdr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
  hdr.msg_control = cmsghdr;
  hdr.msg_controllen = sizeof(cmsghdr);

  if ((ret =  recvmsg(sockfd, &hdr, 0))) {
    printf("Error in receiving message: %s\n", strerror(errno));
    return ret;
  }

  /* extract control info like interface index and destination address */
  pkt_info = (struct in6_pktinfo *) CMSG_DATA(chdr);
  
  if (iif_index) {
    *iif_index = pkt_info->ipi6_ifindex;
  }

  if (dest) {
    memcpy(dest, &pkt_info->ipi6_addr, sizeof(*src));
  }

  /* get the data length */
  *msg_len = iov.iov_len;
  /* extract source address */
  if (src)
    memcpy(src, &sin6.sin6_addr, sizeof(*src));

  return 0;
}
