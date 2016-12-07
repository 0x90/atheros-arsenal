#ifndef JALDI_CLICK_HH
#define JALDI_CLICK_HH

#include <click/packet.hh>
#include "Frame.hh"

template<uint8_t FrameType, uint8_t DestId, typename PayloadType>
WritablePacket* make_jaldi_frame(uint8_t src_id, PayloadType*& payload_out)
{
    WritablePacket* wp = Packet::make(jaldimac::Frame::empty_frame_size + sizeof(PayloadType));
    jaldimac::Frame* f = (jaldimac::Frame*) wp->data();
    f->initialize();
    f->type = FrameType;
    f->src_id = src_id;
    f->dest_id = DestId;
    f->length = jaldimac::Frame::empty_frame_size + sizeof(PayloadType);
    payload_out = (PayloadType*) f->payload;
    return wp;
}

template<uint8_t FrameType, typename PayloadType>
WritablePacket* make_jaldi_frame_dyn_dest(uint8_t src_id, uint8_t dest_id, PayloadType*& payload_out)
{
    WritablePacket* wp = Packet::make(jaldimac::Frame::empty_frame_size + sizeof(PayloadType));
    jaldimac::Frame* f = (jaldimac::Frame*) wp->data();
    f->initialize();
    f->type = FrameType;
    f->src_id = src_id;
    f->dest_id = dest_id;
    f->length = jaldimac::Frame::empty_frame_size + sizeof(PayloadType);
    payload_out = (PayloadType*) f->payload;
    return wp;
}



#endif
