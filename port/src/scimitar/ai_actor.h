#pragma once
// scimitar::AIActor: the engine's actor, Human's base class (RE/10_human_update.md). It runs the actor's contexts on
// five tracks and switches a track's context with TransitToContext. PC: Update 0x7AC40, UpdateTracks 0x7A8C0,
// UpdateAfterPhysics 0x7AC50, TransitToContext 0x7A9D0 / 0x7AC80 / 0x7B1D0, Enter 0x7AE30, SetInitialContext 0x7A830
// (Mac 0x2AFAE0, 0x2AFA70, 0x2AFA30, 0x2AFBB0 / 0x2AFC20 / 0x2AFCB0, 0x2AFB70, 0x2AF740).

#include <functional>

#include "scimitar/ac2/human_context.h"

namespace scimitar {

// scimitar::ActorTrack (Revelations PDB names), 0x30 bytes; AC2 puts the first at AIActor +0x04.
struct ActorTrack {
    bool m_ContextHasChanged = false;  // +0x00 set by a switch; the track's next update is skipped once
    // +0x04 m_ContextList. PORT: the Human's context bundle stands in (AIActor::GetContextByID).
    ac2::HumanContextId m_InitialContextID = ac2::HumanContextId::None;   // +0x0C
    ac2::HumanContextId m_PreviousContextID = ac2::HumanContextId::None;  // +0x10
    ActorContext* m_CurrentContext = nullptr;                             // +0x14
    std::function<void()> m_InitFunctor;  // +0x18 a Functor0, run once before the initial context is entered
    long m_TrackID = 0;                   // +0x2C
};

class AIActor {
public:
    static constexpr int kTrackCount = 5;
    // The main AI track: the locomotion contexts (Human::CreateAI PC 0x9333A0). Tracks 2-4 hold Vocalization, LookAt
    // and UpperBody in the game; the port has none of them.
    static constexpr long kMainTrack = 1;

    AIActor();
    virtual ~AIActor() = default;

    // The context with that id on the track; Human's (PC 0x91CD80, vftable slot 8) creates a missing track-1
    // context and runs its LocalInit. Null when the port doesn't have the context (PORT).
    virtual ActorContext* GetContextByID(long track, ac2::HumanContextId id) = 0;

    // PC 0x7A830: the context a track starts in, and the functor run before it is first entered.
    void SetInitialContext(long track, ac2::HumanContextId id, std::function<void()> initFunctor = {});
    // PC 0x7AE30: the actor enters the world; each track (4 down to 0) runs its init functor and enters its initial
    // context.
    void Enter();

    // PC 0x7AC40 -> UpdateTracks(0) PC 0x7A8C0: every track's context runs Update(0), except a track that switched
    // since its last update, which is skipped once. A track that switches during its own update clears its flag.
    void UpdateTracks(long phase, const PortUpdateArgs& args);
    // PC 0x7AC50: track 1 only, Update(1), under the same rule.
    void UpdateAfterPhysics(const PortUpdateArgs& args);

    // PC 0x7AC80 (Main transitions) / 0x7B1D0 (interface commands), on track 1, immediately: the new context (created
    // if needed), the old one's Exit, the previous id, the transition's Exec, the new current context and the changed
    // flag, the new one's Enter. PORT: false (and nothing changes) when the port doesn't have the context.
    bool TransitToContext(ac2::HumanContextId id, const std::function<void()>& exec = {});

    ActorContext* GetCurrentContext(long track = kMainTrack) const { return m_Tracks[track].m_CurrentContext; }
    ac2::HumanContextId GetCurrentContextID(long track = kMainTrack) const;
    ac2::HumanContextId GetPreviousContextID(long track = kMainTrack) const {
        return m_Tracks[track].m_PreviousContextID;
    }
    bool HasContextChanged(long track = kMainTrack) const { return m_Tracks[track].m_ContextHasChanged; }
    // PORT: the last context a TransitToContext asked for that the port doesn't have (None if none yet).
    ac2::HumanContextId GetPortRefusedContextID() const { return m_PortRefusedContextID; }

protected:
    // PORT: puts a track in a context without Exit / Enter (tests, spawning straight into a context).
    void PortForceContext(long track, ActorContext* context);

    ActorTrack m_Tracks[kTrackCount];
    ac2::HumanContextId m_PortRefusedContextID = ac2::HumanContextId::None;
};

}  // namespace scimitar
