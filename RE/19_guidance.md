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

## 6. The depth detector and the occluded report (2026-10-23; Mac, decoded by a sub-agent, PC not mapped)
Ported in `port/src/scimitar/guidance_depth.{h,cpp}` (tests `test_guidance_depth.cpp`); the world provides its collision meshes (`port::World::ForEachCollisionMesh`; BoxWorld: each box as a closed 12-triangle mesh, the floor as a 2000 m square — PORT). PORT: every triangle of an overlapping mesh is sliced (the game's MOPP query returns a superset too); the broadphase is the world's AABB test; the mask / interval loops are bounded where the game isn't.
Source: `IDB/scimitar_sf.i64` (AC2 Mac, read only). All addresses are **Mac**; PC not mapped. Layout names from
the Revelations PDB (`py RE/tools/pdbtypes.py`) where they match the Mac offsets.

### 6.0 Summary

**What the depth detector computes.** For every guidance line (contact) of an input `GuidanceReport` and for
every `GuidanceDepthDetectorSettings` entry, it builds a flat rectangle ("workspace plane") hanging off the line:
one side is the line itself (extended by `Overshoot` = 0.5 m at each end), the other direction points away from
the line's side face, rotated about the line by `Angle`, `MaxDepth` long. It then slices the **physics collision
meshes** (Havok MOPP `MeshShape` triangles; not guidance data) with each rectangle: every triangle crossing the
rectangle's plane gives a segment, clipped to the rectangle. Those segments are rasterised into a
`GuidanceDepthTrack`: bins of `Resolution` = 0.1 m along the line, each holding the smallest distance (along the
rectangle's depth axis) from the line to any geometry, default `MaxDepth`. Places where geometry touches the line
itself are recorded as signed "intersections"; spans of the line that lie inside solid geometry (by the
intersection parity) get depth 0.

**What the occluded report computes.** For each input contact, a bin is free when its depth is `>= clearance`
in every track (one track per settings entry). The contact is cut into the free sub-segments (clamped to the
real line, without the overshoot); each piece keeps the contact's normals/entity/etc. and gets, per track, a
`GuidanceClearanceInfo` pair (free distance before / after the piece along the line, quantised to 0.1 m, capped at
what the track covers) written into `GuidanceContactObject::m_Floats`. Pieces shorter than 0.0005 m are dropped;
a contact with no free bin produces nothing.

**The pull-down call** (`HumanLedge::CastPullDownGuidanceReport` 0x132B7EA):
`Human::CastGuidanceDepthDetector(report, depthReports, 1.2f, -1.0471976f, &Vector4(0, 0.02, 0.02, 0))` is the
overload **0x12C8CA4** `(const GuidanceReport&, Array<GuidanceDepthReport>&, float maxDepth, float angle, const
Vector4& offset)`: one settings entry, `m_MaxDepth` = 1.2 (0x3F99999A at 0x132BA58), `m_Angle` = −60°
(0xBF860A92 at 0x132BA8B), `m_Offset` = (0, 0.02, 0.02, 0) (xmmword 0x205E280). Then
`CreateOccludedGuidanceReport(report, depthReports, 1.2f, out)` — overload **0x12C9FCE** `(…, float clearance,
GuidanceReport&)`; the "154" in the decompiler is the low byte 0x9A of the float 0x3F99999A = **1.2** passed in
`ebx` (0x132BA58 → `[esp+0Ch]` at 0x132BAB0). So: each level ledge line is cut down to the parts where a strip
1.2 m deep, starting 2 cm out and 2 cm up from the edge and pointing outward-and-60°-downward from the wall face,
is free of collision geometry (no triangle closer than 1.2 m along the strip, and the edge line itself not inside
solid geometry). The output is appended to the caller's report `a2` (CreateOccluded does not clear it;
whether the pull-down's caller clears it first is not decoded here).

---

### 6.1. Data layouts

Array header used everywhere ("SmallArray"): `+0 T* data`, `+4 u32 bits`: bits 0–13 capacity, bit 15 (0x8000)
data is the inline storage, bits 16–29 count, bit 30 (0x40000000) "ordered" (removals shift instead of
swap-with-last), bit 31 (sign) seen tested before destruction loops (meaning not needed). `InplaceArray<T,N>` =
N×sizeof(T) inline storage **followed** by the header.

#### GuidanceDepthDetectorSettings (0x20) — PDB matches
| off | name | type | meaning |
|---|---|---|---|
| +0x00 | m_Offset | Vector4 | offset of the plane origin, in the frame (along-edge, outward, up) — see AddTasks |
| +0x10 | m_Angle | float | rotation of the depth axis about the edge (rad); pull-down −π/3 |
| +0x14 | m_MaxDepth | float | depth of the strip (m); default depth value; pull-down 1.2 |

#### GuidanceDepthDetector (0x9C on Mac; ctor 0x3EA420) — PDB order matches
| off | name | value |
|---|---|---|
| +0x00 | vftable | |
| +0x04 | m_Resolution | ctor 0.1 (0x3DCCCCCD) |
| +0x08 | m_Overshoot | ctor 0.0 |
| +0x10..+0x50 | m_Settings inline storage (2 × 0x20) | |
| +0x50 | m_Settings header (data → +0x10, bits 0x8002) | |
| +0x60..+0x90 | m_WorkspaceCastPlanes inline storage (2 × 0x18) | |
| +0x90 | m_WorkspaceCastPlanes header (data → +0x60, bits 0x8002) | |
| +0x98 | m_InputReport | GuidanceReport* (set by AddTasksToWorkspace) |

The Human's one lives in `HumanGuidance` at **+0xD0** (Human +0x430 = `HumanGuidance*`). `HumanGuidance` +0x170 =
`PhysicWorkspace*`.

#### GuidanceTask (0x10) / GuidanceWorkspaceCastPlanes (0x18) — PDB matches
| off | name | |
|---|---|---|
| +0x00 | vftable (CastPlanes: 0x27C4668) | |
| +0x04 | m_SubType | ctor 0 (0x495F60) → the slice contacts get SubType 0 |
| +0x08 | m_Result | GuidanceReport (Array<GuidanceContactObject>) — the slice segments |
| +0x10 | m_Planes | Array<GuidanceWorkspacePlane> |

#### GuidanceWorkspacePlane (0x60) — PDB matches
| off | name | meaning |
|---|---|---|
| +0x00 | m_Matrix | Matrix44, **workspace-local** (columns X, Y, Z, T; see conventions) |
| +0x40 | m_Offset | the world offset vector added to the origin (see AddTasks step 2.k) |
| +0x50 | m_Length | the strip's depth extent along Y = settings.m_MaxDepth |
| +0x54 | m_Height | extent along Z = segment length + 2·Overshoot |
| +0x58 | m_Parent | GuidanceContactObject* of the input report |

#### GuidanceWorkspace (0xB0; ctor 0x490F30) — PDB matches
| off | name | |
|---|---|---|
| +0x10 | m_Matrix | identity in ctor; GetDepthReports sets translation = AABB centre |
| +0x50 / +0x60 | m_BoxMin / m_BoxMax | local box |
| +0x70 / +0x80 | m_GlobalAABBoxMin / Max | from BroadPhase |
| +0x90 | m_ParentEntity | 0 (ctor) |
| +0x94 | m_Tasks | PtrArray<GuidanceTask> |
| +0x9C | m_OverlapPhysicComponents | PtrArray<PhysicComponent> |
| +0xA4 | m_CollisionFilterInfo | ctor `SetLayer_Callback(5)` (low 6 bits = layer 5; Revelations enum: **Character**) |

#### GuidanceDepthReport (0xC) — PDB matches
`+0 vftable`, `+4 m_Tracks: Array<GuidanceDepthTrack>` (one track per input contact, same index).

#### GuidanceDepthTrack (0xB0; ctor 0x697EB0) — PDB matches
| off | name | meaning | ctor |
|---|---|---|---|
| +0x00 | m_GlobalOffset | = plane.m_Offset | 0 |
| +0x10 | m_PlaneMatrix | global plane matrix (X +0x10, Y +0x20, **Z +0x30**, T +0x40) | identity |
| +0x50 | m_Length | track length along Z = plane.m_Height | −1 |
| +0x54 | m_Resolution | bin size | −1 |
| +0x58 | m_Overshoot | | −1 |
| +0x5C | m_Depths | Array<float>, one per bin | empty |
| +0x64 | m_Intersections | Array<GuidanceIntersection> | empty |
| +0x70 | m_InvPlaneMatrix | affine inverse of m_PlaneMatrix | identity |

#### GuidanceIntersection (0xC)
`+0 m_T` (float, position along Z), `+4 m_Direction` (u32 0/1), `+8 m_Level` (long).

#### GuidanceClearanceInterval (8): `m_T0`, `m_T1`. GuidanceClearanceInfo (8): `m_Clearance0`, `m_Clearance1`.

#### GuidanceContactObject (0x60) — fields used
+0x00 `m_Floats[4]` = clearance infos: track k writes `m_Floats[2k]` = Clearance0, `m_Floats[2k+1]` = Clearance1
(`SetClearances(k, info)` 0x37F010; `SetClearances(Array)` 0x37F110 writes them in order) — so at most 2 tracks
fit. +0x10/+0x20 p0/p1, +0x30/+0x40 normal left/right, +0x50 entity, +0x54 parent guidance object, +0x58
parent contact object, +0x5C subtype byte (& 0x3F).

#### Conventions (decoded here, needed for everything below)
- `Gear::Matrix44` stores 4 `__m128` **columns**: col0 = X axis, col1 = Y, col2 = Z, col3 = translation T.
  `TransformVertex(M, v)` = `v.x·X + v.y·Y + v.z·Z + T` (0x9F8EC0; w forced 0); `InternalTransformVertex` 0x9CAF50
  is the same with w taken from the 4th argument.
- `A *= B` (0x9CA830) gives `A ← A·B` in column-vector math (new col i = A·(B col i)): B applied first.
- `SetRotate(axis, θ)` (0x9F62F0) is the standard right-handed Rodrigues matrix
  (col0 = (c + (1−c)x², (1−c)xy + s·z, (1−c)xz − s·y)); T = (0,0,0,1).
- Cross product pattern `a.yzx*b.zxy − a.zxy*b.yzx` = a × b.
- "trunc(x)" below = the compiled float→u32 conversion of `max(x, 0)` (truncation toward 0).
- "Dot3" = the 4-lane horizontal sum where w is 0 on both operands.

---

### 6.2. Functions (pseudocode)

#### 2.1 Human::CastGuidanceDepthDetector overloads
- **0x12C8CA4** `(report, out, float maxDepth, float angle, const Vector4& off)`: one settings
  `{m_Offset = off, m_Angle = angle, m_MaxDepth = maxDepth}` (stores: +0x10 ← 2nd float 0x12C8CE0, +0x14 ← 1st
  float 0x12C8CE9) → core 0x12C5056. **This is the pull-down's.**
- **0x12C8BF6** `(report, out, f maxDepth0, f angle0, f maxDepth1, f angle1)`: two settings, both offsets =
  xmmword 0x205CC90 = (0, 0.02, 0.02, 0).
- **0x12C8B46** `(report, out, f maxDepth0, f angle0, f maxDepth1, f angle1, const Vector4& off)`: two settings,
  both with `off`.
- **Core 0x12C5056** `(report, out, const Array<Settings>&)`:
  `HumanGuidance::SetupDepthDetector(hg, 0.1, 0.5)` (0x161CA62: detector.m_Resolution = 0.1, m_Overshoot = 0.5)
  then `HumanGuidance::GetDepthReports(hg, report, settings, out)`.

#### 2.2 HumanGuidance::GetDepthReports (0xF05BF6)
```
if report.count == 0: return                          // 0xF05C0E; 'out' is left untouched
ws = GuidanceWorkspace()                              // ctor: layer 5, identity, no parent
(min, max) = GetAaabbFromGuidanceReport(report)       // 0xF04EE6: per contact, p0 (+0x10) and p1 (+0x20),
                                                      //   x,y,z only; init min = (FLT_MAX×3, 0),
                                                      //   max = (−FLT_MAX×3, 0); "min > v → min = v",
                                                      //   "v > max → max = v"
m = 0; for s in settings: m = maxss(s.m_MaxDepth, m)  // 0xF05C76: starts at 0.0
min -= (m,m,m,0); max += (m,m,m,0)
half = (max − min)·0.5;  centre = min + half
ws.m_Matrix = identity with T = centre               // 0xF05DCA
ws.m_BoxMin = min − centre; ws.m_BoxMax = max − centre
detector.m_Settings = settings                        // copy, HumanGuidance+0x120
detector.AddTasksToWorkspace(report, ws)              // 2.3
ws.BroadPhase(hg.m_PhysicWorkspace /*+0x170*/, false) // 2.4
ws.ExecuteTasks(layer = NULL)                         // 2.5
detector.FillReports(ws, out)                         // 2.7
```

#### 2.3 GuidanceDepthDetector::AddTasksToWorkspace(report, ws) (0x3E8D50)
```
this.m_InputReport = &report                                         // 0x3E8D62
destroy all tasks; tasks.resize(settings.count) with fresh CastPlanes (subtype 0, empty planes/result)
if report.count == 0 or settings.count == 0: return
for i in 0 .. settings.count−1:                                      // loop 0x3E9159
  task = tasks[i]; ws.AddTask(task)                                  // 0x48FF80: append pointer
  W = ws.m_ParentEntity ? parentGlobal · ws.m_Matrix : ws.m_Matrix   // here: ws.m_Matrix
  Winv = general inverse(W)                                          // SetInverse 0x3E9244
  s = settings[i]
  for j in 0 .. report.count−1:                                      // loop 0x3E9300
    c = report[j]
    nL = c.+0x30, nR = c.+0x40
    if nR.z <= nL.z: nSide = nR; nTop = nL                           // ucomiss 0x3E9354, jbe
    else:            nSide = nL; nTop = nR
    d   = c.p1 − c.p0;  len = sqrt(Dot4(d,d))                        // 0x3E938E
    dir = d / sqrt(Dot4(d,d))
    perp = (dir.y, −dir.x, 0, 0) / |…|      // dir × K: dir.yzx*(1,0,0,0) − dir.zxy*(0,1,0,0)
                                            //   (consts 0x1F50060 / 0x1F50070), normalised (Dot4, sqrt)
    if 0 > Dot3(nSide, perp): perp = −perp                           // 0x3E945F (ja → negate)
    v113 = normalize(perp × nTop)                                    // 0x3E94A7..0x3E94C6
    sign = (Dot4(dir, v113) < 0) ? −1.0 : +1.0                       // cmpnless 0x3E94FB; −1 at
                                                                     //   0x202D82C, +1 at 0x1F3FB44 (NaN → −1)
    pxd  = perp × dir                                                // xmm5, 0x3E9544
    height = len + (Overshoot + Overshoot)                           // 0x3E9554
    zAxis' = −sign · pxd                                             // xor sign-mask 0x1F50040
    offW = s.m_Offset.y·perp + s.m_Offset.z·zAxis' + s.m_Offset.x·v113   // 0x3E95C9
    B = columns(pxd, perp, dir, <stale>)                             // var_248..var_218
    R = SetRotate(axis = dir, angle = sign · s.m_Angle)              // 0x3E961C
    P = R; P *= B   (P = R·B)                                        // 0x3E967E; col3 then overwritten:
    P.T = (offW + c.p0 − Overshoot·dir) with w = 1                   // Select with Mask<0,0,0,1>, 0x1F50050
    plane.m_Matrix = SetMatrix_Callback(Winv · P)                    // 0x3E9768, 0x3E977A
    plane.m_Offset = offW;  plane.m_Length = s.m_MaxDepth            // +0x40, +0x50 (from s+0x14)
    plane.m_Height = height; plane.m_Parent = &c                     // +0x54, +0x58
    task.m_Planes.push(plane)
```
`GuidanceWorkspacePlane::SetMatrix_Callback(M)` (0x48FE30) orthonormalises: `Y = normalize(M.Y)`,
`X = normalize(Y × M.Z)`, `Z = normalize(X × Y)`, `T = M.T` with w = 1; stores (X, Y, Z, T).

Geometry in words: with `u = sign·dir` (≈ the direction of `perp × nTop`), `R(dir, sign·θ) = R(u, θ)`. `perp` is
the horizontal normal of the line pointing to the side face's side; `u × perp` ≈ up. So the plane's **Z** = the
edge direction (dir), **Y** = `cos θ·perp + sin θ·(u×perp)` (θ = −60°: outward and 60° down), **X** = the slicing
plane's normal. The offset is (x along `v113` ≈ edge, y along perp = out, z along `u×perp` ≈ up). The strip is
`T + y·Y + z·Z`, y ∈ [0, MaxDepth], z ∈ [0, len + 2·Overshoot], with z = Overshoot at p0 (+offset).
Degenerate input (vertical or zero-length line) gives NaN; not guarded (UNKNOWN whether callers prevent it; the
pull-down drops non-level lines first).

#### 2.4 GuidanceWorkspace::BroadPhase(physicWorkspace, bool) (0x490CF0 → 0x490320)
```
world = EngineContext::GetCurrentSimulatedWorld()
m_OverlapPhysicComponents.count = 0
if world == null: return
G = parent ? parentGlobal · m_Matrix : m_Matrix
(m_GlobalAABBoxMin, m_GlobalAABBoxMax) = ConvertOOBBoxToAABBox(G, m_BoxMin, m_BoxMax)
list = []
if !physicWorkspace or !physicWorkspace.GetOverlappingPhysicComponents(aabb, list, filterInfo & 0x3F /*5*/):
   // fallback: Havok broadphase query (world+0x808 → +84 → vfunc +68) over the AABB;
   // for each hit: its collidable's PhysicComponent if CollisionFilter::IsCollisionEnabled(
   //   world filter, this.m_CollisionFilterInfo.GetFilterInfo(), object filter info)
if (bool arg): (false here) remove entries whose byte +0xC is set, back to front — not used
for comp in list:
   if comp is a MultiInertComponent (class-descriptor range check):
       MultiMeshShape::GetInertComponentsInAABB(comp+0x30, globalMin, globalMax, m_OverlapPhysicComponents)
   else: m_OverlapPhysicComponents.push(comp)
```

#### 2.5 GuidanceWorkspace::ExecuteTasks(layer*) (0x4953B0)
For each task: clear `m_Result`; if every task is a `GuidanceWorkspaceCastPlanes` (class-descriptor range
check), `ExecuteTasksAllPlanes(layer)` (0x495340) = for each task `ProcessTriangles2(m_OverlapPhysicComponents,
ws, layer)`. (Otherwise a generic path calls each task's vfunc +0x24 on each mesh's triangles in the whole
workspace AABB — not taken here.) The detector passes layer = NULL.

#### 2.6 GuidanceWorkspaceCastPlanes::ProcessTriangles2(components, ws, layer) (0x493F00) — the slicer
```
W = ws global matrix (parent · m_Matrix, or m_Matrix)
for plane in m_Planes:                                               // stride 0x60
  Pg = W; Pg *= plane.m_Matrix                                       // global plane matrix
  (pmin, pmax) = ConvertPlaneToAABBox(Pg, plane.m_Length, plane.m_Height)
         // 0xA02FC0: corners T, T+L·Y, T+H·Z, T+L·Y+H·Z (T with w=0); componentwise min/max
  pmax = min(pmax, ws.m_GlobalAABBoxMax); pmin = max(pmin, ws.m_GlobalAABBoxMin)   // 0x4940BA
  for comp in components:
    e = comp.+8 (entity); G = e.hierarchy(+0x50) ? Hierarchy::GetGlobalMatrix : e.+0x10
    Ginv = SetInverse(G)
    for k in 0 .. comp.vfunc[+212]()−1:                              // shape count
      shape = comp.vfunc[+216](k)
      if shape.vfunc[+24]() != 0xB22B1A61 (MeshShape class id): continue
      tris = layer ? GetTrianglesInAABBWithLayerCheck(...) : MeshShape::GetTrianglesInAABB(shape, pmin, pmax, Ginv)
             // 0x6D6BC0: ConvertOOBBoxToAABBox(Ginv, pmin, pmax) → local AABB → Havok MOPP query
             // (shape+156 = hkpMoppBvTreeShape) → triangle indices (conservative superset)
      Pl = Ginv; Pl *= W; Pl *= plane.m_Matrix                       // plane in mesh-local space
      X = Pl.X; k0 = −Dot3(X, Pl.T)
      for each tri index t: verts = shape.+64 (float3 array, 12 B) at shape.+88 (u16[3], 6 B) [t]
        v = (x,y,z,0);  dist_i = Dot3(X, v_i) + k0;  side_i = dist_i >= 0.0     // 0x49475F
      for each triangle (v0,v1,v2):
        if side0 & side1 & side2: skip;  if !(side0 | side1 | side2): skip      // 0x4947DE / 0x4947E2
        if !IntersectFinitePlaneWithTriangleEx(Pl, L=plane.m_Length, H=plane.m_Height,
                                                v0,v1,v2, d0,d1,d2, &a, &b): skip
        a = TransformVertex(G, a); b = TransformVertex(G, b)                   // world
        n = (v1 − v0) × (v2 − v0); if |n|² != 0 (any lane): n = n/|n|          // 0x494A95
        n = rotate by G (3×3)
        contact = { p0 = a, p1 = b, normalLeft = normalRight = n, entity = e,
                    parentContact = plane.m_Parent, subtype = task.m_SubType & 0x3F (= 0),
                    parentGuidanceObject = ctor default, m_Floats = ctor default }
        if every lane |b − a| <= 0.0005 (xmmword 0x1F52EA0): skip            // 0x494B41
        task.m_Result.push(contact)
```

**IntersectFinitePlaneWithTriangleEx(M, L, H, v0,v1,v2, d0,d1,d2, &a, &b)** (0x1C6190):
```
T = M.T (w=0); Y = M.Y; Z = M.Z
if max(d0,d1,d2) · min(d0,d1,d2) >= 0: return false            // a vertex exactly on the plane → no hit
m14 = (0 < d0·d1);  m15 = (0 < d2·d1)
lone, dl = m15 ? (v0,d0) : (m14 ? (v2,d2) : (v1,d1))           // the vertex alone on its side
A,  dA   = m15 ? (v1,d1) : (v0,d0)
Bv, dB   = m15 ? (v2,d2) : (m14 ? (v1,d1) : (v2,d2))
P0 = lone + (−dl/(dA − dl))·(A − lone)
P1 = lone + (−dl/(dB − dl))·(Bv − lone)
D = P1 − P0
// clip along Y to [0, L]
EY = Y·L + T;  EY lanes that are exactly 0.0 → 0.0005           // quirk: the guard is on the point (0x1C644F)
s1 = Dot(EY' − P0, Y) / Dot(Y, D);  s0 = Dot(T − P0, Y) / Dot(Y, D)   // no zero guard on Dot(Y,D)
if !IntersectRangeV([0,1], [s1, s0]) → false
// clip along Z to [0, H]
den = Dot(Z, D); if den == 0: den = 0.0005                      // 0x1C6561 (0.0005 = xmmword 0x1F44130)
s1 = Dot(Z·H + T − P0, Z)/den;  s0 = Dot(T − P0, Z)/den
if !IntersectRangeV(current range, [s1, s0]) → false
a = P0 + D·t0;  b = P0 + D·t1;  return true
```
`IntersectRangeV(lo1, hi1, lo2, hi2, outLo, outHi)` (0x261AE0): each pair sorted (swap when hi < lo); overlap iff
`lo1 <= hi2 && hi1 >= lo2` (lane x); out = `[max(lo1,lo2), min(hi1,hi2)]`.

#### 2.7 GuidanceDepthDetector::FillReports(ws, out) (0x3EA920)
```
N = settings.count
out: destroy all, resize to N fresh GuidanceDepthReports (empty tracks)
for i in 0..N−1:
  planes = tasks[i].m_Planes
  out[i].m_Tracks.resize(planes.count) (fresh tracks, ctor 0x697EB0)
  for j: out[i].m_Tracks[j].SetupTrack(ws, planes[j], this.m_Resolution, this.m_Overshoot)   // 2.8
for i in 0..N−1:
  res = tasks[i].m_Result
  if res.count != 0:                                        // 0x3EB18F — guards both loops below
     last = null; idx = 0
     for c in res (in order):
        if c.parentContact != last: last = c.parentContact; idx = (last − inputReport.data)/0x60
        out[i].m_Tracks[idx].AddSegment(c)                  // 2.9
     for track in out[i].m_Tracks: track.ProcessIntersections()   // 2.11
```
So a settings entry whose slicing found nothing keeps every track at `MaxDepth` everywhere with no intersections
processed.

#### 2.8 GuidanceDepthTrack::SetupTrack(ws, plane, res, overshoot) (0x696EA0)
```
n = trunc(plane.m_Height / res) + 1
m_Depths.resize(n); new elements (old count .. n−1) = plane.m_Length   (MaxDepth)
m_Resolution = res; m_Length = plane.m_Height; m_Overshoot = overshoot
m_GlobalOffset = plane.m_Offset
m_PlaneMatrix = W · plane.m_Matrix     (W = ws global; = the global P of 2.3)
m_InvPlaneMatrix = SetAffineInverse(m_PlaneMatrix)
```
`SetPlaneMatrix(M)` (0xA2FAD0): `m_PlaneMatrix = M`, `m_InvPlaneMatrix = affine inverse(M)` (not used on this path).

#### 2.9 GuidanceDepthTrack::AddSegment(contact) (0x698620)
```
a = InvPlane·contact.p0 ; b = InvPlane·contact.p1           // local (x, y = depth, z = along edge)
if b.z <= a.z: (z0,y0) = (b.z,b.y); (z1,y1) = (a.z,a.y)      // 0x6986AB
else:          (z0,y0) = (a.z,a.y); (z1,y1) = (b.z,b.y)
zc0 = (z0 > 0 ? z0 : 0); zc0 = (L > zc0 ? zc0 : L)           // L = m_Length; cmpgt_ss pattern
zc1 = (z1 > 0 ? z1 : 0); zc1 = (L > zc1 ? zc1 : L)
if |y1| <= 0.0005:                                            // 0x69875E
    y1 = 0
    if |y0| > 0.0005: AddIntersection(zc1, m_PlaneMatrix.Z, contact)
    a0 = |y0|
else: a0 = |y0|
if a0 <= 0.0005:                                              // 0x698786
    if |y1| > 0.0005: AddIntersection(zc0, m_PlaneMatrix.Z, contact)
    y0 = 0
n = m_Depths.count
i0 = min(trunc(zc0/res), n−1);  i1 = min(trunc(zc1/res), n−1)
if depths[i0] > y0: depths[i0] = y0
if depths[i1] > y1: depths[i1] = y1
if i1 − i0 > 1:                                               // unsigned
   slope = (y1 − y0)/(zc1 − zc0)
   for k in i0+1 .. i1−1:
      y = ((k + 0.5)·res)·slope + (y0 − zc0·slope)            // 0x698930
      if depths[k] > y: depths[k] = y
```
Note: y0/y1 are the endpoints' depths (unclamped; only zeroed when within 0.0005 of 0); z is clamped.
Negative y (geometry behind the edge line) is stored as-is (min keeps it).

#### 2.10 GuidanceDepthTrack::AddIntersection(t, axis, contact) (0x698380)
`push {m_T = t, m_Direction = (Dot4(contact.normalLeft /*+0x30*/, axis) <= 0.0) ? 1 : 0, m_Level = 0}`.
(The slice contact's normal is the triangle normal; axis = the track's Z = edge direction.)

#### 2.11 GuidanceDepthTrack::ProcessIntersections() (0x698A60)
```
if m_Depths.count == 0: return
push {0, 0, 0}; push {m_Length, 1, 0}
sort ascending by m_T (IsP1LowerThenP2 0xA2F630: p2.T > p1.T; libstdc++ introsort + insertion sort).
   Ties: elements with equal (T, Direction) are identical at this point, and the cancellation below removes
   opposite pairs at equal T regardless of order, so any correct sort gives the same result.
set the ordered flag (0x40000000) → removals keep order
i = 0
while i < count:
   while i < count−1 and a[i].Dir != a[i+1].Dir and a[i].T == a[i+1].T:   // exact float compare
       remove a[i], a[i+1]                                               // then re-test at the same i
   if i >= count: break
   if i > 0 and a[i].Dir != a[i−1].Dir and a[i].T == a[i−1].T:
       remove a[i], a[i−1]; i −= 1
   else: i += 1
// levels
minLevel = 0; level = 0
for k in 1 .. count−1:
   if a[k].Dir == 0 and a[k−1].Dir == 0: level −= 1
   elif a[k].Dir == 1 and a[k−1].Dir == 1: level += 1
   a[k].Level = level; minLevel = min(minLevel, level)
// (a[0].Level stays 0; if count <= 1, minLevel = 0)
// valid intervals
intervals = []
if count != 1:
   i = 0
   loop: next = i + 1
         if a[i].Dir == 0 and a[i+1].Dir == 1 and a[i].Level == minLevel and a[i+1].Level == minLevel:
             intervals.push({a[i].T, a[i+1].T}); next = i + 2
         if count − 1 <= next: break
         i = next
   (count == 0 would read a[0], a[1] out of bounds — UNKNOWN if reachable; it needs m_Length == 0)
mask = GetMaskFromValidIntervals(intervals)
for b: if !mask[b]: m_Depths[b] = 0
```
(Exact control flow of the dedup: inner loop 0x69927D–0x6993D9, outer branch 0x6994B2–0x6995C0.)

**GetMaskFromValidIntervals(intervals, mask)** (0x697980): `mask.resize(trunc(m_Length/res) + 1)`, new entries
false; for each interval: `for b in trunc(T0/res) .. trunc(T1/res)` (inclusive, no clamp): `mask[b] = true`.

#### 2.12 GuidanceDepthTrack helpers
- **GetDepth(t)** (0x696290): `m_Depths[trunc(t/res)]` (no bounds clamp).
- **AddDepth(i, d)** (0x6962F0): `if m_Depths[i] > d: m_Depths[i] = d`.
- **GetClearanceArray(c, mask)** (0x697B30): `mask[b] = m_Depths[b] >= c` for every bin.
- **GetClearanceIntervals(mask, intervals)** (0x697160) — mask passed by value: scan b from 0; at the first true
  bin s: e = s+1; while e < n and mask[e]: e++; push `{s·res, e·res}`; continue scanning at e+1.
- **GetClearanceIntervals(c, intervals)** (0x699F20): the same with "true" = `m_Depths[b] >= c`.
- **GetOvershootClearances(c, &c0, &c1)** (0x6963C0):
  ```
  ov = m_Overshoot
  if ov == 0 or depths empty: c0 = c1 = ov; return
  c0 = c1 = ov
  i = trunc(ov/res)
  if c > depths[i]: c0 = 0
  else for k in 1..i: if c > depths[i−k]: c0 = k·res; break      // none: stays ov
  j = trunc((m_Length − ov)/res) + 1
  if j < n:
     if c > depths[j]: c1 = 0
     else for k = 1.. while j+k < n: if c > depths[j+k]: c1 = k·res; break   // none: stays ov
  ```
- **GetOvershootClearances(mask, &c0, &c1)** (0x6965A0): identical with "blocked" = `!mask[b]`.
- **GetClearanceReport(c, contact, out)** (0x69A290): `GetClearanceIntervals(c)` → `GetClearanceReportFromIntervals`.
- **GetClearanceReport(mask, infos, contact, out)** (0x697BB0): copies the mask, `GetClearanceIntervals(mask)` →
  `GetClearanceReportFromIntervals`; **the `infos` argument is not used**.
- **GetClearanceReportFromIntervals(intervals, contact, out)** (0x696760):
  ```
  dir = (p1 − p0)/|p1 − p0| (Dot4); origin = p0 − ov·dir; hi = m_Length − ov
  piece = copy of contact
  for (t0,t1) in intervals:
     a = (t0 > ov ? t0 : ov); a = (hi > a ? a : hi)
     b = (t1 > ov ? t1 : ov); b = (hi > b ? b : hi)
     if |a − b| <= 0.0005: continue                                // 0x6968F8
     piece.p0 = origin + a·dir; piece.p1 = origin + b·dir
     if every lane |p1 − p0| <= 0.0005 (xmmword 0x1F5CB60): continue
     out.push(piece)        // normals, entity, parents, subtype, m_Floats copied from the contact
  ```
- **FillClearanceInfo(k, c, contact, piece)** (0x696C30) — `contact` unused:
  ```
  a = InvPlane·(piece.p0 + m_GlobalOffset); b = InvPlane·(piece.p1 + m_GlobalOffset)
  swapped = a.z > b.z;  zLo = min(a.z,b.z); zHi = max(a.z,b.z)
  c0 = zLo; c1 = m_Length − zHi
  iLo = trunc(zLo/res); iHi = trunc(zHi/res) + 1
  if c > depths[iLo]: c0 = 0 else for k in 1..iLo: if c > depths[iLo−k]: c0 = k·res; break
  if iHi < n:
     if c > depths[iHi]: c1 = 0
     else for k = 1.. while iHi+k < n: if c > depths[iHi+k]: c1 = k·res; break
  if swapped: swap(c0, c1)
  piece.m_Floats[2k] = c0; piece.m_Floats[2k+1] = c1
  ```

#### 2.13 Human::CreateOccludedGuidanceReport
- **0x12C9FCE** `(report, depthReports, float c, out)`: clearances = [c] → core. **The pull-down's (c = 1.2).**
- **0x12C9F3E** `(report, depthReports, float c0, float c1, out)`: clearances = [c0, c1] → core.
- **Core 0x12C81C0** `(report, depthReports, const Array<float>& clearances, out)`:
  ```
  for i in 0 .. report.count−1:
     tracks = [ &depthReports[k].m_Tracks[i] for k in 0 .. depthReports.count−1 ]
     HumanGuidance::GetCombinedGuidanceReport(report[i], tracks, clearances, out)
        // 0xF04ED2 = jump to GuidanceDepthDetector::GetCombinedGuidanceReport on hg+0xD0
  ```
  `out` is never cleared here (appends).

#### 2.14 GuidanceDepthDetector::GetCombinedGuidanceReport(contact, tracks, clearances, out) (0x3EB6E0)
```
N = tracks.count
infos = N × {0, 0}
mask = []; first = true; ref = null
for k in 0..N−1:
   c = clearances[k]                       // indexed by k: clearances must have >= N entries
   t = tracks[k]
   t.GetOvershootClearances(c, &infos[k].c0, &infos[k].c1)
   if t.depths.count != 0:
      if first: mask.resize(t.depths.count) with true; ref = t; first = false
      for b in 0 .. t.depths.count−1: if c > t.depths[b]: mask[b] = false
if mask.count != 0:
   start = out.count
   ref.GetClearanceReport(mask, <empty infos>, contact, out)       // 2.12
   FillClearanceInfos(tracks, clearances, contact, start, out)      // 2.15
else:
   piece = copy(contact); piece.SetClearances(infos)                // m_Floats ← infos in order
   if not every lane |p1 − p0| <= 0.0005 (xmmword 0x1F500E0): out.push(piece)
```
(The intervals come from `ref`, the first track with bins. All tracks of one contact have the same bin count,
since `n` depends only on the contact's length, Overshoot and Resolution, not on the settings.)

#### 2.15 GuidanceDepthDetector::FillClearanceInfos(tracks, clearances, contact, start, out) (0x3E8A70)
`for each piece in out[start ..]: for k in 0..N−1: tracks[k].FillClearanceInfo(k, clearances[k], contact, piece)`.

---

### 6.3. World queries a reimplementation must provide

The detector uses **only physics collision meshes**, not guidance data:
1. **Broadphase**: all physics components whose shapes overlap a world AABB, filtered for collision with layer
   **5** (`CollisionFilterInfo` set by `GuidanceWorkspace` ctor; Revelations enum name `Character` —
   hypothesis for AC2), through `PhysicWorkspace::GetOverlappingPhysicComponents(aabb, list, layer)` (Havok
   fallback with `CollisionFilter::IsCollisionEnabled`). `MultiInertComponent`s are expanded into their inert
   sub-components inside the AABB (`MultiMeshShape::GetInertComponentsInAABB`).
2. Per component: the owner entity's global matrix (entity +0x50 hierarchy / +0x10), its shapes (vfunc +212
   count, +216 get), keeping only **MeshShape** (class id 0xB22B1A61).
3. Per MeshShape: the triangles whose bounds overlap a **local-space AABB** (MOPP query, conservative; a port may
   return any superset — the plane test is exact), with the vertex array (float3) and the triangle index array
   (u16×3). No per-triangle layer check on this path (layer pointer NULL).
4. No ray casts and no shape casts. Everything else is pure math on the input report.

Port interface sketch: `ForEachCollisionMesh(worldAabb, layer=5, fn(entityGlobalMatrix, mesh))` and
`mesh.ForEachTriangleInLocalAabb(min, max, fn(v0, v1, v2))`.

---

### 6.4. Open / unknown
- `Gear::Matrix44::SetInverse` / `SetAffineInverse` / `ConvertOOBBoxToAABBox` assumed standard (not re-decoded).
- The generic (non-all-planes) `ExecuteTasks` path and `GetTrianglesInAABBWithLayerCheck` — not used here.
- The Y-clip zero guard in `IntersectFinitePlaneWithTriangleEx` replaces zero *coordinates of the far point*
  (0x1C644F), not the zero denominator; reproduced literally above; Dot(Y, D) == 0 then divides by zero
  (±inf / NaN feeding `IntersectRangeV`) — exact outcome for that case not traced.
- Whether the pull-down's out report is cleared by its caller; `RemoveNonLeveledGuidancesFromReport`,
  `MoveGuidancesOfTypeToReport`, ladder helpers — not decoded here.
- PC addresses: none mapped.

## Open
- The other zones (box: edge clamping's `GetEdgeSegmentNearCharacterPos`; capsule, plane, mesh), the cut mode,
  `GuidanceZone::Refine` / `RefineReport` / `CastReport`, `GuidanceSystem` (the partitioned version).
- The ladder helpers, `RemoveNonLeveledGuidancesFromReport`, `RefineReport`, the limb-occlusion capsule, the toe
  probe's `GuidanceWorkspaceCastZPlanes`; then `CastPullDownGuidanceReport` in full, the toes
  (`Human::GetToesDestAnchors` [0x12DBE14]) and `ComputeHangWallEntityPosition` / `…Free…`, so the Ledge
  pull-down can run on anchors; `GetNextHorizontalLimbPos` (shimmying); `GuidanceBeamDetector`, `HumanGuidance`;
  jump targets (`FootHold`, `GetBestEdgeJumpTargetIndex`); PC addresses for section 5.
- Which `EdgeFilter` values the game's data carries, and the subtype masks each caller passes.
- Loading the components from the install (FastLoad [0x3F00B0]) and checking them live against the running game.
