// -acrPlacesTest 1 (server, any world, ~30 s): lists every named map location (towns, villages,
// local names, map icons) with its type and position, so Claude can lay out bases on a new map.
// Lines: [ACR] PLACE <type> <x> <z> <name> | <entity name> | <prefab>
class ACR_TestPlaces
{
 static ref array<IEntity> s_Found = {};

 static bool Collect(IEntity e)
 {
  if (e && e.FindComponent(MapDescriptorComponent)) s_Found.Insert(e);
  return true;
 }

 static void Run(int tick)
 {
  if (tick != 5) return;
  BaseWorld world = GetGame().GetWorld();
  vector mins, maxs;
  world.GetBoundBox(mins, maxs);
  PrintFormat("[ACR] PLACES world box %1 .. %2", mins, maxs);
  s_Found.Clear();
  world.QueryEntitiesByAABB(Vector(mins[0], -500, mins[2]), Vector(maxs[0], 3000, maxs[2]), Collect);
  foreach (IEntity e : s_Found)
  {
   MapDescriptorComponent d = MapDescriptorComponent.Cast(e.FindComponent(MapDescriptorComponent));
   int type = d.GetBaseType();
   string name;
   MapItem item = d.Item();
   if (item) name = item.GetDisplayName();
   if (name.StartsWith("#")) name = name + " = " + WidgetManager.Translate(name);
   string prefab;
   if (e.GetPrefabData()) prefab = FilePath.StripPath(e.GetPrefabData().GetPrefabName());
   vector o = e.GetOrigin();
   PrintFormat("[ACR] PLACE %1 %2 %3 %4 | %5 | %6", typename.EnumToString(EMapDescriptorType, type), (int)o[0], (int)o[2], name, e.GetName(), prefab);
  }
  PrintFormat("[ACR] PLACES %1 found", s_Found.Count());
  // Terrain height under every scenario entity, so the layer's coords can carry real heights.
  array<IEntity> mine = {};
  foreach (ACR_Base b : ACR_Base.s_All) mine.Insert(b);
  foreach (ACR_StartingUnit u : ACR_StartingUnit.s_All) mine.Insert(u);
  foreach (ACR_Helipad pad : ACR_Helipad.s_All) mine.Insert(pad);
  foreach (ACR_FactionSettings f : ACR_FactionSettings.s_All) mine.Insert(f);
  foreach (SCR_SpawnPoint sp : SCR_SpawnPoint.GetSpawnPoints()) mine.Insert(sp);
  foreach (IEntity m : mine)
  {
   if (!m) continue;
   vector at = m.GetOrigin();
   PrintFormat("[ACR] HEIGHT %1 %2 %3 %4", m.GetName(), (int)at[0], (int)at[2], world.GetSurfaceY(at[0], at[2]).ToString(-1, 1));
  }
  Print("[ACR] TEST COMPLETE");
 }

 // -acrEveronSurveyTest 1: ACR_TestSurvey dumps of the Everon HQ sites (airbase NE military site,
 // Saint-Pierre harbour) for placing HQ bases and helipads.
 static void SurveyEveron(int tick)
 {
  if (tick != 5) return;
  ACR_TestSurvey.Region("airbase", 4960, 11900, 210);
  ACR_TestSurvey.Region("harbour", 10090, 1540, 180);
  foreach (string m, int idx : ACR_TestSurvey.s_Mats) PrintFormat("[ACR] SURVEY MATERIAL %1 = %2", idx, m);
  Print("[ACR] TEST COMPLETE");
 }
}
