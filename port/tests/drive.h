#pragma once
// Ways to drive a Human in tests, as the game does: the player's controller (AssassinTask) or an AI-style driver that
// sets the requests through IHumanGround directly. Each runs the Human for one frame after.

#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/assassin_task.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"

// The player: AssassinTask, then the Human (as port::Sim::step does).
inline void PlayerStep(scimitar::ac2::AssassinTask& task, scimitar::ac2::Human& human, const port::MoveInput& in,
                       const port::World& world, float dt) {
    task.Update(in, human, dt);
    human.Update(in, world, dt);
}

// An AI-style driver: the direction, the stick as the speed ratio, break hard while it is 0, and the buttons as
// sprint / movement type. A test fixture, not the game's navigation (GroundNavigationTask isn't decoded).
inline void AiStep(scimitar::ac2::Human& human, const port::MoveInput& in, const port::World& world, float dt) {
    scimitar::ac2::IHumanGround& ground = human.GetContextBundle().GetGround();
    ground.SetDirection(in.moveDir);
    ground.SetSpeedRatio(in.moveMag);
    ground.SetBreakHard(in.moveMag <= 0.0f);
    ground.SetSprint(in.held(port::kLegs));
    ground.SetMovementType(in.held(port::kHighProfile) ? 1 : 0);
    human.Update(in, world, dt);
}
