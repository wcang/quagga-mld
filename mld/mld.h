
#include "thread.h"
#include "linklist.h"
#include <netinet/in.h>
#include "if.h"
#include <stdbool.h>

/* timer value is specified in milliseconds */
#define MLD_ROBUSTNESS              2
#define MLD_QRY_INT                 125000
#define MLD_QRY_RESP_INT            10000
/* FIXME: may need to prune away variable derived from others */
#define MLD_MCAST_LISTENER_INT      (MLD_ROBUSTNESS * MLD_QRY_INT + MLD_QRY_RESP_INT)
#define MLD_OTH_QRY_INT             (MLD_ROBUSTNESS * MLD_QRY_INT + (MLD_QRY_RESP_INT / 2))
#define MLD_STARTUP_QRY_INT         (MLD_QRY_INT / 4)
#define MLD_STARTUP_COUNT           MLD_ROBUSTNESS
#define MLD_LAST_LISTENER_INT       1000
#define MLD_LAST_LISTENER_COUNT     MLD_ROBUSTNESS
#define MLD_UNSOLICITED_REPORT_INT  10000


typedef enum {
  MLD_ST_LISTENER_PRESENT,
  MLD_ST_CHECKING_LISTENER
} mld_grp_state_t;

typedef enum {
  MLD_GRP_EVENT_REPORT,
  MLD_GRP_EVENT_DONE,
  MLD_GRP_EVENT_MCAST_SPECIFIC_QRY,
  MLD_GRP_EVENT_TIMER,
  MLD_GRP_EVENT_RESTRANSMIT_TIMER
} mld_grp_event_t;


struct grp_state {
  mld_grp_state_t state;
  struct in6_addr addr;
  unsigned short timeout;
};


typedef enum {
  MLD_RTR_EVENT_QRY_EXP,    /* General query timer expired */
  MLD_RTR_EVENT_QRY_LOWER,  /* Query received from a router with a lower IP */
  MLD_RTR_EVENT_OTH_EXP     /* Other querier present timer expired */
} mld_rtr_event_t;

struct mld_rtr_state {
  /* thread to keep track of timeout */
  bool querier;
  struct thread * thread;
  struct interface * iface;
  struct in6_addr querier_addr;
  struct in6_addr self_addr;
  struct list * grps; 
  /* querier state: general query timeout
   * non-querier state: other querier present timer
   */
  unsigned long timeout;
  /* counter for timeout */
  unsigned char counter;
};

#define MLD_TYPE_QUERY  130
#define MLD_TYPE_REPORT 131
#define MLD_TYPE_DONE   132

struct mld_header
{
  u_char type;
  u_char code;
  u_int16_t checksum;
  u_int16_t max_delay;
  u_int16_t reserved;
  struct in6_addr address;
};

extern struct thread_master * master;

extern int icmp6_sockfd;

void mld_rtr_send_general_query(struct mld_rtr_state * st);

int mld_rtr_general_qry_expired(struct thread * thread);
