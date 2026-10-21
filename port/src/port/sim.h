#pragma once
// The headless simulation harness: runs one Human at a fixed timestep with no window and records a timeline that
// can be compared frame by frame with a lab recording of the real game (docs/AC1_WORKFLOW.md, section 4).

#include <functional>
#include <ostream>
#include <vector>

#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"

namespace port {

using scimitar::Vec3;
using scimitar::ac2::Human;
using scimitar::ac2::HumanContextId;

struct Frame {
    float t = 0;
    Vec3 position;
    Vec3 velocity;
    float yaw = 0;
    HumanContextId context = HumanContextId::Ground;
    MoveInput input;
};

struct SpawnState {
    Vec3 position;
    float yaw = 0;
    HumanContextId context = HumanContextId::Ground;
};

class Sim {
public:
    // PORT: 30 Hz until the game's movement tick is confirmed (LIVE).
    static constexpr float kDefaultDt = 1.0f / 30.0f;

    explicit Sim(const World& world, SpawnState spawn = {}, float dt = kDefaultDt);

    void step(const MoveInput& in);
    // Run `seconds` of game time, asking `script(t)` for each frame's input.
    void run(float seconds, const std::function<MoveInput(float t)>& script);

    const Human& human() const { return human_; }
    Human& human() { return human_; }
    const std::vector<Frame>& timeline() const { return timeline_; }
    float time() const { return time_; }

private:
    const World& world_;
    Human human_;
    float dt_;
    float time_ = 0;
    std::vector<Frame> timeline_;
};

// The timeline as CSV, with column names shared with the lab's frames.csv where they overlap.
void writeTimelineCsv(std::ostream& out, const std::vector<Frame>& timeline);

}  // namespace port
