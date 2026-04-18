#include <cstdint>

#pragma pack(push, 1)

struct MessageHeader
{
	uint8_t tag;
	uint32_t length;
};

struct ConfigPayload
{
	uint32_t matrix_size;
	uint32_t thread_count;
};

#pragma pack(pop)

// commands
const uint8_t CMD_SEND_CONFIG = 0x01;
const uint8_t CMD_SEND_DATA = 0x02;
const uint8_t CMD_START = 0x03;
const uint8_t CMD_GET_STATUS = 0x04;

// responses
const uint8_t RESP_ACK = 0x11;
const uint8_t RESP_STARTED = 0x12;
const uint8_t RESP_STATUS = 0x13;
const uint8_t RESP_RESULT = 0x14;