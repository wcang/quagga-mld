/* This is SPARTA!!!!! Nuh, but it is close to that. This is playground to
 * have fun with zebra core.
 */
#include <zebra.h>
#include "zclient.h"
#include "if.h"

/* information about zebra. */
struct zclient *zclient = NULL;

/* TODO: play with zebra. send ZEBRA_INTERFACE_ADD to zebra to see if 
 * we can receive all interface information
 */
/* Inteface addition message from zebra. */
static int
mld_zebra_if_add (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface * ifp;

  ifp = zebra_interface_add_read (zclient->ibuf);
  printf("Zebra Interface add: %s index %d\n",
		ifp->name, ifp->ifindex);
  
  if (if_is_up(ifp)) {
    printf("Interface %s is up\n", ifp->name);
  }
 
  return 0;
}

static int
mld_zebra_if_del (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface *ifp;

  if (!(ifp = zebra_interface_state_read(zclient->ibuf)))
    return 0;

  if (if_is_up(ifp))
    printf("Zebra: got delete of %s, but interface is still up\n", ifp->name);

  printf("Zebra Interface delete: %s index %d\n",
		ifp->name, ifp->ifindex);

  ifp->ifindex = IFINDEX_INTERNAL;
  return 0;
}

  
static int
mld_zebra_if_state_update (int command, struct zclient *zclient,
                             zebra_size_t length)
{
  struct interface *ifp;

  ifp = zebra_interface_state_read (zclient->ibuf);
  if (ifp == NULL)
    return 0;
  
  printf("Zebra Interface state change: "
              "%s index %d flags %llx metric %d\n",
    ifp->name, ifp->ifindex, (unsigned long long)ifp->flags, 
    ifp->metric);
  return 0;
}

static int
mld_zebra_if_address_update_add (int command, struct zclient *zclient,
                                   zebra_size_t length)
{
  struct connected *c;
  char buf[128];

  c = zebra_interface_address_read (ZEBRA_INTERFACE_ADDRESS_ADD, zclient->ibuf);
  if (c == NULL)
    return 0;

  printf("Zebra Interface address add: %s %5s %s/%d\n",
  c->ifp->name, prefix_family_str (c->address),
  inet_ntop (c->address->family, &c->address->u.prefix,
       buf, sizeof (buf)), c->address->prefixlen);
/*
  if (c->address->family == AF_INET6)
    pim6_interface_connected_update(c->ifp);
*/
  return 0;
}


static int
mld_zebra_if_address_update_delete (int command, struct zclient *zclient,
                               zebra_size_t length)
{
  struct connected *c;
  char buf[128];

  c = zebra_interface_address_read (ZEBRA_INTERFACE_ADDRESS_DELETE, zclient->ibuf);
  if (c == NULL)
    return 0;

  printf("Zebra Interface address delete: %s %5s %s/%d\n",
  c->ifp->name, prefix_family_str (c->address),
  inet_ntop (c->address->family, &c->address->u.prefix,
       buf, sizeof (buf)), c->address->prefixlen);
/*
  if (c->address->family == AF_INET6)
    pim6_interface_connected_update(c->ifp);
*/
  return 0;
}



void
mld_zebra_init (void)
{
  /* Allocate zebra structure. */
  zclient = zclient_new ();
  zclient_init(zclient, 0);
  zclient->router_id_update = NULL;
  zclient->interface_add = mld_zebra_if_add;
  zclient->interface_delete = mld_zebra_if_del;
  zclient->interface_up = mld_zebra_if_state_update;
  zclient->interface_down = mld_zebra_if_state_update;
  zclient->interface_address_add = mld_zebra_if_address_update_add;
  zclient->interface_address_delete = mld_zebra_if_address_update_delete;
  zclient->ipv4_route_add = NULL;
  zclient->ipv4_route_delete = NULL;
  zclient->ipv6_route_add = NULL;
  zclient->ipv6_route_delete = NULL;

  return;
}


