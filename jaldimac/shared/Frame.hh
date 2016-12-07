#ifndef FRAME_HH
#define FRAME_HH

#include <cstring>

namespace jaldimac {

enum FrameType
{
    BULK_FRAME = 0,
    VOIP_FRAME,
    REQUEST_FRAME,
    CONTENTION_SLOT,
    VOIP_SLOT,
    TRANSMIT_SLOT,
    BITRATE_MESSAGE,
    ROUND_COMPLETE_MESSAGE,
    DELAY_MESSAGE
};

struct Frame
{
    // Fields
    uint8_t preamble[4];
    uint8_t src_id;
    uint8_t dest_id;
    uint8_t type;
    uint8_t tag;            // Currently unused
    uint32_t length;
    uint32_t seq;
    uint8_t payload[0];     // Actual size determined by length

    // Static constants
    static const size_t header_size;
    static const size_t footer_size;
    static const size_t empty_frame_size;

    // Member functions
    inline void initialize();
    inline size_t payload_length() const { return length - empty_frame_size; }

} __attribute__((__packed__));

// Important constants:
const uint8_t CURRENT_VERSION = 1;
const uint8_t PREAMBLE[4] = {'J', 'L', 'D', CURRENT_VERSION};
const unsigned FLOWS_PER_VOIP_SLOT = 4;

inline void Frame::initialize()
{
    // Set up preamble
    std::memcpy(preamble, PREAMBLE, sizeof(PREAMBLE));

    // Set up defaults for other values
    src_id = 0;
    dest_id = 0;
    type = BULK_FRAME;
    tag = 0;
    length = empty_frame_size;
    seq = 0;
}

// Cast the payload to one of the following structs as appropriate for the
// frame type.  After the payload comes an additional 32 bit TX timestamp which
// is added by the driver; it is only used for debugging purposes and should
// not affect the semantics of the protocol.
// BULK_FRAME and VOIP_FRAME do not have a struct below as their payload consists
// of an encapsulated IP packet.

struct RequestFramePayload
{
    uint32_t bulk_request_bytes;
    uint8_t voip_request_flows;
} __attribute__((__packed__));

struct ContentionSlotPayload
{
    uint32_t duration_us;
} __attribute__((__packed__));

struct VoIPSlotPayload
{
    uint32_t duration_us;
    uint8_t stations[FLOWS_PER_VOIP_SLOT];
} __attribute__((__packed__));

struct TransmitSlotPayload
{
    uint32_t duration_us;
    uint8_t voip_granted_flows;
} __attribute__((__packed__));

struct BitrateMessagePayload
{
    /* REPLACE WITH BITRATE ENUM TYPE */ uint32_t bitrate;
} __attribute__((__packed__));

struct RoundCompleteMessagePayload
{
} __attribute__((__packed__));

struct DelayMessagePayload
{
    uint32_t duration_us;
} __attribute__((__packed__));

// Node IDs:
const uint8_t BROADCAST_ID = 0;
const uint8_t DRIVER_ID = 0;
const uint8_t MASTER_ID = 1;
const uint8_t FIRST_STATION_ID = 2;

// MTUs:
const unsigned BULK_MTU__BYTES = 1500;
const unsigned VOIP_MTU__BYTES = 300;

// Sizes:
const uint32_t REQUEST_FRAME_SIZE__BYTES = Frame::empty_frame_size + sizeof(RequestFramePayload);

// Bitrates: (these will be replaced by a better bitrate mechanism)
const unsigned MEGABIT__BYTES = 1000000 /* bits */ / 8 /* bytes */;
const unsigned ETH_10_MEGABIT__BYTES_PER_US = (MEGABIT__BYTES * 10 /* hz */) / 1000000 /* us/s */;
const unsigned ETH_100_MEGABIT__BYTES_PER_US = (MEGABIT__BYTES * 100 /* hz */) / 1000000 /* us/s */;
const unsigned ETH_1000_MEGABIT__BYTES_PER_US = (MEGABIT__BYTES * 1000 /* hz */) / 1000000 /* us/s */;
const unsigned WIFI_20_MEGABIT__BYTES_PER_US = (MEGABIT__BYTES * 20 /* hz */) / 1000000 /* us/s */;
const unsigned BITRATE__BYTES_PER_US = WIFI_20_MEGABIT__BYTES_PER_US;

// VoIP-related constants:
const uint32_t VOIP_SLOT_GUARD_SIZE__BYTES = 2 * BULK_MTU__BYTES;
const uint32_t VOIP_SLOT_SIZE_PER_FLOW__BYTES = REQUEST_FRAME_SIZE__BYTES + VOIP_MTU__BYTES + VOIP_SLOT_GUARD_SIZE__BYTES;
const uint32_t VOIP_SLOT_SIZE__BYTES = FLOWS_PER_VOIP_SLOT * VOIP_SLOT_SIZE_PER_FLOW__BYTES;

// Round constraints:
const uint32_t MIN_CHUNK_SIZE__BYTES = BITRATE__BYTES_PER_US * 1 /* ms */ * 1000 /* us/ms */;
const uint32_t MAX_ROUND_SIZE__BYTES = BITRATE__BYTES_PER_US * 500 /* ms */ * 1000 /* us/ms */;
const uint32_t CONTENTION_SLOT_DURATION__US = 50 /* ms */ * 1000 /* us/ms */;
const uint32_t INTER_VOIP_SLOT_DISTANCE__BYTES = BITRATE__BYTES_PER_US * 40 /* ms */ * 1000 /* us/ms */;
const uint32_t DEFAULT_CONTENTION_SLOT_ONLY_DISTANCE__US = 50 /* ms */ * 1000 /* us/ms */;

// Other temporary constants:
const unsigned STATION_COUNT = 4;

}

#endif
