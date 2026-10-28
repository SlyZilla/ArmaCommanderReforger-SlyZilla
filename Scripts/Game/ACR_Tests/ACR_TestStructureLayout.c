// -acrStructureLayoutTest 1: our tilted structures look the same on a client (ACR_StructureLayout). A
// sandbag position (Fortify) and a barricade (a roadblock) are placed on a slope the real way, which
// tilts them and records their layout. Then each one's prefab is spawned again, level, as a client
// builds it, and gets that record the way a client does (the RplProp's callback): every piece must end
// up where the server's is. Before the record, the level copy's pieces are off by the tilt. A record
// that doesn't fit its composition is refused and moves nothing. The two-process check (a dedicated
// server and a joined client) is the game-client test's.
class ACR_TestStructureLayout : ACR_TestGroundTilt
{
 static const int LAYOUT_CHECKS = 3;

 static void RunLayout(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   ACR_TestRegress.Abort("structlayout", "no Alpha squad");
   return;
  }
  float uphill;
  vector slope = FindGround(alpha.GetAnchor().GetOrigin(), 0.15, 0.3, uphill);
  if (slope == vector.Zero)
  {
   ACR_TestRegress.Abort("structlayout", "no sloped ground near the range");
   return;
  }
  PrintFormat("[ACR] STRUCTLAYOUT slope at %1, uphill %2 deg", slope, (int)uphill);
  IEntity bags = Spawn(ACR_FobDefences.SANDBAGS_US, slope, uphill);
  IEntity barricade = Spawn(ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.BARRICADE, "US", ACR_Roadblocks.RoadWidthAt(slope)), slope + Vector(0, 0, 12), uphill);
  string why;
  bool ok = AsClient(bags, why);
  ACR_TestRegress.Check("structlayout: a tilted sandbag position, built level from its prefab as a client does, takes the server's layout from the record", ok, why);
  ok = AsClient(barricade, why);
  ACR_TestRegress.Check("structlayout: a tilted barricade (a roadblock's own spawn) likewise", ok, why);
  // A record from another composition: refused, nothing moved.
  array<int> wrong = {};
  RecordOf(barricade, wrong);
  IEntity copy = Copy(bags);
  int before = ACR_StructureLayout.s_iMismatch;
  vector was = copy.GetOrigin();
  bool applied = copy && ACR_StructureLayout.Apply(copy, wrong);
  bool refused = copy && !applied && ACR_StructureLayout.s_iMismatch == before + 1 && vector.Distance(copy.GetOrigin(), was) < 0.001;
  ACR_TestRegress.Check("structlayout: a record that doesn't fit its composition is refused and moves nothing", refused, string.Format("applied %1, mismatches %2 -> %3", applied, before, ACR_StructureLayout.s_iMismatch));
  Remove(copy);
  Remove(bags);
  Remove(barricade);
  ACR_TestRegress.Complete("structlayout", LAYOUT_CHECKS);
 }

 // The layout 'root' recorded, its prefab spawned again level (a client's view), then the record
 // delivered through the replicated property's callback: how far its pieces were and are from the server's.
 protected static bool AsClient(IEntity root, out string why)
 {
  why = "not placed";
  if (!root) return false;
  array<int> record = {};
  RecordOf(root, record);
  array<IEntity> server = {};
  ACR_StructureLayout.Parts(root, server);
  IEntity copy = Copy(root);
  if (!copy)
  {
   why = "no copy";
   return false;
  }
  float before = Off(server, copy);
  SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(copy.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (comp) comp.ACR_TestReceiveLayout(record);
  float after = Off(server, copy);
  Remove(copy);
  int n = server.Count();
  int moved = (record.Count() - ACR_StructureLayout.HEADER) / 4;
  why = string.Format("%1 pieces, %2 recorded: %3 bytes (plain floats would be %4); the level copy %5 cm off the server's pieces before, %6 cm after", n, moved, ACR_StructureLayout.Bytes(record.Count()), (13 + n * 12) * 4 + 4, Cm(before), Cm(after));
  PrintFormat("[ACR] STRUCTLAYOUT %1: %2", FilePath.StripPath(root.GetPrefabData().GetPrefabName()), why);
  return n > 0 && moved == n && before > 0.02 && after < 0.01;
 }

 protected static void RecordOf(IEntity root, notnull array<int> record)
 {
  if (!root) return;
  SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(root.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (comp) comp.ACR_TestLayout(record);
 }

 // The same prefab, spawned plainly (no tilt, no record) where 'root' stands.
 protected static IEntity Copy(IEntity root)
 {
  if (!root || !root.GetPrefabData()) return null;
  Resource res = Resource.Load(root.GetPrefabData().GetPrefabName());
  if (!res.IsValid()) return null;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  vector ypr = root.GetYawPitchRoll();
  Math3D.AnglesToMatrix(Vector(ypr[0], 0, 0), params.Transform);
  params.Transform[3] = root.GetOrigin();
  return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
 }

 // The worst distance (m) between a server piece and the copy's piece in the same place in the hierarchy,
 // its origin or 1 m out along each of its axes (so a wrong tilt counts).
 protected static float Off(notnull array<IEntity> server, IEntity copy)
 {
  array<IEntity> mine = {};
  ACR_StructureLayout.Parts(copy, mine);
  if (mine.Count() != server.Count()) return 1000;
  float worst = 0;
  foreach (int i, IEntity part : server)
  {
   vector a[4];
   vector b[4];
   part.GetWorldTransform(a);
   mine[i].GetWorldTransform(b);
   worst = Math.Max(worst, vector.Distance(a[3], b[3]));
   for (int k = 0; k < 3; k++) worst = Math.Max(worst, vector.Distance(a[3] + a[k], b[3] + b[k]));
  }
  return worst;
 }
}

// Test-only hooks on the record (not in the player package).
modded class SCR_CampaignBuildingCompositionComponent
{
 void ACR_TestLayout(notnull array<int> layout) { layout.Copy(m_aACRLayout); }

 // As a client's RplProp callback gets it.
 void ACR_TestReceiveLayout(notnull array<int> layout)
 {
  m_aACRLayout.Copy(layout);
  ACR_OnLayout();
 }
}
