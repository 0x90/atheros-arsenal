#!/usr/bin/env python
# -*-coding: utf-8 -*-

import logging
import time
import subprocess
import zmq

__author__ = "Sven Zehl, Anatolij Zubow"
__copyright__ = "Copyright (c) 2016, Technische Universität Berlin"
__version__ = "1.0.0"
__email__ = "{zehl, zubow}@tkn.tu-berlin.de"

"""
Class for controlling the hybrid TDMA/CSMA MAC.
"""
class HybridTDMACSMAMac(object):
    def __init__(self, log, interface, no_slots_in_superframe, slot_duration_ns,
                 hmac_binary_path='hmac_userspace_daemon/hmac_userspace_daemon',
                 local_mac_processor_port=1217):
        '''
        The configuration of such a MAC is described by:
        :param interface: the wireless interface on which we want to install the HMAC
        :param no_slots_in_superframe: the total number of slots in a superframe
        :param slot_duration_ns: the time duration of each slot (microseconds)
        :param hmac_binary_path: path to the C++ userland HMAC daemon
        :param local_mac_processor_port: ZeroMQ port used for communication with HMAC daemon
        '''
        self.log = log

        self.interface = interface
        self.mNo_slots_in_superframe = no_slots_in_superframe
        self.mSlot_duration_ns = slot_duration_ns
        self.acs = []
        for ii in range(no_slots_in_superframe):
            self.acs.append(None)
        # path to the HMAC C++ userland daemon
        self.hmac_binary_path = hmac_binary_path
        self.local_mac_processor_port = local_mac_processor_port
        self.state = MACState.NOT_RUNNING


    def getInterface(self):
        '''
        Returns the wireless interface
        '''
        return self.interface


    def getNumSlots(self):
        '''
        Get the total number of slots in superframe
        '''
        return self.mNo_slots_in_superframe


    def setAccessPolicy(self, slot_nr, ac):
        '''
        Sets an access policy to a given slot in the superframe
        :param slot_nr: the slot id to which the access policy to apply
        :param ac: the access policy (AccessPolicy class)
        :return: True if correct
        '''
        if slot_nr >= 0 and slot_nr < len(self.acs):
            self.acs[slot_nr] = ac
            return True
        else:
            return False


    def getAccessPolicy(self, slot_nr):
        '''
        Get the access policy assigned to given slot.
        :param slot_nr: ID starting from 0.
        :return: AccessPolicy object
        '''
        if slot_nr >= 0 and slot_nr < len(self.acs):
            return self.acs[slot_nr]
        else:
            return None


    def removeAccessPolicy(self, slot_nr):
        '''
        Removes the access policy assigned to given slot.
        :param slot_nr: ID starting from 0.
        :return: True
        '''
        if slot_nr >= 0 and slot_nr < len(self.acs):
            self.acs[slot_nr] = None
            return True
        else:
            return False


    def getSlotDuration(self):
        '''
        Get time duration of a slot
        '''
        return self.mSlot_duration_ns


    def printConfiguration(self):
        '''
        Return the MAC configuration serialized as string.
        :return:
        '''
        self.log.info('[')
        for ii in range(self.getNumSlots()):
            nline = str(ii) + ': ' + self.getAccessPolicy(ii).printConfiguration()
            self.log.info(nline)
        self.log.info(']')


    def install_mac_processor(self):
        '''
        Installs the given hybrid MAC configuration
        :return: True if successful
        '''
        self.log.debug('install_mac_processor()')

        if self.state == MACState.RUNNING:
            self.log.warn('HMAC is already running; use update_mac_processor() to update at run-time')
            return False

        try:
            # 1. create HMAC configuration string
            conf_str = self._create_configuration_string()

            # construct command argument for HMAC daemon
            processArgs = str(self.hmac_binary_path) + " -d 0 " + " -i" + str(self.interface) \
                          + " -f" + str(self.getSlotDuration()) + " -n" + str(self.getNumSlots()) + " -c" + conf_str

            self.log.debug('Starting HMAC daemon with: %s' % processArgs)

            # start HMAC daemon as a background process
            subprocess.Popen(processArgs.split(), shell=False)

            self.hmac_ctrl_socket = None
            self.state = MACState.RUNNING
            return True
        except Exception as e:
            self.log.fatal("An error occurred while starting HMAC daemon, err_msg: %s" % str(e))

        return False


    def update_mac_processor(self):
        '''
        Updates the given hybrid MAC configuration at run-time with new configuration
        :return: True if successful
        '''

        self.log.debug('update_mac_processor()')

        if self.state == MACState.NOT_RUNNING:
            self.log.info('HMAC is not yet running running; start it')
            return self.install_mac_processor()

        try:
            # 1. create HMAC configuration string
            conf_str = self._create_configuration_string()

            if self.hmac_ctrl_socket is None:
                context = zmq.Context()
                self.hmac_ctrl_socket = context.socket(zmq.REQ)
                self.hmac_ctrl_socket.connect("tcp://localhost:" + str(self.local_mac_processor_port))

            #  update MAC processor configuration
            self.log.info("Send ctrl req message to HMAC: %s" % conf_str)
            self.hmac_ctrl_socket.send(conf_str)
            message = self.hmac_ctrl_socket.recv()
            self.log.info("Received ctrl reply message from HMAC: %s" % message)
            return True
        except zmq.ZMQError as e:
            self.log.fatal("Failed to update running HMAC daemon, err_msg: %s" % str(e))

        return False


    def uninstall_mac_processor(self):
        '''
        Uninstalls the running hybrid MAC
        :return: True if successful
        '''

        self.log.debug('uninstall_mac_processor')

        if self.state == MACState.NOT_RUNNING:
            self.log.warn('HMAC is already stopped')
            return True

        try:
            # set allow all configuration string
            conf_str = self._create_allow_all_conf_string()

            # command string
            terminate_str = 'TERMINATE'

            if self.hmac_ctrl_socket is None:
                context = zmq.Context()
                self.hmac_ctrl_socket = context.socket(zmq.REQ)
                self.hmac_ctrl_socket.connect("tcp://localhost:" + str(self.local_mac_processor_port))

            #  update MAC processor configuration
            self.log.info("Send ctrl req message to HMAC: %s" % conf_str)
            self.hmac_ctrl_socket.send(conf_str)
            message = self.hmac_ctrl_socket.recv()
            self.log.info("Received ctrl reply from HMAC: %s" % message)

            # give one second to settle down
            time.sleep(1)

            # send termination signal to MAC
            self.hmac_ctrl_socket.send(terminate_str)
            message = self.hmac_ctrl_socket.recv()
            self.log.info("Received ctrl reply from HMAC: %s" % message)

            self.state = MACState.NOT_RUNNING
            return True
        except zmq.ZMQError as e:
            self.log.fatal("Failed to uninstall MAC processor %s" % str(e))

        return False

    ''' Helper '''
    def _create_configuration_string(self):
        conf_str = None
        for ii in range(self.getNumSlots()): # for each slot
            ac = self.getAccessPolicy(ii)
            entries = ac.getEntries()

            for ll in range(len(entries)):
                entry = entries[ll]

                # slot_id, mac_addr, tid_mask
                if conf_str is None:
                    conf_str = str(ii) + "," + str(entry[0]) + "," + str(entry[1])
                else:
                    conf_str = conf_str + "#" + str(ii) + "," + str(entry[0]) + "," + str(entry[1])

        return conf_str


    ''' Helper '''
    def _create_allow_all_conf_string(self):
        # generate configuration string
        conf_str = None
        for ii in range(self.getNumSlots()):  # for each slot
            # slot_id, mac_addr, tid_mask
            if conf_str is None:
                conf_str = str(ii) + "," + 'FF:FF:FF:FF:FF:FF' + "," + str(255)
            else:
                conf_str = conf_str + "#" + str(ii) + "," + 'FF:FF:FF:FF:FF:FF' + "," + str(255)

        return conf_str


class MACState:
    RUNNING, NOT_RUNNING = range(2)


"""
Class for controlling the access policy of each time slot using the destination MAC address and IP ToS value.
"""
class AccessPolicy(object):

    def __init__(self):
        self.entries = []


    def disableAll(self):
        '''
        Block usage of time slot for all packets
        '''
        self.entries = []


    def allowAll(self):
        '''
        Unblock usage of time slot for all packets
        '''
        self.entries = []
        self.entries.append(('FF:FF:FF:FF:FF:FF', 255))


    def addDestMacAndTosValues(self, dstHwAddr, *tosArgs):
        """Add destination mac address and list of ToS fields which is allowed to be transmitted in this time slot
        :param dstHwAddr: destination mac address
        :param tosArgs: list of ToS values to be allowed here
        """
        tid_map = 0
        for ii in range(len(tosArgs)):
            # convert ToS into tid
            tos = tosArgs[ii]
            skb_prio = tos & 30 >> 1
            tid =skb_prio & 7
            tid_map = tid_map | 2**tid

        self.entries.append((dstHwAddr, tid_map))


    def getEntries(self):
        '''
        Get saved entries
        '''
        return self.entries


    def printConfiguration(self):
        '''
        For debugging
        '''
        s = ''
        for ii in range(len(self.entries)):
            s = str(self.entries[ii][0]) + "/" + str(self.entries[ii][1]) + "," + s
        return s
