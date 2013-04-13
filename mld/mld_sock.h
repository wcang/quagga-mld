#include <netinet/in.h>

int icmp6_sock();

int icmp6_send(int sockfd, int oif_index, struct in6_addr * src, 
    struct in6_addr * dest, unsigned char * msg, unsigned int msg_len);


/* msg_len should be the raw size of the msg buffer itself when passed in,
 * msg_len will contain the the size of actual data stored within msg
 * when the function returns
 */
int icmp6_recv(int sockfd, int * iif_index, struct in6_addr * src, 
    struct in6_addr * dest, unsigned char * msg, unsigned int * msg_len);

