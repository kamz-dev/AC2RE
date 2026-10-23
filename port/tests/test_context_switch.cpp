// The context switch (RE/10_human_update.md): AIActor's tracks, the update phases and TransitToContext.

#include <string>
#include <vector>

#include "check.h"
#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"
#include "scimitar/ai_actor.h"

using scimitar::ActorContext;
using scimitar::AIActor;
using scimitar::PortUpdateArgs;
using scimitar::ac2::Human;
using scimitar::ac2::HumanContextId;

namespace {
// A context that records what happens to it; it can switch to another context during one of its updates.
struct LogContext : ActorContext {
    LogContext(Human& human, HumanContextId id, std::vector<std::string>& log)
        : ActorContext(human), id(id), log(log) {}
    HumanContextId GetID() const override { return id; }
    void Enter() override { log.push_back("enter " + std::to_string(int(id))); }
    void Exit() override { log.push_back("exit " + std::to_string(int(id))); }
    void Update(long phase, const PortUpdateArgs&) override {
        log.push_back("update" + std::to_string(phase) + " " + std::to_string(int(id)));
        if (switchOnPhase == phase && actor) {
            switchOnPhase = -1;
            actor->TransitToContext(switchTo);
        }
    }
    HumanContextId id;
    std::vector<std::string>& log;
    AIActor* actor = nullptr;
    long switchOnPhase = -1;
    HumanContextId switchTo = HumanContextId::None;
};

struct TestActor : AIActor {
    TestActor(Human& human, std::vector<std::string>& log)
        : ground(human, HumanContextId::Ground, log), inAir(human, HumanContextId::InAir, log) {
        ground.actor = inAir.actor = this;
    }
    ActorContext* GetContextByID(long track, HumanContextId id) override {
        if (track != kMainTrack) return nullptr;
        if (id == HumanContextId::Ground) return &ground;
        if (id == HumanContextId::InAir) return &inAir;
        return nullptr;  // e.g. Climb: not there
    }
    LogContext ground, inAir;
};

struct Frame {
    port::MoveInput input;
    port::BoxWorld world;
    PortUpdateArgs args{input, world, 1.0f / 30.0f};
};
}  // namespace

TEST(context_switch_runs_exit_exec_enter_at_once) {
    Human human;
    std::vector<std::string> log;
    TestActor actor(human, log);
    actor.SetInitialContext(AIActor::kMainTrack, HumanContextId::Ground, [&] { log.push_back("init"); });
    actor.Enter();
    CHECK((log == std::vector<std::string>{"init", "enter 4"}));
    CHECK(actor.GetCurrentContextID() == HumanContextId::Ground);

    log.clear();
    CHECK(actor.TransitToContext(HumanContextId::InAir, [&] { log.push_back("exec"); }));
    // PC 0x7B1D0: the old Exit, the Exec, then the new Enter; the previous id and the changed flag are set.
    CHECK((log == std::vector<std::string>{"exit 4", "exec", "enter 6"}));
    CHECK(actor.GetCurrentContextID() == HumanContextId::InAir);
    CHECK(actor.GetPreviousContextID() == HumanContextId::Ground);
    CHECK(actor.HasContextChanged());
}

TEST(context_switch_to_a_missing_context_changes_nothing) {
    Human human;
    std::vector<std::string> log;
    TestActor actor(human, log);
    actor.SetInitialContext(AIActor::kMainTrack, HumanContextId::Ground);
    actor.Enter();
    log.clear();
    CHECK(!actor.TransitToContext(HumanContextId::Climb));  // PORT: the port has no Climb context yet
    CHECK(log.empty());
    CHECK(actor.GetCurrentContextID() == HumanContextId::Ground);
    CHECK(!actor.HasContextChanged());
}

TEST(context_switched_before_the_update_skips_its_main_update_once) {
    // A controller's command switches before Human::Update: the main track's Update(0) is skipped that frame, but
    // the new context's Update(1) runs (RE/10_human_update.md 3, "Ordering within the frame"; live row 10).
    Human human;
    std::vector<std::string> log;
    TestActor actor(human, log);
    actor.SetInitialContext(AIActor::kMainTrack, HumanContextId::Ground);
    actor.Enter();
    Frame f;
    actor.TransitToContext(HumanContextId::InAir);
    log.clear();
    actor.UpdateTracks(0, f.args);
    CHECK(log.empty());
    CHECK(!actor.HasContextChanged());
    actor.UpdateAfterPhysics(f.args);
    CHECK((log == std::vector<std::string>{"update1 6"}));
    log.clear();
    actor.UpdateTracks(0, f.args);  // next frame
    CHECK((log == std::vector<std::string>{"update0 6"}));
}

TEST(context_switched_in_its_own_update_gets_phase_1_the_same_frame) {
    Human human;
    std::vector<std::string> log;
    TestActor actor(human, log);
    actor.SetInitialContext(AIActor::kMainTrack, HumanContextId::Ground);
    actor.Enter();
    actor.ground.switchOnPhase = 0;
    actor.ground.switchTo = HumanContextId::InAir;
    Frame f;
    log.clear();
    actor.UpdateTracks(0, f.args);
    CHECK((log == std::vector<std::string>{"update0 4", "exit 4", "enter 6"}));
    CHECK(!actor.HasContextChanged());  // cleared after the update that switched
    log.clear();
    actor.UpdateAfterPhysics(f.args);
    CHECK((log == std::vector<std::string>{"update1 6"}));
}

TEST(human_spawns_entering_ground_through_init_main_track) {
    // Human::CreateAI / AIActor::Enter: Ground is the main track's initial context; InitMainTrack hands it a default
    // TransitionSetupDataToMovement (MovementState WaitLow), so EnterMove starts in Wait.
    Human human;
    CHECK(human.GetCurrentContextID() == HumanContextId::Ground);
    CHECK(human.GetPreviousContextID() == HumanContextId::None);
    CHECK(human.GetContextBundle().GetGround().GetMoveState() == scimitar::ac2::HumanGround::State::Wait);
    CHECK(human.m_IsEdgeClampingActive);  // EnterWait
}

TEST(human_landing_hands_over_with_exec_before_ground_enter) {
    // InAir -> Ground: ExecMainToGroundMain sets the ground's entry data before HumanGround::Enter reads it, so a
    // 3+ m drop enters LandingHard.
    port::BoxWorld world;
    Human human;
    human.m_Position = {0, 0, 5};
    human.ForceContext(HumanContextId::InAir);
    port::MoveInput none;
    for (int i = 0; i < 120 && human.GetCurrentContextID() == HumanContextId::InAir; ++i)
        human.Update(none, world, 1.0f / 30.0f);
    CHECK(human.GetCurrentContextID() == HumanContextId::Ground);
    CHECK(human.GetPreviousContextID() == HumanContextId::InAir);
    CHECK(human.GetContextBundle().GetGround().GetMoveState() == scimitar::ac2::HumanGround::State::LandingHard);
}
