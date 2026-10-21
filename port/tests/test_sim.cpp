#include <sstream>

#include "check.h"
#include "port/sim.h"

using namespace port;

TEST(sim_is_deterministic) {
    BoxWorld world;
    world.add({{-2, 1, 0}, {2, 3, 1}});
    auto script = [](float t) {
        MoveInput in;
        in.moveDir = t < 1 ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
        in.moveMag = 1;
        return in;
    };
    Sim a(world), b(world);
    a.run(3.0f, script);
    b.run(3.0f, script);
    CHECK(a.timeline().size() == b.timeline().size());
    CHECK(a.human().m_Position == b.human().m_Position);
}

TEST(sim_frame_count_matches_duration) {
    BoxWorld world;
    Sim sim(world);
    sim.run(2.0f, [](float) { return MoveInput{}; });
    CHECK(sim.timeline().size() == 60);
}

TEST(timeline_csv_has_header_and_rows) {
    BoxWorld world;
    Sim sim(world);
    sim.run(0.1f, [](float) { return MoveInput{}; });
    std::ostringstream out;
    writeTimelineCsv(out, sim.timeline());
    std::string s = out.str();
    CHECK(s.rfind("t,x,y,z,", 0) == 0);
    size_t lines = 0;
    for (char ch : s) lines += ch == '\n';
    CHECK(lines == sim.timeline().size() + 1);
}
