/* simple prototype of MLDv1 router state implementation as defined in RFC 2710
 */
#include <zebra.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "mld.h"


struct mld_rtr_state querier, non_querier;

struct thread_master *master;

bool mld_rtr_is_querier(struct mld_rtr_state * st);

static int mld_rtr_general_qry_expired(struct thread * thread);

void init_mld_rtr_state(struct mld_rtr_state * st, struct in6_addr * own_addr);

static void mld_rtr_reschedule_query(struct mld_rtr_state * st);

static void mld_rtr_send_general_query(struct mld_rtr_state * st);

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

bool mld_rtr_is_querier(struct mld_rtr_state * st)
{
  return IN6_ARE_ADDR_EQUAL(&st->self_addr, &st->querier);
}

void init_mld_rtr_state(struct mld_rtr_state * st, struct in6_addr * own_addr)
{
  memset(st, 0, sizeof(st));
  /* router always starts by assuming it is the querier */
  st->querier = st->self_addr = *own_addr;
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


static void mld_rtr_send_general_query(struct mld_rtr_state * st)
{
  printf("Sending fake general query from %s\n", addr2ascii(&st->self_addr)); 
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
  st->thread = thread_add_timer_msec(master, mld_rtr_other_querier_timeout,
      st, st->timeout);
}

void mld_rtr_state_transition(struct mld_rtr_state * st, mld_rtr_event_t event)
{
  if (mld_rtr_is_querier(st)) {
    switch (event) {
      case MLD_RTR_EVENT_QRY_EXP:
        if (st->counter) {
          st->counter--;
         
          /* not in starting stage */
          if (!st->counter)
            st->timeout = MLD_QRY_INT;
        }

        /* send general query and restart countdown timer */
        mld_rtr_send_general_query(st);
        break;
      case MLD_RTR_EVENT_QRY_LOWER:
        st->timeout = MLD_OTH_QRY_INT;
        /* TODO: restart timer, set querier IP? */
        printf("Is this right? This is not supposed to happen\n");
        break;
    }
  }
  else {
    switch (event) {
      case MLD_RTR_EVENT_OTH_EXP:
        st->timeout = MLD_QRY_INT;
        st->querier = st->self_addr;
        /* send general query and restart countdown timer */
        mld_rtr_send_general_query(st);
        break;
      case MLD_RTR_EVENT_QRY_LOWER:
        st->timeout = MLD_OTH_QRY_INT;
        mld_rtr_reschedule_other_querier_timeout(st);
        break;
    }
  }
}


static int mld_rtr_general_qry_expired(struct thread * thread)
{
  struct mld_rtr_state * mld = THREAD_ARG(thread);

  mld_rtr_state_transition(mld, MLD_RTR_EVENT_QRY_EXP);
  return 0; 
}


static void init_rtr_event(void)
{
  struct in6_addr addr;
  inet_pton(AF_INET6, "fe80::21c:c0ff:fe11:1111", &addr);
  init_mld_rtr_state(&querier, &addr);
  inet_pton(AF_INET6, "fe80::21c:c0ff:fe22:2222", &addr);
  init_mld_rtr_state(&non_querier, &addr);

  mld_rtr_reschedule_query(&non_querier);
  non_querier.thread = thread_add_timer_msec(master, mld_rtr_general_qry_expired,
      &non_querier, non_querier.timeout);
}

/* generate router event */
void generate_rtr_event(void)
{

}


int main(int argc, char * argv[])
{
  struct thread thread;
  
  master = thread_master_create();
  init_rtr_event();

  /* Start finite state machine, here we go! */
  while (thread_fetch(master, &thread))
    thread_call(&thread);


  return 0;
}
