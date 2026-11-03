#include "../physics/simulation.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
unsigned checks = 0;
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
    ++checks;
}
struct Fixture {
    std::filesystem::path directory = std::filesystem::temp_directory_path() / ("theweb-physics-loader-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() {
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Physics fixture already exists");
        std::filesystem::create_directory(directory / "COLLIDE");
        std::filesystem::create_directory(directory / "GENERAL");
        for (const auto* name : {"COLLIDE0", "COLLIDE1", "COLLIDE2", "HEIGHT1", "HEIGHT2", "SWITCH0", "SWITCH1"}) {
            web::write_file(directory / "COLLIDE" / (std::string(name) + ".MAP"), web::Bytes(40 * 77 * 2));
            web::write_file(directory / "COLLIDE" / (std::string(name) + ".SET"), web::Bytes(64, std::string_view(name).starts_with("HEIGHT") ? 255 : 0));
        }
        for (const auto* name : {"BVEC0_10.TBL", "BVEC0_50.TBL"})
            web::write_file(directory / "GENERAL" / name, web::Bytes(512));
    }
    ~Fixture() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};
struct Events : web::physics::PhysicsEventSink {
    unsigned count = 0;
    void on_event(const web::physics::PhysicsEvent&, web::physics::BallSlots&) override {
        ++count;
    }
};
web::physics::BallSlots balls(unsigned layer) {
    web::physics::BallSlots result;
    auto& ball = result[0];
    ball.active(1);
    ball.motion_type(web::physics::ball_type::free);
    ball.layer(std::uint16_t(layer));
    ball.position({100 * 65536, 200 * 65536});
    ball.velocity({32, 16});
    return result;
}
void sample_loaded_ball_physics(const web::physics::BallPhysics& physics) {
    for (unsigned layer = 0; layer < 3; ++layer) {
        auto state = balls(layer);
        web::physics::MotionState motion;
        web::physics::RandomGenerator random;
        Events events;
        physics.step(state, 0, motion, {random, events});
        check(state[0].position() == web::physics::Position2{100 * 65536 + 32, 200 * 65536}, "Loaded physics lost motion or collision geometry");
        check(state[0].layer() == layer && state[0].surface_height() == 0 && events.count == 0, "Loaded surface geometry changed the flat fixture");
    }
}
} // namespace
int main() {
    try {
        Fixture fixture;
        const auto response = web::physics::CollisionResponse::load(fixture.directory);
        sample_loaded_ball_physics(web::physics::BallPhysics::load(fixture.directory));
        sample_loaded_ball_physics(web::physics::BallPhysics::load(fixture.directory, response, nullptr));
        auto loaded = web::physics::BallPhysics::load(fixture.directory);
        auto copied = loaded;
        auto moved = std::move(loaded);
        sample_loaded_ball_physics(copied);
        sample_loaded_ball_physics(moved);
        const auto simulation = web::physics::PhysicsSimulation::load(fixture.directory);
        for (unsigned layer = 0; layer < 3; ++layer) {
            web::physics::SimulationState state;
            state.balls = balls(layer);
            Events events;
            check(simulation.step(state, events) == 17, "Loaded simulation lost its microstep cadence");
            check(state.balls[0].active() == 1 && state.balls[0].layer() == layer && events.count == 0, "Loaded simulation lost shared geometry");
        }
        std::cout << checks << " synthetic physics loader checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
