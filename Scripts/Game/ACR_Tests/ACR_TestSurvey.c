// -acrSurveyTest 1 (server, Arland, ~40 s): dumps what stands round chosen spots so Claude can draw
// a top-down plan and place bases/pads without a human: every static object (prefab, position,
// yaw, model-space box), road polylines, and a 3 m grid of terrain height, object height above the
// terrain and surface material. Lines: SURVEY <region> OBJ|ROAD|ROW|MAT ...
class ACR_TestSurvey
{
 static ref array<IEntity> s_Found = {};
 static ref map<string, int> s_Mats = new map<string, int>();

 static bool Collect(IEntity e)
 {
  if (!e) return true;
  s_Found.Insert(e);
  return true;
 }

 static void Region(string tag, float cx, float cz, float half)
 {
  BaseWorld world = GetGame().GetWorld();
  s_Found.Clear();
  world.QueryEntitiesBySphere(Vector(cx, world.GetSurfaceY(cx, cz), cz), half * 1.42, Collect, null, EQueryEntitiesFlags.STATIC);
  int objects;
  foreach (IEntity e : s_Found)
  {
   vector o = e.GetOrigin();
   if (Math.AbsFloat(o[0] - cx) > half || Math.AbsFloat(o[2] - cz) > half) continue;
   vector mins, maxs;
   e.GetBounds(mins, maxs);
   vector size = maxs - mins;
   if (size[0] < 0.8 && size[2] < 0.8) continue;
   string name = e.ClassName();
   if (e.GetPrefabData()) name = FilePath.StripPath(e.GetPrefabData().GetPrefabName());
   vector ypr = e.GetYawPitchRoll();
   PrintFormat("[ACR] SURVEY %1 OBJ %2 %3 %4 %5 %6 %7 %8 %9", tag, name, o[0].ToString(-1, 1), o[2].ToString(-1, 1), (o[1] - world.GetSurfaceY(o[0], o[2])).ToString(-1, 1), ypr[0].ToString(-1, 1),
    string.Format("%1,%2,%3", mins[0].ToString(-1, 1), mins[1].ToString(-1, 1), mins[2].ToString(-1, 1)), string.Format("%1,%2,%3", maxs[0].ToString(-1, 1), maxs[1].ToString(-1, 1), maxs[2].ToString(-1, 1)), e.GetScale().ToString(-1, 2));
   objects++;
  }
  // roads
  SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
  if (aiWorld && aiWorld.GetRoadNetworkManager())
  {
   array<BaseRoad> roads = {};
   aiWorld.GetRoadNetworkManager().GetRoadsInAABB(Vector(cx - half, -100, cz - half), Vector(cx + half, 1000, cz + half), roads);
   foreach (BaseRoad road : roads)
   {
    array<vector> points = {};
    road.GetPoints(points);
    string line;
    foreach (vector p : points) line += string.Format(" %1,%2", (int)p[0], (int)p[2]);
    PrintFormat("[ACR] SURVEY %1 ROAD%2", tag, line);
   }
  }
  // grid: terrain height (dm), object height above terrain (half metres), material index
  for (float z = cz + half; z >= cz - half; z -= 3)
  {
   string ter, obj, mat;
   for (float x = cx - half; x <= cx + half; x += 3)
   {
    float ty = world.GetSurfaceY(x, z);
    ter += string.Format(" %1", (int)(ty * 10));
    TraceParam trace = new TraceParam();
    trace.Start = Vector(x, ty + 60, z);
    trace.End = Vector(x, ty - 1, z);
    trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
    float fraction = world.TraceMove(trace, null);
    float above = 0;
    if (fraction < 1) above = 60 - 61 * fraction;
    obj += string.Format(" %1", Math.Clamp((int)(above * 2), 0, 199));
    string m = trace.TraceMaterial;
    if (above > 0.5) m = "obj";
    int idx;
    if (!s_Mats.Find(m, idx)) { idx = s_Mats.Count(); s_Mats.Insert(m, idx); }
    mat += string.Format(" %1", idx);
   }
   PrintFormat("[ACR] SURVEY %1 TER %2%3", tag, (int)z, ter);
   PrintFormat("[ACR] SURVEY %1 OBH %2%3", tag, (int)z, obj);
   PrintFormat("[ACR] SURVEY %1 MAT %2%3", tag, (int)z, mat);
  }
  PrintFormat("[ACR] SURVEY %1 DONE centre %2 %3 half %4 objects %5", tag, cx, cz, half, objects);
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 5) return;
  Region("airfield", 1270, 2830, 220);
  Region("barracks", 1445, 2367, 110);
  Region("farm", 2094, 2940, 150);
  Region("heide", 3150, 2790, 220);
  foreach (string m, int idx : s_Mats) PrintFormat("[ACR] SURVEY MATERIAL %1 = %2", idx, m);
  Print("[ACR] TEST COMPLETE");
 }
}
