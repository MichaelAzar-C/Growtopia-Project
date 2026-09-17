#pragma once
#include <string>
#include <variant>

#include "../packet_helper.hpp"

namespace packet::game {
struct Disconnect : GamePacket<PacketId::Disconnect, PACKET_DISCONNECT> {
    bool read(const Payload& payload) override
    {
        return is_payload<GamePayload>(payload);
    }

    Payload write() override
    {
        GamePayload game_payload{};
        game_payload.packet.type = PACKET_TYPE;
        game_payload.packet.net_id = -1;
        return game_payload;
    }
};

struct OnSendToServer : VariantPacket<PacketId::OnSendToServer> {
    uint16_t port{ 0 };
    int32_t token{ 0 };
    int32_t user{ 0 };
    std::string address;
    std::string door_id;
    std::string uuid_token;
    uint8_t login_mode{ 0 };
    std::string username;

    bool read(const Payload& payload) override
    {
        const auto var{ get_payload_if<VariantPayload>(payload) };
        if (!var) {
            return false;
        }

        variant = var->variant;
        game_packet = var->game_packet;

        if (variant.size() < 5) {
            return false;
        }

        const auto& values{ variant.get_variants() };

        // The server may send numbers as signed or unsigned depending on the
        // game version, so accept both instead of assuming one type.
        port = static_cast<uint16_t>(read_number(values[1]));
        token = static_cast<int32_t>(read_number(values[2]));
        user = static_cast<int32_t>(read_number(values[3]));

        const std::string raw_text{ variant.get<std::string>(4) };
        const std::string key{ raw_text.substr(0, raw_text.find_first_of('|')) };

        utils::TextParse text_parse{};
        text_parse.parse(raw_text);

        address = key;
        door_id = text_parse.get(key, 0);
        uuid_token = text_parse.get(key, 1);

        // Optional fields: only read them if they exist, whatever their type.
        if (values.size() > 5) {
            login_mode = static_cast<uint8_t>(read_number(values[5]));
        }

        if (values.size() > 6 && std::holds_alternative<std::string>(values[6])) {
            username = std::get<std::string>(values[6]);
        }

        return true;
    }

    Payload write() override
    {
        // Copy the packet exactly as the server sent it, then change ONLY the
        // port and the IP address. Every other field keeps its original value
        // and type, so a new game version adding or changing fields won't
        // corrupt the redirect.
        PacketVariant modified{ variant };

        if (modified.size() > 4) {
            if (std::holds_alternative<std::uint32_t>(modified.get_variants()[1])) {
                modified.set(1, static_cast<std::uint32_t>(port));
            }
            else {
                modified.set(1, static_cast<std::int32_t>(port));
            }

            // Field 4 looks like "IP|door_id|session_key". Replace only the IP.
            std::string raw_text{ modified.get<std::string>(4) };
            const auto separator{ raw_text.find('|') };
            raw_text = (separator == std::string::npos)
                ? address
                : address + raw_text.substr(separator);

            modified.set(4, raw_text);
        }

        return VariantPayload{ game_packet, modified };
    }

private:
    [[nodiscard]] static int64_t read_number(const packet::variant& value)
    {
        if (std::holds_alternative<std::int32_t>(value)) {
            return std::get<std::int32_t>(value);
        }

        if (std::holds_alternative<std::uint32_t>(value)) {
            return std::get<std::uint32_t>(value);
        }

        if (std::holds_alternative<float>(value)) {
            return static_cast<int64_t>(std::get<float>(value));
        }

        return 0;
    }
};

struct OnSuperMainStartAcceptLogonHrdxs47254722215a : VariantPacket<PacketId::OnSuperMainStartAcceptLogonHrdxs47254722215a> {
    uint32_t item_hash{ 0 };
    std::string u;
    std::string uu;
    std::string uuu;
    std::string settings;
    uint32_t player_tribute_hash{ 0 };

    bool read(const Payload& payload) override
    {
        const auto var{ get_payload_if<VariantPayload>(payload) };
        if (!var) {
            return false;
        }

        variant = var->variant;
        game_packet = var->game_packet;

        if (variant.size() < 5) {
            return false;
        }

        item_hash = variant.get<uint32_t>(1);
        u = variant.get<std::string>(2);
        uu = variant.get<std::string>(3);
        uuu = variant.get<std::string>(4);

        if (variant.size() >= 7) {
            settings = variant.get<std::string>(5);
            player_tribute_hash = variant.get<uint32_t>(6);
        }

        return true;
    }

    Payload write() override
    {
        const PacketVariant variant{
            "OnSuperMainStartAcceptLogonHrdxs47254722215a",
            item_hash,
            u,
            uu,
            uuu,
            settings,
            player_tribute_hash
        };
        return VariantPayload{ game_packet, variant };
    }
};
}
