import socket
import sys
import struct
import subprocess

def get_netiface_index(iface):
    process = subprocess.Popen("ip link show %s|head -1" % iface, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, shell=True)
    (stdout, stderr) = process.communicate()
    
    if stderr:
        print "crap"
        return -1

    return int(stdout.split(":")[0])

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print "Usage: %s ipv6_mcast_addr interface" % sys.argv[0]
        sys.exit(1)

    sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    mreq = socket.inet_pton(socket.AF_INET6, sys.argv[1])
    ifindex = get_netiface_index(sys.argv[2])

    if ifindex == -1:
        sock.close()
        print "Fatal error. Most probably %s is not a valid network interface" %\
            sys.argv[2]
        sys.exit(1)

    mreq += struct.pack("I", ifindex)
    sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_JOIN_GROUP, mreq)
    raw_input("Press enter to exit")
    sock.close()
