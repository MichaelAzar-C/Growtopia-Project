#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "packet_types.hpp"

namespace packet {
// Builds a chat message exactly like the Growtopia client sends it:
//   [4-byte message type] "action|input\n|text|<text>" [zero byte]
// The proxy's TextParse::get_raw() drops the line break before "|text|",
// which makes the server silently ignore the message, so this builds the
// bytes by hand. Send the result with Client::write().
[[nodiscard]] inline std::vector<std::byte> build_chat_packet(const std::string& text)
{
    const std::string body{ "action|input\n|text|" + text };
    const auto message_type{ static_cast<std::uint32_t>(NET_MESSAGE_GENERIC_TEXT) };

    std::vector<std::byte> data(sizeof(message_type) + body.size() + 1);
    std::memcpy(data.data(), &message_type, sizeof(message_type));
    std::memcpy(data.data() + sizeof(message_type), body.data(), body.size());
    data.back() = std::byte{ 0 }; // end marker

    return data;
}
}
