
import sys
if (len(sys.argv) != 3):
    print 'Usage:'
    print 'hostapd-cfg-generator.py <WLAN_IFACE> <CHANNEL_NUM>'
    sys.exit(0)

WLAN_IFACE = sys.argv[1]
CHANNEL_NUM = sys.argv[2]

print '''
interface=%s
driver=nl80211
ssid=odin-hostap-fran
ignore_broadcast_ssid=1
channel=%s
hw_mode=g
''' % (WLAN_IFACE, CHANNEL_NUM)

