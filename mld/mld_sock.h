#include <netinet/in.h>

int icmp6_sock_init();

/* return the number of bytes sent, otherwise -1 on error
 */
int icmp6_send(int sockfd, int oif_index, struct in6_addr * src, 
    struct in6_addr * dest, unsigned char * msg, unsigned int msg_len);


/* return the number of bytes receives 
 */
int icmp6_recv(int sockfd, int * iif_index, struct in6_addr * src, 
    struct in6_addr * dest, unsigned char * msg, unsigned int msg_len);

