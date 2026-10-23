#include "scimitar/ac2/human_ledge.h"

#include "port/world.h"
#include "scimitar/ac2/human.h"

namespace scimitar::ac2 {

void HumanLedge::Enter() {
    // EnterHumanLedge (Mac 0x13477BA). PORT: the capsule, layers, IK, presentation and accomplishment events and the
    // water well aren't ported; only the choice of the state to enter.
    if (m_Human.GetLedgeData().m_SubState == HumanLedgeData::SubState_ParallelJump)
        m_Human.GetLedgeData().m_SubState = HumanLedgeData::SubState_HandPlacement;
    EnterState(EntryStateFromSubState());
}

HumanLedge::State HumanLedge::EntryStateFromSubState() const {
    switch (m_Human.GetLedgeData().m_SubState) {
        case HumanLedgeData::SubState_Entry: return State::HangEntry;
        case HumanLedgeData::SubState_Movement: return State::Movement;
        case HumanLedgeData::SubState_HandPlacement: return State::Movement;  // case 14 (ParallelJump), rewritten to 3
        case HumanLedgeData::SubState_Pullup: return State::Pullup;
        case HumanLedgeData::SubState_HangWallReception: return State::HangWallReception;
        case HumanLedgeData::SubState_HangFreeReception: return State::HangFreeReception;
        case HumanLedgeData::SubState_SwingReception: return State::Swing;
        case HumanLedgeData::SubState_TransitionInFromClimb: return State::Movement;  // PORT: no SetupTransitionFromClimb
        case HumanLedgeData::SubState_PullDown: return State::PullDown;
        case HumanLedgeData::SubState_HandPassOver: return State::HandPassOver;
        case HumanLedgeData::SubState_Grasp: return State::GraspEntry;
        case HumanLedgeData::SubState_SwimEntry: return State::SwimEntry;
        case HumanLedgeData::SubState_PullupToActorGrab:
        case HumanLedgeData::SubState_PullupToActorAssassinate: return State::PullupToActorInterraction;
        case HumanLedgeData::SubState_JumpToActorAssassinate: return State::Pullup;  // PORT: no SetupJumpToActorAssassinate
        default: return State::Movement;  // the game keeps the root's initial state (+0xDE1; LinkFSM: Movement)
    }
}

void HumanLedge::EnterState(State state) {
    m_State = state;
    switch (state) {
        case State::Movement: m_MovementState = State::Wait; break;  // Movement's initial child (LinkFSM)
        case State::PullDown: EnterPullDown(); break;
        case State::Pullup: EnterPullup(); break;
        default: break;  // PORT: the other states' Enters aren't ported
    }
}

void HumanLedge::Update(long phase, const PortUpdateArgs& args) {
    // _UpdateHumanLedge (PC 0x6711A0). PORT: UpdateHumanLedgeMain (PC 0x64F790), the root's transitions to InAir / Swim
    // (rope and water) and the after-physics phase (PC 0x64F860) aren't ported.
    if (phase != 0) return;
    switch (m_State) {
        case State::PullDown: {  // _UpdatePullDown (PC 0x66A880, Mac 0x132D2CA)
            UpdatePullDownMain(args.dt);
            HumanLedgeData& data = m_Human.GetLedgeData();
            if (data.m_PullDownSubState == HumanLedgeData::PullDownSubState_ReleaseToInAir) {
                m_Human.TransitToContext(HumanContextId::InAir);  // _TransitFromPullDownToInAirMain
            } else if (TestPullDownToHangEntryMain()) {
                // _TransitFromPullDownToHangEntryMain. PORT: who resets PullDownSubState isn't decoded; live it reads 0
                // from HangEntry on (RE/20 row 15).
                data.m_PullDownSubState = HumanLedgeData::PullDownSubState_None;
                EnterState(State::HangEntry);
            }
            break;
        }
        case State::HangEntry:  // _UpdateHangEntry (PC 0x66F600, Mac 0x133DE52)
            UpdateHangEntryMain();
            if (m_Human.GetLedgeData().m_SubState == HumanLedgeData::SubState_Movement) EnterState(State::Movement);
            break;
        case State::Movement:
            // Movement's children: Wait (PC 0x66F510) and HandPlacement. PORT: UpdateWaitMain (Mac 0x133B696: moving
            // along the ledge) isn't ported; the hang holds still. The release and the pull-up come through the
            // interface (DoRelease, DoPullup).
            break;
        case State::Pullup: UpdatePullupMain(args.dt); break;  // _UpdatePullup (PC 0x670A40)
        default: break;  // PORT: not ported
    }
}

// --- PullDown (RE/13_ledge_climb.md 5) ---

Vec3 HumanLedge::PortAtEdge(float out, float up) const {
    const Gap& gap = m_Human.GetLedgeData().m_PullDownGap;
    const Vec3 n = normalized(Vec3{gap.m_EdgeNormal.x, gap.m_EdgeNormal.y, 0});
    return gap.m_EdgePosition + n * out + Vec3{0, 0, up};
}

void HumanLedge::PortStartMove(const Vec3& to, float length) {
    m_PortMoveFrom = m_Human.m_Position;
    m_PortMoveTo = to;
    m_PortMoveLength = length;
    m_PortMoveTime = 0;
}

float HumanLedge::PortStepMove(float dt) {
    m_PortMoveTime += dt;
    float ratio = m_PortMoveLength > 0 ? m_PortMoveTime / m_PortMoveLength : 1.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    m_Human.m_Position = m_PortMoveFrom + (m_PortMoveTo - m_PortMoveFrom) * ratio;
    return ratio;
}

void HumanLedge::EnterPullDown() {
    // EnterPullDown (Mac 0x133DE9E): the action g_PullDownActionTable[PullDownType][PullDownSide], a move along it to
    // the anchor under the gap's edge, PullDownSubState = Orientation. PORT: the move goes to the root's live
    // position at the end of the Orientation over its live length (PortLedgeMoves); the body turns to face the wall at
    // once (the game turns it along the action, with a 0.2 s reference-rotation delay).
    HumanLedgeData& data = m_Human.GetLedgeData();
    const PortLedgeMoves& moves = m_Human.GetPortLedgeMoves();
    const Vec3 n = normalized(Vec3{data.m_PullDownGap.m_EdgeNormal.x, data.m_PullDownGap.m_EdgeNormal.y, 0});
    m_Human.m_Velocity = {};
    m_Human.m_Yaw = yawFromDirection(n * -1.0f);
    const int type = data.m_PullDownType == HumanLedgeData::PullDownType_Ground_Soft_Wait ? 1 : 0;
    PortStartMove(PortAtEdge(moves.orientationEnd.out, moves.orientationEnd.up), moves.orientation[type]);
    data.m_PullDownSubState = HumanLedgeData::PullDownSubState_Orientation;
}

void HumanLedge::UpdatePullDownMain(float dt) {
    // Mac 0x132BBA8: each step's move runs to its end (GetProgressionRatio >= 1), then the next one is set up.
    HumanLedgeData& data = m_Human.GetLedgeData();
    const PortLedgeMoves& moves = m_Human.GetPortLedgeMoves();
    const float ratio = PortStepMove(dt);
    if (ratio < 1.0f) return;
    if (data.m_PullDownSubState == HumanLedgeData::PullDownSubState_Orientation) {
        // The hands' anchors either side of the anchor point (GetNextVerticalLimbPos Mac 0xD206F8). PORT: always found,
        // the gap's edge being a guidance line; the move goes 0.8 m down (the decoded drop) to the live position.
        PortStartMove(PortAtEdge(moves.descentEnd.out, moves.descentEnd.up), moves.descent);
        data.m_PullDownSubState = HumanLedgeData::PullDownSubState_Descent;
    } else if (data.m_PullDownSubState == HumanLedgeData::PullDownSubState_Descent) {
        // The toes' anchors on the wall: a wall hang (action 0x6E8BD81, ComputeHangWallEntityPosition), else a free
        // hang (action 0x82F9F3B, ComputeHangFreeEntityPosition).
        if (PortHasWallUnderEdge()) {
            data.m_LedgeHangType = HumanLedgeData::LedgeHangType_Wall;
            PortStartMove(PortAtEdge(moves.hangWall.out, moves.hangWall.up), moves.receptionWall);
        } else {
            data.m_LedgeHangType = HumanLedgeData::LedgeHangType_Free;
            PortStartMove(PortAtEdge(moves.hangFree.out, moves.hangFree.up), moves.receptionFree);
        }
        data.m_PullDownSubState = HumanLedgeData::PullDownSubState_Reception;
    }
}

bool HumanLedge::TestPullDownToHangEntryMain() const {
    // Mac 0x1325458: the current action is the wall or free reception and its move is done.
    return m_Human.GetLedgeData().m_PullDownSubState == HumanLedgeData::PullDownSubState_Reception &&
           m_PortMoveTime >= m_PortMoveLength;
}

bool HumanLedge::PortHasWallUnderEdge() const {
    const port::World* world = m_Human.GetPortWorld();
    if (!world) return false;
    const PortLedgeMoves& moves = m_Human.GetPortLedgeMoves();
    const Gap& gap = m_Human.GetLedgeData().m_PullDownGap;
    const Vec3 n = normalized(Vec3{gap.m_EdgeNormal.x, gap.m_EdgeNormal.y, 0});
    const Vec3 from = gap.m_EdgePosition + n * moves.toesReach + Vec3{0, 0, -moves.toesDepth};
    return world->rayCast(from, n * -1.0f, moves.toesReach + 0.1f, 12).has_value();
}

// --- HangEntry and the hang ---

void HumanLedge::UpdateHangEntryMain() {
    // Mac 0x133DB24: once the reception action has finished, the hang's idle by LedgeHangType and SubState = 1. PORT:
    // the reception's length is the PullDown's last move; the idle has no length. Live, Wait follows within a frame.
    m_Human.GetLedgeData().m_SubState = HumanLedgeData::SubState_Movement;
}

void HumanLedge::SetDirection(const Vec3& direction) { m_Human.GetLedgeData().m_CurDirection = direction; }
void HumanLedge::SetSpeedRatio(float ratio) { m_Human.GetLedgeData().m_CurSpeedRatio = ratio; }
void HumanLedge::SetDirectionType(std::uint32_t type) { m_Human.GetLedgeData().m_DirectionType = type; }
void HumanLedge::SetRelativeDirection(const Vec3& direction) {
    m_Human.GetLedgeData().m_CurRelativeDirection = direction;
}

bool HumanLedge::CanRelease() {
    // Command 3 (_TestInterfaceTransitionsHumanLedge == 2): offered by Movement when the current action can be
    // interrupted by a release (TestMovementToInAirOnInterfaceCommand_Release Mac 0x1324584). PORT: the actions always
    // can; the other states' release transitions (Pullup, the swings, HangWallReception) aren't ported.
    return m_State == State::Movement;
}

void HumanLedge::DoRelease() {
    // _TransitFromMovementToInAirOnInterfaceCommand_Release, with ExecMovementToInAirOnInterfaceCommand_Release (Mac
    // 0x13407A2) -> ReleaseToInAir (Mac 0x13402EA). PORT: ReleaseToInAir isn't decoded; live the fall starts from the
    // hang position at once (RE/20 row 15b).
    if (!CanRelease()) return;
    m_Human.TransitToContext(HumanContextId::InAir, [this] { m_Human.m_Velocity = {}; });
}

bool HumanLedge::CanPullup() {
    // Command 0: from Movement, EvaluatePullupFromMovement(1, 0, 1) (Mac 0x132EDCA; 3 at a water well). PORT: not
    // decoded; the port's ledges always have room.
    return m_State == State::Movement;
}

void HumanLedge::DoPullup() {
    // _TransitFromMovementToPullupOnInterfaceCommand_Pullup (ExecMovementToPullupOnInterfaceCommand_Pullup Mac
    // 0x13312C0). Live, SubState reads 4 (Pullup) in the Pullup state.
    if (!CanPullup()) return;
    m_Human.GetLedgeData().m_SubState = HumanLedgeData::SubState_Pullup;
    EnterState(State::Pullup);
}

// --- Pullup ---

void HumanLedge::EnterPullup() {
    // EnterPullup (Mac 0x131C75C). PORT: a timed move up onto the roof, to the live end position, over the pull-up's
    // live length plus the NarrowObject stretch that follows it in the game.
    const PortLedgeMoves& moves = m_Human.GetPortLedgeMoves();
    const bool wall = m_Human.GetLedgeData().m_LedgeHangType != HumanLedgeData::LedgeHangType_Free;
    const PortLedgeMoves::Offset stand = wall ? moves.standWall : moves.standFree;
    PortStartMove(PortAtEdge(stand.out, stand.up), (wall ? moves.pullupWall : moves.pullupFree));
}

void HumanLedge::UpdatePullupMain(float dt) {
    // PORT: the game goes Pullup -> NarrowObject (ExecPullupToNarrowObjectMain; 0.27 s live) -> Ground. The port has
    // no NarrowObject context: it holds the end position for that time, then hands over to Ground in a standing wait
    // (Human::InitMainTrack's transition data).
    PortStepMove(dt);
    if (m_PortMoveTime < m_PortMoveLength + m_Human.GetPortLedgeMoves().pullupNarrowObject) return;
    m_Human.TransitToContext(HumanContextId::Ground, [this] { m_Human.InitMainTrack(); });
}

}  // namespace scimitar::ac2
