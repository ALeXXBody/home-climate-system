#include "ot_gateway.h"

#if defined(ESP32) && defined(HCS_GW_ENABLE)

namespace hcs {

static inline uint8_t frameType(uint32_t f) {
  return (uint8_t)(((f >> 28) & 0x7));
}

static inline uint8_t frameId(uint32_t f) {
  return (uint8_t)((f >> 16) & 0xFF);
}

/** Synthesise a local answer for a request we did not forward. */
static uint32_t localAnswer(uint8_t req_type, uint8_t id, uint16_t data,
                            bool known) {
  using MT = OpenThermMessageType;
  if (!known) {
    MT bad = (req_type == kTypeWriteData) ? MT::DATA_INVALID
                                          : MT::UNKNOWN_DATA_ID;
    return OpenTherm::buildResponse(bad, (OpenThermMessageID)id, 0);
  }
  MT ack =
      (req_type == kTypeWriteData) ? MT::WRITE_ACK : MT::READ_ACK;
  return OpenTherm::buildResponse(ack, (OpenThermMessageID)id, data);
}

uint32_t OtGateway::handleRequest(uint32_t req) {
  using MT = OpenThermMessageType;

  const uint8_t type = frameType(req);
  const uint8_t id = frameId(req);
  const uint16_t data = OpenTherm::getUInt(req);

  uint16_t out = 0;
  GwPolicy pol = rt_.route(type, id, data, &out);

  if (pol == GwPolicy::AnswerLocal) {
    return localAnswer(type, id, out, rt_.local_answer_known());
  }

  // Forward to the boiler through the master interface
  unsigned long resp = m_.sendRaw(OpenTherm::buildRequest(
      (MT)type, (OpenThermMessageID)id, out));

  if (m_.lastRawStatus() != OpenThermResponseStatus::SUCCESS) {
    rt_.setBoilerLinkUp(false);
    // try cache for this cycle's answer
    uint16_t again = 0;
    if (rt_.route(type, id, data, &again) == GwPolicy::AnswerLocal) {
      return localAnswer(type, id, again, rt_.local_answer_known());
    }
    return localAnswer(type, id, 0, false);
  }
  rt_.setBoilerLinkUp(true);

  const uint8_t rtype = frameType(resp);
  rt_.noteBoilerResponse(rtype, id, OpenTherm::getUInt(resp));

  // Relay genuine slave->master frames verbatim; else synthesise the ack
  if (rtype >= kTypeReadAck) return resp;
  MT ack = (type == kTypeWriteData) ? MT::WRITE_ACK : MT::READ_ACK;
  return OpenTherm::buildResponse(ack, (OpenThermMessageID)id,
                                  OpenTherm::getUInt(resp));
}

}  // namespace hcs

#endif  // ESP32 && HCS_GW_ENABLE
