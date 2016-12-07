
import sys
if (len(sys.argv) != 6):
    print 'Usage:'
    print 'xdpd-conf-generator.py <MASTER_IP> <OF_PORT> <WAN_IFACE> <AP_IFACE> <SWITCH_DPID> '
    sys.exit(0)

MASTER_IP = sys.argv[1]
OF_PORT = sys.argv[2]
WAN_IFACE = sys.argv[3]
AP_IFACE = sys.argv[4]
SWITCH_DPID = sys.argv[5]
print '''
config:{

        openflow:{
                logical-switches:{
                        #Name of the switch dp0
                        dp0:{
                                #Most complex configuration
                                dpid = "%s"; #Must be hexadecimal
                                version = 1.0;
                                description="This is a switch";

                                #Controller connection(s)
                                controller-connections:{
                                        main:{
                                                remote-hostname="%s";
                                                remote-port = %s;
                                        };  
                                };  

                                #Reconnect behaviour
                                reconnect-time=1; #seconds

                                #Tables and MA
                                num-of-tables=1;

                                #Physical ports attached to this logical switch. This is mandatory
                                #The order and position in the array dictates the number of
                                # 1 -> veth0, 2 -> veth2, 3 -> veth4, 4 -> veth6
                                ports = ("%s", "%s");
                        };  
                };  
        };  
};

''' % (SWITCH_DPID, MASTER_IP, OF_PORT, WAN_IFACE, AP_IFACE)

