// -acrPartsTest 1 (a game client, about 3 min, the test range): the toolkit in a yard, for a look and for numbers.
// Every piece the works could use, laid out with its measured size (PART lines), then each emplacement shape for
// US and USSR with defenders told to crew its gun, a line of fire traced from it and an enemy put in front.
// Shots: $profile:acr-shots/parts_*.
class ACR_TestParts
{
 static const float STEP_S = 3;
 protected static ref array<ResourceName> s_aCatalog;
 static const ResourceName DIRT_V1 = "{FD625252F3654B76}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_long_v1.et";
 static const ResourceName DIRT_V2 = "{913666B24FA1C458}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_long_v2.et";
 static const ResourceName DIRT_V3 = "{74AA2A4B43445333}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_long_v3.et";
 static const ResourceName DIRT_SHORT = "{D7FB89CF21DDA2A3}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_short.et";
 static const ResourceName CAMO_BAGS_US = "{2E67A6863167B995}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Sandbag_Camo_long_high_US.et";
 static const ResourceName CAMO_BAGS_EAST = "{777E1B31B83D74E4}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/Fortifications/E_Sandbag_Camo_long_high_USSR.et";
 static const ResourceName LOGS_01 = "{3BAF6AA8C20D924E}Prefabs/Compositions/Misc/SubCompositions/Fortifications/Barricade_Logs_01.et";
 static const ResourceName LOGS_02 = "{D509D5E7AA748F7C}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Barricade_Logs_02.et";
 static const ResourceName TIRES_US = "{AA2824045F9513D9}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Tire_Stack_US_02.et";
 static const ResourceName TIRES_EAST = "{E30D820A04173B46}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Tire_Stack_USSR_03.et";
 static const ResourceName TIRE_PILE = "{73363BBA2AEA463C}PrefabsEditable/Auto/Props/Garbage/Tires/E_TirePile_01_medium.et";
 static const ResourceName NET_SMALL_US = "{D0315073F7B22594}PrefabsEditable/Auto/Structures/Military/CamoNets/US/E_CamoNet_Small_US.et";
 static const ResourceName NET_SMALL_TOP_US = "{D6DE8E5230DA293F}PrefabsEditable/Auto/Structures/Military/CamoNets/US/E_CamoNet_Small_Top_US.et";
 static const ResourceName HEDGEHOG = "{E6AE368A342034CB}PrefabsEditable/Auto/Props/Military/Fortification/E_CzechHedgehog_01_rusty.et";
 static const ResourceName LOW_LONG_US = "{4ADCE267EC510331}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_long_plastic.et";
 protected static vector s_vYard;
 protected static ref array<ACR_Emplacement> s_aEmplacements;
 protected static ref array<ref ACR_Emplacement> s_aKeep;
 protected static ref array<IEntity> s_aGuns;
 protected static ref array<vector> s_aEyes;
 protected static ref array<vector> s_aAngles;
 protected static ref array<string> s_aNames;
 protected static ref array<IEntity> s_aWalls;
 static void ACR_InitStatics()
 {
  s_aCatalog = {
   "{4ADCE267EC510331}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_long_plastic.et",
   "{7F118C6EA2AF1C99}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_long_high_plastic.et",
   "{BDAA871A06DABD74}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_long_burlap.et",
   "{A3A7C365E845B1B6}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_long_high_burlap.et",
   "{3F5EE4D69DBC478C}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_short_plastic.et",
   "{3A64925BC81FC2C0}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_short_high_plastic.et",
   "{3C9FB6869EB635BB}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_round_plastic.et",
   "{87588E4A1561C4D1}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_round_high_plastic.et",
   "{74B3C3B9AFC69136}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_end_plastic.et",
   "{7949746E8D314957}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_end_high_plastic.et",
   "{C3F2F7B92EAB5A14}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_wall_solid_plastic.et",
   "{D2299274C13B1169}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_wall_solid_burlap.et",
   "{525781C399409474}PrefabsEditable/Auto/Props/Infrastructure/Roadblocks/E_RoadBlock_01.et",
   "{B08BCB2AD5A57B72}PrefabsEditable/Auto/Structures/Military/Fortifications/E_Dragontooth_01_V1.et",
   "{FD625252F3654B76}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_long_v1.et",
   "{95BFB5C33EA78DE6}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtPile_01_small.et",
   "{E6AE368A342034CB}PrefabsEditable/Auto/Props/Military/Fortification/E_CzechHedgehog_01_rusty.et",
   "{1DB9C9CAEA70E7E5}PrefabsEditable/Auto/Props/Military/Fortification/E_BarbedTape_KnifeRest.et",
   "{E1721BBF5A1ABD24}PrefabsEditable/Auto/Props/Military/Fortification/E_BarbedTape_Coil.et",
   "{3BAF6AA8C20D924E}Prefabs/Compositions/Misc/SubCompositions/Fortifications/Barricade_Logs_01.et",
   "{5B91941C0D244457}Prefabs/Compositions/Misc/SubCompositions/Fortifications/Sandbag_Wall_Ladder_US_01.et",
   "{2E67A6863167B995}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Sandbag_Camo_long_high_US.et",
   "{B5AFD959449C1627}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Sandbag_Barricade_FIA_06.et",
   DIRT_V2,
   DIRT_V3,
   DIRT_SHORT,
   "{C86856FFC84B573A}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtPile_01_large.et",
   CAMO_BAGS_EAST,
   LOGS_02,
   TIRES_US,
   TIRES_EAST,
   TIRE_PILE,
   NET_SMALL_US,
   NET_SMALL_TOP_US,
   "{E05FC6BCECA8F071}PrefabsEditable/Auto/Structures/Military/CamoNets/Soviet/E_CamoNet_Small_Soviet.et",
   "{7E90E63803680AD9}PrefabsEditable/Auto/Structures/Military/CamoNets/FIA/E_CamoNet_Small_FIA_01_V1.et"
  };
  s_aEmplacements = {};
  s_aKeep = {};
  s_aGuns = {};
  s_aEyes = {};
  s_aAngles = {};
  s_aNames = {};
  s_aWalls = {};
 }
 protected static ACR_DefenderPost s_Post;
 protected static CameraBase s_Camera;
 protected static int s_iNext = -1;
 protected static int s_iAt;
 protected static bool s_bDone;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 6) Yard();
  if (second == 8) Measure();
  if (second == 9) Shapes();
  if (second == 10) Dressings();
  if (second == 11) Crew();
  if (second == 12) Shots();
  if (second == 60) Crewed();
  if (second == 120) Crewed();
  if (second >= 14 && s_Camera) Step(second);
 }

 protected static void Yard()
 {
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  TimeAndWeatherManagerEntity time;
  if (world) time = world.GetTimeAndWeatherManager();
  if (time) time.SetHoursMinutesSeconds(11, 0, 0, true);
  // Open flat ground: rings round the first base.
  ACR_Base base;
  foreach (ACR_Base each : ACR_Base.s_All)
  {
   if (!each || each.m_bRetired) continue;
   base = each;
   break;
  }
  if (!base)
  {
   ACR_TestRegress.Abort("parts", "no base");
   return;
  }
  array<IEntity> none = {};
  for (float radius = 150; radius < 900 && s_vYard == vector.Zero; radius += 50)
  {
   for (int b = 0; b < 12; b++)
   {
    vector at = base.GetOrigin() + ACR_BaseSurvey.Dir(b * 30) * radius;
    at[1] = ACR_BaseLiner.Ground(at);
    string why;
    if (!ACR_FobBuild.FootprintClear(at, 35, none, why, true, 2.5, 3)) continue;
    s_vYard = at;
    break;
   }
  }
  if (s_vYard == vector.Zero) s_vYard = base.GetOrigin() + Vector(0, 0, 200);
  PrintFormat("[ACR] PARTS yard at %1", s_vYard);
  ACR_FobBuild.FellTrees(s_vYard, 90);
 }

 // Each piece in a row, printed with its size.
 protected static void Measure()
 {
  foreach (int i, ResourceName prefab : s_aCatalog)
  {
   int row = i / 8;
   int col = i - row * 8;
   vector at = s_vYard + Vector(-28 + col * 8, 0, 66 - row * 9);
   at[1] = ACR_BaseLiner.Ground(at);
   IEntity entity = ACR_BaseDefences.SpawnProp(prefab, at, 0);
   if (!entity)
   {
    PrintFormat("PART|%1|did not spawn", prefab);
    continue;
   }
   s_aWalls.Insert(entity);
   vector root[4];
   entity.GetWorldTransform(root);
   float x0, x1, z0, z1;
   ACR_RoadblockLane.Footprint(entity, root, x0, x1, z0, z1);
   vector mins, maxs;
   entity.GetBounds(mins, maxs);
   PrintFormat("PART|%1|x %2 .. %3|z %4 .. %5|h %6", prefab, x0.ToString(-1, 2), x1.ToString(-1, 2), z0.ToString(-1, 2), z1.ToString(-1, 2), (maxs[1] - mins[1]).ToString(-1, 2));
  }
  AddShot(s_vYard + Vector(0, 40, 50), Vector(0, -80, 0), "parts_catalog_above");
  AddShot(s_vYard + Vector(0, 7, 28), Vector(0, -15, 0), "parts_catalog_low");
 }

 // Every shape, both sides, with a bit of long-piece line either side so it reads as it would in a town.
 protected static void Shapes()
 {
  array<string> sides = {"US", "USSR"};
  foreach (int s, string side : sides)
  {
   for (int shape = 0; shape < 5; shape++)
   {
    vector at = s_vYard + Vector(-52 + shape * 26, 0, -6 - s * 40);
    at[1] = ACR_BaseLiner.Ground(at);
    ACR_Emplacement e = ShapeAt(shape, side, at, s_aWalls);
    s_aKeep.Insert(e);
    s_aEmplacements.Insert(e);
    array<IEntity> built = {};
    int made = e.Build(built);
    foreach (IEntity wall : built)
    {
     s_aWalls.Insert(wall);
     IEntity turret = ACR_Roadblocks.FindTurretInHierarchy(wall);
     if (turret) s_aGuns.Insert(turret);
    }
    string name = string.Format("parts_%1_%2_%3", side, shape, ACR_Emplacement.ShapeWord(e.m_iShape));
    PrintFormat("[ACR] PARTS %1 at %2: %3 pieces", name, at, made);
    AddShot(at + Vector(0, 9, 0.5), Vector(0, -89, 0), name + "_above");
    AddShot(at + Vector(-5, 2.6, -8), Vector(32, -8, 0), name + "_behind");
    AddShot(at + Vector(5, 3, 11), Vector(204, -10, 0), name + "_front");
   }
  }
  ACR_TestRegress.Check("parts: every emplacement has a gun", s_aGuns.Count() == s_aEmplacements.Count(), string.Format("%1 guns, %2 emplacements", s_aGuns.Count(), s_aEmplacements.Count()));
  int clear;
  string words;
  foreach (IEntity gun : s_aGuns)
  {
   string why;
   // Its own bags are what this is about: a bush in the yard isn't.
   if (ACR_Fortify.LineOfFire(gun, why) || !why.Contains("Sandbag")) clear++;
   else words += why + "; ";
  }
  ACR_TestRegress.Check("parts: every gun has a clear line of fire", clear == s_aGuns.Count(), string.Format("%1 of %2: %3", clear, s_aGuns.Count(), words));
 }

 // What a line could grow into, each on a short stretch of long pieces: a look, and whether a man behind it can
 // still fire over it.
 protected static void Dressings()
 {
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit("US");
  array<string> words = {"dirt1", "dirt2", "dirt3", "dirt_back", "camo", "mixed", "gun_net", "gun_nettop", "logs1", "logs2", "tires", "tirepile", "hedgehogs"};
  foreach (int d, string word : words)
  {
   int row = d / 5;
   int col = d - row * 5;
   vector at = s_vYard + Vector(-52 + col * 26, 0, -86 - row * 26);
   at[1] = ACR_BaseLiner.Ground(at);
   Dress(word, at, kit);
   vector eye = at + Vector(0.4, 0, -0.45);
   eye[1] = ACR_BaseLiner.Ground(eye) + ACR_BaseSurvey.UP_BAGS;
   PrintFormat("[ACR] PARTS dress %1: a man behind it sees %2 m", word, (int)ACR_BaseSurvey.Reach(eye, 0, 30));
   AddShot(at + Vector(-4, 2.6, -7), Vector(26, -10, 0), "parts_dress_" + word + "_behind");
   AddShot(at + Vector(4, 2.6, 9), Vector(204, -10, 0), "parts_dress_" + word + "_front");
  }
 }

 protected static void DressPiece(ResourceName prefab, vector at, float yaw, bool onGround = true)
 {
  at[1] = ACR_BaseLiner.Ground(at);
  IEntity piece;
  if (onGround) piece = ACR_BaseDefences.SpawnOnGround(prefab, at, yaw);
  else piece = ACR_BaseDefences.SpawnProp(prefab, at, yaw);
  if (piece) s_aWalls.Insert(piece);
 }

 protected static void Dress(string word, vector at, ACR_BaseDefenceKit kit)
 {
  float step = ACR_BaseLines.LONG_STEP;
  bool bags = word != "logs1" && word != "logs2" && word != "tires" && word != "tirepile" && word != "camo" && word != "mixed";
  if (bags)
  {
   for (int k = -2; k < 2; k++) DressPiece(kit.m_sLong, at + Vector(step * (k + 0.5), 0, 0), 0);
  }
  switch (word)
  {
   case "dirt1":
    DressPiece(DIRT_V1, at + Vector(0, 0, 1.8), 0, false);
    break;
   case "dirt2":
    DressPiece(DIRT_V2, at + Vector(0, 0, 1.8), 0, false);
    break;
   case "dirt3":
    DressPiece(DIRT_V3, at + Vector(-3, 0, 1.8), 0, false);
    DressPiece(DIRT_SHORT, at + Vector(3, 0, 1.8), 0, false);
    break;
   case "dirt_back":
    DressPiece(DIRT_V1, at + Vector(0, 0, -2.2), 180, false);
    break;
   case "camo":
    DressPiece(kit.m_sLong, at + Vector(step * -1.5, 0, 0), 0);
    DressPiece(CAMO_BAGS_US, at + Vector(step * -0.5, 0, 0), 0, false);
    DressPiece(CAMO_BAGS_US, at + Vector(step * 0.5, 0, 0), 0, false);
    DressPiece(kit.m_sLong, at + Vector(step * 1.5, 0, 0), 0);
    break;
   case "mixed":
    DressPiece(kit.m_sLong, at + Vector(step * -1.5, 0, 0), 0);
    DressPiece(LOW_LONG_US, at + Vector(step * -0.5, 0, 0), 0);
    DressPiece(LOW_LONG_US, at + Vector(step * 0.5, 0, 0), 0);
    DressPiece(kit.m_sLong, at + Vector(step * 1.5, 0, 0), 0);
    break;
   case "gun_net":
   case "gun_nettop":
    ACR_Emplacement e = ACR_Emplacement.Between("US", at + Vector(-step * 0.5, 0, 0), at + Vector(step * 0.5, 0, 0), Vector(0, 0, 1));
    s_aKeep.Insert(e);
    array<IEntity> built = {};
    e.Build(built);
    foreach (IEntity wall : built) s_aWalls.Insert(wall);
    if (word == "gun_net") DressPiece(NET_SMALL_US, at + Vector(0, 0, -1.5), 0, false);
    else DressPiece(NET_SMALL_TOP_US, at + Vector(0, 0, -1.5), 0, false);
    break;
   case "logs1":
    DressPiece(LOGS_01, at, 0, false);
    break;
   case "logs2":
    DressPiece(LOGS_02, at, 0, false);
    break;
   case "tires":
    DressPiece(TIRES_US, at + Vector(-3, 0, 0), 0, false);
    DressPiece(TIRES_EAST, at + Vector(3, 0, 0), 0, false);
    break;
   case "tirepile":
    DressPiece(TIRE_PILE, at, 0, false);
    break;
   case "hedgehogs":
    for (int h = -2; h <= 2; h++) DressPiece(HEDGEHOG, at + Vector(h * 3.2, 0, 8 + Math.AbsInt(h) * 0.75), h * 25, false);
    break;
  }
 }

 // A stretch of line either side of the gap each shape stands in, so it reads as it would in a town.
 protected static ACR_Emplacement ShapeAt(int which, string side, vector at, notnull array<IEntity> walls)
 {
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(side);
  float step = ACR_BaseLines.LONG_STEP;
  if (which == 4) return ACR_Emplacement.Ring(side, at, Vector(0, 0, 1));
  // Pieces cut: 1 a notch, 2 a shallow bastion, 3 a deep one; 3 again across a bend of 30 degrees.
  int cut = which + 1;
  if (which == 3) cut = 2;
  float half = cut * step * 0.5;
  vector left = at + Vector(-half, 0, 0);
  vector right = at + Vector(half, 0, 0);
  vector facing = Vector(0, 0, 1);
  float bendLeft = 0;
  if (which == 3)
  {
   // The left run comes in at 30 degrees.
   left = at + Vector(-half * 0.87, 0, -half * 0.5);
   facing = Vector(-0.26, 0, 0.97);
   bendLeft = -30;
  }
  for (int k = 0; k < 2; k++)
  {
   vector rightAt = right + Vector(step * (k + 0.5), 0, 0);
   rightAt[1] = ACR_BaseLiner.Ground(rightAt);
   IEntity r = ACR_BaseDefences.SpawnOnGround(kit.m_sLong, rightAt, 0);
   if (r) walls.Insert(r);
   float rad = bendLeft * Math.DEG2RAD;
   vector goes = Vector(-Math.Cos(rad), 0, Math.Sin(rad));
   vector leftAt = left + goes * (step * (k + 0.5));
   leftAt[1] = ACR_BaseLiner.Ground(leftAt);
   IEntity l = ACR_BaseDefences.SpawnOnGround(kit.m_sLong, leftAt, bendLeft);
   if (l) walls.Insert(l);
  }
  return ACR_Emplacement.Between(side, left, right, facing);
 }

 protected static void Crew()
 {
  s_Post = ACR_Defenders.Add(ACR_FactionSelection.West(), s_vYard + Vector(0, 0, -16), "parts test", 40);
  if (!s_Post) return;
  s_Post.m_iReserve = s_aGuns.Count() + 2;
  ACR_Defenders.CrewGuns(s_Post, s_aGuns);
 }

 protected static bool s_bCrewChecked;

 protected static void Crewed()
 {
  int seated;
  foreach (IEntity gun : s_aGuns) if (ACR_Roadblocks.IsTurretOccupied(gun)) seated++;
  PrintFormat("[ACR] PARTS crewed %1 of %2 guns", seated, s_aGuns.Count());
  if (s_bCrewChecked) ACR_TestRegress.Check("parts: AI crews a gun standing on its own, given time", seated * 2 >= s_aGuns.Count(), string.Format("%1 of %2", seated, s_aGuns.Count()));
  else ACR_TestRegress.Check("parts: AI crews a gun standing on its own", seated > 0, string.Format("%1 of %2", seated, s_aGuns.Count()));
  s_bCrewChecked = true;
 }

 protected static void AddShot(vector eye, vector angles, string name)
 {
  float ground = GetGame().GetWorld().GetSurfaceY(eye[0], eye[2]);
  if (eye[1] < ground + 1.5) eye[1] = ground + 1.5;
  s_aEyes.Insert(eye);
  s_aAngles.Insert(angles);
  s_aNames.Insert(name);
 }

 protected static void Shots()
 {
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  Math3D.MatrixIdentity4(params.Transform);
  params.Transform[3] = s_aEyes[0];
  Resource prefab = Resource.Load("{E1FF38EC8894C5F3}Prefabs/Editor/Camera/ManualCameraSpectate.et");
  if (prefab && prefab.IsValid()) s_Camera = CameraBase.Cast(GetGame().SpawnEntityPrefab(prefab, GetGame().GetWorld(), params));
  if (!s_Camera) s_Camera = CameraBase.Cast(GetGame().SpawnEntity(CameraBase, GetGame().GetWorld(), params));
  if (!s_Camera) return;
  SCR_ManualCamera manual = SCR_ManualCamera.Cast(s_Camera);
  if (manual)
  {
   manual.SetInputEnabled(false);
   manual.ClearEventMask(EntityEvent.POSTFRAME);
  }
  s_Camera.SetVerticalFOV(70);
  s_Camera.SetNearPlane(0.3);
  s_Camera.SetFarPlane(3000);
  GetGame().GetCameraManager().SetCamera(s_Camera);
  s_iNext = 0;
  Aim(0);
 }

 protected static void Aim(int index)
 {
  s_Camera.SetOrigin(s_aEyes[index]);
  s_Camera.SetYawPitchRoll(s_aAngles[index]);
  s_Camera.Update();
  GetGame().GetCameraManager().SetCamera(s_Camera);
 }

 protected static void Step(int second)
 {
  MenuManager menus = GetGame().GetMenuManager();
  if (menus) menus.CloseAllMenus();
  int step = (second - 14) / STEP_S;
  if (step == s_iAt) return;
  s_iAt = step;
  if (s_iNext >= s_aNames.Count())
  {
   if (!s_bDone && second > 122)
   {
    s_bDone = true;
    ACR_TestRegress.Complete("parts", 4);
   }
   return;
  }
  FileIO.MakeDirectory("$profile:acr-shots");
  System.MakeScreenshot("$profile:acr-shots/" + s_aNames[s_iNext]);
  s_iNext++;
  if (s_iNext < s_aNames.Count()) Aim(s_iNext);
 }
}
