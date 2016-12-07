# atheros-patches
Atheros wireless network driver patches for performance testing and rate adaptation algorithm.

004 : adjust the slottime for long distance network;

010 : disable ack of link layer;

020 : disable CSMA;

582 : set mcs used in xmit;

585 : used for 614\615;

586 : used for 615, identify the aggr size;

593 : record throughput in the receiver(loss rate is calculated by consecutive sequence number, not a recv window);

594 : read rssi in the receiver;

595 : disable rate adaptation when mcs is set;

596 : record xmit rate in the transmitter;

597 : useless;

598 : record recv rate in the receiver, including the fail frames;

599 : record fail frames;

607 : fix aggr size when only mcs is identify;

609 : record noise value;

610 : make retry limit work;

611 : modify lossrate calculation method. (corresponding to 593);

612 : adjust retry for 614 and 615;

613 : useless;

614 : implements of MiRA;

615 : implements of RainBow;

616 : try to implement pipline;

620 : record packet seqno for every packet;

701 : make pktgen work in the driver;

702 : disable backoff;

703 : modify the ibss timeout;

710 : useless;

919 : custom ack for pipline;

920 : custom sequence number;
