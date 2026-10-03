// In-process fake for framed renderer peer responses; it never creates OS windows or pipes.
#pragma once

#include "../src/render_protocol.h"

namespace completionist::render::tests {

class FakeRendererPipe {
public:
    std::optional<Ack> Present(const Snapshot& snapshot, bool available = true) {
        const auto frame = EncodeShow(snapshot);
        if (frame.size() < 4) return std::nullopt;
        const auto body = std::string_view(frame).substr(4);
        auto decoded = ParseShow(body);
        if (!decoded || disconnect) return std::nullopt;
        Identity owner = invalidPeer ? Identity{snapshot.owner.pid + 1, snapshot.owner.hostHwnd,
                                                snapshot.owner.session, snapshot.owner.generation}
                                     : decoded->owner;
        return Ack{std::move(owner), decoded->revision, available};
    }

    bool invalidPeer = false;
    bool disconnect = false;
};

}  // namespace completionist::render::tests
