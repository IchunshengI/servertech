// Shared room catalog.
// Keep this header lightweight: used by websocket and startup warmup.

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_ROOMS_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_ROOMS_HPP

#include <array>
#include <string_view>

namespace chat {

// Rooms are static for now.
static constexpr std::array<std::string_view, 2> room_ids{
    "test_channe1",
    "test_channe2",
};

static constexpr std::array<std::string_view, room_ids.size()> room_names{
    "内测频道1",
    "内测频道2",
};

}  // namespace chat

#endif

