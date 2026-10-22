#include "scimitar/ai_actor.h"

namespace scimitar {

AIActor::AIActor() {
    for (int i = 0; i < kTrackCount; ++i) m_Tracks[i].m_TrackID = i;
}

void AIActor::SetInitialContext(long track, ac2::HumanContextId id, std::function<void()> initFunctor) {
    m_Tracks[track].m_InitialContextID = id;
    m_Tracks[track].m_InitFunctor = std::move(initFunctor);
}

void AIActor::Enter() {
    for (long track = kTrackCount - 1; track >= 0; --track) {
        ActorTrack& t = m_Tracks[track];
        if (t.m_InitialContextID == ac2::HumanContextId::None) continue;
        ActorContext* context = GetContextByID(track, t.m_InitialContextID);
        if (!context) continue;
        if (t.m_InitFunctor) t.m_InitFunctor();
        t.m_CurrentContext = context;
        context->Enter();
    }
}

void AIActor::UpdateTracks(long phase, const PortUpdateArgs& args) {
    for (ActorTrack& t : m_Tracks) {
        if (t.m_ContextHasChanged) {
            t.m_ContextHasChanged = false;
            continue;
        }
        if (t.m_CurrentContext) t.m_CurrentContext->Update(phase, args);
        if (t.m_ContextHasChanged) t.m_ContextHasChanged = false;
    }
}

void AIActor::UpdateAfterPhysics(const PortUpdateArgs& args) {
    ActorTrack& t = m_Tracks[kMainTrack];
    if (t.m_ContextHasChanged) {
        t.m_ContextHasChanged = false;
        return;
    }
    if (t.m_CurrentContext) t.m_CurrentContext->Update(1, args);
    if (t.m_ContextHasChanged) t.m_ContextHasChanged = false;
}

bool AIActor::TransitToContext(ac2::HumanContextId id, const std::function<void()>& exec) {
    ActorTrack& t = m_Tracks[kMainTrack];
    ActorContext* next = GetContextByID(kMainTrack, id);
    if (!next) return false;
    ActorContext* old = t.m_CurrentContext;
    if (old) old->Exit();
    t.m_PreviousContextID = old ? old->GetID() : ac2::HumanContextId::None;
    if (exec) exec();
    t.m_CurrentContext = next;
    t.m_ContextHasChanged = true;
    next->Enter();
    return true;
}

ac2::HumanContextId AIActor::GetCurrentContextID(long track) const {
    const ActorContext* c = m_Tracks[track].m_CurrentContext;
    return c ? c->GetID() : ac2::HumanContextId::None;
}

void AIActor::PortForceContext(long track, ActorContext* context) {
    m_Tracks[track].m_CurrentContext = context;
    m_Tracks[track].m_ContextHasChanged = false;
}

}  // namespace scimitar
