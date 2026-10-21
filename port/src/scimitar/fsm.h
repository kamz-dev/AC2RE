#pragma once
// The engine's state machines. What the Mac symbols show so far (IDB/scimitar_sf.i64, 2026-10-21):
//
// - Each Human context is a state machine class: scimitar::ac2::HumanGround, HumanGroundHurtState, ...
// - LinkFSM() builds the machine (HumanGround::LinkFSM, Mac 0xFA4130, 0x8D9 bytes).
// - Each state has Enter<State>() / Exit<State>() / Update<State>() methods (HumanGround::EnterFrozen, ExitPush, ...).
// - Transitions are pairs of Test<Transition>(const FSMParam*, FSMParam*) -> bool and Exec<Transition>(const
//   FSMParam*), some named after an interface command, e.g.
//   HumanGroundHurtState::TestFallToInAirOnHumanGroundInterfaceCommand_FallOffEdge.
//
// The port reproduces each context's FSM state by state once its LinkFSM table is decoded (RE/ doc per context).
// Until then the contexts use the stand-ins marked PORT:.

namespace scimitar {

// Mac: scimitar::FSMParam (passed to every Test / Exec). Layout unknown.
struct FSMParam;

}  // namespace scimitar
