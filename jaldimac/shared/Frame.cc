#include <click/config.h>

#include "Frame.hh"

using namespace jaldimac;

CLICK_DECLS

const size_t Frame::header_size = 4 * sizeof(uint8_t) /* preamble */
				+ sizeof(uint8_t) /* src_id */
                                + sizeof(uint8_t) /* dest_id */
				+ sizeof(uint8_t) /* type */
                                + sizeof(uint8_t) /* tag */
				+ sizeof(uint32_t) /* length */
                                + sizeof(uint32_t) /* seq */;

const size_t Frame::footer_size = sizeof(uint32_t); /* timestamp */

const size_t Frame::empty_frame_size = Frame::header_size + Frame::footer_size;

CLICK_ENDDECLS
ELEMENT_PROVIDES(Frame)
