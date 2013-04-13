/* simple prototype of MLDv1 router state implementation as defined in RFC 2710
 */
#include <zebra.h>
#include "prefix.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "mld.h"
#include "mld_sock.h"
#include <sys/types.h>
#include <sys/socket.h>


struct thread_master *master;

/* thread that receives ICMPv6 socket */
struct thread * thread_recv;

int icmp6_sockfd;

void init_mld_rtr_state(struct mld_rtr_state * st, struct in6_addr * own_addr);

static void mld_rtr_reschedule_query(struct mld_rtr_state * st);

static int mld_rtr_other_querier_timeout(struct thread * thread);

static void mld_rtr_reschedule_other_querier_timeout(struct mld_rtr_state * st);

static char * addr2ascii(struct in6_addr * addr);

static char * addr2ascii(struct in6_addr * addr)
{
  char buffer[50];
  inet_ntop(AF_INET6, addr, buffer, sizeof(buffer));
  return buffer;
}


void mld_rtr_state_transition(struct mld_rtr_state * st, mld_rtr_event_t event);


void init_mld_rtr_state(struct mld_rtr_state * st, struct in6_addr * own_addr)
{
  memset(st, 0, sizeof(st));
  /* router always starts by assuming it is the querier */
  st->querier = true;
  st->self_addr = *own_addr;
  st->grps = list_new();

  if (st->grps == NULL) {
    fprintf(stderr, "List allocation for grps failed");
    abort();
  }

  st->timeout = MLD_STARTUP_QRY_INT;
  st->counter = MLD_STARTUP_COUNT;
}


static void mld_rtr_reschedule_query(struct mld_rtr_state * st)
{
  printf("Reschedule general query for timeout in %lu\n", st->timeout);
  st->thread = thread_add_timer_msec(master, mld_rtr_general_qry_expired,
      st, st->timeout);
}


void mld_rtr_send_general_query(struct mld_rtr_state * st)
{
  int ret;
  struct mld_header hdr;
  struct interface * ifp;
  struct in6_addr dest;

  memset(&hdr, 0, sizeof(hdr));
  hdr.type = MLD_TYPE_QUERY;
  hdr.max_delay = htons(MLD_QRY_RESP_INT);
  printf("Sending general query from %s\n", addr2ascii(&st->self_addr));
  ifp = st->iface;
  inet_pton(AF_INET6, "ff02::1", &dest);
  ret = icmp6_send(icmp6_sockfd, ifp->ifindex, &st->self_addr, 
      &dest, (unsigned char *) &hdr, sizeof(hdr));

  if (ret != sizeof(hdr)) {
    printf("error sending general query: %s\n", strerror(errno));
  }

  mld_rtr_reschedule_query(st); 
}


static int mld_rtr_other_querier_timeout(struct thread * thread)
{
  struct mld_rtr_state * st = THREAD_ARG(thread);

  mld_rtr_state_transition(st, MLD_RTR_EVENT_OTH_EXP);
  return 0;
}


static void mld_rtr_reschedule_other_querier_timeout(struct mld_rtr_state * st)
{
  thread_cancel(st->thread);
  st->timeout = MLD_OTH_QRY_INT;
  st->thread = thread_add_timer_msec(master, mld_rtr_other_querier_timeout,
      st, st->timeout);
}

void mld_rtr_state_transition(struct mld_rtr_state * st, mld_rtr_event_t event)
{
  if (st->querier) {
    switch (event) {
      case MLD_RTR_EVENT_QRY_EXP:
        if (st->counter) {
          st->counter--;
         
          /* not in starting stage */
          if (!st->counter)
            st->timeout = MLD_QRY_INT;
        }
        printf("General query timeout\n");
        /* send general query and restart countdown timer */
        mld_rtr_send_general_query(st);
        break;
      case MLD_RTR_EVENT_QRY_LOWER:
        st->timeout = MLD_OTH_QRY_INT;
        /* TODO: flush all list? */
        printf("Transiting to non-querier\n");
        st->querier = false;
        mld_rtr_reschedule_other_querier_timeout(st);
        break;
    }
  }
  else {
    switch (event) {
      case MLD_RTR_EVENT_OTH_EXP:
        st->timeout = MLD_QRY_INT;
        st->querier = true;
        printf("Querier timeout\n");
        /* send general query and restart countdown timer */
        mld_rtr_send_general_query(st);
        break;
      case MLD_RTR_EVENT_QRY_LOWER:
        printf("scheduling waiting for querier to timeout\n");
        mld_rtr_reschedule_other_querier_timeout(st);
        break;
    }
  }
}


static void 
mld_process_icmpv6_rcv(struct mld_rtr_state * mld, struct in6_addr * src,
    struct in6_addr * dest, unsigned char * msg, int len)
{
  struct mld_header * hdr;

  hdr = (struct mld_header *) msg;

  if (len < sizeof(*hdr))
    return;

  switch (hdr->type) {
    case MLD_TYPE_QUERY:
      /* check if other router has lower IP than us, reschedule for query timeout */
      if (IPV6_ADDR_CMP(src, dest) < 0) {
        mld->querier_addr = *src;
        mld_rtr_state_transition(mld, MLD_RTR_EVENT_QRY_LOWER);
      }
      break;
    case MLD_TYPE_REPORT:
      if (mld->querier) {
        printf("Received MLD report\n");
      }
      break;
    case MLD_TYPE_DONE:
      if (mld->querier) {
        printf("Receive MLD done\n");
      }
      break;
    default:
      printf("Something gone wrong. Perhaps, ICMPv6 filter is not installed correctly");
      break;
  } 
}


int mld_rtr_icmpv6_rcv(struct thread * thread)
{
  int ret;
  int ifindex;
  struct in6_addr src, dest;
  struct interface * ifp;
  unsigned char msg[1500];

  ret = icmp6_recv(icmp6_sockfd, &ifindex, &src, &dest, msg, sizeof(msg));

  if (ret != -1) {
    printf("Received from ifindex %d len %d\n", ifindex, ret);
    printf("Src: %s\n", addr2ascii(&src));
    printf("Dest: %s\n", addr2ascii(&dest));

    ifp = if_lookup_by_index(ifindex);

    if (ifp && ifp->info) {
      mld_process_icmpv6_rcv((struct mld_rtr_state *) ifp->info, &src, &dest, 
          msg, ret);
    }
  }

  thread_recv = thread_add_read(master, mld_rtr_icmpv6_rcv, NULL, icmp6_sockfd);  
}


int mld_rtr_general_qry_expired(struct thread * thread)
{
  struct mld_rtr_state * mld = THREAD_ARG(thread);

  mld_rtr_state_transition(mld, MLD_RTR_EVENT_QRY_EXP);
  return 0; 
}


int main(int argc, char * argv[])
{
  struct thread thread;

  if_init();  
  master = thread_master_create();
  icmp6_sockfd = icmp6_sock_init(); 
  mld_zebra_init();
  thread_recv = thread_add_read(master, mld_rtr_icmpv6_rcv, NULL, icmp6_sockfd);  

  /* Start finite state machine, here we go! */
  while (thread_fetch(master, &thread))
    thread_call(&thread);


  return 0;
}
