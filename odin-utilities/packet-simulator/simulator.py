#!/usr/bin/env python

"""
802.11 Scapy Packet Generator
Author: Fran Gonzalez, 2014
Adapted from the original script "802.11 Scapy Packet Example" written by Joff Thyer
"""

# if we set logging to ERROR level, it supresses the warning message
# from Scapy about ipv6 routing
#   WARNING: No route found for IPv6 destination :: (no default route?)
import logging
logging.getLogger("scapy.runtime").setLevel(logging.ERROR)

import time

from scapy.all import *

if (len(sys.argv) != 2):
    print 'Usage:   simulator.py [ MODE ]'
    print 'where    MODE := { sequence | probgen | probspec | auth | assoc | dhcp-ack | dns-query }\n'
    print '         sequence                - For simulating a probe request generic, probe request specific, open-auth request and association request with a time interval of 1s'
    print '         probgen                 - For simulating a probe request generic'
    print '         probspec                - For simulating a probe request specific'
    print '         auth                    - For simulating an open-auth request'
    print '         assoc                   - For simulating an association request'
    print '         dhcp-ack                - For simulating the reception of a DHCP ACK'
    print '         dns-query               - For simulating a DNS query'
    sys.exit(0)

# Parse the pcap files
pcap = rdpcap('/home/fran/Desktop/wifi_frames.pcap')
pcap_dhcp_ack = rdpcap('/home/fran/Desktop/dhcp_ack.pcap')
#pcap_dhcp = rdpcap('/home/fran/Desktop/dhcp.pcap')

# Generic probe request
probe_req_gen = pcap[0]
#Specific probe request
probe_req_esp = pcap[1]
#Authentication request
auth_req = pcap[2]
#Association request
assoc_req = pcap[3]
#DHCP ACK
dhcp_ack = pcap_dhcp_ack[0]

# Interface for injecting the DHCP ACK
IFACE_DHCP = 'veth1'
# Time interval between packets simulated with the sequence mode
interval = 1

class Scapy80211():

    def  __init__(self,\
        iface='',\
        ssid='',\
        source='00:00:00:00:00:11',\
        bssid='ff:ff:ff:ff:ff:ee',\
        srcip='10.10.10.10'):

        self.iface = iface
        self.ssid = ssid
        self.source  = source
        self.bssid = bssid
        self.srcip = srcip

        self.rates = "\x03\x12\x96\x18\x24\x30\x48\x60"

    # set Scapy conf.iface
    # conf.iface = self.iface

    def WifiSequence(self):
        try:
            sendp(probe_req_gen, iface=self.iface)
            print 'Probe request generic sent\n'
            time.sleep(interval)
            sendp(probe_req_esp, iface=self.iface)
            print 'Probe request especific sent\n'
            time.sleep(interval)
            sendp(auth_req, iface=self.iface)
            print 'Authentication request sent\n'
            time.sleep(interval)
            sendp(assoc_req, iface=self.iface)
            print 'Association request sent\n'
            time.sleep(interval)
            sendp(dhcp_ack, iface=IFACE_DHCP)
            print 'DHCP ACK sent\n'
        except:
            raise

###################### Custom-packet-creation functions #######################

    def Beacon(self,count=10,ssid='',dst='ff:ff:ff:ff:ff:ff'):
      if not ssid: ssid=self.ssid
      beacon = Dot11Beacon(cap=0x2104)
      essid  = Dot11Elt(ID='SSID',info=ssid)
      rates  = Dot11Elt(ID='Rates',info=self.rates)
      dsset  = Dot11Elt(ID='DSset',info='\x01')
      tim    = Dot11Elt(ID='TIM',info='\x00\x01\x00\x00')
      pkt = RadioTap()\
        /Dot11(type=0,subtype=8,addr1=dst,addr2=self.source,addr3=self.bssid)\
        /beacon/essid/rates/dsset/tim

      print '[*] 802.11 Beacon: SSID=[%s], count=%d' % (ssid,count)
      try:
        sendp(pkt,iface=self.iface,count=count,inter=0.1,verbose=0)
      except:
        raise


    def ProbeReq(self,count=10,ssid='',dst='ff:ff:ff:ff:ff:ff'):
      if not ssid: ssid=self.ssid
      param = Dot11ProbeReq()
      essid = Dot11Elt(ID='SSID',info=ssid)
      rates  = Dot11Elt(ID='Rates',info=self.rates)
      dsset = Dot11Elt(ID='DSset',info='\x01')
      pkt = RadioTap()\
        /Dot11(type=0,subtype=4,addr1=dst,addr2=self.source,addr3=self.bssid)\
        /param/essid/rates/dsset

      print '[*] 802.11 Probe Request: SSID=[%s], count=%d' % (ssid,count)
      try:
        sendp(pkt,count=count,inter=0.1,verbose=0)
      except:
        raise


    def ARP(self,targetip,count=1,toDS=False):
      if not targetip: return

      arp = LLC()/SNAP()/ARP(op='who-has',psrc=self.srcip,pdst=targetip,hwsrc=self.source)
      if toDS:
        pkt = RadioTap()\
                /Dot11(type=2,subtype=32,FCfield='to-DS',\
                addr1=self.bssid,addr2=self.source,addr3='ff:ff:ff:ff:ff:ff')\
                /arp
      else:
        pkt = RadioTap()\
                /Dot11(type=2,subtype=32,\
                addr1='ff:ff:ff:ff:ff:ff',addr2=self.source,addr3=self.bssid)\
                /arp

      print '[*] ARP Req: who-has %s' % (targetip)
      try:
        sendp(pkt,inter=0.1,verbose=0,count=count)
      except:
        raise

      ans = sniff(lfilter = lambda x: x.haslayer(ARP) and x.op == 2,
        store=1,count=1,timeout=1)

      if len(ans) > 0:
        return ans[0][ARP].hwsrc
      else:
        return None


    def DNSQuery(self,query='www.google.com',qtype='A',ns=None,count=1,toDS=False):
      if ns == None: return
      dstmac = self.ARP(ns)

      dns = LLC()/SNAP()/IP(src=self.srcip,dst=ns)/\
        UDP(sport=random.randint(49152,65535),dport=53)/\
        DNS(qd=DNSQR(qname=query,qtype=qtype))

      if toDS:
        pkt = RadioTap()\
                /Dot11(type=2,subtype=32,FCfield='to-DS',\
                addr1=self.bssid,addr2=self.source,addr3=dstmac)/dns
      else:
        pkt = RadioTap()\
                /Dot11(type=2,subtype=32,\
                addr1=dstmac,addr2=self.source,addr3=self.bssid)/dns

      print '[*] DNS query %s (%s) -> %s?' % (query,qtype,ns)
      try:
        sendp(pkt,count=count,verbose=0,iface=self.iface)
      except:
        raise

################################################################################

if __name__ == "__main__":

    test = Scapy80211(iface='mon0')

    if(sys.argv[1] == "sequence"):
        test.WifiSequence()

    elif(sys.argv[1] == "probgen"):
        sendp(probe_req_gen, iface=self.iface)
        print 'Probe request generic sent\n'

    elif(sys.argv[1] == "probspec"):
        sendp(probe_req_esp, iface=self.iface)
        print 'Probe request specific sent\n'

    elif(sys.argv[1] == "auth"):
        sendp(auth_req, iface=self.iface)
        print 'Authentication request sent\n'

    elif(sys.argv[1] == "assoc"):
        sendp(assoc_req, iface=self.iface)
        print 'Association request sent\n'

    elif(sys.argv[1] == "dhcp-ack"):
        sendp(dhcp_ack, iface=self.iface)
        print 'DHCP ACK sent\n'

    elif(sys.argv[1] == "dns-query"):
        test.DNSQuery(ns='10.10.10.2')
