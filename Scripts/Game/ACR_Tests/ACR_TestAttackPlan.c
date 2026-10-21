// -acrAttackPlanTest 1 (server-capable): the AI commander's attack plan and defence (Jacob
// 20 Oct, Everon run): attack approach lines that are measurably apart even when every squad
// comes from the same side, a base of fire that still holds an overwatch spot with a clear
// fire line after the assault starts (no enemy seen yet), fires only at a seen enemy and goes
// back to overwatch when it is no longer seen, and a defence that pushes a squad out to a
// forward line towards the attack, keeps a reserve at the point and pulls back when the enemy
// closes in. Uses its own US commander (its intel is only what the test notes) on the test
// range bases Outpost (USSR) and Farm (US); the scenario's AI commanders are off.
class ACR_TestAttackPlan
{
 static const ResourceName US_TEAM = "{3BF36BDEEB33AEC9}Prefabs/Groups/BLUFOR/Group_US_SentryTeam.et";
 static const ResourceName USSR_TEAM = "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";
 static const int EXPECTED = 10;
 static ref ACR_AICommander s_AI;
 static ref ACR_AIAttack s_Attack;
 static ACR_Base s_Target;
 static ACR_Base s_Home;
 static SCR_AIGroup s_FireGroup;
 static SCR_AIGroup s_AssaultGroup;
 static SCR_AIGroup s_DepthAGroup;
 static SCR_AIGroup s_DepthBGroup;
 static SCR_AIGroup s_Raiders;
 static SCR_AIGroup s_Watched;
 static ACR_Squad s_Fire;
 static ACR_Squad s_Assault;
 static ACR_Squad s_Forward;
 static ACR_Squad s_Reserve;
 static vector s_RaidersAt;
 static float s_ForwardStartGap;
 static float s_ForwardStartOut;
 static float s_FireStartGap = -1;

 static ACR_Base BaseNamed(string name)
 {
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName == name) return base;
  return null;
 }

 static vector Toward(vector from, vector to)
 {
  vector dir = to - from;
  dir[1] = 0;
  dir.Normalize();
  return dir;
 }

 static SCR_AIGroup Spawn(ResourceName prefab, vector at, bool passive)
 {
  PrintFormat("[ACR] ATTACKPLAN info: spawn at %1 water %2", at, BBB_Terrain.IsWaterAt(at));
  SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(prefab, ACR_Manager.Ground(at)));
  if (group && passive && group.GetGroupUtilityComponent()) group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  return group;
 }

 // Hold fire and take no damage: an enemy that is there to be seen, not to change the fight.
 static void Passive(SCR_AIGroup group)
 {
  if (!group) return;
  if (group.GetGroupUtilityComponent()) group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter man = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (man && man.GetDamageManager()) man.GetDamageManager().EnableDamageHandling(false);
  }
 }

 static void Bodies(SCR_AIGroup group, notnull array<IEntity> bodies)
 {
  bodies.Clear();
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) if (agent.GetControlledEntity()) bodies.Insert(agent.GetControlledEntity());
 }

 static vector Anchor(ACR_Squad squad)
 {
  if (squad && squad.GetAnchor()) return squad.GetAnchor().GetOrigin();
  return vector.Zero;
 }

 static void Run(ACR_TestHarness harness, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1) Setup(manager);
  if (!s_AI || !s_Target || !s_Home) return;
  if (tick == 3) Approaches();
  if (tick == 5) SpawnAll();
  if (tick == 7) Passive(s_Raiders);
  if (tick == 10) Start(manager);
  if (tick == 25 && s_Attack && s_Fire) s_FireStartGap = vector.DistanceXZ(Anchor(s_Fire), s_Attack.m_vOverwatch);
  if (tick == 40) HoldDepth();
  if (tick == 70) Hold();
  if (tick == 73) Passive(s_Watched);
  if (tick == 76) Seen();
  if (tick == 78) Forgotten();
  if (tick == 80) KillAssault(harness);
  if (tick == 84) Lifted();
 }

 static vector North() { return Toward(s_Target.GetOrigin(), s_Home.GetOrigin()); }

 static void Setup(ACR_Manager manager)
 {
  ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
  if (soviets) soviets.m_bAICommander = false;
  ACR_FactionSettings us = ACR_FactionSettings.Find("US");
  if (us) us.m_bAICommander = false;
  s_Target = BaseNamed("Outpost");
  s_Home = BaseNamed("Farm");
  if (!s_Target || !s_Home || s_Target.m_sOwner != "USSR" || s_Home.m_sOwner != "US")
  {
   ACR_TestRegress.Abort("attackplan", "test range bases Outpost (USSR) and Farm (US) not found");
   s_Target = null;
   return;
  }
  s_AI = new ACR_AICommander(manager, "US");
 }

 // 1. Approach lines: four squads all coming from the Farm side (one road) still get
 //    staging points on separate lines round the objective.
 static void Approaches()
 {
  vector center = s_Target.GetOrigin();
  vector north = North();
  ACR_AIAttack plan = new ACR_AIAttack();
  plan.m_Target = s_Target;
  plan.m_vApproach = north;
  plan.m_Staging = ACR_Manager.Ground(center + north * (s_Target.m_fRadius + ACR_AICommander.STAGING_DISTANCE));
  string text;
  for (int i = 0; i < 4; i++)
  {
   vector stage;
   int slot = s_AI.PickApproach(plan, s_Home.GetOrigin() + Vector(i * 15, 0, 0), stage);
   plan.Add(null, stage, slot);
   text += string.Format("slot %1 at %2 m bearing %3 | ", slot, Math.Round(vector.DistanceXZ(stage, center)), Math.Round(ACR_AICommander.TurnBetween(stage - center, north)));
  }
  float minTurn = 360;
  float minGap = float.MAX;
  float minRing = float.MAX;
  bool allLines = true;
  foreach (int a, vector first : plan.m_aStaging)
  {
   if (plan.m_aSlot[a] < 0) allLines = false;
   minRing = Math.Min(minRing, vector.DistanceXZ(first, center));
   for (int b = a + 1; b < plan.m_aStaging.Count(); b++)
   {
    minTurn = Math.Min(minTurn, ACR_AICommander.TurnBetween(first - center, plan.m_aStaging[b] - center));
    minGap = Math.Min(minGap, vector.DistanceXZ(first, plan.m_aStaging[b]));
   }
  }
  ACR_TestRegress.Check("attack approach lines from one side are spread at least 40 degrees and 150 m apart", allLines && minTurn >= ACR_AICommander.APPROACH_SEPARATION && minGap >= 150 && minRing >= s_Target.m_fRadius + 150, string.Format("min turn %1, min gap %2 m, min ring %3 m: %4", Math.Round(minTurn), Math.Round(minGap), Math.Round(minRing), text));
  float mainTurn = ACR_AICommander.TurnBetween(plan.m_aStaging[0] - center, north);
  ACR_TestRegress.Check("the first squad takes the main approach line facing the attackers' side", plan.m_aSlot[0] == 0 && mainTurn <= 35, string.Format("slot %1, %2 degrees off the main approach", plan.m_aSlot[0], Math.Round(mainTurn)));
 }

 static void SpawnAll()
 {
  vector center = s_Target.GetOrigin();
  vector home = s_Home.GetOrigin();
  vector north = North();
  vector east = Vector(north[2], 0, -north[0]);
  s_FireGroup = Spawn(US_TEAM, center + north * 330, false);
  s_AssaultGroup = Spawn(US_TEAM, center + north * 330 + east * 160, false);
  s_DepthAGroup = Spawn(US_TEAM, home + Vector(4, 0, 0), false);
  s_DepthBGroup = Spawn(US_TEAM, home + Vector(-4, 0, 4), false);
  s_RaidersAt = ACR_Manager.Ground(home + Vector(380, 0, 0));
  s_Raiders = Spawn(USSR_TEAM, s_RaidersAt, true);
 }

 // 2. Assault starts with nobody seen at the objective: the base of fire holds overwatch.
 static void Start(ACR_Manager manager)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  vector center = s_Target.GetOrigin();
  s_Fire = manager.FindSquadByGroup(s_FireGroup);
  s_Assault = manager.FindSquadByGroup(s_AssaultGroup);
  if (s_Fire && s_Assault)
  {
   s_Assault.Halt("Holding");
   s_Attack = new ACR_AIAttack();
   s_Attack.m_Target = s_Target;
   s_Attack.m_vApproach = North();
   s_Attack.Add(s_Fire, Anchor(s_Fire), 0);
   s_Attack.Add(s_Assault, Anchor(s_Assault), 1);
   s_Attack.m_Suppressor = s_Fire;
   s_Attack.m_bAssaulting = true;
   s_Attack.m_fAssaultStart = now;
   s_AI.TickFireBase(s_Attack);
  }
  bool holding = s_Attack && s_Attack.m_Suppressor == s_Fire && s_Fire.OrderState() == ACR_EOrderState.DEFEND && s_Attack.m_bOverwatch;
  float range = 0;
  bool line = false;
  if (s_Attack && s_Attack.m_bOverwatch)
  {
   range = vector.DistanceXZ(s_Attack.m_vOverwatch, center);
   line = ACR_AICommander.FireLine(s_Attack.m_vOverwatch, center);
  }
  string order = "none";
  if (s_Fire) order = typename.EnumToString(ACR_EOrderState, s_Fire.OrderState());
  ACR_TestRegress.Check("base of fire holds overwatch at the assault start with no enemy seen", holding && range >= s_Target.m_fRadius + ACR_AICommander.OVERWATCH_MIN_GAP && range <= ACR_AICommander.FIRE_BASE_RANGE && line, string.Format("squads %1/%2, order %3, overwatch %4 m out, fire line %5", s_Fire != null, s_Assault != null, order, Math.Round(range), line));
  StartDepth(manager, now);
 }

 // 3. An attack is seen coming at Farm: one squad forward, one in reserve.
 static void StartDepth(ACR_Manager manager, float now)
 {
  string name = "defence in depth: one squad to a forward line 100-300 m towards the attack, one in reserve";
  vector home = s_Home.GetOrigin();
  ACR_Squad depthA = manager.FindSquadByGroup(s_DepthAGroup);
  ACR_Squad depthB = manager.FindSquadByGroup(s_DepthBGroup);
  if (!depthA || !depthB)
  {
   ACR_TestRegress.Check(name, false, string.Format("defender squads found %1/%2", depthA != null, depthB != null));
   return;
  }
  depthA.m_iAITask = ACR_EAITask.DEFEND;
  depthA.m_AITarget = s_Home;
  depthB.m_iAITask = ACR_EAITask.DEFEND;
  depthB.m_AITarget = s_Home;
  array<IEntity> raiders = {};
  Bodies(s_Raiders, raiders);
  foreach (IEntity raider : raiders) s_AI.NoteAtForTest(raider, raider.GetOrigin(), now);
  s_AI.AssessForTest();
  s_AI.DefendInDepth();
  vector spotA;
  vector spotB;
  bool forwardA = s_AI.ForwardSpotOf(depthA, spotA);
  bool forwardB = s_AI.ForwardSpotOf(depthB, spotB);
  vector spot = spotB;
  s_Forward = depthB;
  s_Reserve = depthA;
  if (forwardA)
  {
   spot = spotA;
   s_Forward = depthA;
   s_Reserve = depthB;
  }
  float outward = vector.DistanceXZ(spot, home);
  float turn = ACR_AICommander.TurnBetween(spot - home, s_RaidersAt - home);
  s_ForwardStartGap = vector.DistanceXZ(Anchor(s_Forward), spot);
  s_ForwardStartOut = vector.DistanceXZ(Anchor(s_Forward), home);
  bool reserveKept = s_Reserve.m_iAITask == ACR_EAITask.DEFEND && s_Reserve.m_AITarget == s_Home;
  ACR_TestRegress.Check(name, forwardA != forwardB && raiders.Count() > 0 && outward >= ACR_AICommander.FORWARD_MIN && outward <= ACR_AICommander.FORWARD_MAX && turn <= 30 && reserveKept, string.Format("forward A %1 B %2, %3 raiders noted, line %4 m out, %5 degrees off the attack, reserve kept %6", forwardA, forwardB, raiders.Count(), Math.Round(outward), Math.Round(turn), reserveKept));
 }

 // A minute into the assault: the base of fire stands on its overwatch spot with a fire line.
 static void Hold()
 {
  vector center = s_Target.GetOrigin();
  if (s_Attack) s_AI.TickFireBase(s_Attack);
  vector at = Anchor(s_Fire);
  bool still = s_Attack && s_Fire && s_Attack.m_Suppressor == s_Fire && s_Fire.OrderState() == ACR_EOrderState.DEFEND && vector.DistanceXZ(s_Fire.m_MoveTarget, s_Attack.m_vOverwatch) < 20;
  float range = vector.DistanceXZ(at, center);
  float fromSpot = -1;
  if (s_Attack) fromSpot = vector.DistanceXZ(at, s_Attack.m_vOverwatch);
  bool line = ACR_AICommander.FireLine(at, center);
  ACR_TestRegress.Check("base of fire still holds overwatch with a clear fire line 60 s into the assault", still && fromSpot >= 0 && fromSpot <= ACR_AICommander.OVERWATCH_REACHED && range <= ACR_AICommander.FIRE_BASE_RANGE && line, string.Format("still %1, %2 m from its spot (%3 m at 15 s), %4 m from the objective, fire line %5", still, Math.Round(fromSpot), Math.Round(s_FireStartGap), Math.Round(range), line));
  s_Watched = Spawn(USSR_TEAM, center, true);
 }

 // Thirty seconds on: the forward squad's physical position.
 static void HoldDepth()
 {
  vector home = s_Home.GetOrigin();
  string name = "forward squad walked out towards its line; the reserve stayed at the point";
  if (s_Forward && s_Reserve)
  {
   vector spot;
   bool forward = s_AI.ForwardSpotOf(s_Forward, spot);
   float gap = vector.DistanceXZ(Anchor(s_Forward), spot);
   float outward = vector.DistanceXZ(Anchor(s_Forward), home);
   float reserveOut = vector.DistanceXZ(Anchor(s_Reserve), home);
   ACR_TestRegress.Check(name, forward && gap <= s_ForwardStartGap - 10 && outward >= s_ForwardStartOut + 10 && reserveOut <= s_Home.m_fRadius + 40, string.Format("forward gap %1 -> %2 m, out %3 -> %4 m, reserve %5 m from the centre", Math.Round(s_ForwardStartGap), Math.Round(gap), Math.Round(s_ForwardStartOut), Math.Round(outward), Math.Round(reserveOut)));
  }
  else ACR_TestRegress.Check(name, false, "no forward squad");
 }

 // An enemy is seen at the objective: suppress exactly there.
 static void Seen()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<IEntity> watched = {};
  Bodies(s_Watched, watched);
  foreach (IEntity seen : watched) s_AI.NoteAtForTest(seen, seen.GetOrigin(), now);
  if (s_Attack) s_AI.TickFireBase(s_Attack);
  float nearest = float.MAX;
  string order = "none";
  if (s_Fire)
  {
   order = typename.EnumToString(ACR_EOrderState, s_Fire.OrderState());
   foreach (IEntity body : watched) nearest = Math.Min(nearest, vector.DistanceXZ(body.GetOrigin(), s_Fire.m_MoveTarget));
  }
  bool suppressing = s_Attack && s_Fire && s_Attack.m_Suppressor == s_Fire && s_Fire.OrderState() == ACR_EOrderState.SUPPRESS;
  ACR_TestRegress.Check("base of fire suppresses the enemy this side has seen at the objective", suppressing && !watched.IsEmpty() && nearest <= 10, string.Format("order %1, %2 seen, aim %3 m from the nearest", order, watched.Count(), Math.Round(nearest)));
 }

 // No longer seen: no fire at an empty spot, back to overwatch. The attackers on Farm are
 // now seen 70 m out: too close for the forward line.
 static void Forgotten()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  vector home = s_Home.GetOrigin();
  array<IEntity> forgotten = {};
  Bodies(s_Watched, forgotten);
  foreach (IEntity gone : forgotten) s_AI.ForgetForTest(gone);
  if (s_Attack) s_AI.TickFireBase(s_Attack);
  bool back = s_Attack && s_Fire && s_Attack.m_Suppressor == s_Fire && s_Fire.OrderState() == ACR_EOrderState.DEFEND && vector.DistanceXZ(s_Fire.m_MoveTarget, s_Attack.m_vOverwatch) < 20;
  string order = "none";
  if (s_Fire) order = typename.EnumToString(ACR_EOrderState, s_Fire.OrderState());
  ACR_TestRegress.Check("base of fire stops suppressing once the enemy is no longer seen and holds overwatch again", back, string.Format("order %1", order));
  array<IEntity> raiders = {};
  Bodies(s_Raiders, raiders);
  foreach (IEntity raider : raiders) s_AI.NoteAtForTest(raider, ACR_Manager.Ground(home + Vector(70, 0, 0)), now);
  s_AI.AssessForTest();
  s_AI.DefendInDepth();
  vector spot;
  bool stillForward = s_Forward && s_AI.ForwardSpotOf(s_Forward, spot);
  float ordered = -1;
  if (s_Forward) ordered = vector.DistanceXZ(s_Forward.m_MoveTarget, home);
  ACR_TestRegress.Check("forward squad falls back to the point when the enemy closes in", s_Forward && !stillForward && s_Forward.OrderState() == ACR_EOrderState.DEFEND && ordered >= 0 && ordered <= s_Home.m_fRadius + 90, string.Format("still forward %1, ordered to %2 m from the centre", stillForward, Math.Round(ordered)));
 }

 static void KillAssault(ACR_TestHarness harness)
 {
  array<IEntity> assaulters = {};
  Bodies(s_AssaultGroup, assaulters);
  foreach (IEntity man : assaulters) harness.Kill(man);
 }

 // Nobody left to cover: the base of fire joins the assault.
 static void Lifted()
 {
  if (s_Attack) s_AI.TickFireBase(s_Attack);
  bool joined = s_Attack && s_Attack.m_Suppressor == null && s_Fire && s_Fire.OrderState() == ACR_EOrderState.ASSAULT;
  string order = "none";
  if (s_Fire) order = typename.EnumToString(ACR_EOrderState, s_Fire.OrderState());
  ACR_TestRegress.Check("base of fire lifts and joins the assault when nobody is left to cover", joined, string.Format("order %1", order));
  array<SCR_AIGroup> groups = {s_FireGroup, s_AssaultGroup, s_DepthAGroup, s_DepthBGroup, s_Raiders, s_Watched};
  foreach (SCR_AIGroup group : groups)
  {
   if (!group) continue;
   array<IEntity> men = {};
   Bodies(group, men);
   foreach (IEntity body : men) SCR_EntityHelper.DeleteEntityAndChildren(body);
   SCR_EntityHelper.DeleteEntityAndChildren(group);
  }
  ACR_TestRegress.Complete("attackplan", EXPECTED);
 }
}
