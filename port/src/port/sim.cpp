#include "port/sim.h"

namespace port {

Sim::Sim(const World& world, SpawnState spawn, float dt) : world_(world), dt_(dt) {
    human_.m_Position = spawn.position;
    human_.m_Yaw = spawn.yaw;
    human_.ForceContext(spawn.context);
}

void Sim::step(const MoveInput& in) {
    task_.Update(in, human_, dt_);
    human_.Update(in, world_, dt_);
    time_ += dt_;
    timeline_.push_back(
        {time_, human_.m_Position, human_.m_Velocity, human_.m_Yaw, human_.GetCurrentContextID(), in});
}

void Sim::run(float seconds, const std::function<MoveInput(float t)>& script) {
    const float end = time_ + seconds - dt_ * 0.5f;
    while (time_ < end) step(script(time_));
}

void writeTimelineCsv(std::ostream& out, const std::vector<Frame>& timeline) {
    out << "t,x,y,z,vx,vy,vz,yaw,ctx,mdx,mdy,mmag,buttons\n";
    for (const Frame& f : timeline) {
        out << f.t << ',' << f.position.x << ',' << f.position.y << ',' << f.position.z << ',' << f.velocity.x << ','
            << f.velocity.y << ',' << f.velocity.z << ',' << f.yaw << ','
            << scimitar::ac2::HumanContextName(f.context) << ',' << f.input.moveDir.x << ',' << f.input.moveDir.y
            << ',' << f.input.moveMag << ',' << f.input.buttons << '\n';
    }
}

}  // namespace port
