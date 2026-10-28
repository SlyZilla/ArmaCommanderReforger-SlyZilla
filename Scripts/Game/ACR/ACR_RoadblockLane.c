// A friendly lane through every roadblock (pn12d playtest, 27 Oct: the AI US side's medium
// checkpoint on its own supply road jammed its M923, its truck and its Humvee for two minutes,
// "stuck, backing up" until they gave up, and most likely ran over the checkpoint's own men).
//
// The vanilla checkpoints carry a road-network bridge for drivers: in the composition's frame (X
// across the road, Z along it) it runs on the +X half for +Z and the -X half for -Z, jogging 4 m
// across the middle. Their dragon's teeth, wire and signs leave about 3 m gaps on that line, too
// tight for a truck or an APC, and the jog has no room to turn. After the composition spawns, every
// solid piece standing in the lane is slid sideways (across the road, never along it) until it
// clears: the lane follows the bridge's line, LANE_OFFSET either side of the centre line with a
// 6 m diagonal between the halves (2 * LANE_TURN), LANE_HALF_WIDTH each side of its line (4.8 m: an
// M923, Ural, LAV-25 or BTR-70 with room to steer). It stays a chicane: the halves are 4 m apart,
// so a 2.5 m vehicle cannot drive it straight (a 1.4 m straight gap) and has to weave past the gun,
// which sits beside the lane in its sandbags (every checkpoint has it on the -X side, 6-7.5 m
// from the lane's line). A plain barricade is a line across the road: its two halves are
// staggered first (the -X half BARRICADE_STAGGER forward, the +X half back), so it becomes the
// same chicane. Decals stay where they are. The holders post beside the lane (Post), and our own
// AI drivers take it slowly (ACR_Squad.LaneSpeed).
class ACR_RoadblockLane
{
 static const float LANE_HALF_WIDTH = 2.4;
 static const float LANE_OFFSET = 2.0;       // m off the road's centre line, each half (the bridge's)
 static const float LANE_TURN = 3.0;         // the diagonal between the halves runs over Z -3..3
 static const float LANE_MARGIN = 0.3;       // a moved piece's edge this far clear of the lane
 static const float BARRICADE_STAGGER = 3.0; // each half of a plain barricade, along the road
 static const float POST_RADIUS = 5;         // the holders' Defend round their post (the gun inside it)
 static const float POST_GAP = 8;           // the post: this far past the lane's edge, so the Defend stays 3 m off it
 static bool s_bOff;                         // tests: the vanilla layout, for the before/after
 // Our vehicle coming through (ACR_Squad.LaneSpeed): holders on foot in the lane or within
 // STEP_CLEAR of its edge, within STEP_ALONG of the roadblock along the road, step out to STEP_CLEAR
 // past the edge on their own side (an auto-playtest, 27 Oct: a man the Defend had left at the
 // lane's edge was brushed by our passing truck).
 static const float STEP_CLEAR = 2.0;
 static const float STEP_ALONG = 15;
 static const float STEP_PRIORITY = 91;      // over attacking (90); danger and medical still outrank it
 static ref map<IEntity, ref SCR_AIMoveIndividuallyBehavior> s_mSteps = new map<IEntity, ref SCR_AIMoveIndividuallyBehavior>();
 static int s_iStepsForTest;                 // step-clear moves ordered

 // The lane's centre line across the road (local X) at local Z.
 static float LaneX(float z)
 {
  if (z >= LANE_TURN) return LANE_OFFSET;
  if (z <= -LANE_TURN) return -LANE_OFFSET;
  return LANE_OFFSET * z / LANE_TURN;
 }

 // How far a footprint (local X x0..x1, Z z0..z1) is clear of the lane (negative: inside it by that).
 static float Clearance(float x0, float x1, float z0, float z1)
 {
  float lo = LaneX(z0) - LANE_HALF_WIDTH;
  float hi = LaneX(z1) + LANE_HALF_WIDTH;
  return Math.Max(lo - x1, x0 - hi);
 }

 static bool IsDecal(IEntity e)
 {
  string name = PrefabName(e);
  return name.Contains("Decal");
 }

 static string PrefabName(IEntity e)
 {
  if (!e) return "";
  EntityPrefabData data = e.GetPrefabData();
  if (!data) return "";
  // A composition's parts are instances in its prefab: the first ancestor that is a prefab file.
  string name = data.GetPrefabName();
  if (name.IsEmpty()) name = SCR_BaseContainerTools.GetPrefabResourceName(data.GetPrefab());
  return name;
 }

 // The pieces moved as one: the composition's children, but a log barricade nested in a
 // composition (the USSR medium barricade's) piece by piece.
 static void Units(IEntity root, notnull array<IEntity> units)
 {
  IEntity child = root.GetChildren();
  while (child)
  {
   if (PrefabName(child).Contains("Barricade_Logs") && child.GetChildren()) Units(child, units);
   else units.Insert(child);
   child = child.GetSibling();
  }
 }

 // A piece's footprint in the composition's frame (root): its own and its children's model
 // bounds; a piece with no model (a patrol point) is 0.8 m round its origin.
 static void Footprint(IEntity unit, vector root[4], out float x0, out float x1, out float z0, out float z1)
 {
  x0 = float.MAX;
  x1 = -float.MAX;
  z0 = float.MAX;
  z1 = -float.MAX;
  Grow(unit, root, x0, x1, z0, z1);
  if (x0 <= x1) return;
  vector at = unit.GetOrigin() - root[3];
  float x = vector.Dot(at, root[0]);
  float z = vector.Dot(at, root[2]);
  x0 = x - 0.4;
  x1 = x + 0.4;
  z0 = z - 0.4;
  z1 = z + 0.4;
 }

 protected static void Grow(IEntity e, vector root[4], inout float x0, inout float x1, inout float z0, inout float z1)
 {
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  if (maxs[0] - mins[0] > 0.05 || maxs[2] - mins[2] > 0.05)
  {
   vector t[4];
   e.GetWorldTransform(t);
   for (int k = 0; k < 4; k++)
   {
    float cx = mins[0];
    if (k % 2 == 1) cx = maxs[0];
    float cz = mins[2];
    if (k >= 2) cz = maxs[2];
    vector corner = t[3] + t[0] * cx + t[2] * cz - root[3];
    float x = vector.Dot(corner, root[0]);
    float z = vector.Dot(corner, root[2]);
    x0 = Math.Min(x0, x);
    x1 = Math.Max(x1, x);
    z0 = Math.Min(z0, z);
    z1 = Math.Max(z1, z);
   }
  }
  IEntity child = e.GetChildren();
  while (child)
  {
   Grow(child, root, x0, x1, z0, z1);
   child = child.GetSibling();
  }
 }

 // Opens the lane through a roadblock just spawned (before its navmesh rebuild); returns how many
 // pieces moved.
 static int Open(IEntity ent, bool barricade)
 {
  if (!ent || s_bOff) return 0;
  vector root[4];
  ent.GetWorldTransform(root);
  array<IEntity> units = {};
  Units(ent, units);
  int moved = 0;
  foreach (IEntity unit : units)
  {
   if (!unit || IsDecal(unit)) continue;
   float x0, x1, z0, z1;
   Footprint(unit, root, x0, x1, z0, z1);
   float dz = 0;
   if (barricade)
   {
    dz = -BARRICADE_STAGGER;
    if (x0 + x1 < 0) dz = BARRICADE_STAGGER;
   }
   float dx = 0;
   if (Clearance(x0, x1, z0 + dz, z1 + dz) < LANE_MARGIN)
   {
    float left = LaneX(z0 + dz) - LANE_HALF_WIDTH - LANE_MARGIN - x1;   // < 0
    float right = LaneX(z1 + dz) + LANE_HALF_WIDTH + LANE_MARGIN - x0;  // > 0
    dx = right;
    if (-left < right) dx = left;
   }
   if (dx == 0 && dz == 0) continue;
   Shift(unit, root[0] * dx + root[2] * dz);
   moved++;
  }
  ent.Update();
  return moved;
 }

 // Moves a piece and everything under it by 'offset', kept at its height above the ground.
 protected static void Shift(IEntity unit, vector offset)
 {
  BaseWorld world = GetGame().GetWorld();
  vector from = unit.GetOrigin();
  vector to = from + offset;
  offset[1] = world.GetSurfaceY(to[0], to[2]) - world.GetSurfaceY(from[0], from[2]);
  array<IEntity> parts = {};
  Collect(unit, parts);
  array<vector> origins = {};
  foreach (IEntity part : parts) origins.Insert(part.GetOrigin());
  foreach (int i, IEntity part : parts)
  {
   vector t[4];
   part.GetWorldTransform(t);
   t[3] = origins[i] + offset;
   part.SetWorldTransform(t);
   // (As the game editor moves a piece: its physics goes with it.)
   GenericEntity generic = GenericEntity.Cast(part);
   if (generic) generic.OnTransformReset();
   part.Update();
  }
 }

 protected static void Collect(IEntity e, notnull array<IEntity> parts)
 {
  parts.Insert(e);
  IEntity child = e.GetChildren();
  while (child)
  {
   Collect(child, parts);
   child = child.GetSibling();
  }
 }

 // The worst clearance of any solid piece from the lane (tests; negative: a piece in it).
 static float WorstClearance(IEntity ent, out string worst)
 {
  worst = "";
  if (!ent) return 0;
  vector root[4];
  ent.GetWorldTransform(root);
  array<IEntity> units = {};
  Units(ent, units);
  float least = float.MAX;
  foreach (IEntity unit : units)
  {
   if (!unit || IsDecal(unit)) continue;
   float x0, x1, z0, z1;
   Footprint(unit, root, x0, x1, z0, z1);
   float c = Clearance(x0, x1, z0, z1);
   if (c >= least) continue;
   least = c;
   worst = FilePath.StripPath(PrefabName(unit));
  }
  return least;
 }

 // The roadblock's men on foot: the squads holding it and its defenders.
 static void Holders(ACR_Roadblock rb, notnull array<IEntity> into)
 {
  into.Clear();
  array<AIAgent> agents = {};
  array<AIAgent> some = {};
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad || squad.m_ActiveRoadblock != rb) continue;
    squad.GetAgents(some);
    agents.InsertAll(some);
   }
  }
  if (rb.m_Defenders)
  {
   if (rb.m_Defenders.m_Group && rb.m_Defenders.m_Group.GetAgents(some) > 0) agents.InsertAll(some);
   if (rb.m_Defenders.m_GunCrew && rb.m_Defenders.m_GunCrew.GetAgents(some) > 0) agents.InsertAll(some);
  }
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && !ACR_Transport.VehicleOf(body) && !EntityUtils.IsPlayer(body) && !into.Contains(body)) into.Insert(body);
  }
 }

 // One of our vehicles is within ACR_Roadblocks.LANE_SLOW_RANGE (ACR_Squad.LaneSpeed): every
 // holder on foot in its way steps clear, to the nearer side of the lane (never across it in front
 // of the vehicle). Nothing for one standing still, or past and pulling away. Returns how many were sent.
 static int ClearFor(ACR_Roadblock rb, IEntity vehicle)
 {
  if (!rb || !rb.m_Entity || !vehicle || s_bOff) return 0;
  vector root[4];
  rb.m_Entity.GetWorldTransform(root);
  vector at = Local(rb.m_Entity, vehicle.GetOrigin());
  // A vehicle standing still (parked by the road) is in nobody's way.
  Physics phys = vehicle.GetPhysics();
  if (!phys || phys.GetVelocity().Length() < 1) return 0;
  if (Math.AbsFloat(at[2]) > 10 && at[2] * vector.Dot(phys.GetVelocity(), root[2]) > 0) return 0;
  // (Steps done, or of men gone, are forgotten.)
  for (int k = s_mSteps.Count() - 1; k >= 0; k--)
  {
   SCR_AIMoveIndividuallyBehavior old = s_mSteps.GetElement(k);
   if (!s_mSteps.GetKey(k) || !old || old.GetActionState() == EAIActionState.COMPLETED || old.GetActionState() == EAIActionState.FAILED) s_mSteps.RemoveElement(k);
  }
  array<IEntity> men = {};
  Holders(rb, men);
  int sent = 0;
  foreach (IEntity man : men)
  {
   vector l = Local(rb.m_Entity, man.GetOrigin());
   if (Math.AbsFloat(l[2]) > STEP_ALONG || Clearance(l[0] - 0.3, l[0] + 0.3, l[2], l[2]) >= STEP_CLEAR) continue;
   AIControlComponent control = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
   if (!control || !control.GetControlAIAgent()) continue;
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(control.GetControlAIAgent().FindComponent(SCR_AIUtilityComponent));
   if (!utility) continue;
   // A step he is carrying out stands. One that something else has put aside while he is still
   // in the way (his Defend walking him back across the lane to a post on the far side, srv:rblane
   // 27 Oct) is replaced, for as long as the vehicle comes on.
   SCR_AIMoveIndividuallyBehavior was = s_mSteps.Get(man);
   if (was && utility.GetCurrentAction() == was) continue;
   if (was && was.GetActionState() != EAIActionState.COMPLETED && was.GetActionState() != EAIActionState.FAILED) was.Fail();
   // The nearer side: the shortest walk out. (Always the post's side was tried: from the middle of
   // the lane that walk was longer and its end sat among the gun nest's sandbags, and men were still
   // in the lane when the truck came, srv:rblane 27 Oct.)
   float side = 1;
   if (l[0] < LaneX(l[2])) side = -1;
   vector to = World(rb.m_Entity, LaneX(l[2]) + side * (LANE_HALF_WIDTH + STEP_CLEAR + 0.5), l[2]);
   SCR_AIMoveIndividuallyBehavior step = new SCR_AIMoveIndividuallyBehavior(utility, null, to, priority: STEP_PRIORITY, radius: 1.0);
   utility.AddAction(step);
   s_mSteps.Set(man, step);
   sent++;
  }
  if (sent > 0)
  {
   s_iStepsForTest += sent;
   PrintFormat("[ACR] ROADBLOCK lane at %1: %2 holder(s) step clear for %3 (%4 m off along the road)", rb.m_vPos, sent, FilePath.StripPath(PrefabName(vehicle)), Math.Round(at[2]));
  }
  return sent;
 }

 // A point in the composition's frame, in the world.
 static vector World(IEntity ent, float x, float z)
 {
  vector root[4];
  ent.GetWorldTransform(root);
  return ACR_Manager.Ground(root[3] + root[0] * x + root[2] * z);
 }

 // A world point in the composition's frame (x across, z along; y unused).
 static vector Local(IEntity ent, vector pos)
 {
  vector root[4];
  ent.GetWorldTransform(root);
  vector d = pos - root[3];
  return Vector(vector.Dot(d, root[0]), 0, vector.Dot(d, root[2]));
 }

 // Where the roadblock's holders and defenders post: beside the lane on the gun's side, level with
 // the gun (no gun: the -X side, level with the middle), POST_GAP past the lane's edge there.
 static vector Post(IEntity ent, IEntity turret)
 {
  if (!ent) return vector.Zero;
  float side = -1;
  float z = 0;
  if (turret)
  {
   vector gun = Local(ent, turret.GetOrigin());
   if (gun[0] > LaneX(gun[2])) side = 1;
   z = gun[2];
  }
  float x = LaneX(z) + side * (LANE_HALF_WIDTH + POST_GAP);
  return World(ent, x, z);
 }
}
