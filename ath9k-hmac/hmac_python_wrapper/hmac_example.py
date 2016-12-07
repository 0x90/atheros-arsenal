#!/usr/bin/env python
# -*-coding: utf-8 -*-
import logging
import time


from hmac_python_wrapper import HybridTDMACSMAMac, AccessPolicy

__author__ = "Sven Zehl, Anatolij Zubow"
__copyright__ = "Copyright (c) 2016, Technische Universität Berlin"
__version__ = "1.0.0"
__email__ = "{zehl, zubow}@tkn.tu-berlin.de"

if __name__ == "__main__":

    ''' Example script

        Create a HMAC configuration with:
        - 10 time slots per superframe
        - a slot duration of 20000 microseonds (20 ms)
        - for wireless interface wlan0
        - two configurations are used:
        - A. the first 4 time slots can be used by any best effort traffic towards STA with MAC address 34:13:e8:24:77:be
        - A. the last 6 time slots are guard time slots, i.e. blocked from being used
        - B. time slots 5-8 can be used by any best effort traffic towards STA with MAC address 34:13:e8:24:77:be
        - B. the other time slots are guard time slots, i.e. blocked from being used
    '''
    log = logging.getLogger()
    log.setLevel(logging.INFO)
    handler = logging.StreamHandler()
    handler.setLevel(logging.INFO)
    formatter = logging.Formatter("%(levelname)s - %(message)s")
    handler.setFormatter(formatter)
    log.addHandler(handler)

    # configuration of hybrid MAC
    dstHWAddr = "34:13:e8:24:77:be" # STA destination MAC address
    total_slots = 10
    slot_duration = 20000
    iface = 'wlan0'

    # create new MAC for local node
    mac = HybridTDMACSMAMac(log, iface, total_slots, slot_duration)

    be_slots = [1,2,3,4]

    # assign access policies to each slot in superframe
    for slot_nr in range(total_slots):
        if slot_nr in be_slots:
            # those are slots for best effort traffic towards our STA
            acBE = AccessPolicy()
            acBE.addDestMacAndTosValues(dstHWAddr, 0)
            mac.setAccessPolicy(slot_nr, acBE)
        else:
            # those are guard slots
            acGuard = AccessPolicy()
            acGuard.disableAll() # guard slot
            mac.setAccessPolicy(slot_nr, acGuard)

    # install MAC Processor
    if mac.install_mac_processor():
        log.info('HMAC is running ...')
        mac.printConfiguration()

        # wait 20 seconds
        time.sleep(20)

        log.info('Update HMAC with new configuration ...')

        # new configuration
        be_slots = [5,6,7,8]
        # assign access policies to each slot in superframe
        for slot_nr in range(total_slots):
            if slot_nr in be_slots:
                # those are slots for best effort traffic towards our STA
                acBE = AccessPolicy()
                acBE.addDestMacAndTosValues(dstHWAddr, 0)
                mac.setAccessPolicy(slot_nr, acBE)
            else:
                # those are guard slots
                acGuard = AccessPolicy()
                acGuard.disableAll() # guard slot
                mac.setAccessPolicy(slot_nr, acGuard)

        # update MAC Processor
        if mac.update_mac_processor():
            log.info('HMAC is updated ...')
            mac.printConfiguration()

            time.sleep(20)

            log.info("Stopping HMAC")

            if mac.uninstall_mac_processor():
                log.info('HMAC is stopped ...')
            else:
                log.fatal('Failed to stop HMAC!')
        else:
            log.fatal('HMAC is not running ... check your installation!')
    else:
        log.fatal('HMAC is not running ... check your installation!')
