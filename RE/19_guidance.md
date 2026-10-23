# 19: Guidance (the climbable-geometry data and its queries)

Status 2026-10-23: started. The data (`GuidanceSystemComponent`, the packed `GuidanceObject`, the compressed points,
the normals, the `EdgeFilter`), the contact objects, and the sphere query from `CastWorld` down to one segment are
decoded (Mac, then matched on PC). Next: the box, capsule, plane and mesh zones, the cut mode, the report helpers
(`GuidanceReport`, `AnchorPoint`, `GetClosestPointInBoxWithOffset`, `GetNextVerticalLimbPos`), jump targets.

Addresses: **PC** first, Mac (`IDB/scimitar_sf.i64`) in brackets. Layouts from the Revelations PDB (`py
RE/tools/pdbtypes.py`) and AC2's reflection (`py RE/tools/refl.py`); they agree for every class below.

**Bitfields.** The Revelations PDB is the Xbox 360's (big-endian PowerPC), whose compiler allocates bitfields from
the top bit down; x86 (PC and Mac) allocates them from bit 0 up. So a field the PDB puts at bits 26..30 of a dword is
at bits 1..5 on PC. Checked on every field below against the Mac / PC accessors. (`pdbtypes.py` prints the PDB's
bit positions since 2026-10-23.)

## 1. The data on an entity
Guidance is data attached to world entities, in their own local space.

**`scimitar::GuidanceSystemComponent`** (a `Component`, 0x2C; reflected, descriptor PC 0x1CB5EE8; PC vftable
0x18D5DC0):

| PC | Name | Type |
|---|---|---|
| +0x04 | (Component flags) bit 0x4000000 (byte +7 bit 2): the component takes part in casts | u32 |
| +0x08 | the owner entity (`Component`) | `Entity*` |
| +0x0C | `CompressPoints` | `Array<u16>`: 3 per point |
| +0x14 | `GuidanceObjects` | `Array<GuidanceObject>` (count at +0x1A & 0x3FFF) |
| +0x1C | `EdgeFilter` | `EdgeFilter` (0xC) |
| +0x28 | `Dynamic` | bool |

`scimitar::GuidanceSystem` (0x30) is a `GuidanceSystemComponent` with a `Partitioner*` at +0x2C.

**Points:** point *i* is the three `u16` at `CompressPoints + 6i`, and its local position is `u16 × 0.005 −
g_CompressVector3To48_Range` per axis, where the range is (163.84, 163.84, 163.84, 0) (PC 0x1E3E6F0, set by a static
initializer [0x15D10]; read inlined at PC 0x3FDBC0): a ±163.8 m box at 5 mm.

**`scimitar::GuidanceObject`** (0xC; reflected 0x1CB66A0): one line between two points.
- **+0 `m_Packing`** (PC bits, LSB first): bit 0 `Valid`; bits 1–5 `SubType` (`GuidanceObjectSubType`);
  bits 6–18 `Index0`; bits 19–31 `Index1` (`GetLineIndex0` PC 0x415AF0 [0xA4B360]: `(p >> 6) & 0x1FFF`;
  `GetLineIndex1` PC 0x415B00 [0xA4B350]: `p >> 19`; `SetSubType` [0xA4B2B0]: `(p & ~0x3E) | (t & 0x1F) << 1`).
- **+4 `Normal0`** = `LineNormalLeft`, **+8 `Normal1`** = `LineNormalRight`: `Gear::VectorDecN4`, x / y / z the
  signed 10-bit fields at bits 0, 10, 20, each / 511, w the signed 2 bits at 30 (`GetLineNormalLeft` PC 0x416B50
  [0xA4B420], `…Right` PC 0x416B90 [0xA4B4E0]). The two faces that meet at the line (local space).

`GuidanceObjectSubType`: 0 None, 1 LedgeGrab, 2 Beam, 3 Ladder, 4 Pole, 5 Rope, 6 Surface, 7 Quadruped, 8 Kiosk,
9 BoatPlank, 10 BoatInnerEndEdge, 11 BoatInnerSideEdge, 12 BoatMiddleEdge, 13 HayStack, 14 ZipLine (Revelations;
AC2's numbering not checked past what the code shows).

**`scimitar::EdgeFilter`** (0xC): +0 `SlopeCosAngle`, +4 `WallAngle`, +8 the filter bits. **`AcceptEdge`** (PC
0x4858E0 [0x4513B0]) takes the two normals (rotated to world) and classifies each face by its z: **ground** above
`SlopeCosAngle`, **ceiling** below −`SlopeCosAngle`, else **wall**. It accepts the pair when the filter has its bit
(PC bits; the PDB's are the mirror): 1 WallWall (and the angle between the two normals is above `WallAngle`),
2 WallGround, 4 WallCeiling, 0x10 GroundCeiling (the Void bits 8, 0x20, 0x40 aren't tested here). The left face is
classified first; both orders are covered (ground + wall needs bit 2 either way).

**`GuidanceSystemComponent::IsValid(object)`** (PC 0x40EFE0 [0x9F79E0]): `Valid` and `AcceptEdge(EdgeFilter, the
entity's rotation × LineNormalLeft, × LineNormalRight)`.

## 2. Contact objects and reports
**`scimitar::GuidanceContactObject`** (0x60; reflected 0x1CB70B8): one line in world space, as a query returns it.

| PC | Name |
|---|---|
| +0x00 | `m_Floats[4]` |
| +0x10 / +0x20 | `LineGlobalPoint0` / `LineGlobalPoint1` |
| +0x30 / +0x40 | `LineGlobalNormalLeft` / `LineGlobalNormalRight` |
| +0x50 | `Entity` (the owner) |
| +0x54 | `ParentGuidanceObject` |
| +0x58 | `ParentContactObject` |
| +0x5C | `SubType`: bits 0–5 of the byte (`GuidanceObjectSubType`; `& 0x3F` in the ground code) |

`GuidanceContactObject::Transform(matrix)` (PC 0x4214B0 [0x37F470]) moves the points and turns the normals.
`scimitar::GuidanceReport` (8 bytes) is an `Array<GuidanceContactObject>`. A query's accumulator holds the report and
a **subtype mask**: a contact is kept when `(1 << SubType) & mask` (`GuidanceContactObjectSubType` is that bit:
1 None, 2 LedgeGrab, 4 Beam, 8 Ladder, … 0x2000 HayStack, 0x4000 ZipLine; −1 all).

## 3. Zones and the cast
**`scimitar::GuidanceZone`** (0x60 on PC, as Revelations): +0x10 `m_Matrix`, +0x50 `m_ParentEntity` (the zone's
global matrix is the parent's × its own when there is one). Shapes: `GuidanceZoneSphere` (+0x60 `m_Radius`; PC
constructor 0x3FAC60, vftable 0x18D5784; Mac packs it at +0x54), `GuidanceZoneBox` (+0x60 `m_Min`, +0x70 `m_Max`),
capsule, plane, mesh. Zone vftable (PC): +0x20 `GetAABBox`, +0x28 `Cast(accumulator, component, cut)`.

**`GuidanceZone::CastWorld(report, workspace, cut, mask, world, clear)`** (PC 0x3FEA00 [0x62D230]):
1. With `clear`, empty the report.
2. The world: the given one, else the current simulated world, else the current game world.
3. The zone's AABB (`GetAABBox`; the sphere's is its centre ± radius, PC 0x3FB240).
4. With a physic workspace of that world: `PhysicWorkspace::GetOverlappingEntities(aabb, layer 5)` (PC 0x17C080)
   → `CastEntities` (PC 0x3FE950). Without a workspace, or when that call fails: the Havok broadphase over the
   AABB, and for each object's entity `CastEntityInternal`, then the same for each of the entity's children when its
   class id (vftable +0x14) is 0xC4EF6CF2 (PC: the list at entity +0x140, count +0x146 & 0x3FFF).
5. Returns whether anything was added.

**`CastEntityInternal(entity, accumulator, cut)`** (PC 0x3FE720 [0x62CC70]): for an entity with a
`MultiInertComponent`, its inert sub-components inside the AABB (`MultiMeshShape::GetInertComponentsInAABB`) and each
one's entity; otherwise the entity itself. For each of their `GuidanceSystemComponent`s with the cast flag (+4 &
0x4000000), the zone's `Cast(accumulator, component, cut)`.

**The sphere** (`GuidanceZoneSphere::Cast` PC 0x3FB3E0 → component vftable +0xB8
`InternalCollideWithZoneSphere` PC 0x40F390 [0x3F0ED0]):
1. The zone's global centre into the component's local space (the entity's affine inverse).
2. For each object with `IsValid`:
   - **contact mode:** `CollideWithSphere` (PC 0x416260 [0x803360]): the decoded segment against the sphere,
     `IntersectSegmentWithSphere` (PC 0x10F8110 [0x1C16D0]): the squared distance from the centre to the segment's
     closest point (`PointSegmentProjection`) is below radius² (strict). Hit: **`AddContactPoint`** (PC 0x3FE3C0
     [0x62C770]): a contact object with the entity, the object, its subtype, the two points and normals, moved to
     world by the entity's global matrix; kept if its subtype is in the mask and the line isn't degenerate (every
     lane of p1 − p0 within a small epsilon).
   - **cut mode:** `IncludeCutWithSphere` (PC 0x416430 [0x803A60]) clips the segment to the sphere; the cut moves
     to world (`GuidanceCutObject::Transform` PC 0x421340) and `AddContactPoint`'s cut overload (PC 0x3FE4F0
     [0x62C2F0]) adds it.
3. Returns whether anything was added.

**The box** (`InternalCollideWithZoneBox` [0x3F08D0]; PC not mapped yet): the box's matrix brought into the
component's local space (the entity's affine inverse × the zone's global matrix), then per valid object:
- **contact mode:** `CollideWithBox` [0x8034B0] → `IntersectSegmentWithOOBBox(p0, p1, matrix, min, max)`
  [0x1C5980], a separating-axis test: centre `M·(min+max)/2`, half-extents `(max−min)/2`, the segment's midpoint and
  half vector `h`; separated along a box axis *a* when |(mid − centre)·a| > |h·a| + extent; then along the three
  axes `h × (mid − centre)` dotted with each box axis, against the matching pair of extents, where a pair within
  0.0005 of 0 on both sides doesn't separate.
- **cut mode:** `IncludeCutWithBox` [0x803BC0] → `IntersectSegmentOOBBox` [0x1C5780]: the segment into the box's
  frame (the matrix's inverse), `IntersectSegmentAABBox` [0x1C1780]: a ray from p0 along p1 − p0 and one from p1
  back along p0 − p1, each `IntersectRayAABBox` [0x1C13B0] (Woo's test: the candidate planes, the largest t,
  then the hit point checked against the other two slabs; a start inside the box is its own hit); both entry
  points, back to world.

**The cut mode** keeps only the part of the line inside the zone: the contact's two points are the cut's
(`GuidanceCutObject`: two points; `AddContactPoint`'s cut overload [0x62C2F0] copies them and turns the normals).
For the sphere, `IntersectSegmentWithSphere(p0, p1, centre, radius, &a, &b)` [0x1C2C10] solves |p0 + t·d − c| = r
(d = p1 − p0, a = d·d, b = (p0 − c)·d, disc = b² − ((p0 − c)² − r²)·a): no root → no cut; one root t in [0, 1] →
[p0 + t·d, p1]; two roots t0 < t1: t0 > 1 → none; t0 < 0: t1 < 0 → none, else [p0, p0 + t1·d (or p1 when that is
past it)]; else [p0 + t0·d, t1 > 1 ? p1 : p0 + t1·d].

**Degenerate lines:** both `AddContactPoint`s drop a line whose p1 − p0 is within 0.0005 on every lane.

**Defaults:** the component's constructor [0x3EEDB0] resets its `EdgeFilter` (`Reset` [0x450E20]):
`SlopeCosAngle` = `DEFAULT_SLOPE_COS_ANGLE` 0.70703125 (≈ cos 45°), `WallAngle` = `DEFAULT_WALL_ANGLE` 0.3926991
(22.5°), and **no filter bits**: a component accepts no edge until its data (FastLoad [0x3F00B0]) sets them.

## 4. The port (`port/src/scimitar/guidance.{h,cpp}`, 2026-10-23)
The data (`GuidanceObject`'s packing, `LoadDecN4` / `StoreDecN4`, the compressed points, `EdgeFilter::AcceptEdge`),
`GuidanceContactObject`, `GuidanceZoneSphere` / `GuidanceZoneBox` with both modes, `CastWorld`, and the geometry
(`IntersectSegmentWithSphere` both forms, `IntersectSegmentWithOOBBox`, `IntersectRayAABBox`, `IntersectSegmentAABBox`,
`IntersectSegmentOOBBox`, `PointSegmentProjection`) follow the decode above. The gap search (`Human::GetGapList`) and
the edge clamping (`GetEdgeSegmentNearCharacterPos`) now cast these zones as the game does (contacts, mask −1).
PORT stand-ins:
- the broadphase is the world's own AABB overlap (`port::World::GetGuidanceSystemComponents`); no
  `MultiInertComponent` split, no entity children;
- a component carries its owner entity's matrix (`m_PortGlobalMatrix`), orthonormal, so `SetInverse` is the affine
  inverse; the zone has no parent entity;
- the component's vftable +0xC4 check before a sphere or box collide isn't decoded (passes);
- `PortAddPoint` rounds to the nearest 5 mm (the game's points are precompiled);
- `BoxWorld` makes each box an entity with one component: its four top edges, LedgeGrab, top face / side face,
  `EdgeFilter` WallGround (the game's data sets its own); the box zone's AABB is its corners' (not decoded).

## 5. Anchor points (Mac; PC not mapped yet)
**`scimitar::ac2::AnchorPoint`** (0x50, Revelations PDB): +0x00 `m_GlobalPosition`, +0x10 `m_GlobalNormal`, +0x20
`m_LocalObjectPosition`, +0x30 `m_LocalObjectNormal` (in the object's frame), +0x40 `m_Object`
(`EntityInWorldHandle`), +0x44 `m_AnchorPointType` (0 Ledge, 1 Surface, 2 Rope, 3 Pole, 4 ZipLine), +0x48 `m_RopeT`.
`SetFromGlobal(position, normal, entity, type)` [0x1449F88] fills the locals through the entity's inverse matrix,
then `SetupRopeAnchor`. A contact's anchor type: Rope → Rope, Pole → Pole, Surface → Surface, anything else → Ledge.

**`GetFlatNormalFromGuidanceSubType(contact)`** [0x144A4E0]: `GetLineGlobalFlatNormal`, except a Ladder's: its
bottom-most normal (`GetLineBottomMostNormal`, not decoded), flattened.

**`ProjectPointOnSegmentWithMinDist(point, p0, p1, minDist)`** [0xD1AFFE]: the segment shortened by `minDist` at each
end, and the point projected onto it (`PointSegmentProjection`); when |p1 − p0| ≤ 2·minDist + 0.0005, the middle.

**`GetBestPointOnSegmentInPlane(out, planePoint, planeNormal, p0, p1, minDist)`** [0xD1B29A]: where the segment
crosses the plane (`IntersectSegmentWithInfinitePlane`), moved in by `minDist` when it isn't 0; when it doesn't
cross, the end nearer the plane (each projected along the normal), then `ProjectPointOnSegmentWithMinDist`.

**`GetClosestPointInBoxWithOffset(anchor, workspace, report, position, Y, Z, hx, hy, hz, maxAngle, point, local,
minDist, mask, …)`** [0xD1F156 → `…WithLimbOcclusion` 0xD1EA7A]:
1. A `GuidanceZoneBox`: X = Y × Z, Y, Z, at `position`, bounds ±(hx, hy, hz).
2. With a report: `GuidanceZone::RefineReport(report, cut, 8)` (not decoded); without: `CastWorld` in cut mode,
   every subtype. Zero-length lines are dropped (`RemoveZeroLengthGuidancesFromReport` [0x8185F0]).
3. The point is in the box's frame when `local`.
4. For each contact in `mask` (1 takes all) whose flat normal (above) is at least π − `maxAngle` from Y (it faces back
   along −Y): `ProjectPointOnSegmentWithMinDist(point, …, minDist)`; the nearest (squared distance, from 1000) wins.
   A ladder's point is moved onto its step (`CorrectPositionOnLaddetStep` [0xD1D73A]).
5. The anchor is that point with the flat normal and the contact's entity; none: a cleared anchor, false.
`HumanLedge::EnterPullDown` [0x133E8DA] calls it at the gap's edge with Y = −edge normal, Z = K, 0.1 m half extents,
45°, the box centre, minDist 0 and mask 0x2032 (LedgeGrab, Pole, Rope, HayStack).

**`GetNextVerticalLimbPos(anchor, report, entity, position, up, normal, minZ, maxZ, width, lateral, occlusion, f, f,
withSurfaces, …)`** [0xD206F8 → `…WithLimbOcclusion` 0xD1F226]: the next hold for a limb climbing.
1. The frame: X = normalize(normal × K), Y = normal (facing the wall), Z = X × Y, at `position`.
2. Up or down: when `up` · the entity's banking ≥ 0, the target is minZ − 0.1, else maxZ + 0.1.
3. With `occlusion`, a `GuidanceZoneCapsule` cast (`CastReport`, cut type 2) and two cutting planes turned off the
   frame's J axis limit the report (not decoded).
4. For each line (Surface only with `withSurfaces`): cut to the frame's box x ∈ ±width/2, y ∈ ±0.52, z ∈
   [minZ, maxZ] (`IntersectSegmentOOBBox`); its flat normal · Y < −0.5; the point on the cut in the plane through
   `position + X·lateral` with normal X (`GetBestPointOnSegmentInPlane`, minDist 0); its local z's distance from the
   target scores it. The highest score wins (the line farthest from the target: going up, the highest); scores within
   0.01 go to the point nearer x = `lateral`.
5. The anchor: that point, the flat normal, the contact's entity and type; none: cleared, false.
`HumanLedge::UpdatePullDownMain` [0x132BBA8] calls it for the two hands with minZ −0.3, maxZ 0.3, width 0.7 on a
report from **`CastPullDownGuidanceReport(report, offset)`** [0x132B7EA]: a `GuidanceZoneBox` in the sight frame
(`MakeSightRotationMatrix(sight, K)`) at the entity's position plus `offset` turned into that frame, bounds (−1, −0.5,
−0.5) to (1, 1, 0.5), cast in cut mode; ladders moved to their own report (`MoveGuidancesOfTypeToReport`, 8);
non-level lines dropped (`RemoveNonLeveledGuidancesFromReport`); then `Human::CastGuidanceDepthDetector` (1.2, −1.0471976 = −60°,
(0, 0.02, 0.02)) and `Human::CreateOccludedGuidanceReport(…, 154)` (not decoded), and the lines the ladders obstruct
removed (`BuildLadderListFromGuidanceReport`, `RemoveGuidanceObstructedByLadders`).

**Where the body hangs** (PC names by the pull-down's call order and their bodies; Mac `HumanLedge::UpdatePullDownMain`
[0x132BBA8] calls them with the two hand anchors, their flattened normals and, for the wall, the toes):
- `Human::ComputeHangFreeEntityPosition(pos, sight, up, h0, h1, n0, n1)` (PC 0x9200A0 [0x12CDE14]): pos = the hands'
  middle, at the higher hand's z, minus **2.4**·K; sight = −normalize(flat(n0 + n1)); up = K. Live (`13` row 15): the
  free hang on the edge line, 2.400 m down — exactly this.
- `Human::ComputeHangWallEntityPosition(pos, sight, up, h0, h1, n0, n1, t0, t1, higher)` (PC 0x91FDE0 [0x12D61CE]):
  sight₀ = normalize(−n0 − n1) (n0 when n0·n1 < −0.98480773); side = normalize(sight₀ × K); the toes' middle projected
  onto the plane through the hands' middle with normal `side`; up = normalize(hands − that); sight = normalize(up ×
  side); pos = the hands' middle (z of the higher hand, or the lower without `higher`) − **1.1**·up − **0.5**·sight.
  Live: 0.539 out, 1.082 down = this formula with up tilted 2° off the wall, i.e. the toes about 4 cm in front of the
  hands' line.
- `Human::GetToesDestAnchors(t0, t1, pos, normal, f, b)` (PC 0x9212B0 [0x12DBE14]; the pull-down passes f = 0.65):
  two probes (PC 0xEC9A50, Mac name not matched) around pos − 1.0·K + normal·(f/2 + 0.25 − 0.5) ∓ 0.2·side with a
  0.25 × (f/2 + 0.25) × 0.15 box, a 0.07 step and offsets (0, 0, ±0.1); each first casts a ray along the normal, else
  slices the collision with horizontal planes every 0.07 m (a `GuidanceWorkspace` plane cast making Surface lines)
  and takes the nearest line facing back (flat normal · normal < −0.707, point within 0.05). With both found and Human
  +0x972 set, two toes closer than 0.04 m in the side plane are kept, else the one farther from the centre is
  dropped (hypothesis on the last step).

Ported (`port/src/scimitar/ac2/anchor_point.{h,cpp}`, 2026-10-23): `AnchorPoint`, the type mapping,
`GetFlatNormalFromGuidanceSubType` (PORT: a ladder's bottom-most normal is the lower face's), the two projections,
`GetClosestPointInBoxWithOffset` (PORT: a given report is used as it is; no ladder steps) and `GetNextVerticalLimbPos`
(PORT: no occlusion), `ComputeHangWallEntityPosition` and `ComputeHangFreeEntityPosition`. Not yet used by HumanLedge (its pull-down still moves between live positions).

## Open
- The other zones (box: edge clamping's `GetEdgeSegmentNearCharacterPos`; capsule, plane, mesh), the cut mode,
  `GuidanceZone::Refine` / `RefineReport` / `CastReport`, `GuidanceSystem` (the partitioned version).
- `Human::CastGuidanceDepthDetector` / `GuidanceDepthDetector` (~15 KB), `CreateOccludedGuidanceReport`, the ladder
  helpers, `RefineReport`, the limb-occlusion capsule; then `CastPullDownGuidanceReport` in full, the toes
  (`Human::GetToesDestAnchors` [0x12DBE14]) and `ComputeHangWallEntityPosition` / `…Free…`, so the Ledge
  pull-down can run on anchors; `GetNextHorizontalLimbPos` (shimmying); `GuidanceBeamDetector`, `HumanGuidance`;
  jump targets (`FootHold`, `GetBestEdgeJumpTargetIndex`); PC addresses for section 5.
- Which `EdgeFilter` values the game's data carries, and the subtype masks each caller passes.
- Loading the components from the install (FastLoad [0x3F00B0]) and checking them live against the running game.
