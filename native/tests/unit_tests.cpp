#include "../compatibility/physics_snapshot.hpp"
#include "../compatibility/effect_snapshot.hpp"
#include "../compatibility/rule_snapshot.hpp"
#include "../compatibility/player_record.hpp"
#include "../web.hpp"
#include "../physics/collision.hpp"
#include "../physics/integer.hpp"
#include "../physics/motion.hpp"
#include "../physics/mechanisms.hpp"
#include "../physics/simulation.hpp"
#include "../rules/ball_rules.hpp"
#include "../rules/session_rules.hpp"
#include "../gameplay.hpp"
#include "../render/table_renderer.hpp"
#include "../frame_timing.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace web;
using namespace web::physics;
static_assert(!std::is_convertible_v<BallType, MotionMode>);
static_assert(!std::is_convertible_v<Position2, Vector2>);
static_assert(!std::is_convertible_v<Vector2, Position2>);
namespace {
unsigned checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F function) {
    ++checks;
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("Invalid input was accepted");
}
Bytes hex(const std::string& value) {
    Bytes result;
    for (std::size_t i = 0; i < value.size(); i += 2)
        result.push_back(std::uint8_t(std::stoul(value.substr(i, 2), nullptr, 16)));
    return result;
}
TileMap uniform(std::uint8_t value = 0) {
    TileMap map;
    map.columns = 40;
    map.rows = 77;
    map.cells.resize(3080);
    map.tiles.resize(64, value);
    return map;
}
void cell(TileMap& map, int x, int physics_y, std::uint8_t value) {
    int y = 614 - physics_y;
    auto index = std::size_t(y / 8) * 40 + x / 8;
    auto start = std::size_t(map.cells[index]) * 64;
    Bytes copy(map.tiles.begin() + start, map.tiles.begin() + start + 64);
    map.cells[index] = std::uint16_t(map.tiles.size() / 64);
    map.tiles.insert(map.tiles.end(), copy.begin(), copy.end());
    map.tiles[map.tiles.size() - 64 + (y & 7) * 8 + (x & 7)] = value;
}
void configuration() {
    auto defaults = default_configuration();
    check(defaults[0] == 0, "Initializer version");
    auto serialized = serialize_configuration(defaults);
    check(serialized.size() == 84 && u32(serialized, 0) == 1, "Configuration format");
    check(u32(serialized, 24) == 'F' && u32(serialized, 28) == 'J' && u32(serialized, 60) == 22050, "Known default fields");
    auto parsed = parse_configuration(serialized);
    defaults[0] = 1;
    check(parsed == defaults, "Configuration round trip");
    serialized[0] = 2;
    rejects([&] { parse_configuration(serialized); });
    rejects([] { parse_configuration(Bytes(83)); });
}
void scores() {
    // All-zero payload captured from WGAME 0x41ba34, not from the native encoder.
    auto original =
        hex("010000004f000000c7dbf0b6b4abbdaae8f0e9c2a0defceaa9eacba1e8eff5aca1f9d8b7a6aaadadcdedbebff5e9a5f5b2e6c7dbf0b6b4abbdaae8f0e9c2a0defceaa9eacba1e8eff5aca1f9d8b7a6aaadadcdedbebff5e9a5f5b2e6c7"
            "dbf0b6b4abbdaae8f0e9c2a0defceaa9eacba1e8eff5aca1f9d8b7a6aaadadcdedbebff5e9a5f5b2e6c7dbf0b6b4abbdaae8f0e9c2a0defceaa9eacba1e8eff5aca1f9d8b7a6aa");
    check(serialize_scores(Scores{}) == original, "Original zero score fixture");
    check(parse_scores(original).payload == Scores{}.payload, "Original score reader fixture");
    Scores value;
    for (std::size_t i = 0; i < value.payload.size(); i++)
        value.payload[i] = std::uint8_t(i);
    auto encoded = serialize_scores(value);
    check(parse_scores(encoded).payload == value.payload, "Scores round trip");
    encoded[50] ^= 1;
    rejects([&] { parse_scores(encoded); });
    original[0] = 2;
    rejects([&] { parse_scores(original); });
    rejects([] { parse_scores(Bytes(163)); });
}
void nudge() {
    NudgeController controller;
    check(controller.step(0) == 0, "Idle tilt event");
    check(controller.state().x == 0 && controller.state().y == 0 && controller.state().axis_stop == 1, "Idle clamp");
    controller.step(0x100);
    check(controller.state().vx == 192 && controller.state().x == 192, "Side impulse");
    controller.reset();
    controller.step(0x300);
    check(controller.state().up_held == 1 && controller.state().positive_held == 0 && controller.state().negative_held == 0, "Combined upward nudge");
    unsigned events = 0;
    controller.reset();
    for (unsigned i = 0; i < 512; i++)
        events += controller.step(0x20);
    check(events == 1 && controller.state().tilt == 256 && controller.state().y == 0x30000, "Held nudge threshold and saturation");
    for (unsigned i = 0; i < 64; i++)
        controller.step(0);
    check(controller.state().tilt == 255, "Idle tilt decay");
    controller.reset();
    check(controller.state().decay_tick == 0 && controller.state().tilt == 0, "Reset nudge");
    rejects([] { nudge_trace(Bytes{1}); });
}
void tiles_and_patches() {
    auto map = uniform();
    map.tiles.resize(128);
    map.cells[0] = 1;
    map.tiles[64 + 63] = 0x12;
    map.tiles[0] = 0xab;
    check(map.sample(7, 7) == 0x12 && map.sample(8, 0) == 0xab, "Tile and intra-tile selection");
    map.cells[0] = 0;
    map.tiles[63] = 0xcd;
    map.tiles[64] = 0xef;
    check(map.sample_word(7, 7) == 0xefcd, "Word crosses tile bank boundary");
    rejects([&] { map.sample(-1, 0); });
    rejects([&] { map.sample(320, 0); });
    rejects([&] { map.sample(0, 616); });
    // A 1x1 ZMP at map cell (1,1), one frame referencing tile 1.
    Container patch;
    patch.origin = 16;
    patch.bytes = hex("01000000010000000100000001000000080000000a0000000100");
    patch.offsets = {8, 10};
    map.patch(patch, 0);
    check(map.cells[41] == 1, "ZMP rectangle destination");
    rejects([&] { map.patch(patch, 1); });
}
void sprites() {
    Container uninitialized;
    check(uninitialized.size() == 0, "Default container has no frames");
    rejects([&] { uninitialized.block(0); });
    // Two indexed spans, second X = prior X + count + signed deltaX.
    Container bank;
    bank.bytes = hex("ffff02000200070901feff010003ff");
    bank.offsets = {0, std::uint32_t(bank.bytes.size())};
    auto spans = decode_sprite(bank, 0, 1);
    check(spans.size() == 2 && spans[0].x == -1 && spans[0].y == 2 && spans[1].x == -1 && spans[1].y == 3, "Sprite signed coordinates and deltas");
    Bitmap target;
    target.width = 2;
    target.height = 4;
    target.pixels.resize(8);
    target.palette[9] = 0x123456;
    draw_sprite(target, bank, 0, 1);
    check(target.pixels[4] == 0x123456, "Sprite clipping");
    Container mask;
    mask.bytes = hex("000000000200ff");
    mask.offsets = {0, 7};
    check(decode_sprite(mask, 0, 0)[0].pixels.size() == 2, "Geometry span carries no pixels");
    rejects([&] { draw_sprite(target, mask, 0, 0); });
    Container empty;
    empty.bytes.resize(6);
    empty.offsets = {0, 6};
    check(decode_sprite(empty, 0, 1).empty(), "Empty sprite");
    bank.bytes.pop_back();
    bank.offsets[1]--;
    rejects([&] { decode_sprite(bank, 0, 1); });
    rejects([] { u16(Bytes{0}, 0); });
    rejects([] { u32(Bytes(3), 0); });
}
void ball_and_integer() {
    Bytes record(157, 0xa5);
    auto ball = web::compatibility::read_ball(record);
    ball.position({-1, 0x12345678});
    ball.velocity({-32768, 32767});
    ball.layer(2);
    check(ball.position() == web::physics::Position2{-1, 0x12345678} && ball.velocity() == Vector2{-32768, 32767}, "Packed ball fields");
    check(ball.layer() == 2 && web::compatibility::encode(ball)[0x13] == 0xa5 && web::compatibility::encode(ball)[156] == 0xa5, "Preserve unmapped ball bytes");
    rejects([] { (void)web::compatibility::read_ball(Bytes(156)); });
    check(integer::add(0x7fffffff, 1) == (-2147483647 - 1), "x86 addition overflow");
    check(integer::mul(0x40000000, 4) == 0, "x86 multiplication overflow");
    check(integer::sub(-2147483647 - 1, 1) == 0x7fffffff, "x86 subtraction overflow");
}
void classification() {
    for (unsigned sample = 0; sample < 256; sample++) {
        if (sample >= 0x10 && sample < 0x18)
            continue;
        CollisionClassifier classifier(CollisionGeometry({uniform(std::uint8_t(sample)), uniform(0x40), uniform(0x80)}));
        BallState ball;
        ball.position({100 << 16, 200 << 16});
        const auto expected = sample >= 0x40 && sample < 0xc0 ? int((sample & 63) * 4 + (sample > 127 ? 256 : 0)) : -1;
        check(classifier.classify(ball) == expected, "Collision sample classification");
        check(ball.position() == web::physics::Position2{100 << 16, 200 << 16}, "No repair for terminal cell");
        ball.layer(1);
        check(classifier.classify(ball) == 0, "Layer one selection");
        ball.layer(2);
        check(classifier.classify(ball) == 256, "Layer two selection");
    }
    constexpr std::array<Vector2, 8> directions = {
        Vector2{0,  -1},
        {-1, -1},
        {-1, 0 },
        {-1, 1 },
        {0,  1 },
        {1,  1 },
        {1,  0 },
        {1,  -1}
    };
    for (unsigned direction = 0; direction < 8; direction++) {
        auto map = uniform();
        cell(map, 100, 200, std::uint8_t(0x10 + direction));
        CollisionClassifier classifier(CollisionGeometry({map, uniform(), uniform()}));
        BallState ball;
        ball.position({(100 << 16) + 123, (200 << 16) + 456});
        check(classifier.classify(ball) == -1, "Repair finds empty neighbor");
        auto actual = ball.position();
        auto delta = directions[direction];
        check(actual.x == (delta.x == -1 ? (100 << 16) - 1 : delta.x == 1 ? 101 << 16 : (100 << 16) + 123), "Repair X fixed point boundary");
        check(actual.y == (delta.y == -1 ? (200 << 16) - 1 : delta.y == 1 ? 201 << 16 : (200 << 16) + 456), "Repair Y fixed point boundary");
    }
    CollisionClassifier classifier(CollisionGeometry({uniform(), uniform(), uniform()}));
    BallState invalid;
    invalid.layer(3);
    rejects([&] { classifier.classify(invalid); });
    invalid.layer(0);
    invalid.position({-65536, 0});
    rejects([&] { classifier.classify(invalid); });
    auto cycle = uniform();
    cell(cycle, 100, 200, 0x14);
    cell(cycle, 100, 201, 0x10);
    CollisionClassifier cyclic(CollisionGeometry({cycle, uniform(), uniform()}));
    BallState trapped;
    trapped.position({100 << 16, 200 << 16});
    rejects([&] { cyclic.classify(trapped); });
}
void reflection() {
    check(AngleLookup::direction({0, 1}) == 0 && AngleLookup::direction({1, 0}) == 0x4000, "Angle axes");
    check(AngleLookup::direction({0, -1}) == 0x8000 && AngleLookup::direction({-1, 0}) == 0xc000, "Angle negative axes");
    check(AngleLookup::direction({0, 0}) == 0x2000 && AngleLookup::direction({-1, 1}) == 0xe000, "Angle diagonal and zero");
    rejects([] { AngleLookup::direction({262136, 262137}); }); // Original reads past its allocation here.
    rejects([] { AngleLookup::direction({-2147483647 - 1, 0}); });
    CollisionResponse::Coefficients low{}, high{};
    for (unsigned i = 0; i < 64; i++) {
        low[i * 4] = 16384;
        low[i * 4 + 3] = -16384;
        high[i * 4] = 8192;
        high[i * 4 + 3] = -8192;
    }
    CollisionResponse response(low, high);
    BallState ball;
    ball.velocity({3, 7});
    response.reflect(ball, 0);
    check(ball.velocity() == Vector2{1, -3}, "Q15 truncates toward zero");
    ball.velocity({3, 7});
    response.reflect(ball, 0x200);
    check(ball.velocity() == Vector2{0, -1}, "Flag 0x200 selects high table");
    ball.velocity({-5, 7});
    response.reflect(ball, 0x100);
    check(ball.velocity() == Vector2{-1, -1}, "Flag 0x100 selects high table");
    ball.velocity({32768, 65535});
    response.reflect(ball, 0);
    check(ball.velocity() == Vector2{16384, -32766}, "Large velocity predivision");
    ball.velocity({7, -8});
    response.reflect(ball, 0, {5, 3});
    check(ball.velocity() == Vector2{7, -8}, "Outgoing contact skips bounce and restores nudge");
}
struct TestEvents : PhysicsEventSink {
    std::vector<PhysicsEvent> events;
    bool change_surface = false;
    void on_event(const PhysicsEvent& event, BallSlots& balls) override {
        events.push_back(event);
        if (change_surface && event.kind() == PhysicsEvent::Kind::surface_rule)
            balls.at(event.slot).layer(2);
    }
};
void motion_modes() {
    BallSlots balls;
    auto& ball = balls[0];
    ball.active(1);
    ball.motion_type(web::physics::ball_type::free);
    ball.position({100, 200});
    ball.velocity({10, 20});
    ball.acceleration({3, -4});
    ball.z(99);
    RandomGenerator random;
    TestEvents events;
    PhysicsServices services{random, events};
    MotionState context;
    context.nudge = {5, 6};
    BallMotion motion;
    motion.step(balls, 0, context, services);
    check(ball.previous_position() == web::physics::Position2{100, 200} && ball.velocity() == Vector2{13, 0}, "Free motion stores previous position and gravity/acceleration");
    check(ball.position() == web::physics::Position2{118, 194} && ball.z() == 0, "Free motion uses signed nudge axes");
    ball.motion_mode(web::physics::motion_mode::draining);
    ball.transition_kind(2);
    ball.vertical_velocity(32);
    ball.z(-0x140000);
    motion.step(balls, 0, context, services);
    check(events.events.empty(), "Drain threshold is strict");
    ball.z(-0x140001);
    ball.vertical_velocity(32);
    motion.step(balls, 0, context, services);
    check(events.events.size() == 1 && events.events[0].kind() == PhysicsEvent::Kind::drain, "Drain callback boundary");
    ball.motion_mode(web::physics::motion_mode::held);
    ball.z(999);
    motion.step(balls, 0, context, services);
    check(ball.z() == 0, "Mode two clears Z");
    ball.motion_mode(web::physics::motion_mode::lift);
    ball.z(0x228000);
    motion.step(balls, 0, context, services);
    check(ball.motion_mode() == web::physics::motion_mode::lift && random.state() == 1, "Lift release threshold is strict");
    motion.step(balls, 0, context, services);
    check(ball.motion_mode() == web::physics::motion_mode::table && ball.layer() == 2 && ball.position().x == 0xd90000, "Lift release sets layer and position");
    check(ball.velocity() == Vector2{std::int32_t(16838 & 1023) - 19000, 2000}, "Lift release uses original RNG");
    ball.motion_mode(web::physics::motion_mode::falling);
    ball.surface_height(100);
    ball.falling_height(26);
    ball.falling_velocity(0);
    motion.step(balls, 0, context, services);
    check(ball.motion_mode() == web::physics::motion_mode::falling && ball.falling_height() == 0, "Fall landing threshold is strict");
    motion.step(balls, 0, context, services);
    check(ball.motion_mode() == web::physics::motion_mode::table && ball.layer() == 0 && ball.surface_height() == 0, "Fall landing resets surface mode");
    check(web::compatibility::parameters(events.events.back()) == std::array<std::int32_t, 4>{188, 64, 64, 15}, "Landing sound raw arguments");
    auto before = web::compatibility::encode(ball);
    ball.active(0);
    before[0x8c] = 0;
    motion.step(balls, 0, context, services);
    check(std::equal(before.begin(), before.end(), web::compatibility::encode(ball).begin()), "Inactive motion gate");
    rejects([&] { motion.step(balls, 6, context, services); });
}
void controlled_motion_and_force() {
    BallSlots balls;
    RandomGenerator random;
    TestEvents events;
    MotionState context;
    BallMotion motion;
    balls[5].position({100 * 65536 + 4096, 200 * 65536 + 4096});
    balls[5].velocity({100, -100});
    context.control_enabled = 1;
    context.actions = 64;
    motion.step(balls, 0, context, {random, events});
    check(context.control_target == Vector2{100, 200} && context.control_latched == 1, "Control latches sixth-ball integer position");
    check(balls[5].velocity() == Vector2{91, -95} && balls[0].velocity() == Vector2{}, "Control changes slot five while current ball inactive");
    context.actions = 0;
    motion.step(balls, 0, context, {random, events});
    check(context.control_latched == 0, "Control release clears latch");
    balls[0].position({55 * 65536, 329 * 65536});
    context.force_timer = 1;
    motion.step(balls, 0, context, {random, events});
    check(balls[0].velocity() == Vector2{10, 10}, "Original center force includes zero-vector diagonal");
    balls[0].velocity({100, -100});
    context.force_timer = 0x18000;
    motion.step(balls, 0, context, {random, events});
    check(balls[0].velocity() == Vector2{100, -100}, "Force timer upper boundary");
    RandomGenerator golden;
    for (auto expected : std::array<unsigned, 8>{16838, 5758, 10113, 17515, 31051, 5627, 23010, 7419})
        check(golden.next() == expected, "Original Watcom RNG fixture");
    check(golden.state() == 0x9cfbae39, "Original Watcom RNG final state");
    check(AngleLookup::sine(0) == 0 && AngleLookup::cosine(0) == 32767, "Recovered sine axes");
}
void surface_and_callbacks() {
    auto flat = uniform(128);
    auto left = flat;
    cell(left, 97, 200, 120);
    cell(left, 103, 200, 136);
    cell(left, 100, 197, 136);
    cell(left, 100, 203, 120);
    auto collision = uniform();
    cell(collision, 100, 200, 15);
    auto switches = uniform();
    cell(switches, 100, 200, 224);
    BallSurface surface(SurfaceGeometry(CollisionGeometry({uniform(), collision, uniform()}), {left, flat}, {uniform(), switches}));
    BallSlots balls;
    auto& ball = balls[0];
    ball.position({100 * 65536, 200 * 65536});
    ball.layer(1);
    RandomGenerator random;
    TestEvents events;
    surface.step(balls, 0, {random, events});
    check(ball.acceleration() == Vector2{22, -22}, "Layer-one gradients clamp to seven before scaling");
    check(ball.surface_height() == 127 * 0x6d0000 / 2550, "Layer-one height scale");
    check(ball.motion_mode() == web::physics::motion_mode::falling && ball.falling_velocity() == -328 && ball.falling_height() == ball.surface_height(), "Surface drop initializes falling state");
    check(events.events.size() == 2 && events.events[0].kind() == PhysicsEvent::Kind::surface_rule && events.events[1].kind() == PhysicsEvent::Kind::sound, "Switch precedes drop sound");
    auto consumed = random.state();
    surface.step(balls, 0, {random, events});
    check(random.state() == consumed, "Existing mode-four drop consumes no random values");
    BallSlots changed;
    changed[0].position({100 * 65536, 200 * 65536});
    changed[0].layer(1);
    RandomGenerator unchanged;
    TestEvents mutations;
    mutations.change_surface = true;
    surface.step(changed, 0, {unchanged, mutations});
    check(changed[0].layer() == 0 && changed[0].motion_mode() == web::physics::motion_mode::table && unchanged.state() == 1, "Surface honors synchronous rule changes before transition");
    check(web::compatibility::parameters(mutations.events.back()) == std::array<std::int32_t, 4>{188, -64, 32, 15}, "Changed layer selects layer-two sound path");
    ball.layer(3);
    rejects([&] { surface.step(balls, 0, {random, events}); });
    ball.layer(1);
    ball.position({0, 200 * 65536});
    rejects([&] { surface.step(balls, 0, {random, events}); });
}
void composed_ball_physics() {
    CollisionResponse::Coefficients coefficients{};
    coefficients[3] = -16384;
    BallPhysics physics(
        CollisionClassifier(CollisionGeometry({uniform(0x40), uniform(), uniform()})),
        CollisionResponse(coefficients, coefficients),
        BallSurface(SurfaceGeometry(CollisionGeometry({uniform(0x40), uniform(), uniform()}), {uniform(128), uniform(128)}, {uniform(), uniform()})));
    BallSlots balls;
    auto& ball = balls[0];
    ball.active(1);
    ball.motion_type(web::physics::ball_type::free);
    ball.position({100 * 65536, 200 * 65536});
    ball.velocity({0, 100});
    ball.acceleration({0, 0});
    MotionState context;
    RandomGenerator random;
    TestEvents events;
    physics.step(balls, 0, context, {random, events});
    check(ball.position().y == 200 * 65536 + 84 && ball.velocity().y == -42, "Pipeline moves before classification/reflection");
    check(ball.acceleration() == Vector2{0, 0} && ball.surface_height() == 0, "Pipeline applies surface after reflection");
    ball.active(0);
    auto previous = web::compatibility::encode(ball);
    physics.step(balls, 0, context, {random, events});
    check(std::equal(previous.begin(), previous.end(), web::compatibility::encode(ball).begin()), "Pipeline skips inactive slots");
}
void mechanism_state_and_drive() {
    using F = web::compatibility::FlipperField;
    auto state = MechanismState::defaults();
    auto encoded = web::compatibility::encode(state);
    check(encoded.size() == 496 && web::compatibility::encode(web::compatibility::read_mechanisms(encoded)) == encoded, "Mechanism packed-state round trip");
    check(
        web::compatibility::get(state.flippers[0], F::angle) == 21845 && web::compatibility::get(state.flippers[1], F::angle) == 43690 && web::compatibility::get(state.flippers[2], F::angle) == 29127,
        "Original flipper startup angles");
    check(web::compatibility::get(state.spinners[0], web::compatibility::SpinnerField::phase) == 0 && state.enabled == 0, "Original spinner and enable defaults");
    rejects([] { web::compatibility::read_mechanisms(Bytes(495)); });
    rejects([] { (void)web::compatibility::read_flipper(Bytes(147)); });
    rejects([] { (void)web::compatibility::read_spinner(Bytes(23)); });
    BallSlots balls;
    RandomGenerator random;
    TestEvents events;
    FlipperDrive drive;
    state.enabled = 1;
    auto& left = state.flippers[0];
    web::compatibility::set(left, F::rest_angle, 2000);
    web::compatibility::set(left, F::powered_angle, 0);
    web::compatibility::set(left, F::angle, 304);
    drive.step(state, balls, 1, {random, events});
    check(web::compatibility::get(left, F::angle) == 0 && web::compatibility::get(left, F::drive) == 1 && events.events.empty(), "Negative flipper endpoint comparison is strict");
    drive.step(state, balls, 1, {random, events});
    check(web::compatibility::get(left, F::drive) == 0 && web::compatibility::get(left, F::powered_latch) == 1, "Negative flipper clamps after crossing endpoint");
    check(events.events.size() == 1 && web::compatibility::parameters(events.events[0]) == std::array<std::int32_t, 4>{154, -64, 0, 4}, "Left flipper sound raw arguments");
    check(events.events[0].slot == PhysicsEvent::no_ball, "Mechanism callback has no source ball");
    drive.step(state, balls, 1, {random, events});
    check(events.events.size() == 1, "Held flipper sound is latched");
    state.enabled = 0;
    drive.step(state, balls, 1, {random, events});
    check(
        web::compatibility::get(left, F::angle) == 128 && web::compatibility::get(left, F::drive) == -1 && web::compatibility::get(left, F::powered_latch) == 0,
        "Disabled drive returns even while button held");
    state = MechanismState::defaults();
    state.enabled = 1;
    events.events.clear();
    auto& right = state.flippers[1];
    web::compatibility::set(right, F::rest_angle, 0);
    web::compatibility::set(right, F::powered_angle, 2000);
    web::compatibility::set(right, F::angle, 1696);
    drive.step(state, balls, 2, {random, events});
    check(web::compatibility::get(right, F::angle) == 2000 && web::compatibility::get(right, F::drive) == 0, "Positive flipper endpoint comparison is inclusive");
    check(events.events.size() == 1 && web::compatibility::parameters(events.events[0]) == std::array<std::int32_t, 4>{154, 64, 0, 5}, "Right flipper sound raw arguments");
}
void spinner_dynamics() {
    using F = web::compatibility::SpinnerField;
    auto state = MechanismState::defaults();
    SpinnerDynamics dynamics;
    BallSlots balls;
    RandomGenerator random;
    TestEvents events;
    dynamics.kick(state, 0, -3);
    check(web::compatibility::get(state.spinners[0], F::velocity) == -3 && web::compatibility::get(state.spinners[0], F::phase) == 0x40000, "Spinner kick sets speed and initial phase");
    dynamics.step(state, balls, {random, events});
    check(web::compatibility::get(state.spinners[0], F::velocity) == -1, "Spinner negative damping");
    dynamics.step(state, balls, {random, events});
    check(web::compatibility::get(state.spinners[0], F::velocity) == 0, "Spinner damping does not reverse sign");
    web::compatibility::set(state.spinners[0], F::phase, 1);
    web::compatibility::set(state.spinners[0], F::velocity, -4);
    web::compatibility::set(state.spinners[1], F::phase, 0x7ffff);
    web::compatibility::set(state.spinners[1], F::velocity, 4);
    dynamics.step(state, balls, {random, events});
    check(events.events.size() == 2 && events.events[0].kind() == PhysicsEvent::Kind::spinner_crossing, "Spinner detects signed phase crossings");
    check(web::compatibility::parameters(events.events[0])[0] == 0 && web::compatibility::parameters(events.events[1])[0] == 1, "Spinner callbacks preserve update order");
    check(web::compatibility::get(state.spinners[0], F::phase) == -1 && web::compatibility::get(state.spinners[1], F::phase) == 0x80001, "Spinner phases wrap through boundary");
    web::compatibility::set(state.spinners[0], F::phase, 2147483647);
    web::compatibility::set(state.spinners[0], F::velocity, 3);
    dynamics.step(state, balls, {random, events});
    check(web::compatibility::get(state.spinners[0], F::phase) == (-2147483647 - 1), "Spinner phase uses x86 overflow");
    rejects([&] { dynamics.kick(state, 2, 1); });
}
void ball_contacts() {
    BallInteractions interactions;
    BallSlots balls;
    auto& a = balls[0];
    auto& b = balls[1];
    a.position({-65536, 0});
    b.position({0, 0});
    a.velocity({20000, 0});
    b.velocity({-20000, 0});
    check(interactions.resolve(balls, 0, 1) == 39856, "Original quantized impact return value");
    check(a.velocity() == Vector2{-19928, 0} && b.velocity() == Vector2{19928, 0}, "Original head-on ball impulse");
    a.velocity({0, 0});
    b.velocity({0, 0});
    interactions.resolve(balls, 0, 1);
    check(a.velocity() == Vector2{-63, 0} && b.velocity() == Vector2{63, 0}, "Original minimum impulse quirk");
    a.velocity({20000, 0});
    b.velocity({-20000, 0});
    a.active(1);
    b.active(1);
    a.motion_type(web::physics::ball_type::free);
    b.motion_type(web::physics::ball_type::free);
    b.layer(1);
    RandomGenerator random;
    TestEvents events;
    interactions.step(balls, {random, events});
    check(a.velocity() == Vector2{20000, 0} && events.events.empty(), "Ball broad phase rejects differing layers");
    b.layer(0);
    b.motion_type(web::physics::ball_type::pending_drain);
    interactions.step(balls, {random, events});
    check(events.events.empty(), "Ball broad phase excludes type seven");
    b.motion_type(web::physics::ball_type::free);
    interactions.step(balls, {random, events});
    check(events.events.size() == 1 && web::compatibility::parameters(events.events[0]) == std::array<std::int32_t, 4>{155, 0, 1, 16}, "Ball-impact audio arguments");
    a.velocity({20000, 0});
    b.velocity({-20000, 0});
    a.position({14 * 65536, 0});
    events.events.clear();
    a.contact_retry(9);
    interactions.step(balls, {random, events});
    check(a.velocity() == Vector2{20000, 0} && events.events.empty() && a.contact_retry() == 0, "Strict radius bound and retry reset");
    rejects([&] { interactions.resolve(balls, 0, 0); });
    rejects([&] { interactions.resolve(balls, 0, 6); });
    a.velocity({2147483647, 2147483647});
    b.velocity({0, 0});
    rejects([&] { interactions.resolve(balls, 0, 1); });
    // Original hardware-x87 CRT setting (0x127f), including the sqrt case that
    // exposed the oracle's former uninitialized single-precision environment.
    balls[5].position({-98339, -725988});
    balls[5].velocity({-11662, 10556});
    balls[2].position({741788, -347077});
    balls[2].velocity({-42417, -13631});
    check(interactions.resolve(balls, 5, 2) == 42294, "Original double-precision magnitude fixture");
    check(balls[5].velocity() == Vector2{-45156, -4568} && balls[2].velocity() == Vector2{-8923, 1493}, "Original double-precision impact fixture");
    check(AngleLookup::direction({5513, 8192}) == 6178 && AngleLookup::direction({5599, 8192}) == 6253, "Angle constants use initialized x87 precision");
    check(AngleLookup::sine(941 * 8) == 21649 && AngleLookup::sine(3652 * 8) == 10945, "Sine constants use initialized x87 precision");
}
void flipper_contacts() {
    using F = web::compatibility::FlipperField;
    CollisionResponse::Coefficients coefficients{};
    FlipperContacts contacts(CollisionResponse(coefficients, coefficients));
    auto mechanisms = MechanismState::defaults();
    auto& flipper = mechanisms.flippers[0];
    for (auto [field, value] : std::initializer_list<std::pair<F, int>>{
             {F::pivot_x,                     0          },
             {F::pivot_y,                     0          },
             {F::angle,                       0x4000     },
             {F::world_x_min,                 -10000     },
             {F::world_x_max,                 10000      },
             {F::world_y_min,                 -10000     },
             {F::world_y_max,                 10000      },
             {F::local_x_min,                 -3000      },
             {F::local_x_max,                 7000       },
             {F::local_y_min,                 -3000      },
             {F::local_y_max,                 3000       },
             {F::length,                      4000       },
             {F::slope,                       0          },
             {F::intercept,                   5000       },
             {F::base_radius,                 1000       },
             {F::tip_radius,                  500        },
             {F::base_contact_radius_squared, 2800 * 2800},
             {F::tip_contact_radius_squared,  2300 * 2300},
             {F::lower_normal,                0x8000     },
             {F::upper_normal,                0          },
             {F::normal_offset_x,             0          },
             {F::normal_offset_y,             1800       },
             {F::drive,                       0          }
    })
        web::compatibility::set(flipper, field, value);
    BallState ball;
    ball.position({256000, -256000});
    FlipperContacts::transform(ball, {0, 0}, 0x4000);
    check(ball.local_position() == web::physics::Position2{999, -999}, "Flipper transform preserves Q15 truncation on both signs");
    FlipperContactState state{99, 88, 77, 66, 55, 44};
    ball.position({10000 * 256, 0});
    check(contacts.classify(ball, flipper, state) == -1 && ball.local_position() == web::physics::Position2{999, -999}, "Strict world-boundary miss leaves local coordinates unchanged");
    check(state.angle == 99 && state.surface_y == 88 && state.surface_x == 77 && state.flag == 0 && state.tip_angle == 0x8000 && state.probe == 44, "Miss resets only the original scratch fields");
    ball.position({1000 * 256, 1000 * 256});
    check(contacts.classify(ball, flipper, state) == 1 && state.angle == 0x8000 && state.surface_x == 999 && state.surface_y == -801, "Lower side contact geometry");
    ball.position({1000 * 256, -1000 * 256});
    check(contacts.classify(ball, flipper, state) == 1 && state.angle == 0 && state.surface_y == 801, "Upper side contact geometry");
    ball.position({-1000 * 256, 0});
    check(contacts.classify(ball, flipper, state) == 1 && state.angle == 0x14000 && state.surface_x == -999 && state.tip_angle == 0x8000, "Base cap retains the unmasked contact angle");
    ball.position({5000 * 256, 0});
    check(contacts.classify(ball, flipper, state) == 1 && state.angle == 0xc000 && state.surface_x == 4499 && state.tip_angle == 0xc000, "Tip cap stores attenuation angle separately");
    state.angle = 0x8000;
    ball.velocity({0, 0});
    auto y = ball.position().y;
    contacts.reflect(ball, flipper, state);
    check(ball.position().y == y + 16, "Stationary correction is sixteen raw units");
    state.angle = 0x5000;
    y = ball.position().y;
    contacts.reflect(ball, flipper, state);
    check(ball.position().y == y, "Stationary correction excludes its angle endpoint");
    web::compatibility::set(flipper, F::drive, 1);
    web::compatibility::set(flipper, F::direction, -1);
    ball.local_position({4000, 1});
    state = {0x8000, 0, 4096, 0, 0xc000, 0};
    ball.velocity({0, 0});
    contacts.reflect(ball, flipper, state);
    check(ball.velocity() == Vector2{0, 0} && state.tip_angle == 0, "Tip angle attenuation can suppress the powered minimum");
    state = {0x8000, 0, 4096, 0, 0x8000, 0};
    ball.velocity({0, 0});
    contacts.reflect(ball, flipper, state);
    check(ball.velocity() == Vector2{0, 30557}, "Powered impulse uses the original double constants and quantized basis");
    state.surface_x = 50000;
    rejects([&] { contacts.reflect(ball, flipper, state); });
}
PhysicsSimulation flat_simulation() {
    CollisionResponse::Coefficients coefficients{};
    return PhysicsSimulation(
        BallPhysics(
            CollisionClassifier(CollisionGeometry({uniform(), uniform(), uniform()})),
            CollisionResponse(coefficients, coefficients),
            BallSurface(SurfaceGeometry(CollisionGeometry({uniform(), uniform(), uniform()}), {uniform(128), uniform(128)}, {uniform(), uniform()}))),
        FlipperContacts(CollisionResponse(coefficients, coefficients)));
}
void simulation_order_and_schedule() {
    using F = web::compatibility::FlipperField;
    using S = web::compatibility::SpinnerField;
    check(PhysicsSimulation::microstep_count(60, 0) == 17 && PhysicsSimulation::microstep_count(60, 1) == 34, "Original frame scheduling truncates before doubling");
    check(PhysicsSimulation::microstep_count(144, 0) == 7 && PhysicsSimulation::microstep_count(1051, 1) == 0, "High refresh rates preserve original integer scheduling");
    check(PhysicsSimulation::microstep_count(-1, 1) == 0, "Negative schedules perform no microsteps");
    rejects([] { PhysicsSimulation::microstep_count(0, 0); });
    auto simulation = flat_simulation();
    SimulationState state;
    TestEvents events;
    state.refresh_rate = 1050;
    state.actions = 0x22;
    state.nudge.tilt = 255;
    state.mechanisms.enabled = 1;
    auto& right = state.mechanisms.flippers[1];
    web::compatibility::set(right, F::rest_angle, 0);
    web::compatibility::set(right, F::powered_angle, 2000);
    web::compatibility::set(right, F::angle, 1696);
    web::compatibility::set(state.mechanisms.spinners[0], S::phase, 1);
    web::compatibility::set(state.mechanisms.spinners[0], S::velocity, -4);
    for (std::size_t slot = 0; slot < 2; slot++) {
        auto& ball = state.balls[slot];
        ball.active(1);
        ball.motion_type(web::physics::ball_type::free);
        ball.position({int(100 + slot) * 65536, 200 * 65536});
        ball.velocity({slot ? -20000 : 20000, 0});
    }
    check(simulation.step(state, events) == 1, "Full simulation returns its executed microstep count");
    check(
        events.events.size() == 4 && events.events[0].kind() == PhysicsEvent::Kind::tilt && web::compatibility::parameters(events.events[1])[0] == 154 &&
            events.events[2].kind() == PhysicsEvent::Kind::spinner_crossing && web::compatibility::parameters(events.events[3])[0] == 155,
        "Whole tick preserves tilt, drive, spinner and pair callback order");
    check(state.nudge.decay_tick == 1 && state.contact.probe == 0 && state.contact.tip_angle == 0x8000, "Full tick advances nudge and flipper contact scratch");
    auto replay = state;
    state.actions = 3;
    replay.actions = 3;
    events.events.clear();
    TestEvents repeated;
    for (unsigned i = 0; i < 8; i++) {
        simulation.step(state, events);
        simulation.step(replay, repeated);
    }
    for (std::size_t i = 0; i < 6; i++)
        check(std::ranges::equal(web::compatibility::encode(state.balls[i]), web::compatibility::encode(replay.balls[i])), "Copied simulation state replays identical ball records");
    check(
        web::compatibility::encode(state.mechanisms) == web::compatibility::encode(replay.mechanisms) && state.random.state() == replay.random.state() &&
            events.events.size() == repeated.events.size(),
        "Replay preserves mechanisms, RNG and event count");
}
void synchronous_tilt_boundary() {
    class TiltMutation : public PhysicsEventSink {
    public:
        SimulationState& state;
        bool seen = false;
        explicit TiltMutation(SimulationState& value) :
            state(value) {}
        void on_event(const PhysicsEvent& event, BallSlots&) override {
            if (event.kind() != PhysicsEvent::Kind::tilt)
                return;
            check(state.nudge.tilt == 256 && state.nudge.vy == 200 && state.nudge.y == 20, "Tilt callback sees state before velocity integration");
            state.nudge.vy = 7;
            state.nudge.tilt = 0;
            state.mechanisms.enabled = 0;
            seen = true;
        }
    };
    SimulationState state;
    state.refresh_rate = 1050;
    state.actions = 0x20;
    state.nudge.tilt = 255;
    state.nudge.vy = 200;
    state.nudge.y = 20;
    TiltMutation events(state);
    auto simulation = flat_simulation();
    simulation.step(state, events);
    check(events.seen && state.nudge.vy == 199 && state.nudge.y == 219 && state.nudge.tilt == 0, "Simulation resumes from synchronous tilt mutations");
    state.refresh_rate = 1051;
    auto previous = state.nudge.decay_tick;
    simulation.step(state, events);
    check(state.nudge.decay_tick == previous, "Zero microstep count leaves physics state unchanged");
}
struct RuleEffects : web::rules::BallRuleEffectSink {
    std::vector<web::rules::BallRuleEffect> effects;
    void on_effect(const web::rules::BallRuleEffect& event, SimulationState&, web::rules::TurnState&) override {
        effects.push_back(event);
    }
};
void rule_state_and_switches() {
    using R = web::rules::TurnState;
    using F = web::compatibility::TurnField;
    web::rules::PlayerRoster rule_players;
    R rules(rule_players);
    web::compatibility::set(rules, F::returns_paused, 17);
    web::compatibility::set(rules, F::launch_enabled, 1);
    rules.player_relaunch(3, 99);
    rules.frame_phase(0x10);
    web::rules::PlayerRoster decoded_players;
    auto decoded = web::compatibility::read_turn(decoded_players, web::compatibility::encode(rules));
    check(
        web::compatibility::get(decoded, F::returns_paused) == 17 && decoded.player_relaunch(3) == 99 && decoded.frame_phase() == 0x10,
        "Packed rule state preserves both sides of excluded physics timer");
    rules.switch_contact(-1);
    check(rules.switch_word(0) == 1, "Original negative switch aliases event zero");
    rules.switch_contact(64);
    check(rules.switch_word(29) == 1 && rules.switch_word(30) == 1, "Compound switch 64 activates both adjacent latches");
    rules.switch_contact(65);
    rules.switch_contact(66);
    rules.switch_contact(67);
    check(rules.switch_word(28) == 1 && rules.switch_word(25) == 1 && rules.switch_word(26) == 1 && rules.switch_word(27) == 1, "All compound switch fanouts");
    web::compatibility::set(rules, F::phase, 7);
    rules.switch_contact(58);
    check(web::compatibility::get(rules, F::phase) == 1, "Original switch storage alias with rule phase is preserved");
    auto previous = web::compatibility::encode(rules);
    rules.switch_contact(68);
    rules.switch_contact(2147483647);
    check(std::equal(previous.begin(), previous.end(), web::compatibility::encode(rules).begin()), "Unsupported positive switch ids do not write state");
    rejects([] {
        web::rules::PlayerRoster players;
        (void)web::compatibility::read_turn(players, Bytes(web::compatibility::turn_snapshot_bytes - 1));
    });
    rejects([&] { rules.switch_word(64); });
    rejects([&] { rules.player_relaunch(4); });
}
void rule_serve_launch_and_exits() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    SimulationState state;
    PlayerRoster rules_players;
    TurnState rules(rules_players);
    BallLifecycle lifecycle;
    RuleEffects effects;
    for (auto& ball : state.balls)
        ball.active(2);
    state.balls[1].active(0);
    state.balls[5].active(0);
    state.balls[5].motion_mode(web::physics::motion_mode::falling);
    state.balls[5].z(123);
    state.balls[5].rule_timer(77);
    lifecycle.serve(state, rules, effects);
    check(state.balls[1].active() == 0 && state.balls[5].active() == 1 && state.balls[5].motion_type() == web::physics::ball_type::launcher, "Serve selects last exactly-inactive slot");
    check(
        state.balls[5].position() == web::physics::Position2{294 * 65536, 108 * 65536} && state.balls[5].layer() == 0 && state.balls[5].velocity() == Vector2{0, 0},
        "Original serve placement and reset fields");
    check(state.balls[5].motion_mode() == web::physics::motion_mode::falling && state.balls[5].z() == 123 && state.balls[5].rule_timer() == 77, "Serve preserves fields the original leaves untouched");
    check(
        effects.effects.size() == 1 && effects.effects[0].slot == 5 && web::compatibility::parameters(effects.effects[0]) == std::array<std::int32_t, 4>{160, 64, 0, 15},
        "Serve sound source and arguments");
    for (auto& ball : state.balls)
        ball.active(1);
    lifecycle.serve(state, rules, effects);
    check(effects.effects.size() == 1, "No free ball slot produces no serve sound");
    state = SimulationState{};
    rules.reset();
    effects.effects.clear();
    web::compatibility::set(rules, F::launch_enabled, 1);
    for (auto slot : {1, 4}) {
        state.balls[slot].transition_kind(16);
        state.balls[slot].motion_type(web::physics::ball_type::launcher);
    }
    lifecycle.launch(state, rules, effects);
    check(state.balls[1].velocity().y == 50454 && state.balls[4].velocity().y == 51662, "Launch consumes shared RNG in ascending slot order");
    check(state.balls[1].active() == 0 && state.balls[1].motion_type() == web::physics::ball_type::free && effects.effects.size() == 2, "Original launch does not filter inactive candidates");
    check(
        web::compatibility::get(rules, F::launch_enabled) == 0 && web::compatibility::get(rules, F::launch_timer) == 0x24000 && web::compatibility::get(rules, F::save_timer) == 0x50000,
        "Second launch candidate overwrites first launch window");
    lifecycle.eject(state, rules, 2, BallLifecycle::Exit::right, effects);
    check(
        state.balls[2].position() == web::physics::Position2{269 * 65536, 313 * 65536} && state.balls[2].velocity() == Vector2{-1000, -9000} && state.balls[2].layer() == 1,
        "Right return placement and velocity");
    state.balls[2].z(99);
    state.balls[2].vertical_velocity(123);
    lifecycle.eject(state, rules, 2, BallLifecycle::Exit::lift, effects);
    check(
        state.balls[2].position() == web::physics::Position2{223 * 65536, 345 * 65536} && state.balls[2].motion_mode() == web::physics::motion_mode::lift && state.balls[2].z() == 99 &&
            state.balls[2].vertical_velocity() == 123,
        "Lift return changes only recovered fields");
    rejects([&] { lifecycle.auto_launch(state, rules, 6, effects); });
}
void rule_drain_and_timed_returns() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    SimulationState state;
    PlayerRoster rules_players;
    TurnState rules(rules_players);
    BallLifecycle lifecycle;
    RuleEffects effects;
    auto& ball = state.balls[4];
    ball.active(1);
    ball.motion_type(web::physics::ball_type::free);
    ball.motion_mode(web::physics::motion_mode::draining);
    web::compatibility::set(rules, F::drain_extension, 1);
    lifecycle.drained(state, rules, 4);
    check(
        ball.active() == 1 && ball.motion_type() == web::physics::ball_type::centre_return && ball.rule_timer() == 0x10000 && rules.switch_word(20) == 1,
        "Drain queues extended return without deactivating ball");
    web::compatibility::set(rules, F::elapsed, 0x10000);
    lifecycle.update_returns(state, rules, effects);
    check(
        ball.motion_type() == web::physics::ball_type::free && ball.motion_mode() == web::physics::motion_mode::table && ball.position() == web::physics::Position2{15 * 65536, 366 * 65536} &&
            web::compatibility::get(rules, F::return_toggle) == 1,
        "Expired return alternates exits and resumes motion");
    web::compatibility::set(rules, F::returns_paused, 1);
    ball.rule_timer(100);
    lifecycle.update_returns(state, rules, effects);
    check(ball.rule_timer() == 100, "Paused return clock preserves timer");
    web::compatibility::set(rules, F::returns_paused, 0);
    web::compatibility::set(rules, F::live_balls, 1);
    web::compatibility::set(rules, F::save_timer, 0);
    ball.rule_timer(0);
    ball.motion_type(web::physics::ball_type::pending_drain);
    lifecycle.update_returns(state, rules, effects);
    check(
        ball.active() == 0 && web::compatibility::get(rules, F::live_balls) == 0 && ball.motion_type() == web::physics::ball_type::pending_drain,
        "Completed unsaved drain decrements live count and retains type");
    ball.active(1);
    ball.motion_type(web::physics::ball_type::pending_drain);
    web::compatibility::set(rules, F::save_timer, 1);
    web::compatibility::set(rules, F::live_balls, 1);
    effects.effects.clear();
    lifecycle.update_returns(state, rules, effects);
    check(
        ball.active() == 0 && web::compatibility::get(rules, F::serve_requests) == 1 && web::compatibility::get(rules, F::live_balls) == 1,
        "Saved drain queues a new serve without decrementing live count");
    check(
        effects.effects.size() == 2 && effects.effects[0].kind() == BallRuleEffect::Kind::display && web::compatibility::parameters(effects.effects[0]) == std::array<std::int32_t, 4>{32, 1, 0, 0} &&
            web::compatibility::parameters(effects.effects[1])[0] == 43,
        "Saved drain requests display before option-dependent sound");
    state = SimulationState{};
    rules.reset();
    web::compatibility::set(rules, F::live_balls, 2);
    web::compatibility::set(rules, F::current_player, 2);
    rules.player_relaunch(2, 1);
    state.balls[5].active(2);
    state.balls[5].motion_type(web::physics::ball_type::station_capture);
    lifecycle.update_returns(state, rules, effects);
    check(
        state.balls[5].active() == 0 && state.balls[5].motion_type() == web::physics::ball_type::none && state.balls[0].active() == 1 && state.balls[1].active() == 1,
        "Type nine completion reactivates slots zero and one");
    check(web::compatibility::get(rules, F::serve_requests) == 2 && rules.player_relaunch(2) == 0, "Type nine consumes current-player relaunch flag");
}
void rule_surface_and_physics_callbacks() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    SimulationState state;
    state.refresh_rate = 1050;
    PlayerRoster rules_players;
    TurnState rules(rules_players);
    BallLifecycle lifecycle;
    RuleEffects effects;
    TestEvents remaining;
    BallRuleCallbacks callbacks(state, rules, effects, remaining);
    auto& ball = state.balls[0];
    ball.active(1);
    ball.motion_type(web::physics::ball_type::free);
    ball.position({100 * 65536, 200 * 65536});
    ball.velocity({-3, 5});
    lifecycle.surface_transition(state, rules, 0, 1, effects);
    check(ball.motion_mode() == web::physics::motion_mode::draining && ball.vertical_velocity() == -3, "Surface entry stores transition and signed half-speed vertical fall");
    ball.z(-0x3c0001);
    auto physics = flat_simulation();
    physics.step(state, callbacks);
    check(
        ball.motion_type() == web::physics::ball_type::upper_return && ball.rule_timer() == 0x6000 && rules.switch_word(31) == 1,
        "Recovered drain callback changes state inside complete physics tick");
    callbacks.on_event(web::physics::PhysicsEvent{0, web::physics::SwitchContact{64}}, state.balls);
    check(rules.switch_word(29) == 1 && rules.switch_word(30) == 1, "Physics switch request reaches compound rule latches");
    auto prior = ball.transition_kind();
    callbacks.on_event(web::physics::PhysicsEvent{PhysicsEvent::no_ball, web::physics::TiltContact{}}, state.balls);
    check(remaining.events.size() == 1 && remaining.events[0].kind() == PhysicsEvent::Kind::tilt && ball.transition_kind() == prior, "Unported tilt forwards without speculative effects");
    web::compatibility::set(rules, F::launch_enabled, 1);
    lifecycle.surface_transition(state, rules, 0, 16, effects);
    check(ball.transition_kind() == 16 && rules.switch_word(38) == 1, "Launcher-ready transition records its actual tag and switch latch");
    web::compatibility::set(rules, F::save_timer, 0);
    effects.effects.clear();
    lifecycle.surface_transition(state, rules, 0, 17, effects);
    check(
        ball.motion_type() == web::physics::ball_type::pending_drain && ball.motion_mode() == web::physics::motion_mode::held && ball.rule_timer() == 0x8000 && ball.z() == 0,
        "Drain surface prepares timed removal");
    check(effects.effects.size() == 2 && web::compatibility::parameters(effects.effects[1]) == std::array<std::int32_t, 4>{182, 0, 64, 18}, "Drain surface preserves unusual sound EBX argument");
    BallSlots detached;
    rejects([&] { callbacks.on_event(web::physics::PhysicsEvent{0, web::physics::DrainContact{}}, detached); });
}
void physical_surface_reactions() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    SimulationState state;
    PlayerRoster rules_players;
    TurnState rules(rules_players);
    BallLifecycle reactions;
    RuleEffects effects;
    auto& ball = state.balls[0];
    ball.position({100 * 65536, 200 * 65536});
    ball.velocity({2049, -10000});
    reactions.surface_transition(state, rules, 0, 6, effects);
    check(ball.velocity() == Vector2{18969, -4373}, "Sling keeps original double intermediates and single-precision kick stores");
    check(rules.switch_word(10) == 1 && web::compatibility::parameters(effects.effects.back()) == std::array<std::int32_t, 4>{162, -64, 0, 6}, "Positive sling latches after its original sound");
    state.random = RandomGenerator(1);
    ball.velocity({-2049, -10000});
    reactions.surface_transition(state, rules, 0, 7, effects);
    check(ball.velocity() == Vector2{-18969, -4373} && rules.switch_word(11) == 1, "Negative sling mirrors only its X impulse");
    check(web::compatibility::parameters(effects.effects.back()) == std::array<std::int32_t, 4>{162, -64, 0, 6}, "Both original sling arms use the same sample and pan");
    auto seed = state.random.state();
    web::compatibility::set(rules, F::positive_sling_inhibit, 1);
    ball.velocity({3000, 1});
    auto sounds = effects.effects.size();
    reactions.surface_transition(state, rules, 0, 6, effects);
    check(ball.velocity() == Vector2{3000, 1} && state.random.state() == seed && effects.effects.size() == sounds, "Sling inhibit preserves velocity/RNG but still latches contact");
    rules.reset();
    state.random = RandomGenerator(1);
    ball.position({0x450000 + 6400, 0x2090000 + 12800});
    ball.velocity({3000, 0});
    reactions.surface_transition(state, rules, 0, 8, effects);
    check(ball.velocity() == Vector2{47, 67} && rules.switch_word(32) == 1, "Bumper uses signed displacement and two ordered random divisors");
    check(web::compatibility::parameters(effects.effects.back()) == std::array<std::int32_t, 4>{156, 0, 0, 8}, "Bumper raw sound arguments");
    rules.reset();
    web::compatibility::set(rules, F::tilt_lock, 1);
    seed = state.random.state();
    ball.velocity({3000, 0});
    sounds = effects.effects.size();
    reactions.surface_transition(state, rules, 0, 8, effects);
    check(
        ball.velocity() == Vector2{3000, 0} && state.random.state() == seed && effects.effects.size() == sounds && rules.switch_word(32) == 1,
        "Tilt suppresses bumper impulse while retaining contact latch");
    rules.reset();
    state.actions = 3;
    reactions.surface_transition(state, rules, 0, 8, effects);
    check(rules.switch_word(32) == 0 && state.random.state() == seed, "Both flipper actions suppress bumper contact before latching");
    ball.velocity({0, -9});
    reactions.surface_transition(state, rules, 0, 4, effects);
    check(
        web::compatibility::get(state.mechanisms.spinners[0], web::compatibility::SpinnerField::velocity) == -1 &&
            web::compatibility::get(state.mechanisms.spinners[0], web::compatibility::SpinnerField::phase) == 0x40000 && rules.switch_word(12) == 1,
        "Spinner reaction truncates negative eighth-speed and sets phase");
    web::compatibility::set(rules, F::diverter_enabled, 1);
    ball.velocity({-2147483647 - 1, 2147483647});
    reactions.surface_transition(state, rules, 0, 12, effects);
    check(ball.velocity() == Vector2{0, -1}, "Diverter redirect preserves wrapped X/Y sum");
    web::compatibility::set(rules, F::auto_kick_enabled, 1);
    state.random = RandomGenerator(1);
    ball.velocity({0, 32767});
    reactions.surface_transition(state, rules, 0, 15, effects);
    check(ball.velocity().y == 33222 && rules.switch_word(1) == 1 && web::compatibility::parameters(effects.effects.back())[0] == 158, "Automatic kick uses its strict speed threshold and shared RNG");
    reactions.surface_transition(state, rules, 5, 11, effects);
    check(
        effects.effects.back().kind() == BallRuleEffect::Kind::station_contact && web::compatibility::parameters(effects.effects.back())[0] == 5,
        "Space Station arm preserves ball-index argument at explicit boundary");
    auto count = effects.effects.size();
    reactions.surface_transition(state, rules, 0, -1, effects);
    check(ball.transition_kind() == -1 && effects.effects.size() == count, "Unknown surface index still updates original transition tag");
}
struct SessionEffects : web::rules::SessionEffectSink {
    std::vector<web::rules::SessionEffect> effects;
    std::vector<std::int32_t> launch_enabled_at_effect;
    void on_effect(const web::rules::SessionEffect& effect, SimulationState&, web::rules::TurnState& rules, web::rules::SharedGameState&) override {
        effects.push_back(effect);
        launch_enabled_at_effect.push_back(web::compatibility::get(rules, web::compatibility::TurnField::launch_enabled));
    }
};
void session_players_and_ball_setup() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    PlayerRoster rules_players;
    TurnState rules(rules_players);
    SharedGameState session(rules_players);
    SessionRules engine;
    SimulationState simulation;
    auto player = web::compatibility::player(session, 2);
    web::compatibility::set(player, web::compatibility::PlayerRecord::Field::relaunch, 19);
    check(rules.player_relaunch(2) == 19, "Player view aliases lifecycle relaunch storage");
    rules.player_relaunch(2, 23);
    check(web::compatibility::get(player, web::compatibility::PlayerRecord::Field::relaunch) == 23, "Lifecycle flag changes are visible through player view");
    web::compatibility::set(player, 0x78, 99);
    web::compatibility::set(player, 0xe0, 77);
    web::compatibility::set(rules, F::player_cycle, 5);
    engine.initialize_player(rules, session, 2);
    check(
        web::compatibility::get(player, 0) == 1 && web::compatibility::get(player, 4) == 0 && web::compatibility::get(player, 8) == 0 && web::compatibility::get(player, 0xa8) == 0,
        "Original player initializer resets ball, score and relaunch fields");
    check(
        web::compatibility::get(player, 0xc) == 25000000 && web::compatibility::get(player, 0xac) == 10000000 && web::compatibility::get(player, 0xd0) == 15 &&
            web::compatibility::get(player, 0x130) == 1,
        "Original numeric player defaults");
    check(web::compatibility::get(player, 0x50) == 5 && web::compatibility::get(rules, F::player_cycle) == 0, "Player cycle stores old value before wrapping");
    check(web::compatibility::get(player, 0x78) == 99 && web::compatibility::get(player, 0xe0) == 77, "Player initializer preserves fields it never writes");
    web::compatibility::set(rules, F::current_player, 2);
    web::compatibility::set(rules, F::lamp_queue, 7);
    web::compatibility::set(player, 0, 3);
    web::compatibility::set(player, 4, 9999999);
    web::compatibility::set(player, 8, 0);
    web::compatibility::set(player, 0x64, 0);
    web::compatibility::set(player, 0x120, 1);
    web::compatibility::set(player, 0x11c, 1);
    web::compatibility::set(rules, F::tilt_lock, 1);
    simulation.force_timer = 77;
    engine.setup_ball(simulation, rules, session);
    check(
        web::compatibility::get(player, 0x120) == 0 && web::compatibility::get(player, 0x11c) == 0 && web::compatibility::get(player, 0x78) == 0,
        "Per-ball setup consumes hold flags and resets its transient field");
    check(
        web::compatibility::get(player, 0x98) == 1 && web::compatibility::get(player, 0x9c) == 3 && web::compatibility::get(player, 0xa0) == 2 && web::compatibility::get(player, 0x94) == 1,
        "Original third-ball low-score setup");
    check(
        web::compatibility::get(rules, F::lamp_queue) == 1 && web::compatibility::get(rules, F::warnings_left) == 3 && web::compatibility::get(rules, F::tilt_lock) == 0 &&
            simulation.force_timer == 0 && simulation.mechanisms.enabled == 1,
        "Ball setup restores physics/tilt gates and wraps lamp queue twice");
    auto decoded = web::compatibility::read_session(rules_players, web::compatibility::encode(session));
    check(web::compatibility::get(web::compatibility::player(decoded, 2), 0xe0) == 77, "Session packed-state round trip preserves unknown player fields");
    rejects([&] { web::compatibility::player(session, 4); });
    rejects([&] { web::compatibility::get(player, 3); });
    rejects([&] { web::compatibility::set(player, 308, 0); });
    rejects([] {
        PlayerRoster bad_players;
        (void)web::compatibility::read_session(bad_players, Bytes(web::compatibility::session_snapshot_bytes - 1));
    });
}
void session_lamps_and_tilt() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    using S = web::compatibility::SessionField;
    SimulationState simulation;
    PlayerRoster rules_players;
    TurnState rules(rules_players);
    SharedGameState session(rules_players);
    SessionRules engine;
    SessionEffects effects;
    session.lamp_cells()[17] = 255;
    session.lamp_index(0, 17);
    session.lamp_index(1, 18);
    engine.sample_lamps(session);
    check(session.lamp_value(0) == 1 && session.lamp_value(1) == 0, "Lamp sampling converts selected bytes to boolean DWORDs");
    web::compatibility::set(rules, F::phase, 3);
    web::compatibility::set(rules, F::warnings_left, 1);
    rules.frame_phase(0x30);
    web::compatibility::set(rules, F::launch_enabled, 1);
    simulation.mechanisms.enabled = 1;
    engine.tilt(simulation, rules, session, effects);
    check(
        web::compatibility::get(rules, F::warnings_left) == 0 && web::compatibility::get(rules, F::lamp_mode) == 2 && web::compatibility::get(rules, F::lamp_animation) == 1 &&
            web::compatibility::get(session, S::lamp_animation_enabled) == 1,
        "Last tilt warning starts recovered lamp animation");
    check(session.lamp_value(0) == 0 && std::all_of(session.lamp_cells().begin(), session.lamp_cells().end(), [](auto value) { return value == 0; }), "Warning clears lamp grid before animation");
    check(
        effects.effects.size() == 2 && effects.effects[0].kind() == SessionEffect::Kind::rule_display &&
            web::compatibility::parameters(effects.effects[0]) == std::array<std::int32_t, 4>{58, 12, 1, 0},
        "Warning display request precedes sound");
    check(
        web::compatibility::parameters(effects.effects[1]) == std::array<std::int32_t, 4>{46, 64, 0, 1} && web::compatibility::get(session, S::warning_display) == 0,
        "Warning uses original one-bit option mask");
    effects.effects.clear();
    effects.launch_enabled_at_effect.clear();
    engine.tilt(simulation, rules, session, effects);
    check(
        effects.effects.size() == 3 && effects.effects[0].kind() == SessionEffect::Kind::music_command && web::compatibility::parameters(effects.effects[1])[1] == 256 &&
            web::compatibility::parameters(effects.effects[2])[0] == 103,
        "Final tilt stops music, requests display then selects sound");
    check(effects.launch_enabled_at_effect[0] == 1 && effects.launch_enabled_at_effect[1] == 0, "Final tilt callback order preserves launch state until music boundary");
    check(
        web::compatibility::get(session, S::music_state) == -1 && web::compatibility::get(rules, F::tilted) == 1 && web::compatibility::get(rules, F::tilt_lock) == 1 &&
            simulation.mechanisms.enabled == 0 && web::compatibility::get(session, S::warning_display) == -1,
        "Final tilt disables flippers and marks tilt state");
    auto count = effects.effects.size();
    engine.tilt(simulation, rules, session, effects);
    check(effects.effects.size() == count, "Repeated final tilt is latched");
    web::compatibility::set(rules, F::phase, 2);
    web::compatibility::set(rules, F::warnings_left, 3);
    engine.tilt(simulation, rules, session, effects);
    check(web::compatibility::get(rules, F::warnings_left) == 3, "Tilt request is ignored outside original phase three");
    session.lamp_index(0, 2048);
    rejects([&] { engine.sample_lamps(session); });
}
void session_start_and_restart() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    using S = web::compatibility::SessionField;
    SimulationState simulation;
    PlayerRoster rules_players;
    TurnState rules(rules_players);
    SharedGameState session(rules_players);
    SessionRules engine;
    SessionEffects effects;
    for (unsigned i = 0; i < 4; i++)
        web::compatibility::set(web::compatibility::player(session, i), 0, 1);
    engine.start(simulation, rules, session, effects);
    check(
        web::compatibility::get(rules, F::phase) == 1 && web::compatibility::get(rules, F::player_count) == 1 && web::compatibility::get(rules, F::start_latch) == 1 &&
            web::compatibility::get(rules, F::announce_pending) == 1 && simulation.mechanisms.enabled == 1,
        "Start initializes first player and enters phase one");
    engine.start(simulation, rules, session, effects);
    engine.start(simulation, rules, session, effects);
    engine.start(simulation, rules, session, effects);
    check(web::compatibility::get(rules, F::player_count) == 4 && web::compatibility::get(web::compatibility::player(session, 3), 0) == 1, "Add-player requests initialize all four slots");
    engine.start(simulation, rules, session, effects);
    check(web::compatibility::get(rules, F::player_count) == 4 && effects.effects.empty(), "First-ball four-player limit ignores further start requests");
    web::compatibility::set(web::compatibility::player(session, 0), 0, 2);
    web::compatibility::set(rules, F::phase, 4);
    web::compatibility::set(session, S::music_state, 0);
    engine.start(simulation, rules, session, effects);
    check(
        web::compatibility::get(rules, F::player_count) == 1 && web::compatibility::get(rules, F::current_player) == 0 && web::compatibility::get(rules, F::phase) == 1 &&
            web::compatibility::get(rules, F::save_timer) == 0x28000,
        "Start during later ball resets session and adds first player again");
    check(
        effects.effects.size() == 2 && effects.effects[0].kind() == SessionEffect::Kind::music_command && effects.effects[1].kind() == SessionEffect::Kind::sound,
        "Restart keeps original music and announcer order");
    web::compatibility::set(rules, F::phase, 5);
    auto players = web::compatibility::get(rules, F::player_count);
    engine.start(simulation, rules, session, effects);
    check(web::compatibility::get(rules, F::player_count) == players, "Start is blocked during phase five");
    web::compatibility::set(rules, F::phase, 0);
    web::compatibility::set(rules, F::player_count, 4);
    web::compatibility::set(rules, F::start_latch, -1);
    web::compatibility::set(web::compatibility::player(session, 0), 0, 2);
    rejects([&] { engine.start(simulation, rules, session, effects); });
}
void session_physics_tilt_callback() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    SimulationState simulation;
    simulation.refresh_rate = 1050;
    simulation.actions = 0x21;
    simulation.nudge.tilt = 255;
    simulation.mechanisms.enabled = 1;
    PlayerRoster rules_players;
    TurnState rules(rules_players);
    web::compatibility::set(rules, F::phase, 3);
    web::compatibility::set(rules, F::launch_enabled, 1);
    SharedGameState session(rules_players);
    SessionEffects effects;
    TestEvents remaining;
    SessionRuleCallbacks handler(simulation, rules, session, effects, remaining);
    RuleEffects ball_effects;
    BallRuleCallbacks callbacks(simulation, rules, ball_effects, handler);
    auto engine = flat_simulation();
    engine.step(simulation, callbacks);
    check(
        web::compatibility::get(rules, F::tilted) == 1 && simulation.mechanisms.enabled == 0 &&
            web::compatibility::get(simulation.mechanisms.flippers[0], web::compatibility::FlipperField::drive) == 0,
        "Synchronous tilt disables flipper before its same-tick drive update");
    check(effects.effects.size() == 3 && remaining.events.empty(), "Recovered tilt runs through chained physics rule handlers");
    BallSlots detached;
    rejects([&] { handler.on_event(web::physics::PhysicsEvent{6, web::physics::TiltContact{}}, detached); });
}
void presentation_and_clock() {
    SimulationClock clock;
    check(clock.until_next_frame_ns() == 16666667, "Initial frame deadline rounds upward to a whole nanosecond");
    clock.advance(16666666);
    check(clock.until_next_frame_ns() == 1, "Frame wait honors the remaining fractional tick");
    clock.reset();
    check(clock.advance(16666666) == 0 && clock.advance(1) == 1, "Clock preserves fractional tick without early rounding");
    clock.reset();
    unsigned frames = 0;
    for (unsigned i = 0; i < 1000; i++)
        frames += clock.advance(1000000);
    check(frames == 60, "One second produces exactly sixty original frame profiles");
    check(clock.advance(10000000000ULL) == 15, "Large clock gaps have bounded catch-up");
    clock.advance(8000000);
    clock.reset();
    check(clock.advance(10000000) == 0, "Focus/pause reset discards fractional pending time");
    auto mechanisms = MechanismState::defaults();
    for (auto& spinner : mechanisms.spinners) {
        web::compatibility::set(spinner, web::compatibility::SpinnerField::phase, 0);
        check(TableRenderer::spinner_frame(spinner) == 15, "Spinner display uses the original previous phase");
        web::compatibility::set(spinner, web::compatibility::SpinnerField::phase, 0x8000);
        check(TableRenderer::spinner_frame(spinner) == 0, "Spinner display wraps to frame zero at its original boundary");
    }
    check(table_depth_top(0x74) == 192 && table_depth_top(0x54) == 120, "Backbox views use a quarter-screen depth origin");
    check(table_depth_top(0x64) == 150, "800x600 depth origin excludes tile padding");
    check(table_depth_top(0x62) == 0, "Full-table views have no depth crop");
    for (auto& flipper : mechanisms.flippers) {
        using F = web::compatibility::FlipperField;
        check(TableRenderer::flipper_frame(flipper) == 0, "Initial mechanism selects rest sprite");
        web::compatibility::set(flipper, F::angle, web::compatibility::get(flipper, F::powered_angle));
        check(TableRenderer::flipper_frame(flipper) == 11, "Powered mechanism selects final sprite");
        web::compatibility::set(flipper, F::angle, web::compatibility::get(flipper, F::rest_angle));
        const auto delta = web::compatibility::get(flipper, F::direction) == 1 ? 0x1555 : -0x1555;
        web::compatibility::set(flipper, F::angle, integer::add(web::compatibility::get(flipper, F::rest_angle), delta));
        check(TableRenderer::flipper_frame(flipper) == 6, "Half angle selects recovered frame six");
    }
    BallState ball;
    ball.position({0x1260000, 0x620000});
    for (unsigned view : {0, 1, 2, 4, 5, 6}) {
        const auto p = BallProjection(0x70 | view).project(ball);
        check(p.frame >= 0 && p.frame < 128 && p.x > 512 && p.x < 1100 && p.y > 300 && p.y < 900, "All high-quality cameras project launcher into right table region");
    }
    rejects([] { BallProjection invalid(0x73); });
    Bitmap canvas;
    canvas.width = 3;
    canvas.height = 2;
    canvas.pixels.resize(6, 0xffffff);
    auto map = uniform(16);
    const std::array<Span, 1> spans = {
        Span{-1, 0, {0x7c00, 0x03e0, 0x001f, 0x7fff}}
    };
    std::uint8_t d = 17;
    TableRenderer::composite(canvas, map, spans, 0, 0, 0, 2, &d);
    check(canvas.pixels[0] == 0xffffff, "Depth threshold hides a ball behind foreground geometry");
    d = 16;
    TableRenderer::composite(canvas, map, spans, 0, 0, 0, 2, &d);
    check(canvas.pixels[0] == 0x00ff00 && canvas.pixels[1] == 0x0000ff && canvas.pixels[2] == 0xffffff, "Equal depth draws exact RGB555 colors with left clipping");
    TableRenderer::composite(canvas, map, spans, 0, 1, 0, 2, &d, true);
    check(canvas.pixels[3] == 0xc5c5c5 && canvas.pixels[5] == 0xc5c5c5, "RGB555 shadow subtracts one quarter per channel with original rounding");
}
void lamp_queue_and_composition() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    using S = web::compatibility::SessionField;
    PlayerRoster r_players;
    TurnState r(r_players);
    SharedGameState s(r_players);
    auto& l = s.lamps;
    LampRules engine;
    SimulationState simulation;
    engine.initialize(r, s, l);
    engine.clear_queue(r, s);
    check(s.lamp_index(0) == 1744 && s.lamp_index(69) == 240 && web::compatibility::get(r, F::lamp_mode) == 99, "Original lamp lookup and startup mode initialize");
    r.frame_phase(63);
    for (auto [duration, expected] : std::array<std::pair<int, int>, 9>{
             {{-1, 1}, {0, 0}, {0x8000, 0}, {0x8001, 4}, {0x18000, 4}, {0x18001, 8}, {0x40001, 16}, {0x80000, 16}, {0x80001, 32}}
    }) {
        engine.blink(r, s, l, 78, duration);
        check(s.lamps.outputs[78] == expected, "Blink boundaries preserve original raw phase bits");
    }
    for (unsigned i = 0; i < 9; i++)
        engine.queue(r, s, 70 + i, 100);
    check(web::compatibility::get(r, F::lamp_queue) == 1 && web::compatibility::get(s, S(0x431220)) == 78, "Eight-entry queue wraps and overwrites oldest flash");
    web::compatibility::set(s, S(0x431208), 99);
    web::compatibility::set(r, F::elapsed, 100);
    engine.step_queue(r, s, l);
    check(web::compatibility::get(s, S(0x4311e8)) == 0 && s.lamps.outputs[78] == 4, "Flash still writes on its expiration tick");
    engine.clear_queue(r, s);
    check(web::compatibility::get(s, S(0x431208)) == 99, "Queue clear preserves adjacent first drop-target alias");
    web::compatibility::set(r, F::save_timer, 0x30000);
    engine.compose_table(simulation, r, s, l);
    check(s.lamp_value(1) == 8 && LampRules::value(s, l, 73) == 0 && LampRules::value(s, l, 74) == 1, "Ball-save blink and target indicators use recovered states");
    web::compatibility::set(r, F::tilt_lock, 1);
    engine.compose_table(simulation, r, s, l);
    check(s.lamp_value(1) == 0 && LampRules::value(s, l, 74) == 1, "Tilt clears lamps while retaining target presentation");
    web::compatibility::set(r, F::tilt_lock, 0);
    web::compatibility::set(r, F::drain_gate, 1);
    rejects([&] { engine.compose_table(simulation, r, s, l); });
    rejects([&] { engine.queue(r, s, 79, 1); });
    web::compatibility::set(r, F::lamp_queue, 8);
    rejects([&] { engine.queue(r, s, 1, 1); });
    rejects([&] { LampRules::value(s, l, 79); });
}
void lamp_animation() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    using S = web::compatibility::SessionField;
    PlayerRoster r_players;
    TurnState r(r_players);
    SharedGameState s(r_players);
    auto& l = s.lamps;
    LampRules engine;
    engine.initialize(r, s, l);
    engine.begin_animation(r, s, 0);
    r.frame_phase(1);
    engine.step_animation(r, s);
    check(s.lamp_cells()[63 * 32] == 1 && s.lamp_cells()[62 * 32] == 0 && web::compatibility::get(r, F::lamp_frame) == 1, "Animation sweeps one row from table bottom");
    engine.step_animation(r, s);
    check(web::compatibility::get(r, F::lamp_frame) == 1, "Repeated phase does not advance animation twice");
    web::compatibility::set(r, F::lamp_frame, 256);
    r.frame_phase(2);
    engine.step_animation(r, s);
    check(web::compatibility::get(r, F::lamp_frame) == 0, "Attract sweep loops after its final column");
    engine.begin_animation(r, s, 1);
    web::compatibility::set(r, F::lamp_frame, 128);
    engine.step_animation(r, s);
    check(web::compatibility::get(r, F::lamp_animation) == 0 && web::compatibility::get(s, S::lamp_animation_enabled) == 0, "One-shot animation terminates at original boundary");
    engine.begin_animation(r, s, 99);
    web::compatibility::set(r, F::lamp_frame, 48);
    r.frame_phase(3);
    web::compatibility::set(s, S::dmd_status, -1);
    engine.step_animation(r, s);
    check(web::compatibility::get(r, F::lamp_mode) == 99, "Startup pattern waits for original DMD status gate");
    web::compatibility::set(s, S::dmd_status, 0);
    r.frame_phase(4);
    engine.step_animation(r, s);
    check(
        web::compatibility::get(r, F::lamp_mode) == 0 && web::compatibility::get(r, F::lamp_frame) == 1 && web::compatibility::get(r, F::lamp_animation) == 1,
        "Startup hands off to looping sweep with original phase order");
}
void drop_target_hits_and_resets() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    using S = web::compatibility::SessionField;
    PlayerRoster r_players;
    TurnState r(r_players);
    SharedGameState s(r_players);
    auto& l = s.lamps;
    DropTargetRules engine;
    LampRules lights;
    SimulationState simulation;
    lights.initialize(r, s, l);
    lights.clear_queue(r, s);
    engine.initialize(s, l);
    check(web::compatibility::get(s, S(0x431208)) == 1 && DropTargetRules::state(s, l, 5) == 1, "Target initialization retains the first word's session alias");
    web::compatibility::set(r, F::target_bank_a_timer, 546);
    web::compatibility::set(r, F::target_bank_b_timer, 1092);
    web::compatibility::set(r, F::elapsed, 546);
    engine.step(r, s, l);
    check(DropTargetRules::state(s, l, 0) == 0 && DropTargetRules::state(s, l, 3) == 1, "Startup reset raises first bank independently");
    engine.step(r, s, l);
    check(DropTargetRules::state(s, l, 3) == 0, "Second startup bank raises on its own deadline");
    auto p = web::compatibility::player(s, 0);
    for (unsigned i = 0; i < 6; i++) {
        web::compatibility::set(r, F(0x4319bc + i * 4), 1);
        check(engine.step(r, s, l) == 1, "Rising target contact lowers exactly one target");
        check(DropTargetRules::state(s, l, i) == 1, "Hit target remains down before reset expires");
        lights.compose_table(simulation, r, s, l);
        check(LampRules::value(s, l, 73 + i) == 0, "Lowered target selects original OFF appearance");
        check(engine.step(r, s, l) == 0, "Repeated contact cannot lower an already down target again");
        web::compatibility::set(r, F(0x4319bc + i * 4), 0);
    }
    check(
        web::compatibility::get(p, 0x8c) == 1 && web::compatibility::get(p, 0xb0) == 1 && web::compatibility::get(p, 0x100) == 0,
        "All-six completion preserves original dock insert and clears progress");
    for (unsigned i = 0; i < 50; i++)
        engine.step(r, s, l);
    bool raised = true;
    for (unsigned i = 0; i < 6; i++)
        raised &= DropTargetRules::state(s, l, i) == 0;
    check(raised, "Completed banks reset all six targets");
    lights.compose_table(simulation, r, s, l);
    check(LampRules::value(s, l, 73) == 1 && LampRules::value(s, l, 78) == 1, "Raised targets select original ON appearance");
    DropTargetRules::state(s, l, 2, 1);
    web::compatibility::set(r, F::target_bank_a_timer, 0);
    engine.step(r, s, l);
    check(DropTargetRules::state(s, l, 2) == 1, "Partial bank stays down until completion");
    web::compatibility::set(r, F(0x4319bc), 1);
    web::compatibility::set(r, F::target_bank_a_timer, 546);
    engine.step(r, s, l);
    check(DropTargetRules::state(s, l, 0) == 0 && DropTargetRules::state(s, l, 2) == 0, "Pre-existing reset expiry wins over a hit in the same update");
    for (unsigned i = 0; i < 6; i++)
        DropTargetRules::state(s, l, i, 1);
    web::compatibility::set(p, 0x98, 2);
    web::compatibility::set(r, F::target_bank_a_timer, 0);
    web::compatibility::set(r, F::target_bank_b_timer, 0);
    engine.step(r, s, l);
    check(
        web::compatibility::get(r, F::target_bank_a_timer) == 0 && web::compatibility::get(r, F::target_bank_b_timer) == 0 && DropTargetRules::state(s, l, 0) == 1,
        "Original multiball mode suppresses automatic bank reset");
    rejects([&] { DropTargetRules::state(s, l, 6); });
    rejects([&] { DropTargetRules::state(s, l, 6, 1); });
}
void frame_timing() {
    FrameTiming timing;
    auto first = timing.presented(100, 200, 1, 7);
    check(
        first.frame == 1 && first.game_frame == 7 && first.interval_ns == 0 && first.work_ns == 200 && first.simulation_steps == 1,
        "First presented frame carries counters/work but has no interval");
    std::uint64_t now = 100;
    for (unsigned i = 0; i < 100; i++) {
        now += i < 95 ? 16666667 : 32000000;
        timing.presented(now, 1000000, 1, 8 + i);
    }
    auto stats = timing.statistics();
    check(stats.samples == 100 && stats.over_25ms == 5, "Only intervals over 25 ms count as slow");
    check(stats.p95_ms > 16.66 && stats.p95_ms < 16.67 && stats.p99_ms == 32 && stats.max_ms == 32, "Nearest-rank p95/p99 include actual long frames");
    check(stats.stddev_ms > 3 && stats.fps < 60 && stats.fps > 55, "Jitter and FPS use measured durations");
    for (unsigned i = 0; i < FrameTiming::capacity; i++) {
        now += 16666667;
        timing.presented(now, 1000000, 1, 200 + i);
    }
    stats = timing.statistics();
    check(stats.samples == 240 && stats.over_25ms == 0 && stats.max_ms < 16.67 && stats.stddev_ms < .0001, "Rolling window expires old hitches");
    const auto before = timing.frames();
    timing.suspend();
    now += 5000000000;
    auto resumed = timing.presented(now, 3000000, 2, 999);
    check(timing.frames() == before + 1 && resumed.interval_ns == 0 && timing.statistics().samples == 0, "Pause breaks timing segment without resetting lifetime frame counter");
    rejects([&] { timing.presented(now - 1, 0, 1, 0); });
}
void table_switch_controllers() {
    using namespace web::rules;
    using F = web::compatibility::TurnField;
    SimulationState simulation;
    PlayerRoster r_players;
    TurnState r(r_players);
    SharedGameState s(r_players);
    auto& lamps = s.lamps;
    LampRules lights;
    TableMechanismRules table;
    lights.initialize(r, s, lamps);
    SessionRules{}.initialize_player(r, s, 0);
    SessionRules{}.setup_ball(simulation, r, s);
    auto p = web::compatibility::player(s, 0);
    web::compatibility::set(r, F::elapsed, 546);
    check(web::compatibility::get(p, 0x90) == 1 && simulation.force_timer == 0, "Magnet starts available with its force inactive");
    check(web::compatibility::get(p, 0xfc) == 1 && web::compatibility::get(r, F::auto_kick_enabled) == 0, "Kickback starts available, waiting for its outlane contact to arm the solenoid");
    check(web::compatibility::get(r, F::save_timer) == 0, "Drain replacement window waits for launch");
    const auto contact = [&](unsigned index, int value) { web::compatibility::set(r, F(0x431958 + 4 * index), value); };
    contact(8, 1);
    table.magnet(simulation, r, s);
    check(
        simulation.force_timer == 0x28000 && web::compatibility::get(p, 0x90) == 0 && web::compatibility::get(r, F(0x43180c)) == 0x19999,
        "Sinkhole contact consumes default magnetic hold and arms the delayed original force");
    simulation.force_timer = 0;
    table.magnet(simulation, r, s);
    check(simulation.force_timer == 0, "Consumed magnetic hold cannot retrigger without a qualifying combo");
    contact(8, 0);
    contact(3, 1);
    table.magnet(simulation, r, s);
    check(web::compatibility::get(r, F(0x431824)) == 0x20000 - 546, "Qualifying combo contact starts the magnetic hold window");
    contact(3, 0);
    contact(8, 1);
    table.magnet(simulation, r, s);
    check(
        simulation.force_timer == 0x28000 && web::compatibility::get(r, F(0x431824)) == 0 && web::compatibility::get(r, F(0x43181c)) == 0x30000 - 546,
        "Combo can rearm the consumed magnetic hold and sets the ramp control timer");
    contact(8, 0);
    web::compatibility::set(r, F(0x43181c), 0);
    contact(1, 1);
    table.kickback(r, s);
    check(web::compatibility::get(r, F::auto_kick_enabled) == 0x40000 - 546 && web::compatibility::get(p, 0xfc) == 0, "Outlane edge arms kick and consumes availability after launch save expires");
    check(web::compatibility::get(p, 0x4c) == 0, "First kickback voice latch changes with the original arm");
    web::compatibility::set(p, 0xfc, 1);
    web::compatibility::set(r, F::save_timer, 0x78000);
    table.kickback(r, s);
    check(web::compatibility::get(r, F::auto_kick_enabled) == 0x4000 - 546 && web::compatibility::get(p, 0xfc) == 1, "Launch save preserves kickback availability and uses its shorter kick window");
    web::compatibility::set(r, F::save_timer, 0);
    web::compatibility::set(r, F(0x431870), 1);
    table.kickback(r, s);
    check(web::compatibility::get(p, 0xfc) == 1 && web::compatibility::get(r, F::auto_kick_enabled) == 0x40000 - 546, "Original kickback hold flag preserves availability");
    contact(1, 0);
    web::compatibility::set(r, F::auto_kick_enabled, 1);
    table.kickback(r, s);
    check(web::compatibility::get(r, F::auto_kick_enabled) == 0, "Kickback timer expires without becoming permanently enabled");
    web::compatibility::set(p, 0xfc, 0);
    contact(2, 1);
    table.kickback(r, s);
    check(web::compatibility::get(r, F(0x4317f4)) == 0x20000 - 546 && web::compatibility::get(p, 0xfc) == 0, "Qualifying switch starts rearm window without instantly awarding a kickback");
    contact(2, 0);
    contact(6, 1);
    table.kickback(r, s);
    check(
        web::compatibility::get(p, 0xfc) == 1 && web::compatibility::get(r, F(0x4317f4)) == 0 && web::compatibility::get(r, F(0x431b78)) == 1,
        "Rearm completion restores the player availability and announcer latch");
    web::compatibility::set(p, 0xfc, 0);
    web::compatibility::set(r, F(0x4317f4), 546);
    table.kickback(r, s);
    check(web::compatibility::get(p, 0xfc) == 0, "Expired rearm window cannot be completed on the same tick");
    contact(6, 0);
    contact(13, 1);
    contact(14, 1);
    table.inlanes(r);
    check(web::compatibility::get(r, F(0x431820)) == 0x78000 - 546 && web::compatibility::get(r, F(0x43182c)) == 0x78000 - 546, "Both inlanes light and advance their spinner timers");
    web::compatibility::set(r, F(0x431820), -1);
    table.inlanes(r);
    check(web::compatibility::get(r, F(0x431820)) == -1, "Permanent spinner lighting survives contacts and elapsed time");
    web::compatibility::set(p, 0xb0, 0);
    web::compatibility::set(p, 0x84, 0);
    web::compatibility::set(r, F::diverter_enabled, 0);
    web::compatibility::set(r, F::drain_extension, 0);
    web::compatibility::set(r, F::launch_timer, 0x24000);
    table.ramp(r, s);
    check(
        web::compatibility::get(r, F::diverter_enabled) == 1 && web::compatibility::get(r, F::drain_extension) == 1 && web::compatibility::get(r, F::launch_timer) == 0,
        "Ramp toggles diverter, clears skillshot timer and restores the drain extension");
    table.ramp(r, s);
    check(web::compatibility::get(r, F::diverter_enabled) == 0, "Second unblocked ramp toggles the diverter back");
    web::compatibility::set(r, F(0x43188c), 1);
    table.ramp(r, s);
    check(web::compatibility::get(r, F::diverter_enabled) == 1, "Lit ramp hold enables the diverter");
    web::compatibility::set(p, 0xb0, 1);
    table.ramp(r, s);
    check(web::compatibility::get(r, F::diverter_enabled) == 0, "Lock qualification overrides normal diverter toggle");
    contact(35, 1);
    table.rollovers(r, s, 0);
    check(web::compatibility::get(p, 0x124) == 1 && web::compatibility::get(p, 0x128) == 0 && web::compatibility::get(p, 0x12c) == 0, "Rollover persists after its short flash");
    contact(35, 0);
    table.rollovers(r, s, 1);
    check(web::compatibility::get(p, 0x124) == 0 && web::compatibility::get(p, 0x128) == 1 && web::compatibility::get(p, 0x12c) == 0, "Flipper rising edge rotates rollover progress");
    table.rollovers(r, s, 0);
    check(web::compatibility::get(p, 0x128) == 1, "Held flipper produces no second rotation");
    web::compatibility::set(p, 0x124, 1);
    contact(37, 1);
    table.rollovers(r, s, 0);
    check(
        web::compatibility::get(p, 0x124) == 0 && web::compatibility::get(p, 0x128) == 0 && web::compatibility::get(p, 0x12c) == 0,
        "Completed rollover bank flashes and resets without introducing scoring policy");
    table.outlanes(r, s);
    check(web::compatibility::get(r, F::lamp_mode) != 1, "No outlane edge leaves the animation unchanged");
    contact(5, 1);
    table.outlanes(r, s);
    check(
        web::compatibility::get(r, F::lamp_animation) == 1 && web::compatibility::get(r, F::lamp_mode) == 1 && web::compatibility::get(r, F::lamp_frame) == 0,
        "Unsaved right outlane starts the recovered drain animation");
}
void scoring_and_hud_values() {
    using P = web::compatibility::PlayerRecord::Field;
    using F = web::compatibility::TurnField;
    rules::PlayerRoster state_players;
    rules::TurnState state(state_players);
    rules::SharedGameState session(state_players);
    rules::ScoringRules scoring(state, session);
    check(scoring.score().text() == "0", "Ready HUD shows zero points");
    scoring.tenths(1110);
    check(scoring.score().text() == "1,110", "HUD restores original trailing zero and groups digits");
    scoring.hundreds(10);
    check(scoring.score().points() == 11110, "Drop award uses wrapped hundred-unit helper");
    auto player = web::compatibility::player(session, 0);
    web::compatibility::set(player, P::score, 99999999);
    scoring.add(1);
    check(scoring.score().units == 0 && scoring.score().rollovers == 1 && scoring.score().text() == "1,000,000,000", "HUD preserves billion-point carry");
    check(scoring.replay().credits == 1 && scoring.replay().awards == 1 && web::compatibility::get(player, P::replay_awarded) == 1, "Replay threshold awards one credit");
    scoring.add(100000000);
    check(scoring.replay().credits == 1, "Replay is awarded once per player");
    web::compatibility::set(state, F::current_player, 1);
    scoring.add(37);
    check(scoring.score().points() == 370 && web::compatibility::player_word(session, 0, P::score_rollovers) == 2, "Scoring selects current player without changing another record");
    rejects([&] { state.switch_edge(43); });
    rejects([&] { web::compatibility::player_word(session, 4, P::score); });
    rejects([&] { web::compatibility::player_word(session, 0, P::relaunch); });
    auto& lamps = session.lamps;
    rules::MissionState mission_state;
    RandomGenerator random;
    rules::MissionRules missions(state, session, lamps, scoring, random, mission_state);
    auto inconsistent = web::compatibility::player(session, 1);
    for (unsigned i = 0; i < unsigned(rules::Mission::count); ++i)
        web::compatibility::set(inconsistent, 0x20 + i * sizeof(std::int32_t), 1);
    web::compatibility::set(inconsistent, P::all_missions_played, 0);
    web::compatibility::set(inconsistent, P::mission_selection, 0);
    rejects([&] { missions.advance_selection(); });
}
void sound_playback() {
    SoundBank bank;
    bank.samples = {
        {1000, -2000, 32767, -32768},
        {20000, 20000}
    };
    bank.definitions[1] = {50, 1, {0}};
    bank.definitions[2] = {100, 1, {1}};
    SoundMixer mixer(bank, 2);
    check(mixer.request({1, 0, 0, 7}) == 0, "First sound channel");
    check(mixer.active(1, 0, 7), "Sound query filters");
    std::array<std::int16_t, 10> output;
    mixer.render(output);
    check(output == std::array<std::int16_t, 10>{1000, 1000, -2000, -2000, 32767, 32767, -32768, -32768, 0, 0}, "Mono decoding, stereo frames and end-of-sample silence");
    check(!mixer.active(), "Completed sound frees its channel");
    SoundMixer queued(bank, 1);
    queued.retain_until_played = true;
    queued.request({2, 0, 0, 7});
    queued.render(output);
    check(queued.active(2, 0, 7), "Queued final PCM must retain query state until playback completes");
    check(queued.request({1}) == -1, "Queued final PCM released its arbitration priority too early");
    queued.complete(0);
    check(!queued.active() && queued.request({1}) == 0, "Device completion did not release the retained voice");
    mixer.request({2, 64, 0, 1});
    mixer.render(std::span(output).first(2));
    check(output[1] == 20000 && output[0] > 9700 && output[0] < 9800, "Original pan level attenuates the opposite channel");
    mixer.stop();
    mixer.request({2, -128, 0, 1});
    mixer.render(std::span(output).first(2));
    check(output[0] == 20000 && output[1] == 20000, "Original -128 pan safely retains the sample183 zero-step alias");
    mixer.stop();
    mixer.request({2, 0, 0, 1});
    mixer.request({2, 0, 0, 2});
    mixer.render(std::span(output).first(2));
    check(output[0] == 32767 && output[1] == 32767, "Overlapping voices saturate rather than wrap");
    mixer.stop(-1, -1, 1);
    check(!mixer.active(-1, -1, 1) && mixer.active(-1, -1, 2), "Stopping one group preserves another voice");
    mixer.stop();
    mixer.request({1, 0, 127, -1});
    mixer.render(output);
    check(std::all_of(output.begin(), output.end(), [](auto value) { return value == 0; }), "Maximum attenuation produces silence");
    mixer.enabled = false;
    check(mixer.request({1}) == -1, "Disabled sound rejects requests");
    rejects([&] { mixer.render(std::span(output).first(3)); });
    rejects([&] { SoundMixer invalid(bank, 0); });
    std::array<std::int16_t, 4> too_short{};
    rejects([&] { SoundDecoder::decode(std::array<std::uint8_t, 3>{}, 0, too_short); });
}
} // namespace
int main() {
    const std::array<std::pair<const char*, std::function<void()>>, 35> tests = {
        {{"configuration", configuration},
         {"scores", scores},
         {"decoded sound playback and voice lifetime", sound_playback},
         {"original scoring and HUD values", scoring_and_hud_values},
         {"nudge", nudge},
         {"tiles and patches", tiles_and_patches},
         {"sprites", sprites},
         {"packed state and integer math", ball_and_integer},
         {"collision classification", classification},
         {"velocity reflection", reflection},
         {"motion modes", motion_modes},
         {"controlled motion, force and RNG", controlled_motion_and_force},
         {"surface handling and callbacks", surface_and_callbacks},
         {"composed ball physics", composed_ball_physics},
         {"mechanism state and flipper drive", mechanism_state_and_drive},
         {"spinner dynamics", spinner_dynamics},
         {"ball contacts and initialized x87 constants", ball_contacts},
         {"flipper contact geometry and impulses", flipper_contacts},
         {"simulation order, schedule and replay", simulation_order_and_schedule},
         {"synchronous tilt boundary", synchronous_tilt_boundary},
         {"rule state and switch aliases", rule_state_and_switches},
         {"serve, launch and return helpers", rule_serve_launch_and_exits},
         {"drain and timed return rules", rule_drain_and_timed_returns},
         {"surface transitions and physics rule callbacks", rule_surface_and_physics_callbacks},
         {"physical surface impulses and gates", physical_surface_reactions},
         {"session player initialization and ball setup", session_players_and_ball_setup},
         {"session lamp and tilt effects", session_lamps_and_tilt},
         {"session start and restart rules", session_start_and_restart},
         {"session tilt inside physics", session_physics_tilt_callback},
         {"camera, mechanism frames and fixed clock", presentation_and_clock},
         {"presentation counters and jitter statistics", frame_timing},
         {"lamp queue and common indicators", lamp_queue_and_composition},
         {"lamp animation and phase", lamp_animation},
         {"drop target hits, banks and resets", drop_target_hits_and_resets},
         {"kickback, magnet, diverter, inlanes and rollovers", table_switch_controllers}}
    };
    unsigned failed = 0;
    for (const auto& [name, test] : tests)
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            failed++;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    std::cout << checks << " checks, " << failed << " failed groups\n";
    return failed ? 1 : 0;
}
