
import sys
if (len(sys.argv) != 5):
    print 'Usage:'
#   print 'ap-generator.py <AP_CHANNEL> <QUEUE_SIZE> <HW_ADDR> <ODIN_MASTER_IP> <ODIN_MASTER_PORT>'
    print 'agent-generator.py <AP_CHANNEL> <QUEUE_SIZE> <ODIN_MASTER_IP> <DEBUGFS_PATH>'
    sys.exit(0)

#AP_UNIQUE_IP_WITH_MASK = "172.17.2.53/24"
#AP_UNIQUE_IP_WITH_MASK = "172.16.250.175/24"
#AP_UNIQUE_BSSID = "00-1B-B1-F2-EF-FE"
#ODIN_MASTER_PORT = sys.argv[5]
#DEFAULT_CLIENT_MAC = "e8-39-df-4c-7c-ee"

#not needed
HW_ADDR = "10-0b-a9-f1-e0-60"

ODIN_MASTER_PORT = 2819

AP_CHANNEL = sys.argv[1]
QUEUE_SIZE = sys.argv[2]
ODIN_MASTER_IP = sys.argv[3]
DBGFS = sys.argv[4]

#not needed
DF_GW = "10.0.0.1"

INTERNAL_IFACE = "ap"

print '''
odinagent::OdinAgent(%s, RT rates, CHANNEL %s, DEFAULT_GW %s, DEBUGFS %s)
TimedSource(2, "ping\n")->  odinsocket::Socket(UDP, %s, %s, CLIENT true)
''' % (HW_ADDR, AP_CHANNEL, DF_GW, DBGFS, ODIN_MASTER_IP, ODIN_MASTER_PORT)


print '''

odinagent[3] -> odinsocket

rates :: AvailableRates(DEFAULT 24 36 48 108);

control :: ControlSocket("TCP", 6777);
chatter :: ChatterSocket("TCP", 6778);

// ----------------Packets going down
// 12 = offset, 0806 = arp, 20 = offset, 0001 = type request, output 1, rest to output 2
// The arp responder configuration here doesnt matter, odinagent.cc sets it according to clients
FromHost(%s, HEADROOM 50)
  -> fhcl :: Classifier(12/0806 20/0001, -)
  -> fh_arpr :: ARPResponder(172.17.250.170 98:d6:f7:67:6b:ee) // Resolve STA's ARP
  -> ARPPrint("Resolving client's ARP by myself")
  -> ToHost(%s)
''' % (INTERNAL_IFACE, INTERNAL_IFACE)

print '''

q :: Queue(%s)
  -> SetTXRate (12)
  -> RadiotapEncap()
  -> to_dev :: ToDevice (mon0);

// Anything from host that isn't an ARP request
fhcl[1]
  -> [1]odinagent

''' % (QUEUE_SIZE)


print '''
// Not looking for an STA's ARP? Then let it pass.
fh_arpr[1]
  -> [1]odinagent

odinagent[2]
  -> q

// ----------------Packets going down


// ----------------Packets coming up
from_dev :: FromDevice(mon0, HEADROOM 50)
  -> RadiotapDecap()
  -> ExtraDecap()
  -> phyerr_filter :: FilterPhyErr()
  -> tx_filter :: FilterTX()
  -> dupe :: WifiDupeFilter()
  -> [0]odinagent

odinagent[0]
  -> q

// Data frames
// The arp responder configuration here doesnt matter, odinagent.cc sets it according to clients
odinagent[1]
  -> decap :: WifiDecap()
  -> RXStats
  -> arp_c :: Classifier(12/0806 20/0001, -)
  -> arp_resp::ARPResponder (172.16.250.170 00-1B-B3-67-6B-EE) // ARP fast path for STA
  -> [1]odinagent


// ARP Fast path fail. Re-write MAC address
// to reflect datapath or learning switch will drop it
arp_resp[1]
  -> ToHost(%s)



// Non ARP packets. Re-write MAC address to
// reflect datapath or learning switch will drop it
arp_c[1]
  -> ToHost(%s)
''' % (INTERNAL_IFACE, INTERNAL_IFACE)

