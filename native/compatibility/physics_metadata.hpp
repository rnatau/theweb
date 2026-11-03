#pragma once
#include "../physics/mechanisms.hpp"
namespace web::compatibility {
// Source startup bytes without confirmed gameplay meaning. Runtime bootstrap
// retains these separately from the development snapshot serialization API.
struct PhysicsMetadata {
    static void initialize_unused(physics::BallState& state) {
        // Unsupported original metadata is observable in legacy snapshots, not physics.
        constexpr std::array<std::size_t, 79> offsets = {12,  13,  14,  15,  16,  17,  18,  19,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
                                                         75,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
                                                         122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 142, 143, 150, 151, 154, 155, 156};
        for (std::size_t i = 0; i < offsets.size(); ++i) {
            if (offsets[i] == 14 || offsets[i] == 18 || offsets[i] == 100 || offsets[i] == 104 || offsets[i] == 108 || offsets[i] == 112)
                state.compatibility_bytes_[i] = 100;
        }
    }
    static void initialize_unused(physics::FlipperState& state) {
        // Original asset/animation metadata; retained only in historical snapshots.
        constexpr std::array<std::size_t, 32> offsets = {8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27, 60, 61, 62, 63, 64, 65, 66, 67, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139};
        for (std::size_t i = 0; i < offsets.size(); ++i)
            if (offsets[i] == 0x80 || offsets[i] == 0x84)
                state.compatibility_bytes_[i] = 6;
    }
    static void initialize_unused(physics::SpinnerState& state) {
        state.compatibility_bytes_[0] = 8;
        state.compatibility_bytes_[4] = 8;
    }
};
} // namespace web::compatibility
