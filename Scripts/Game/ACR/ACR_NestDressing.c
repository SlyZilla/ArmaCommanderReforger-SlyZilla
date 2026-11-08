// What a zombie nest looks like (#107, ACR_Outbreak): its site is dressed when the nest settles
// (at the start, or where a horde starts a new one), and stays dressed for the match, cleared or
// not: the remains of what happened there.
//  - Blood on the ground and walls round the site, a burnt patch or two, sometimes a fire still
//    smouldering: decals and a particle effect, which have no collision and don't replicate, so
//    every machine lays them out itself from the site (seeded from its position: the same on every
//    machine). A dedicated server draws nothing.
//  - Vehicle garbage, a pile or two on clear ground off the roads: replicated props the server spawns.
// The sites reach clients through the game mode (m_aACRNestSites, joiners too).
class ACR_NestDressing
{
 static const int MAX_SITES = 120;
 static const float SAME_SITE = 30;       // m: a site this close to a dressed one isn't dressed again
 static const float REACH = 20;           // m round the site
 protected static ref array<vector> s_aDressed;
 static void ACR_InitStatics()
 {
  s_aDressed = {};
 }
 static int s_iDecals;                    // laid on this machine (tests)
 static int s_iFires;
 static int s_iPiles;                     // spawned by the server

 protected static const ResourceName FIRE = "{C0E2E7DC28B71E2C}Particles/Enviroment/Campfire_medium_normal.ptc";

 protected static string Blood(int i)
 {
  if (i == 0) return "{F11E9EF96AC392A7}Assets/Decals/Blood/Data/BloodStain_Large_01.emat";
  if (i == 1) return "{AFF6B7BAD54E1CA8}Assets/Decals/Blood/Data/BloodStain_Large_02.emat";
  if (i == 2) return "{FE115E2F08E5B0D8}Assets/Decals/Blood/Data/BloodStain_Drops_01.emat";
  if (i == 3) return "{D49861D036BAD131}Assets/Decals/Blood/Data/BloodStain_Small_01.emat";
  return "{34C4CC186F4F2FE3}Assets/Decals/Blood/Decal_Blood3.emat";
 }

 protected static string Burnt(int i)
 {
  if (i == 0) return "{085CF0243B09A2CF}Assets/Decals/Miscellaneous/Damage/Damage_Burnt_01.emat";
  return "{6FE49D82D358226F}Assets/Decals/Miscellaneous/Damage/Damage_Burnt_02.emat";
 }

 protected static ResourceName Pile(int i)
 {
  if (i == 0) return "{A1AA6AAF5F88D859}Prefabs/Compositions/Misc/SubCompositions/Garbage/Garbage_Vehicle_01.et";
  if (i == 1) return "{CDFE5E4FE34C5777}Prefabs/Compositions/Misc/SubCompositions/Garbage/Garbage_Vehicle_02.et";
  if (i == 2) return "{286212B6EFA9C01C}Prefabs/Compositions/Misc/SubCompositions/Garbage/Garbage_Vehicle_03.et";
  return "{D5062960B7211EB4}Prefabs/Props/Construction/GarbageStack_01/GarbageStack_01_Long.et";
 }

 // Server: a nest settles at 'site'. Every machine dresses it; the server adds the garbage.
 static void Add(vector site)
 {
  if (!Replication.IsServer() || !GetGame()) return;
  // As a client gets it (decimetres), so the seeded layout is the same everywhere.
  site = Vector(Math.Round(site[0] * 10) * 0.1, site[1], Math.Round(site[2] * 10) * 0.1);
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (!mode || !mode.ACR_AddNestSite(site)) return;
  Piles(site);
  DressLocal(site);
 }

 // One or two garbage piles on clear ground off the roads.
 protected static void Piles(vector site)
 {
  RandomGenerator rng = Rng(site, 7);
  int count = 1;
  if (rng.RandFloat01() < 0.5) count = 2;
  for (int i = 0; i < count; i++)
  {
   vector around = site + Offset(rng, 6, REACH);
   vector spot;
   if (!ACR_BaseSystem.ClearGround(spot, around, 25, 3) || vector.DistanceXZ(spot, site) > REACH * 2 || ACR_Minefields.RoadClearance(spot) < 5 || BBB_Terrain.IsWaterAt(spot)) continue;
   Resource resource = Resource.Load(Pile((int)(rng.RandFloat01() * 3.99)));
   if (!resource.IsValid()) continue;
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   Math3D.AnglesToMatrix(Vector(rng.RandFloat01() * 360, 0, 0), params.Transform);
   params.Transform[3] = spot;
   if (GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params)) s_iPiles++;
  }
 }

 // This machine lays out the site's blood, burns and fire (once).
 static void DressLocal(vector site)
 {
  foreach (vector done : s_aDressed) if (vector.DistanceXZ(done, site) < 1) return;
  s_aDressed.Insert(site);
  if (System.IsConsoleApp()) return;
  RandomGenerator rng = Rng(site, 1);
  int blood = 6 + (int)(rng.RandFloat01() * 3.99);
  for (int b = 0; b < blood; b++) Decal(site + Offset(rng, 1, REACH * 0.8), Blood((int)(rng.RandFloat01() * 4.99)), 1 + rng.RandFloat01() * 2, rng.RandFloat01() * 360);
  int burns = 1;
  if (rng.RandFloat01() < 0.5) burns = 2;
  for (int n = 0; n < burns; n++) Decal(site + Offset(rng, 3, REACH * 0.7), Burnt((int)(rng.RandFloat01() * 1.99)), 5 + rng.RandFloat01() * 3, rng.RandFloat01() * 360);
  if (rng.RandFloat01() < 0.5) Fire(site + Offset(rng, 4, 12));
 }

 // A decal on whatever is under 'at' (ground, road, a floor).
 protected static void Decal(vector at, string material, float size, float angle)
 {
  World world = GetGame().GetWorld();
  float ground = world.GetSurfaceY(at[0], at[2]);
  TraceParam trace = new TraceParam();
  trace.Start = Vector(at[0], ground + 3, at[2]);
  trace.End = Vector(at[0], ground - 6, at[2]);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float hit = world.TraceMove(trace, null);
  if (hit >= 1 || !trace.TraceEnt) return;
  vector point = trace.Start + (trace.End - trace.Start) * hit;
  if (world.CreateDecal(trace.TraceEnt, point + Vector(0, 0.5, 0), Vector(0, -1, 0), 0, 1.5, angle * Math.DEG2RAD, size, 1, material, -1, 0xFFFFFFFF)) s_iDecals++;
 }

 protected static void Fire(vector at)
 {
  ParticleEffectEntitySpawnParams params = new ParticleEffectEntitySpawnParams();
  params.Transform[3] = ACR_Manager.Ground(at);
  if (ParticleEffectEntity.SpawnParticleEffect(FIRE, params)) s_iFires++;
 }

 protected static vector Offset(RandomGenerator rng, float near, float far)
 {
  float angle = rng.RandFloat01() * Math.PI2;
  float r = near + rng.RandFloat01() * (far - near);
  return Vector(Math.Cos(angle) * r, 0, Math.Sin(angle) * r);
 }

 // The same numbers on every machine for one site.
 protected static RandomGenerator Rng(vector site, int salt)
 {
  RandomGenerator rng = new RandomGenerator();
  rng.SetSeed((int)site[0] * 7919 + (int)site[2] * 104729 + salt);
  return rng;
 }
}

// The nest sites, for every machine (ACR_NestDressing): x and z in decimetres, in pairs.
modded class SCR_BaseGameMode
{
 [RplProp(onRplName: "ACR_OnNestSites")]
 protected ref array<int> m_aACRNestSites = {};

 bool ACR_AddNestSite(vector site)
 {
  int sites = m_aACRNestSites.Count() / 2;
  if (sites >= ACR_NestDressing.MAX_SITES) return false;
  for (int i = 0; i < sites; i++)
  {
   vector known = Vector(m_aACRNestSites[i * 2] * 0.1, 0, m_aACRNestSites[i * 2 + 1] * 0.1);
   if (vector.DistanceXZ(known, site) < ACR_NestDressing.SAME_SITE) return false;
  }
  m_aACRNestSites.Insert(Math.Round(site[0] * 10));
  m_aACRNestSites.Insert(Math.Round(site[2] * 10));
  Replication.BumpMe();
  return true;
 }

 protected void ACR_OnNestSites()
 {
  for (int i = 0; i + 1 < m_aACRNestSites.Count(); i += 2)
  {
   float x = m_aACRNestSites[i] * 0.1;
   float z = m_aACRNestSites[i + 1] * 0.1;
   ACR_NestDressing.DressLocal(Vector(x, GetGame().GetWorld().GetSurfaceY(x, z), z));
  }
 }
}
