# 10: Human::Update and the context switch

Status 2026-10-22: static, read from the Mac symbols and matched on PC by call shape and vftable slots; the core
(`TransitToContext` 0x7B1D0, `UpdateTracks` 0x7A8C0, `_ExecInterfaceTransitionHumanGround` 0x637FA0) re-read on PC.
Covers how `scimitar::AIActor` holds the Human's contexts in tracks, how a frame updates them, and how one locomotion
context hands over to another. The functions are named in the PC IDB (open item 5). **Ported** (section "Port").

Addresses: **PC** first (IDB rebased to 0), Mac (`IDB/scimitar_sf.i64`) in brackets. Field names are the
Revelations PDB's (`py RE/tools/pdbtypes.py scimitar::AIActor`, `scimitar::ActorTrack`).

## 1. Tracks: where the contexts live
`scimitar::ac2::Human` derives from `scimitar::AIActor`. The AIActor holds **five `ActorTrack`s** of 0x30 bytes
each, starting at **+0x04** in AC2 (Revelations puts a flags byte there and starts them at +0x08). Each track runs
one context at a time:

| Track + | Field (PDB name) | Meaning |
|---|---|---|
| +0x00 | `m_ContextHasChanged` (bool) | set by a context switch; makes the next update of this track skip once |
| +0x04 | `m_ContextList` (`Array<ActorContext*>`, size word at +0x0A) | the contexts registered on this track |
| +0x0C | `m_InitialContextID` | the context id the track starts in |
| +0x10 | `m_PreviousContextID` | the id of the context it left last |
| +0x14 | `m_CurrentContext` (`ActorContext*`) | the running context |
| +0x18 | `m_InitFunctor` (`Functor0`, 0x10 bytes) | runs once before the initial context is entered |
| +0x28 / +0x2C | `m_Owner` (`AIActor*`) / `m_TrackID` | the actor and this track's index |

So for track *n*, the current context is at AIActor + 0x18 + 0x30·*n*. Evidence: `AIActor::UpdateTracks` PC
0x7A8C0 [0x2AFA70] (stride 48, flag at +4, context at +0x18), `AIActor::GetContextByID` PC 0x7B000 [0x2AF660],
`GetCurrentContext` [0x2AF510] and `GetPreviousContextID` [0x2AF4B0], `AIActor::Enter` PC 0x7AE30 [0x2AFB70].

**The Human's tracks** are filled by `Human::CreateAI` PC 0x9333A0 [0x12CD932] (PC Human vftable slot 9). It calls
`AIActor::AddContext(ctx, track)` PC 0x7B530 [0x2AFE70] and `AIActor::SetInitialContext(ctx, track)` PC 0x7A830
[0x2AF740]:

| Track | Contexts | Initial |
|---|---|---|
| 0 | none (hypothesis: nothing else adds one for a Human) | — |
| **1, the main AI track** | HumanGround at first; every locomotion context is added on demand (below) | Ground, with `Human::InitMainTrack` as the init functor (`InitializeInGroundState` PC 0x91CE20 [0x12CD874]) |
| 2 | Vocalization (id 20) | Vocalization |
| 3 | LookAt (id 23) | LookAt |
| 4 | UpperBody (id 27) | UpperBody |

So UpperBody, LookAt and Vocalization run **alongside** the locomotion context, each on its own track, as live
row 9 saw. Only track 1 ever switches in movement. HumanGround's LocalInit call `Human->vfunc+0x24(4, 0x1B)`
(`11_ground.md` 17) is `GetContextByID(track 4, UpperBody)`, so the object at HumanGround +0x790 is the
**UpperBody context**. (This settles that section's hypothesis.)

`AddContext` sets the context's actor pointer (`ctx +0x08`). Then it calls the context's `SetAIActorData` slot (PC slot
11) and appends the context to the track's list. `Human::InitializeInRidingState` PC 0x9179E0 [0x12C735E] makes
Riding (12) the initial context of track 1 instead.

**Human +0x48 is track 1's `m_CurrentContext`** (0x04 + 0x30 + 0x14), and Human +0x44 is its
`m_PreviousContextID`. Only four places write +0x48: the three `TransitToContext` overloads (section 3), the
track Enter inlined in `AIActor::Enter` PC 0x7AE30, and `AIActor::Exit` PC 0x7B060 [0x2AF870], which exits
every track's context and clears the tracks on despawn.

**Getting a context by id.** `Human::GetContextByID(track, id)` PC 0x91CD80 [0x12CD3AE] is PC Human vftable slot 8
(byte +0x20; Mac +0x24). It takes the entity lock, searches the track's list (`AIActor::GetContextByID` PC 0x7B000),
and for track 1 creates a missing context with `Human::AddContextToMainAITrack(id)` PC 0x91CB10 [0x12CD0AC]. That
function switches on the game's context id (live row 9) and calls `HumanContextBundle::CreateContextOnDemandHelper<T>`
for the bundle slot. Then it calls `AddContext(ctx, 1)` and the new context's `LocalInit` (PC slot 2). So a locomotion
context is created and LocalInit'ed the first time anything asks for it, usually at its first switch.

## 2. A frame
`Human::Update` PC 0x933480 [0x12EDA06] (PC Human vftable slot 14):
1. Caches the entity's global position (Human +0x130).
2. `AIActor::Update` PC 0x7AC40 [0x2AFAE0] → `UpdateTracks(0)` PC 0x7A8C0 [0x2AFA70]: for tracks 0..4 in order,
   - if `m_ContextHasChanged` is set, clear it and **skip** this track this frame;
   - otherwise call `m_CurrentContext->Update(0)` (PC slot 14, byte +0x38); if the flag is set afterwards (the
     context switched during its own update), clear it.
3. With Human +0xA2C set (cleared here), a skeleton pose ratio set from a query on the HumanGround (its IHumanGround slot +0x7E0; not movement).
4. `EdgeClampingComputation` PC 0x931330, then `UpdateUnbalanceIgnoreList` PC 0x92E420 (`11_ground.md` 16).

`Human::UpdateAfterPhysics` PC 0x922390 [0x12EDFBC] (PC slot 17) runs after the physics step:
- `UpdateGlobalPositionForPathfind`.
- **`AIActor::UpdateAfterPhysics` PC 0x7AC50 [0x2AFA30], which runs track 1 only.** If its flag is set it clears it and
  skips. Otherwise it calls `m_CurrentContext->Update(1)` and clears the flag if that update switched context.
- Then IK (`ProcessIK`, `HumanBody::SolveIK`), a few per-frame flag resets, `UpdateBumpingData` and
  `UpdateWaterData`.

So phase 1 ("after physics", `_UpdateX(long)`, `11_ground.md` 3) is the main track's alone. The other tracks get only
phase 0. AIActor has a separate base `UpdateAfterPhysics` context slot (PC slot 6, `ActorContext::UpdateAfterPhysics`
[0x509930], an empty body). The track code never calls it. The after-physics work goes through `Update(1)`.

## 3. The switch: `AIActor::TransitToContext`
Every hand-over between movement contexts goes through one of three `AIActor` methods. They all work on **track
1** (fixed offsets +0x34 … +0x60):

| PC | Mac | Signature | Used by |
|---|---|---|---|
| 0x7A9D0 | 0x2AFBB0 | `TransitToContext(long id)` | `Human::DoSwitchToDebugMode` PC 0x913A60 only |
| 0x7AC80 | 0x2AFC20 | `TransitToContext(long id, Functor0& exec)` | the FSM's **Main** transitions (`_TransitFromXToYMain`) |
| 0x7B1D0 | 0x2AFCB0 | `TransitToContext(long id, Functor1<const FSMParam*>& exec, const FSMParam*)` | **interface-command** transitions (`_TransitFromXToYOn…InterfaceCommand_Z`) |

The body, in the game's order:
1. `next = m_Owner->GetContextByID(m_TrackID, id)` (creating it if needed, section 1);
2. `old->Exit()` (PC slot 13, +0x34): the old context's FSM exits (section 4);
3. `m_PreviousContextID = old->GetActorContextData()->id` (PC slot 9; the id at data +4);
4. **the transition's Exec** (the functor, with the FSM parameter for an interface command). It runs after the
   old context's Exit and before the new one's Enter, so it can read the old context and write the new one's data;
5. `m_CurrentContext = next`, `m_ContextHasChanged = 1`;
6. `next->Enter()` (PC slot 12, +0x30).

**The switch is immediate.** It has no pending-context field and no end-of-frame step. The `_TransitFrom…` function
calls it in the middle of whatever is running, and the FSM code returns right after.

The generated wrappers only build the functor:
- `HumanGround::_TransitFromHumanGroundToInAirMain` PC 0x5F1370 [0xF9A8A4]: `ExecHumanGroundToInAirMain`, id 6.
- `_TransitFromMoveToInAirMain` PC 0x5F14C0 [0xF9AA74], `_TransitFromNavigationToInAirMain` PC 0x5FA7D0 [0xF9AA00],
  `_TransitFromVulnerableToInAirMain` PC 0x5F13E0 [0xF9A918]: id 6.
- `_TransitFromHumanGroundToInAirOnHumanGroundInterfaceCommand_FallOffEdge` PC 0x60BA30 [0xF9951A]: Exec PC
  0x6056F0 [0xFCB952] → `ExecuteFallOffEdge` PC 0x5FC890 [0xFCB4B2], id 6.
- `_TransitFromMoveToLedgeOnHumanGroundInterfaceCommand_LedgePullDown` [0xF99D34] and `…EdgeStopToLedge…` [0xF9A45A]:
  Exec `ExecMoveToLedgeOnHumanGroundInterfaceCommand_LedgePullDown` [0xF98DA6], id 7 (Ledge). PC wrappers not
  located yet.
- `HumanInAir::_TransitFromMainToGroundMain` PC 0x69DB60 [0x122AA54]: `ExecMainToGroundMain` PC 0x69B730, id 4.

The same wrapper shape (Mac size 0x74 / 0x79) appears in every context: Boat, Riding, Kiosk, NarrowObjectBeam,
RagdollGround, GliderPilot, CannonOperator, the HumanGround sub-machines (`HumanGroundHurtState`
`_TransitFromFallToInAirOnHumanGroundInterfaceCommand_FallOffEdge` [0xE0EDAC]), and others. No other switch path
exists.

### Where a switch starts
- **Main transitions** are tested inside the context's own `Update(0)`. For example `_UpdateHumanGround` PC
  0x6381D0 [0xFED468] runs `UpdateHumanGroundMain`, then `TestHumanGroundToInAirMain` → transit, and then
  **returns**. The rest of the tree isn't updated that frame. The children (`_UpdateVulnerable`, `_UpdateMove` …)
  test their own Main transitions the same way.
- **Interface commands** run inside the caller, usually the controller (`AssassinTask`, `21_input.md`):
  - **`CanX`** (e.g. `CanFallOffEdge` PC 0x62F0D0 [0xFDA29A]) calls `_TestInterfaceTransitionsHumanGround(cmd, param)`
    PC 0x62B9B0 [0xFD773A]. That walks the active branch, runs the matching `Test…` and records each Test that passes
    in the `FSMSuccessfulInterfaceTransitionTracker` (HumanGround +0x44, `11_ground.md` 2); 2 means success.
  - **`DoX`** (e.g. `DoFallOffEdge` PC 0x639E20 [0xFEA7BE], command 86) calls `_ExecInterfaceTransitionHumanGround`
    PC 0x637FA0 [0xFE7A20]. It descends the active branch first (deepest state first). At each level, when that
    level's Test for the command is in the tracker, it clears the tracker and calls the `_TransitFrom…` wrapper at
    once.
  - The tracker is cleared at the start of every `_UpdateHumanGround` (both phases). So a `Do` only works after a
    `Can` that passed **since the context's last update**: in practice, the same controller frame.

### Ordering within the frame
- A switch made by the **controller** (before `Human::Update`, hypothesis on the exact call order; see the live
  note) sets the flag. In `UpdateTracks(0)` track 1 then skips: **neither context gets a main update that frame**.
  In `UpdateAfterPhysics` the flag is clear, so **the new context's `Update(1)` runs the same frame**.
- A switch made by the **old context's own `Update(0)`** sets the flag, which is cleared right after that update.
  `UpdateAfterPhysics` then runs the new context's `Update(1)` the same frame.
- Either way the new context's first `Update(0)` is on the **next frame**, and its `Enter` has already run in the
  middle of this one.
- Live row 10 fits this: on each fall's first InAir frame, `m_LandingSpeedRatio` still held the previous value. The
  controller had run its Ground branch, sent FallOffEdge, and the switch happened inside that call. `UpdateInAir`
  ran from the next frame on.

## 4. Exit and Enter
**Exit runs the whole active branch's exit chain.** `HumanGround::Exit` PC 0x6367F0 [0x1543BD4] → `_ExitHumanGround`
PC 0x634C00 [0xFC57B6]:
- calls the current child's `_Exit` (`_ExitVulnerable` PC 0x630610, which recurses to the leaf);
- then `ExitHumanGround` PC 0x60C400 [0xFC25D8];
- then sets the root's current id (+0x42) to 0.

So a Ground state with Enter / Exit pairs (the edge-clamping states of `11_ground.md` 16, MoveCycle's
`ExitMoveCycle`, …) gets its Exit when the context is left. The leaf's Exit runs first, the root's last.

**Enter restarts the FSM from its initial states.** `HumanGround::Enter` PC 0x633730 [0x1544B6A] → `_EnterHumanGround`
PC 0x628710 [0xFEC5B4]:
1. `EnterHumanGround` PC 0x627490 [0xFC061C] does the context-wide setup:
   - `PushState(1)`;
   - character controller sticking on at 0.58 and bumping on at 0.37;
   - the collision layer, unless the entry is Agonizing;
   - the feet IK;
   - and it picks the root's initial child (+0x41) from **`HumanGroundData` +0x250, the entry state**, which the
     handing-over Exec sets (`m_EntryState`, `11_ground.md` 11):

   | Entry state | Root child |
   |---|---|
   | 0, 7, 8 | **Vulnerable** (normal) |
   | 1 | HumanGroundHurt |
   | 2, 3, 4 | Assassinate (`AssassinateState::SetupExternalAssassination`) |
   | 6 | HumanGroundAgonizing (layer left alone) |
   | 9 | LayingDownHurt |
   | 10 | Tackle (`TackleState::SetupExternalTackle`) |

2. Current := initial (+0x42 := +0x41), then the child's `_Enter`. `_EnterVulnerable` PC 0x626150 [0xFE7058] enters
   its own initial child (Navigation in normal play), and so on down. Move's `EnterMove` picks Move's first child
   from `HumanGroundData`'s **MovementState** (+0x1A0), as in `11_ground.md` 11.

**The data handed over** goes through the Exec of step 4 in section 3, before `Enter`. InAir → Ground:
`ExecMainToGroundMain` PC 0x69B730 writes `TransitionSetupDataToMovement` into `HumanGroundData`. `SetupTransitionData`
PC 0x90EFA0 copies the MovementState, landing action, `TransitionToMotionType`, rotation ratio and step (or, with an
air assassination / tackle, only the entry state). Ground → InAir: `ExecuteFallOffEdge` PC 0x5FC890 gets the Gap and
the `FallOffEdgeParam` float (`param +0x44`). Ground → Ledge: the LedgePullDown Exec copies the gap into the Ledge
data (`11_ground.md` 17). The game's first Ground entry at spawn goes through `Human::InitMainTrack` PC 0x914F20
[0x12C3820], the track-1 init functor. It calls `SetupTransitionData` with a default `TransitionSetupDataToMovement`:
MovementState 0 (WaitLow), `TransitionToMotionType` 1, and a 0.5 float (rotation-ratio step, hypothesis from
`11_ground.md` 11's field order). So the spawn enters Move → Wait like any hand-over.

## 5. The ActorContext slots in the switch (PC slot; Mac slot = PC + 1)
| PC slot (byte) | Name | Role here |
|---|---|---|
| 0 | dtor | — |
| 1 (+0x04) | `CreateAI` | called on the contexts by `AIActor::CreateAI` PC 0x7B760 (hypothesis from the name) |
| 2 (+0x08) | `LocalInit` | called once by `AddContextToMainAITrack` when a track-1 context is created |
| 3 (+0x0C) | `GlobalInit(World*)` | world setup (not in the switch) |
| 4 (+0x10) | `TriggerEnter` | **unused for Humans**: the base [0x509940] is empty and no AC2 class overrides it (Mac symbols); no caller found in the track code |
| 5 (+0x14) | `Shutdown(World*)` | — |
| 6 (+0x18) | `UpdateAfterPhysics` | base no-op [0x509930]; **not** what runs after physics (that is `Update(1)`) |
| 7 (+0x1C) | `ReleaseResources` | — |
| 8 (+0x20) | `QueryInterface` | `AIActor::QueryInterface`-style loop over the five tracks' current contexts, PC 0x7A940 |
| 9 / 10 (+0x24 / +0x28) | `GetActorContextData` (+ const) | the data's +4 is the context id (`m_PreviousContextID`, `GetCurrentContextID` PC 0x7A980) |
| 11 (+0x2C) | `SetAIActorData` | called by `AddContext` |
| 12 (+0x30) | `Enter` | step 6 of the switch; also the track's first entry (`AIActor::Enter`, tracks 4 → 0) |
| 13 (+0x34) | `Exit` | step 2 of the switch; `AIActor::Exit` on despawn |
| 14 (+0x38) | `Update(long phase)` | phase 0 from `UpdateTracks` (all tracks), phase 1 from `AIActor::UpdateAfterPhysics` (track 1 only) |
| 15 (+0x3C) | `LinkFSM` | builds the FSM (LocalInit) |
| 16 (+0x40) | `GetCurrentStateName` | debug |

## Open
1. The call order of the player's controller (`AssassinTask`, an AI component) and `Human::Update` within the
   frame. The live data puts the controller first. The engine's component update loop isn't read.
2. Who calls `ActorContext::TriggerEnter` (slot 4) at all, and whether track 0 is ever used.
3. The PC addresses of the LedgePullDown wrappers and Exec (Mac 0xF99D34, 0xF9A45A, 0xF98DA6), and of the
   `HumanGround::Exec…` for each InAir Main transition.
4. The exact field order of the default `TransitionSetupDataToMovement` in `InitMainTrack` (which float is 0.5).
5. ~~Naming~~ done 2026-10-22: `AIActor::UpdateTracks` 0x7A8C0, `Update` 0x7AC40, `UpdateAfterPhysics`
   0x7AC50, `TransitToContext` 0x7A9D0 / 0x7AC80 / 0x7B1D0, `GetContextByID` 0x7B000, `AddContext` 0x7B530,
   `SetInitialContext` 0x7A830 / 0x7AB70, `Enter` 0x7AE30, `Exit` 0x7B060, `Reset` 0x7ABF0, `CreateAI` 0x7B760;
   `Human::GetContextByID` 0x91CD80, `AddContextToMainAITrack` 0x91CB10, `CreateAI` 0x9333A0,
   `InitializeInGroundState` 0x91CE20, `InitializeInRidingState` 0x9179E0, `InitMainTrack` 0x914F20,
   `UpdateAfterPhysics` 0x922390, `DoSwitchToDebugMode` 0x913A60; `HumanGround::_EnterHumanGround` 0x628710,
   `EnterHumanGround` 0x627490, `_ExitHumanGround` 0x634C00, `ExitHumanGround` 0x60C400,
   `_TransitFromHumanGroundToInAirMain` 0x5F1370, the FallOffEdge wrapper 0x60BA30 / Exec 0x6056F0 /
   `ExecuteFallOffEdge` 0x5FC890; `HumanInAir::_TransitFromMainToGroundMain` 0x69DB60.

## Port
**Done 2026-10-22** (`port/src/scimitar/ai_actor.*`, `human.*`; tests `port/tests/test_context_switch.cpp`):
`scimitar::AIActor` has the five `ActorTrack`s, `SetInitialContext` / `Enter`, `UpdateTracks` / `UpdateAfterPhysics`
with the skip-once rule, and `TransitToContext(id, exec)`. `Human` derives from it: `GetContextByID` (the bundle),
`InitMainTrack`, and `Update` = `UpdateTracks(0)`, edge clamping, then the main track's `Update(1)` (PORT: in the same
call, as the port's physics step). `ActorContext::Update(long phase, PortUpdateArgs)` replaces the two port methods.
HumanGround's Main transition to InAir, its FallOffEdge / LedgePullDown commands and InAir's landing (with
`ExecMainToGroundMain` as the Exec) call `TransitToContext` at once. PORT: `TransitToContext` to a context the port
doesn't have (Ledge) returns false and changes nothing; `ForceContext` stays as a test / spawn helper; the
transitions' own Execs other than the landing aren't ported. The plan it followed:
- Give `Human` an `AIActor`-style **main track**: `m_CurrentContext`, `m_PreviousContextID`, `m_ContextHasChanged`
  (+0x48 / +0x44 / +0x34).
- Replace `RequestContext` with **`TransitToContext(id, exec)`**. It gets the context from the bundle (creating it,
  with its LocalInit, on first use), calls `old->Exit()`, records the previous id, runs `exec` (the transition's
  Exec, e.g. `HumanInAir::ExecMainToGroundMain`, `HumanGround::ExecuteFallOffEdge`), sets the current context and
  the flag, then calls `new->Enter()`, **immediately**.
- The contexts call it from their transitions, and the commands' `Do…` call it from inside the controller. So
  `RunPendingCommand`'s deferral goes, and a FallOffEdge sent by `AssassinTask` switches before `Human::Update`.
- `Human::Update` becomes `UpdateTracks(0)`: skip and clear when the flag is set, else `Update(0)` and clear if it
  switched. Then edge clamping. `Human::UpdateAfterPhysics` becomes the track's phase 1 under the same rule. Only
  the context (no queue) decides, and the new context gets its phase 1 the same frame and its phase 0 the next.
- `ActorContext::Update(input, world, dt)` and `UpdateAfterPhysics` would become one `Update(phase)`, matching slot
  14 (PORT: input and dt still passed in).
- `ForceContext` stays a `PORT:` helper for tests. The spawn path should instead be
  `SetInitialContext(Ground)` + `InitMainTrack` + `Enter`, so the first Ground entry gets the default
  `TransitionSetupDataToMovement` (MovementState 0).
- UpperBody / LookAt / Vocalization tracks stay out of scope. Tests to add: the switch order (Exit → Exec → Enter),
  the skipped main update on a controller-made switch, phase 1 of the new context in the switch frame, and the
  `m_PreviousContextID`.
