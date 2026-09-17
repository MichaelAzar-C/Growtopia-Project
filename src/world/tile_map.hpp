#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "tile.hpp"
#include "../utils/byte_stream.hpp"

class WorldTileMap final {
public:
    WorldTileMap()
        : size_{ 0, 0 }
    {

    }

    void serialize(utils::ByteStream<>& bs, const std::uint16_t version)
    {
        bs.read(size_.x);
        bs.read(size_.y);

        std::uint32_t count{};
        bs.read(count);
        bs.skip(5);

        // Every tile takes several bytes, so a count larger than the data
        // left means the map was misread. Don't try to allocate it.
        tiles_.clear();
        if (count > bs.remaining()) {
            return;
        }

        tiles_.resize(count);

        for (auto& tile : tiles_) {
            if (bs.remaining() == 0) {
                break;
            }
            tile.serialize(bs, version);
        }
    }

    [[nodiscard]] const glm::ivec2& get_size() const { return size_; }
    [[nodiscard]] glm::ivec2& get_size() { return size_; }
    [[nodiscard]] const std::vector<world::Tile>& get_tiles() const { return tiles_; }
    [[nodiscard]] std::vector<world::Tile>& get_tiles() { return tiles_; }

private:
    glm::ivec2 size_;
    std::vector<world::Tile> tiles_;
};
