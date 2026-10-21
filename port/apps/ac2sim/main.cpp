// ac2sim: run the movement headless and print or save the timeline.
//
//   ac2sim                  the built-in demo (run forward off a 3 m box), context timeline on stdout
//   ac2sim --csv out.csv    also write every frame as CSV

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "port/game_dir.h"
#include "port/sim.h"

int main(int argc, char** argv) {
    using namespace port;
    const char* csvPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) csvPath = argv[++i];
    }

    if (auto dir = findGameDir())
        std::printf("game install: %s\n", dir->string().c_str());
    else
        std::printf("game install: not found (set AC2_GAME_DIR); running on a test world\n");

    BoxWorld world;
    world.add({{-5, -5, 0}, {5, 5, 3}});
    Sim sim(world, {.position = {0, 0, 3}});
    sim.run(4.0f, [](float) {
        MoveInput in;
        in.moveDir = {0, 1, 0};
        in.moveMag = 1;
        return in;
    });

    HumanContextId last = HumanContextId::Count;
    for (const Frame& f : sim.timeline()) {
        if (f.context != last) {
            std::printf("%6.3f s  %-8s pos (%.2f, %.2f, %.2f)\n", f.t,
                        std::string(scimitar::ac2::HumanContextName(f.context)).c_str(), f.position.x, f.position.y,
                        f.position.z);
            last = f.context;
        }
    }
    if (csvPath) {
        std::ofstream out(csvPath);
        writeTimelineCsv(out, sim.timeline());
        std::printf("wrote %zu frames to %s\n", sim.timeline().size(), csvPath);
    }
    return 0;
}
