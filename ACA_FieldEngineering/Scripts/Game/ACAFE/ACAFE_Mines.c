// Minefields (server): where the mines of an order go, the live fields with their mines and
// signs, the owning side's friendly-fire rule (SCR_PressureTriggerComponent asks Spares) and
// mine clearing. Mines are the vanilla editable ones: they arm themselves when spawned.
class ACAFE_Minefield : Managed
{
 int m_iId;
 int m_eOrder;
 string m_sFaction;
 bool m_bAI;                         // laid for the AI commander
 vector m_vCenter;
 float m_fRadius;                    // for the map ring
 ref array<IEntity> m_aMines = {};
 ref array<IEntity> m_aSigns = {};
 bool m_bLaying;                     // its job is still placing mines

 string Label()
 {
  return string.Format("%1 (%2)", ACAFE_Orders.ShortName(m_eOrder), m_aMines.Count());
 }
}

class ACAFE_Mines
{
 static ref array<ref ACAFE_Minefield> s_Fields = {};
 protected static ref map<IEntity, string> s_Owner = new map<IEntity, string>(); // live mine -> faction that laid it
 protected static int s_iNextId;

 static void Reset()
 {
  s_Fields.Clear();
  s_Owner.Clear();
  s_iNextId = 0;
 }

 // ---------------------------------------------------------------- friendly fire

 // A mine of ours touched by 'other': true = it stays quiet (see ACAFE_Config).
 static bool Spares(IEntity mine, IEntity other)
 {
  if (!mine || !other || s_Owner.IsEmpty()) return false;
  string owner;
  if (!s_Owner.Find(mine, owner)) return false;
  string rule = ACAFE_Settings.OwnMines();
  if (rule == "real") return false;
  IEntity root = other.GetRootParent();
  if (!root) root = other;
  ChimeraCharacter person = ChimeraCharacter.Cast(root);
  if (!person && Vehicle.Cast(root)) person = ACR_Squad.DriverOf(root);
  if (!person) return false;
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(person.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction() || affiliation.GetAffiliatedFaction().GetFactionKey() != owner) return false;
  if (ACR_Squad.FindController(person) != 0) return rule == "side";
  return true;
 }

 // ---------------------------------------------------------------- planning

 // Fills the job's mine plan (positions and kinds) and its site and stage. false + refusal
 // when there is no room.
 static bool Plan(ACAFE_Job job, vector click, vector from, out string refusal)
 {
  refusal = "";
  BaseWorld world = GetGame().GetWorld();
  click[1] = world.GetSurfaceY(click[0], click[2]);
  if (BBB_Terrain.IsWaterAt(click))
  {
   refusal = "that spot is in water";
   return false;
  }
  float half = ACAFE_Config.MINEFIELD_SIZE * 0.5;
  vector roadPoint;
  vector roadDir;
  float roadWidth;
  bool road = ACAFE_Roads.Snap(click, half + 5, roadPoint, roadDir, roadWidth);
  vector approach = click - from;
  approach[1] = 0;
  if (approach.Length() < 1) approach = "0 0 1";
  approach.Normalize();
  // The field lies along a road through it, else square to the squad's approach.
  vector forward = approach;
  if (road) forward = roadDir;
  vector center = click;
  if (road && job.m_eOrder != ACAFE_EOrder.MINES_AP) center = roadPoint;
  job.m_vSite = center;
  job.m_vForward = forward;

  foreach (ACAFE_Minefield field : s_Fields)
  {
   if (field.m_sFaction == job.m_sFaction && vector.DistanceXZ(field.m_vCenter, center) < half * 1.5)
   {
    refusal = "there is already a minefield of ours there";
    return false;
   }
  }
  if (CountMines(job.m_sFaction) + 1 > ACAFE_Config.MAX_MINES_PER_SIDE)
  {
   refusal = string.Format("your side already has %1 mines down (limit %2)", CountMines(job.m_sFaction), ACAFE_Config.MAX_MINES_PER_SIDE);
   return false;
  }

  int at = 0;
  int ap = 0;
  if (job.m_eOrder == ACAFE_EOrder.MINES_AP) ap = ACAFE_Config.MINES_AP_COUNT;
  if (job.m_eOrder == ACAFE_EOrder.MINES_AT) at = ACAFE_Config.MINES_AT_COUNT;
  if (job.m_eOrder == ACAFE_EOrder.MINES_MIXED)
  {
   at = ACAFE_Config.MIXED_AT_COUNT;
   ap = ACAFE_Config.MIXED_AP_COUNT;
  }
  int wantedTotal = at + ap;
  if (ACAFE_Settings.KitMines() && !KitPlan(job, at, ap, refusal)) return false;
  job.m_aPlan.Clear();
  job.m_aPlanAT.Clear();
  // AT: staggered in both lanes of the road through the field; no road, anywhere.
  if (at > 0 && road) RoadMines(job, center, forward, roadWidth, half, at);
  int atLeft = at - job.m_aPlan.Count();
  if (atLeft > 0) ScatterMines(job, center, forward, half, atLeft, true, ACAFE_Config.AT_SPACING, false);
  // AP: off the road (when there is one).
  if (ap > 0) ScatterMines(job, center, forward, half, ap, false, ACAFE_Config.AP_SPACING, road);
  if (job.m_aPlan.IsEmpty())
  {
   refusal = "no room for mines there (water or buildings)";
   return false;
  }
  // The squad works from the near edge, outside the field.
  job.m_vStage = ACR_Manager.Ground(center - approach * (half + 8));
  if (BBB_Terrain.IsWaterAt(job.m_vStage)) job.m_vStage = ACR_Manager.Ground(center);
  job.m_fWorkRadius = half + ACAFE_Config.WORK_RADIUS * 0.6;
  // A smaller field (few mines in kit, no room) takes less time.
  job.m_fDuration = ACAFE_Orders.Duration(job.m_eOrder) * Math.Max(0.25, job.m_aPlan.Count() / (float)Math.Max(1, wantedTotal));
  return true;
 }

 // Kit mines: as many of the wanted mines as the soldiers carry, the rest from the construction
 // truck's supplies (KIT_TRUCK_SUPPLIES_AP / _AT each, costs setting applied). Fewer mines
 // than wanted make a smaller field; none at all is refused.
 protected static bool KitPlan(ACAFE_Job job, inout int at, inout int ap, out string refusal)
 {
  refusal = "";
  int kitAP;
  int kitAT;
  ACAFE_Kit.Count(job.m_Squad, kitAP, kitAT);
  IEntity truck;
  float truckSupplies = ACAFE_Kit.TruckSupplies(job.m_Squad, truck);
  float scale = ACAFE_Settings.CostScale();
  int perAT = (int)Math.Round(ACAFE_Config.KIT_TRUCK_SUPPLIES_AT * scale);
  int perAP = (int)Math.Round(ACAFE_Config.KIT_TRUCK_SUPPLIES_AP * scale);
  int atKit = Math.ClampInt(kitAT, 0, at);
  int apKit = Math.ClampInt(kitAP, 0, ap);
  int atTruck = 0;
  int apTruck = 0;
  if (truck)
  {
   atTruck = at - atKit;
   apTruck = ap - apKit;
   if (perAT > 0) atTruck = Math.ClampInt((int)(truckSupplies / perAT), 0, atTruck);
   truckSupplies -= atTruck * perAT;
   if (perAP > 0) apTruck = Math.ClampInt((int)(truckSupplies / perAP), 0, apTruck);
  }
  int wantedAT = at;
  int wantedAP = ap;
  at = atKit + atTruck;
  ap = apKit + apTruck;
  job.m_iKitAT = atKit;
  job.m_iKitAP = apKit;
  job.m_iTruckMineSupplies = atTruck * perAT + apTruck * perAP;
  PrintFormat("[ACAFE] %1 kit mines: carries %2 AP / %3 AT, truck %4 -> laying %5 AP / %6 AT", job.SquadName(), kitAP, kitAT, truck != null, ap, at);
  if (at + ap > 0) return true;
  string kind = "AP mines";
  if (wantedAP == 0) kind = "AT mines";
  if (wantedAP > 0 && wantedAT > 0) kind = "mines";
  refusal = string.Format("the squad carries no %1 and has no construction truck with supplies for them within 50 m (US and USSR sappers carry demolition charges, not mines; FIA sappers carry PMN-4s)", kind);
  return false;
 }

 protected static void RoadMines(ACAFE_Job job, vector center, vector forward, float roadWidth, float half, int count)
 {
  float step = Math.Max(ACAFE_Config.AT_SPACING, (half * 2) / count);
  float lane = Math.Max(0.6, roadWidth * 0.25);
  float along = -half + step * 0.5;
  int placed;
  for (int i = 0; i < count * 2 && placed < count && along <= half; i++)
  {
   float across = lane;
   if (i % 2 == 1) across = -lane;
   vector guess = ACAFE_Roads.ToWorld(center, forward, Vector(across, 0, along + Math.RandomFloat(-0.8, 0.8)));
   // Follow the road where it bends.
   vector onRoad;
   vector dir;
   float width;
   if (ACAFE_Roads.Snap(guess, 8, onRoad, dir, width))
   {
    vector side = Vector(dir[2], 0, -dir[0]);
    guess = onRoad + side * across;
    guess[1] = GetGame().GetWorld().GetSurfaceY(guess[0], guess[2]);
   }
   along += step;
   if (!Usable(job, guess, ACAFE_Config.AT_SPACING * 0.75)) continue;
   job.m_aPlan.Insert(guess);
   job.m_aPlanAT.Insert(true);
   placed++;
  }
  // Snap() above replaced the remembered road: take the field's own road again.
  vector keepPoint;
  vector keepDir;
  float keepWidth;
  ACAFE_Roads.Snap(center, half + 5, keepPoint, keepDir, keepWidth);
 }

 protected static void ScatterMines(ACAFE_Job job, vector center, vector forward, float half, int count, bool antiTank, float spacing, bool offRoad)
 {
  int placed;
  for (int tries = 0; tries < count * 20 && placed < count; tries++)
  {
   vector local = Vector(Math.RandomFloat(-half, half), 0, Math.RandomFloat(-half, half));
   vector pos = ACAFE_Roads.ToWorld(center, forward, local);
   if (offRoad && ACAFE_Roads.OffRoadDistance(pos) < 1.0) continue;
   if (!Usable(job, pos, spacing)) continue;
   job.m_aPlan.Insert(pos);
   job.m_aPlanAT.Insert(antiTank);
   placed++;
  }
 }

 // Dry, not inside a building or wall, and clear of the mines already planned.
 protected static bool Usable(ACAFE_Job job, vector pos, float spacing)
 {
  if (BBB_Terrain.IsWaterAt(pos)) return false;
  foreach (vector other : job.m_aPlan)
   if (vector.DistanceXZ(other, pos) < spacing) return false;
  s_Blocked = false;
  GetGame().GetWorld().QueryEntitiesBySphere(pos, 1.0, CollectBlocker, null, EQueryEntitiesFlags.STATIC);
  return !s_Blocked;
 }

 protected static bool s_Blocked;
 protected static bool CollectBlocker(IEntity e)
 {
  if (!e || e.GetParent()) return true;
  if (SCR_DestructibleBuildingEntity.Cast(e) || BaseBuilding.Cast(e) || WallGeneratorEntity.Cast(e))
  {
   s_Blocked = true;
   return false;
  }
  return true;
 }

 // ---------------------------------------------------------------- laying

 // Mines go in as the work progresses: placed = planned x progress.
 static void Lay(ACAFE_Job job, float fraction)
 {
  int wanted = Math.ClampInt((int)Math.Floor(job.m_aPlan.Count() * fraction), 0, job.m_aPlan.Count());
  while (job.m_iLaid < wanted)
  {
   ACAFE_Minefield field = FieldOf(job);
   int index = job.m_iLaid;
   job.m_iLaid++;
   bool antiTank = job.m_aPlanAT[index];
   ResourceName prefab = ACAFE_Orders.MineFor(job.m_sFaction, antiTank);
   // Kit mines first (each comes out of a soldier's kit), then the truck's.
   if (antiTank && job.m_iKitAT > 0)
   {
    job.m_iKitAT--;
    if (!job.m_Squad || !ACAFE_Kit.Take(job.m_Squad, true, job.m_sFaction, prefab)) continue;
   }
   else if (!antiTank && job.m_iKitAP > 0)
   {
    job.m_iKitAP--;
    if (!job.m_Squad || !ACAFE_Kit.Take(job.m_Squad, false, job.m_sFaction, prefab)) continue;
   }
   IEntity mine = SpawnMine(prefab, job.m_aPlan[index]);
   if (!mine) continue;
   field.m_aMines.Insert(mine);
   s_Owner.Set(mine, job.m_sFaction);
  }
 }

 // The job's field, made with its signs when the first mine goes in.
 static ACAFE_Minefield FieldOf(ACAFE_Job job)
 {
  if (job.m_Field) return job.m_Field;
  ACAFE_Minefield field = new ACAFE_Minefield();
  s_iNextId++;
  field.m_iId = s_iNextId;
  field.m_eOrder = job.m_eOrder;
  field.m_sFaction = job.m_sFaction;
  field.m_bAI = job.m_bAI;
  field.m_vCenter = job.m_vSite;
  field.m_fRadius = ACAFE_Config.MINEFIELD_SIZE * 0.6;
  field.m_bLaying = true;
  s_Fields.Insert(field);
  job.m_Field = field;
  if (ACAFE_Config.MINEFIELD_SIGNS) PlaceSigns(field, job.m_vForward);
  PrintFormat("[ACAFE] %1 minefield %2 started at %3 (%4 mines planned)", job.m_sFaction, field.m_iId, field.m_vCenter, job.m_aPlan.Count());
  return field;
 }

 static IEntity SpawnMine(ResourceName prefab, vector pos)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid())
  {
   PrintFormat("[ACAFE] mine prefab missing: %1", prefab);
   return null;
  }
  EntitySpawnParams spawnParams = new EntitySpawnParams();
  spawnParams.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(Math.RandomFloat(-180, 180), 0, 0), spawnParams.Transform);
  pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
  spawnParams.Transform[3] = pos;
  SCR_TerrainHelper.OrientToTerrain(spawnParams.Transform);
  return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
 }

 // Vanilla minefield signs at the four corners, facing out.
 protected static void PlaceSigns(ACAFE_Minefield field, vector forward)
 {
  Resource resource = Resource.Load(ACAFE_Orders.MINEFIELD_SIGN);
  if (!resource.IsValid()) return;
  float edge = ACAFE_Config.MINEFIELD_SIZE * 0.5 + 2;
  array<vector> corners = {};
  corners.Insert(Vector(edge, 0, edge));
  corners.Insert(Vector(-edge, 0, edge));
  corners.Insert(Vector(edge, 0, -edge));
  corners.Insert(Vector(-edge, 0, -edge));
  foreach (vector corner : corners)
  {
   vector pos = ACAFE_Roads.ToWorld(field.m_vCenter, forward, corner);
   if (BBB_Terrain.IsWaterAt(pos)) continue;
   vector outward = pos - field.m_vCenter;
   outward[1] = 0;
   EntitySpawnParams spawnParams = new EntitySpawnParams();
   spawnParams.TransformMode = ETransformMode.WORLD;
   Math3D.AnglesToMatrix(Vector(ACAFE_Roads.Yaw(outward) + ACAFE_Config.SIGN_YAW_OFFSET, 0, 0), spawnParams.Transform);
   spawnParams.Transform[3] = pos;
   IEntity sign = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
   if (sign) field.m_aSigns.Insert(sign);
  }
 }

 // The job is over (done or called off): a field that never got a mine goes again.
 static void Finish(ACAFE_Job job)
 {
  ACAFE_Minefield field = job.m_Field;
  if (!field) return;
  field.m_bLaying = false;
  if (field.m_aMines.IsEmpty()) RemoveField(field);
 }

 // ---------------------------------------------------------------- upkeep and clearing

 // Every second: forget mines that went off or were lifted; empty fields go with their signs.
 static void Tick()
 {
  for (int f = s_Fields.Count() - 1; f >= 0; f--)
  {
   ACAFE_Minefield field = s_Fields[f];
   for (int m = field.m_aMines.Count() - 1; m >= 0; m--)
   {
    IEntity mine = field.m_aMines[m];
    if (mine && !Spent(mine)) continue;
    field.m_aMines.RemoveOrdered(m);
   }
   if (!field.m_bLaying && field.m_aMines.IsEmpty())
   {
    PrintFormat("[ACAFE] %1 minefield %2 is gone (every mine went off or was lifted)", field.m_sFaction, field.m_iId);
    RemoveField(field);
   }
  }
  // Rebuild the owner lookup from the live fields (deleted mines leave empty keys behind).
  if (s_Owner.Count() > CountMines(""))
  {
   s_Owner.Clear();
   foreach (ACAFE_Minefield live : s_Fields)
   {
    foreach (IEntity liveMine : live.m_aMines)
    {
     if (liveMine) s_Owner.Set(liveMine, live.m_sFaction);
    }
   }
  }
 }

 // A mine that already went off (its trigger fired) counts as gone.
 static bool Spent(IEntity mine)
 {
  BaseTriggerComponent trigger = BaseTriggerComponent.Cast(mine.FindComponent(BaseTriggerComponent));
  return trigger && trigger.WasTriggered();
 }

 static void RemoveField(ACAFE_Minefield field)
 {
  foreach (IEntity sign : field.m_aSigns) if (sign) SCR_EntityHelper.DeleteEntityAndChildren(sign);
  field.m_aSigns.Clear();
  foreach (IEntity mine : field.m_aMines)
  {
   if (!mine) continue;
   s_Owner.Remove(mine);
  }
  s_Fields.RemoveItem(field);
 }

 // Live mines of a side ("" = every side).
 static int CountMines(string faction)
 {
  int count;
  foreach (ACAFE_Minefield field : s_Fields)
   if (faction.IsEmpty() || field.m_sFaction == faction) count += field.m_aMines.Count();
  return count;
 }

 static int CountFields(string faction)
 {
  int count;
  foreach (ACAFE_Minefield field : s_Fields) if (field.m_sFaction == faction) count++;
  return count;
 }

 static ACAFE_Minefield FieldNear(string faction, vector pos, float radius)
 {
  foreach (ACAFE_Minefield field : s_Fields)
   if ((faction.IsEmpty() || field.m_sFaction == faction) && vector.DistanceXZ(field.m_vCenter, pos) <= radius) return field;
  return null;
 }

 // Armed mines lying in the world round a point (ours, the enemy's, Game Master's), nearest
 // to 'from' first.
 protected static ref array<IEntity> s_Found = {};
 static IEntity NearestMine(vector center, float radius, vector from)
 {
  s_Found.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(center, radius, CollectMine);
  IEntity best;
  float bestDistance = float.MAX;
  foreach (IEntity mine : s_Found)
  {
   float d = vector.DistanceXZ(mine.GetOrigin(), from);
   if (d < bestDistance) { bestDistance = d; best = mine; }
  }
  s_Found.Clear();
  return best;
 }

 static int CountMinesAt(vector center, float radius)
 {
  s_Found.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(center, radius, CollectMine);
  int count = s_Found.Count();
  s_Found.Clear();
  return count;
 }

 protected static bool CollectMine(IEntity e)
 {
  if (!e || e.GetParent()) return true; // carried in someone's kit
  SCR_PressureTriggerComponent trigger = SCR_PressureTriggerComponent.Cast(e.FindComponent(SCR_PressureTriggerComponent));
  if (!trigger || !trigger.IsActivated() || trigger.WasTriggered()) return true;
  s_Found.Insert(e);
  return true;
 }

 // Lift one mine: it is deleted (disarmed and carried off).
 static void Lift(IEntity mine)
 {
  if (!mine) return;
  s_Owner.Remove(mine);
  foreach (ACAFE_Minefield field : s_Fields) field.m_aMines.RemoveItem(mine);
  SCR_EntityHelper.DeleteEntityAndChildren(mine);
 }

 // ---------------------------------------------------------------- command map

 // aiToo: also the AI commanders' fields of other sides (test runs).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels, bool aiToo = false)
 {
  foreach (ACAFE_Minefield field : s_Fields)
  {
   if (field.m_aMines.IsEmpty()) continue;
   string owner = "";
   if (field.m_sFaction != faction)
   {
    if (!aiToo || !field.m_bAI) continue;
    owner = "AI " + field.m_sFaction + " ";
   }
   positions.Insert(field.m_vCenter);
   radii.Insert(field.m_fRadius);
   labels.Insert(owner + field.Label());
  }
 }
}
