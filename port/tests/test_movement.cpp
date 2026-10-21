// Behaviour tests for the Human contexts. The values checked here are the PORT: placeholders; when a context gets
// its RE values, its test changes with it and cites the same RE section.

#include "check.h"
#include "port/sim.h"
#include "scimitar/ac2/human_ground.h"
#include "scimitar/ac2/human_in_air.h"

using namespace port;

namespace {
MoveInput forward(float mag = 1) {
    MoveInput in;
    in.moveDir = {0, 1, 0};
    in.moveMag = mag;
    return in;
}
}  // namespace

TEST(ground_idle_stays_put) {
    BoxWorld world;
    Sim sim(world);
    sim.run(1.0f, [](float) { return MoveInput{}; });
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ground);
    CHECK(sim.human().m_Position == Vec3{});
}

TEST(ground_walk_moves_forward) {
    BoxWorld world;
    Sim sim(world);
    sim.run(1.0f, [](float) { return forward(); });
    // PORT: the distance depends on the stand-in speeds; only the direction and an upper bound are checked.
    CHECK(sim.human().m_Position.y > 0.0f);
    CHECK(sim.human().m_Position.y < sim.human().GetPortGroundSpeeds().atRatio[1]);
    CHECK_NEAR(sim.human().m_Position.x, 0.0f, 1e-6f);
    CHECK_NEAR(sim.human().m_Yaw, 0.0f, 1e-6f);
}

TEST(walk_off_edge_falls_and_lands) {
    BoxWorld world;
    world.add({{-5, -5, 0}, {5, 5, 3}});
    Sim sim(world, {.position = {0, 0, 3}});
    sim.run(4.0f, [](float) {
        MoveInput in = forward();
        in.buttons = kHighProfile;  // run: a stick-only walk (ratio 0.2) wouldn't reach the edge in time
        return in;
    });

    bool sawAir = false;
    for (const Frame& f : sim.timeline()) sawAir |= f.context == HumanContextId::InAir;
    CHECK(sawAir);
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ground);
    CHECK(sim.human().GetPreviousContextID() == HumanContextId::InAir);
    CHECK_NEAR(sim.human().m_Position.z, 0.0f, 1e-6f);
}

TEST(no_floor_keeps_falling) {
    BoxWorld world(false);
    Sim sim(world, {.position = {0, 0, 10}, .context = HumanContextId::InAir});
    sim.run(1.0f, [](float) { return MoveInput{}; });
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::InAir);
    CHECK(sim.human().m_Position.z < 10 - 4);
}

TEST(context_bundle_creates_on_demand) {
    scimitar::ac2::Human human;
    auto& bundle = human.GetContextBundle();
    CHECK(&bundle.GetGround() == &bundle.GetGround());
    CHECK(bundle.Get(HumanContextId::Ground) == &bundle.GetGround());
    CHECK(bundle.Get(HumanContextId::InAir)->GetID() == HumanContextId::InAir);
    CHECK(bundle.Get(HumanContextId::Ledge) == nullptr);  // not implemented yet
}

TEST(context_names_cover_every_id) {
    for (int i = 0; i < static_cast<int>(HumanContextId::Count); ++i)
        CHECK(scimitar::ac2::HumanContextName(static_cast<HumanContextId>(i)) != "?");
}
