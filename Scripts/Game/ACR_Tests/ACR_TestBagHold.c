// What a man does with the bags in front of him, by the second. Shared by the tests that put men behind a line:
// how far he is from a place, whether bags stand between him and the nearest enemy, his stance, the rounds he
// fires, and whether a shot from his muzzle at the enemy clears the bags.
class ACR_BagSampler : Managed
{
 static const float ON_POST = 2.0;     // m from a place: he's on it
 static const float BEHIND_REACH = 3;  // m the bags can be away and still cover him
 int m_iSamples, m_iOnPost, m_iBehind, m_iStand, m_iCrouch, m_iProne;
 int m_iSees, m_iBlindBags;
 float m_fGap, m_fFarthest;
 int m_iRounds;
 int m_iFireSamples, m_iFireClear, m_iFireBags, m_iFireOther;
 int m_iMuzzleSamples;
 float m_fMuzzleHeight, m_fMuzzleOut;
 int m_iAxisPrints;
 int m_iShots, m_iShotsClear, m_iShotsBags, m_iShotsOther, m_iShotsLow;
 ref map<IEntity, int> m_Ammo = new map<IEntity, int>();
 ref array<IEntity> m_aMen = {};
 ref map<IEntity, int> m_ShotAmmo = new map<IEntity, int>();
 ref map<IEntity, string> m_LastState = new map<IEntity, string>();
 ref array<IEntity> m_aDeaths = {};

 // Men hit since the last look, and what they were doing a second before.
 void Deaths(int second)
 {
  foreach (IEntity body : m_aMen)
  {
   if (!body || ACR_Respawn.IsAlive(body) || m_aDeaths.Contains(body)) continue;
   m_aDeaths.Insert(body);
   PrintFormat("[ACR] BAGMAN hit at t=%1: %2", second, m_LastState.Get(body));
  }
 }

 // Ten times a second: whoever's magazine went down fired just now. Where his barrel points, for the first few
 // metres (muzzle effects don't run on a server, and a look a second later finds him ducked).
 void ListenForShots()
 {
  GetGame().GetCallqueue().CallLater(HearShots, 100, true);
 }

 protected void HearShots()
 {
  foreach (IEntity body : m_aMen)
  {
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
   if (!combat || !combat.GetCurrentWeapon() || !combat.GetCurrentWeapon().GetCurrentMagazine()) continue;
   int ammo = combat.GetCurrentWeapon().GetCurrentMagazine().GetAmmoCount();
   int was = ammo;
   if (m_ShotAmmo.Contains(body)) was = m_ShotAmmo.Get(body);
   m_ShotAmmo.Set(body, ammo);
   if (ammo >= was) continue;
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
   vector mat[4];
   if (!weapons || !weapons.GetCurrentMuzzleTransform(mat)) continue;
   m_iShots++;
   TraceParam trace = new TraceParam();
   trace.Start = mat[3];
   trace.End = mat[3] + mat[2] * 6;
   trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
   float fraction = GetGame().GetWorld().TraceMove(trace, ACR_Garrison.SolidFilter);
   if (fraction >= 1) m_iShotsClear++;
   else if (IsBag(trace.TraceEnt)) m_iShotsBags++;
   else m_iShotsOther++;
  }
 }

 void StopListening()
 {
  GetGame().GetCallqueue().Remove(HearShots);
 }

 // One line per man: what he's up to (a stuck reserve, a man off his place).
 static void Dump(string tag, SCR_AIGroup group, notnull array<vector> places)
 {
  if (!group) return;
  SCR_AIGroupUtilityComponent groupUtility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  string activity = "none";
  if (groupUtility && groupUtility.GetCurrentAction()) activity = groupUtility.GetCurrentAction().ClassName();
  AIWaypoint waypoint = group.GetCurrentWaypoint();
  string where = "no waypoint";
  if (waypoint) where = string.Format("waypoint %1 radius %2", waypoint.GetOrigin(), waypoint.GetCompletionRadius());
  PrintFormat("[ACR] BAGMAN %1 group: %2, %3", tag, activity, where);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   float gap = 9999;
   foreach (vector place : places) gap = Math.Min(gap, vector.DistanceXZ(place, body.GetOrigin()));
   string behaviour = "none";
   string cover = "";
   if (utility)
   {
    if (utility.GetCurrentAction()) behaviour = utility.GetCurrentAction().ClassName();
    if (utility.m_CombatMoveState) cover = string.Format("in cover %1 exposed %2 moving %3", utility.m_CombatMoveState.m_bInCover, utility.m_CombatMoveState.m_bExposedInCover, utility.m_CombatMoveState.IsMoving());
    if (utility.m_ACRBagPlace) cover += string.Format(", own place %1 m", vector.DistanceXZ(utility.m_ACRBagPlace.m_vPos, body.GetOrigin()).ToString(-1, 1));
   }
   PrintFormat("[ACR] BAGMAN %1 %2 m, %3, stance %4, %5", tag, gap.ToString(-1, 1), behaviour, typename.EnumToString(ECharacterStance, body.GetCharacterController().GetStance()), cover);
  }
 }

 // 'facing' is the bags' firing side, for how far past his feet the muzzle reaches; zero to skip it.
 void Sample(IEntity body, IEntity enemy, notnull array<vector> places, vector facing)
 {
  if (!body) return;
  m_iSamples++;
  if (!m_aMen.Contains(body)) m_aMen.Insert(body);
  vector at = body.GetOrigin();
  // What he was doing, for the line that goes out when he's hit (Deaths).
  string state = "";
  float away = -1;
  if (enemy) away = vector.DistanceXZ(enemy.GetOrigin(), at);
  ChimeraCharacter who = ChimeraCharacter.Cast(body);
  if (who && who.GetCharacterController()) state = typename.EnumToString(ECharacterStance, who.GetCharacterController().GetStance());
  m_LastState.Set(body, string.Format("%1, nearest enemy %2 m", state, (int)away));
  float gap = 9999;
  foreach (vector place : places) gap = Math.Min(gap, vector.DistanceXZ(place, at));
  if (places.IsEmpty()) gap = 0;
  m_fGap += gap;
  m_fFarthest = Math.Max(m_fFarthest, gap);
  if (gap < ON_POST) m_iOnPost++;
  vector aim = at + facing * 50;
  if (enemy) aim = Target(enemy);
  if (BagsBetween(at, aim)) m_iBehind++;
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (character && character.GetCharacterController())
  {
   ECharacterStance stance = character.GetCharacterController().GetStance();
   if (stance == ECharacterStance.STAND) m_iStand++;
   else if (stance == ECharacterStance.CROUCH) m_iCrouch++;
   else m_iProne++;
  }
  if (character && enemy)
  {
   TraceParam look = new TraceParam();
   look.Start = character.EyePosition();
   look.End = aim;
   look.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
   float view = GetGame().GetWorld().TraceMove(look, ACR_Garrison.SolidFilter);
   if (view >= 1) m_iSees++;
   else if (IsBag(look.TraceEnt)) m_iBlindBags++;
  }
  int fired = Fired(body);
  if (fired <= 0 || !enemy) return;
  m_iRounds += fired;
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
  vector mat[4];
  if (!weapons || !weapons.GetCurrentMuzzleTransform(mat)) return;
  vector muzzle = mat[3];
  m_iMuzzleSamples++;
  m_fMuzzleHeight += muzzle[1] - at[1];
  m_fMuzzleOut += vector.Dot(muzzle - at, facing);
  m_iFireSamples++;
  if (m_iAxisPrints < 3)
  {
   m_iAxisPrints++;
   vector toward = (aim - muzzle).Normalized();
   PrintFormat("[ACR] BAGHOLD muzzle at %1 over his feet, axes on the enemy x %2 y %3 z %4, height %5, %6 past his feet", at, vector.Dot(mat[0], toward).ToString(-1, 2), vector.Dot(mat[1], toward).ToString(-1, 2), vector.Dot(mat[2], toward).ToString(-1, 2), (muzzle[1] - at[1]).ToString(-1, 2), vector.Dot(muzzle - at, facing).ToString(-1, 2));
  }
  TraceParam trace = new TraceParam();
  trace.Start = muzzle;
  trace.End = aim;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, ACR_Garrison.SolidFilter);
  if (fraction >= 1) m_iFireClear++;
  else if (IsBag(trace.TraceEnt)) m_iFireBags++;
  else m_iFireOther++;
 }

 static IEntity Nearest(notnull array<IEntity> bodies, vector at)
 {
  IEntity best;
  float nearest = float.MAX;
  foreach (IEntity body : bodies)
  {
   if (!body) continue;
   float d = vector.DistanceXZ(body.GetOrigin(), at);
   if (d < nearest)
   {
    nearest = d;
    best = body;
   }
  }
  return best;
 }

 // The living bodies of a group, added to 'bodies'.
 static void Alive(AIGroup group, notnull array<IEntity> bodies)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body)) bodies.Insert(body);
  }
 }

 static vector Target(IEntity enemy)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(enemy);
  if (character) return character.EyePosition() - Vector(0, 0.2, 0);
  return enemy.GetOrigin() + Vector(0, 1, 0);
 }

 // Bags on the way from his chest to the enemy, close by.
 static bool BagsBetween(vector at, vector enemy)
 {
  vector from = at + Vector(0, 0.5, 0);
  vector dir = enemy - from;
  dir[1] = 0;
  if (dir.Length() < 0.1) return false;
  dir.Normalize();
  TraceParam trace = new TraceParam();
  trace.Start = from;
  trace.End = from + dir * BEHIND_REACH;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, ACR_Garrison.SolidFilter);
  return fraction < 1 && IsBag(trace.TraceEnt);
 }

 static bool IsBag(IEntity hit)
 {
  while (hit)
  {
   if (hit.GetPrefabData() && hit.GetPrefabData().GetPrefabName().Contains("Sandbag")) return true;
   hit = hit.GetParent();
  }
  return false;
 }

 // Rounds gone from his magazine since the last look (a new magazine counts as none).
 int Fired(IEntity body)
 {
  SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
  if (!combat || !combat.GetCurrentWeapon() || !combat.GetCurrentWeapon().GetCurrentMagazine()) return 0;
  int ammo = combat.GetCurrentWeapon().GetCurrentMagazine().GetAmmoCount();
  int fired;
  if (m_Ammo.Contains(body) && ammo < m_Ammo.Get(body)) fired = m_Ammo.Get(body) - ammo;
  m_Ammo.Set(body, ammo);
  return fired;
 }

 protected static int Percent(int part, int whole)
 {
  if (whole <= 0) return 0;
  return Math.Round(100.0 * part / whole);
 }

 string Words()
 {
  float gap, height, past;
  if (m_iSamples > 0) gap = m_fGap / m_iSamples;
  if (m_iMuzzleSamples > 0)
  {
   height = m_fMuzzleHeight / m_iMuzzleSamples;
   past = m_fMuzzleOut / m_iMuzzleSamples;
  }
  string where = string.Format("on post %1%%, behind bags %2%%, mean gap %3 m, farthest %4 m", Percent(m_iOnPost, m_iSamples), Percent(m_iBehind, m_iSamples), gap.ToString(-1, 1), m_fFarthest.ToString(-1, 1));
  string stance = string.Format("stand %1%%, crouch %2%%, prone %3%%, sees the enemy %4%%, blind behind bags %5%%", Percent(m_iStand, m_iSamples), Percent(m_iCrouch, m_iSamples), Percent(m_iProne, m_iSamples), Percent(m_iSees, m_iSamples), Percent(m_iBlindBags, m_iSamples));
  string fire = string.Format("rounds %1, shots heard %2: clear %3, into bags %4, other %5; muzzle height %6, %7 m past his feet", m_iRounds, m_iShots, m_iShotsClear, m_iShotsBags, m_iShotsOther, height.ToString(-1, 2), past.ToString(-1, 2));
  return string.Format("%1, %2, %3 (%4 samples)", where, stance, fire, m_iSamples);
 }
}

// srv:baghold (about 4 min, Everon): do men with a place behind bags stay there and shoot from there? Open ground,
// a real line of long pieces, eight men on places behind it, one USSR rifle squad walking in from 150 m, damage on.
// It reports; AI fights differ by the run, so compare several. srv:bagholdlate has the same men start 60 m behind
// the line with the attackers already coming. "[ACR] BAGHOLD" lines.
class ACR_TestBagHold
{
 static const ResourceName ENEMY = "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
 static const string ATTACK = "Bag Attack";
 static const int PIECES = 10;
 static const int MEN = 8;
 static const float LANE = 150;
 static const float LATE_BEHIND = 60;
 static const int LENGTH = 240;
 static const int ON_PLACES_LEAST = 80;
 static const int BEHIND_LEAST = 60;
 static const int INTO_BAGS_MOST = 15;
 protected static vector s_vAt, s_vFacing;
 protected static ref array<IEntity> s_aBags = {};
 protected static ref array<vector> s_aPlaces = {};
 protected static SCR_AIGroup s_Group;
 protected static ref ACR_BagSampler s_Sampler = new ACR_BagSampler();
 protected static int s_iDefenders, s_iAttackers, s_iContact = -1;
 protected static int s_iOrdered = -1;
 protected static bool s_bDone;

 static bool Late() { return System.IsCLIParam("acrBagHoldLateTest"); }

 // srv:bagholdlow: the same with the low pieces.
 static ResourceName Prefab()
 {
  if (System.IsCLIParam("acrBagHoldLowTest")) return ACR_BaseDefences.LONG_LOW_PLASTIC;
  return ACR_BaseDefences.LONG_PLASTIC;
 }

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 1 && System.IsCLIParam("acrBagHoldOffTest")) ACR_Options.Set("acrBagHold", "0");
  if (second == 6) Site();
  if (second == 8) Line();
  // Late: the attackers are off before the men are told, and the men start far behind the line.
  int menAt = 14;
  int attackAt = 44;
  if (Late())
  {
   menAt = 50;
   attackAt = 30;
  }
  if (second == menAt) Men(manager);
  if (second == attackAt) SendAttackers(manager);
  if (second == attackAt + 4) Orders(manager, second);
  if (s_bDone || s_iOrdered < 0 || !s_Group) return;
  ACR_Squad.KeepSimulated(s_Group);
  ACR_Squad.FightLikeSquad(s_Group);
  Sample(manager, second);
  s_Sampler.Deaths(second);
  if (s_iContact < 0)
  {
   if (second - s_iOrdered > LENGTH) Finish(manager, second);
   return;
  }
  int fought = second - s_iContact;
  if (fought % 15 == 0) Report(manager, fought);
  if (fought % 30 == 0) ACR_BagSampler.Dump(string.Format("t=%1", fought), s_Group, s_aPlaces);
  if (fought >= LENGTH || (fought > 5 && (Defenders(manager) == 0 || AttackersAlive(manager) == 0))) Finish(manager, second);
 }

 // Flat open ground with a clear view down a lane of LANE metres: rings round the first base.
 protected static void Site()
 {
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  TimeAndWeatherManagerEntity time;
  if (world) time = world.GetTimeAndWeatherManager();
  if (time) time.SetHoursMinutesSeconds(11, 0, 0, true);
  ACR_Base base;
  foreach (ACR_Base each : ACR_Base.s_All)
  {
   if (!each || each.m_bRetired) continue;
   base = each;
   break;
  }
  if (!base)
  {
   ACR_TestRegress.Abort("baghold", "no base");
   return;
  }
  array<IEntity> none = {};
  for (float radius = 200; radius < 1500 && s_vAt == vector.Zero; radius += 50)
  {
   for (int b = 0; b < 12 && s_vAt == vector.Zero; b++)
   {
    vector at = base.GetOrigin() + ACR_BaseSurvey.Dir(b * 30) * radius;
    at[1] = ACR_BaseLiner.Ground(at);
    string why;
    if (!ACR_FobBuild.FootprintClear(at, 25, none, why, true, 2.5, 3)) continue;
    for (int f = 0; f < 8; f++)
    {
     vector facing = ACR_BaseSurvey.Dir(f * 45);
     if (!LaneClear(at, facing)) continue;
     s_vAt = at;
     s_vFacing = facing;
     break;
    }
   }
  }
  if (s_vAt == vector.Zero)
  {
   ACR_TestRegress.Abort("baghold", "no open ground");
   return;
  }
  ACR_FobBuild.FellTrees(s_vAt, 40);
  PrintFormat("[ACR] BAGHOLD site %1, facing %2", s_vAt, s_vFacing);
 }

 protected static bool LaneClear(vector at, vector facing)
 {
  vector side = Vector(facing[2], 0, -facing[0]);
  for (int i = -1; i <= 1; i++)
  {
   vector from = at + side * (i * 15.0);
   vector to = from + facing * LANE;
   from[1] = ACR_BaseLiner.Ground(from) + 1.7;
   to[1] = ACR_BaseLiner.Ground(to) + 1.0;
   if (!ACR_Garrison.ClearLine(from, to)) return false;
  }
  return true;
 }

 protected static void Line()
 {
  if (s_vAt == vector.Zero) return;
  vector side = Vector(s_vFacing[2], 0, -s_vFacing[0]);
  float yaw = s_vFacing.VectorToAngles()[0];
  for (int i = 0; i < PIECES; i++)
  {
   vector at = s_vAt + side * ((i + 0.5 - PIECES * 0.5) * ACR_BaseDefences.LONG_LEN);
   at[1] = ACR_BaseLiner.Ground(at);
   IEntity piece = ACR_BaseDefences.SpawnOnGround(Prefab(), at, yaw);
   if (!piece) continue;
   s_aBags.Insert(piece);
   ACR_Navmesh.RebuildAfterSpawn(piece);
  }
  // How high the bags really are, down traces on the middle piece.
  if (s_aBags.IsEmpty()) return;
  IEntity middle = s_aBags[PIECES / 2];
  vector mins, maxs;
  middle.GetBounds(mins, maxs);
  string tops = "";
  for (float back = 0.9; back >= -0.9; back -= 0.3)
  {
   vector look = middle.GetOrigin() - s_vFacing * back;
   TraceParam down = new TraceParam();
   down.Start = look + Vector(0, 3, 0);
   down.End = look - Vector(0, 1, 0);
   down.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
   float hit = GetGame().GetWorld().TraceMove(down, ACR_Garrison.SolidFilter);
   float top = down.Start[1] - 4 * hit - ACR_BaseLiner.Ground(look);
   tops += string.Format(" %1:%2", back.ToString(-1, 1), top.ToString(-1, 2));
  }
  PrintFormat("[ACR] BAGHOLD %1 pieces, bounds height %2, depth %3; top over the ground by metres behind the middle:%4", s_aBags.Count(), (maxs[1] - mins[1]).ToString(-1, 2), (maxs[2] - mins[2]).ToString(-1, 2), tops);
 }

 protected static void Men(ACR_Manager manager)
 {
  if (s_aBags.IsEmpty()) return;
  string side = ACR_FactionSelection.West();
  float behind = 15;
  if (Late()) behind = LATE_BEHIND;
  vector from = ACR_Manager.Ground(s_vAt - s_vFacing * behind);
  s_Group = manager.m_Bases.SpawnGuards(side, from, MEN, "Bag men", 20);
  if (!s_Group)
  {
   ACR_TestRegress.Abort("baghold", "no men");
   return;
  }
  s_iDefenders = MEN;
  s_Sampler.ListenForShots();
  array<ref ACR_GarrisonSpot> spots = {};
  // From the middle of the line out, as ManLine does from the enemy's side.
  for (int n = 0; n < MEN; n++)
  {
   int index = PIECES / 2;
   if (n % 2 == 0) index += n / 2;
   else index -= (n + 1) / 2;
   IEntity piece = s_aBags[Math.ClampInt(index, 0, s_aBags.Count() - 1)];
   ACR_GarrisonSpot spot = ACR_BagHold.SpotBehind(piece.GetOrigin(), s_vFacing, Prefab(), piece);
   spots.Insert(spot);
   s_aPlaces.Insert(spot.m_Pos);
  }
  vector center = spots[0].m_Pos;
  float radius;
  if (ACR_Garrison.PostsAt(s_Group, spots, center, radius) <= 0)
  {
   ACR_TestRegress.Abort("baghold", "no posts");
   return;
  }
  ACR_Guerrillas.Order(s_Group, ACR_Squad.WP_GARRISON, center, radius, false);
  PrintFormat("[ACR] BAGHOLD %1 men told off to %2 places, radius %3", MEN, spots.Count(), radius);
 }

 protected static void SendAttackers(ACR_Manager manager)
 {
  if (s_vAt == vector.Zero) return;
  vector from = ACR_Manager.Ground(s_vAt + s_vFacing * LANE);
  manager.m_sSpawningFaction = ACR_FactionSelection.East();
  manager.SpawnUnit(ATTACK, ENEMY, "", false, "", from, from);
  manager.m_sSpawningFaction = "";
 }

 protected static void Orders(ACR_Manager manager, int second)
 {
  ACR_Squad squad = Attack(manager);
  if (!squad)
  {
   ACR_TestRegress.Abort("baghold", "no attackers");
   return;
  }
  squad.MoveTo(ACR_Manager.Ground(s_vAt - s_vFacing * 40));
  s_iAttackers = squad.CountAlive();
  s_iOrdered = second;
  PrintFormat("[ACR] BAGHOLD %1 attackers from %2 m", s_iAttackers, LANE);
 }

 protected static ACR_Squad Attack(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == ATTACK) return squad;
  return null;
 }

 protected static int AttackersAlive(ACR_Manager manager)
 {
  ACR_Squad squad = Attack(manager);
  if (!squad) return 0;
  return squad.CountAlive();
 }

 protected static int Defenders(ACR_Manager manager)
 {
  return manager.m_Bases.GuardsAlive(s_Group);
 }

 protected static IEntity NearestEnemy(ACR_Manager manager, vector at)
 {
  ACR_Squad squad = Attack(manager);
  if (!squad) return null;
  array<IEntity> bodies = {};
  ACR_BagSampler.Alive(squad.m_Group, bodies);
  return ACR_BagSampler.Nearest(bodies, at);
 }

 protected static void Sample(ACR_Manager manager, int second)
 {
  array<AIAgent> agents = {};
  s_Group.GetAgents(agents);
  int rounds = s_Sampler.m_iRounds;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   // Before the first shot only the magazines are read: what counts is what they do in the fight.
   if (!s_Sampler.m_aMen.Contains(body)) s_Sampler.m_aMen.Insert(body);
   if (s_iContact < 0)
   {
    int fired = s_Sampler.Fired(body);
    if (fired > 0) s_iContact = second;
    continue;
   }
   s_Sampler.Sample(body, NearestEnemy(manager, body.GetOrigin()), s_aPlaces, s_vFacing);
  }
  if (s_iContact < 0 && (Defenders(manager) < s_iDefenders || AttackersAlive(manager) < s_iAttackers)) s_iContact = second;
  if (s_iContact == second)
  {
   IEntity first = NearestEnemy(manager, s_vAt);
   float distance = -1;
   if (first) distance = vector.DistanceXZ(first.GetOrigin(), s_vAt);
   PrintFormat("[ACR] BAGHOLD contact at t=%1, nearest attacker %2 m, on places %3 of %4", second, (int)distance, OnPlaces(agents), agents.Count());
  }
 }

 protected static int OnPlaces(array<AIAgent> agents)
 {
  int on;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   foreach (vector place : s_aPlaces)
   {
    if (vector.DistanceXZ(place, body.GetOrigin()) < ACR_BagSampler.ON_POST)
    {
     on++;
     break;
    }
   }
  }
  return on;
 }

 protected static void Report(ACR_Manager manager, int fought)
 {
  IEntity first = NearestEnemy(manager, s_vAt);
  float distance = -1;
  if (first) distance = vector.DistanceXZ(first.GetOrigin(), s_vAt);
  PrintFormat("[ACR] BAGHOLD t=%1: attackers %2 (nearest %3 m), defenders %4; %5", fought, AttackersAlive(manager), (int)distance, Defenders(manager), s_Sampler.Words());
 }

 protected static void Finish(ACR_Manager manager, int second)
 {
  s_bDone = true;
  s_Sampler.StopListening();
  int fought = 0;
  if (s_iContact >= 0) fought = second - s_iContact;
  PrintFormat("[ACR] BAGHOLD RESULT attackers lost %1 of %2, defenders lost %3 of %4, %5 s, %6", s_iAttackers - AttackersAlive(manager), s_iAttackers, s_iDefenders - Defenders(manager), s_iDefenders, fought, s_Sampler.Words());
  PrintFormat("[ACR] BAGHOLD held: into cover %1 times, sent back %2 times, holds %3, places lost %4", ACR_BagHold.s_iEntered, ACR_BagHold.s_iSentBack, ACR_BagHold.s_iHolds, ACR_BagHold.s_iLost);
  ACR_TestRegress.Check("baghold: a fight happened", s_iContact >= 0, "");
  if (System.IsCLIParam("acrBagHoldOffTest"))
  {
   ACR_TestRegress.Complete("baghold", 1);
   return;
  }
  // Floors well under what it does (on places 97-100%, behind the bags 86-100%, into the bags 2-5%): fights differ.
  ACR_TestRegress.Check("baghold: men stay on their places", s_Sampler.m_iOnPost * 100 >= s_Sampler.m_iSamples * ON_PLACES_LEAST, s_Sampler.Words());
  ACR_TestRegress.Check("baghold: men keep the bags between them and the enemy", s_Sampler.m_iBehind * 100 >= s_Sampler.m_iSamples * BEHIND_LEAST, "");
  ACR_TestRegress.Check("baghold: shots go over the bags, not into them", s_Sampler.m_iShotsBags * 100 <= s_Sampler.m_iShots * INTO_BAGS_MOST, string.Format("%1 of %2", s_Sampler.m_iShotsBags, s_Sampler.m_iShots));
  ACR_TestRegress.Complete("baghold", 4);
 }
}
