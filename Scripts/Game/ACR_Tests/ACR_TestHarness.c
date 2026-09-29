// Optional automated regression runs for the test range, driven by startup flags:
//   -acrAutoTest 1        possession, chained swaps, map markers, input state
//   -acrCommandTest 1     native squad command response (Move / Follow)
//   -acrMapHandoffTest 1  map gadget + view-control cleanup across swaps
// Runs on the host only. Results are "[ACR] ..." lines in console.log.
class ACR_TestHarness : Managed
{
 static const int MODE_AUTO = 1;
 static const int MODE_COMMAND = 2;
 static const int MODE_MAP_HANDOFF = 3;
 static const int MODE_VEHICLE = 4;
 static const int MODE_BASES = 5;
 static const int MODE_ASSAULT = 6;
 static const int MODE_REQUISITION = 7;
 static const int MODE_GAMEMASTER = 8;
 static const int MODE_AI = 9;
 static const int MODE_GUNNER = 10;
 static const int MODE_PROBE = 11;
 static const int MODE_RESPAWN = 12;
 static const int MODE_SUPPORT = 13;
 static const int MODE_GUNSHIP = 14;
 static const int MODE_TACTICS = 15;
 static const int MODE_TRANSPORT = 16;
 static const int MODE_STALL = 17;
 static const int MODE_RIDE = 18;
 static const int MODE_LOGISTICS = 19;

 ACR_TestRange m_Range;
 int m_Mode;
 int m_Ticks;
 bool m_Ready;
 vector m_Start;
 IEntity m_TestOriginal;
 IEntity m_TestFirst;
 IEntity m_MapTestBody;
 float m_MapTestYaw;
 vector m_MapTestPosition;
 ref array<IEntity> m_OrderUnits = {};
 ref array<float> m_OrderDistances = {};
 vector m_OrderTarget;

 static ACR_TestHarness CreateFromCommandLine(ACR_TestRange range)
 {
  string flag;
  int mode;
  if (System.GetCLIParam("acrAutoTest", flag)) mode = MODE_AUTO;
  if (System.GetCLIParam("acrCommandTest", flag)) mode = MODE_COMMAND;
  if (System.GetCLIParam("acrMapHandoffTest", flag)) mode = MODE_MAP_HANDOFF;
  if (System.GetCLIParam("acrVehicleTest", flag)) mode = MODE_VEHICLE;
  if (System.GetCLIParam("acrBasesTest", flag)) mode = MODE_BASES;
  if (System.GetCLIParam("acrAssaultTest", flag)) mode = MODE_ASSAULT;
  if (System.GetCLIParam("acrRequisitionTest", flag)) mode = MODE_REQUISITION;
  if (System.GetCLIParam("acrGameMasterTest", flag)) mode = MODE_GAMEMASTER;
  if (System.GetCLIParam("acrAITest", flag)) mode = MODE_AI;
  if (System.GetCLIParam("acrGunnerTest", flag)) mode = MODE_GUNNER;
  if (System.GetCLIParam("acrProbeTest", flag)) mode = MODE_PROBE;
  if (System.GetCLIParam("acrRespawnTest", flag)) mode = MODE_RESPAWN;
  if (System.GetCLIParam("acrSupportTest", flag)) mode = MODE_SUPPORT;
  if (System.GetCLIParam("acrGunshipTest", flag)) mode = MODE_GUNSHIP;
  if (System.GetCLIParam("acrTacticsTest", flag)) mode = MODE_TACTICS;
  if (System.GetCLIParam("acrTransportTest", flag)) mode = MODE_TRANSPORT;
  if (System.GetCLIParam("acrStallTest", flag)) mode = MODE_STALL;
  if (System.GetCLIParam("acrRideTest", flag)) mode = MODE_RIDE;
  if (System.GetCLIParam("acrLogisticsTest", flag)) mode = MODE_LOGISTICS;
  if (!mode) return null;
  ACR_TestHarness harness = new ACR_TestHarness();
  harness.m_Range = range;
  harness.m_Mode = mode;
  PrintFormat("[ACR] Test harness mode=%1", mode);
  return harness;
 }

 ACR_Squad Squad()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !m_Range) return null;
  return manager.FindSquadByGroup(m_Range.m_Group);
 }

 vector Ground(string point) { return ACR_Manager.Ground(point.ToVector()); }

 void Tick()
 {
  m_Ticks++;
  // Lets the launcher script close the game as soon as the run is over.
  if ((m_Mode == MODE_AUTO && m_Ticks == 66) || (m_Mode == MODE_COMMAND && m_Ticks == 92) || (m_Mode == MODE_MAP_HANDOFF && m_Ticks == 66))
   Print("[ACR] TEST COMPLETE");
  if (m_Mode == MODE_VEHICLE && m_Ticks == 140) Print("[ACR] TEST COMPLETE");
  if (m_Mode == MODE_VEHICLE) { TestVehicle(); return; }
  if (m_Mode == MODE_BASES) { TestBases(); return; }
  if (m_Mode == MODE_ASSAULT) { TestAssault(); return; }
  if (m_Mode == MODE_REQUISITION) { TestRequisition(); return; }
  if (m_Mode == MODE_GAMEMASTER) { TestGameMaster(); return; }
  if (m_Mode == MODE_AI) { TestAI(); return; }
  if (m_Mode == MODE_GUNNER) { TestGunner(); return; }
  if (m_Mode == MODE_PROBE) { TestProbe(); return; }
  if (m_Mode == MODE_RESPAWN) { TestRespawn(); return; }
  if (m_Mode == MODE_SUPPORT) { TestSupport(); return; }
  if (m_Mode == MODE_GUNSHIP) { TestGunship(); return; }
  if (m_Mode == MODE_TACTICS) { TestTactics(); return; }
  if (m_Mode == MODE_TRANSPORT) { TestTransport(); return; }
  if (m_Mode == MODE_STALL) { TestStall(); return; }
  if (m_Mode == MODE_RIDE) { TestRide(); return; }
  if (m_Mode == MODE_LOGISTICS) { TestLogistics(); return; }
  if (m_Mode == MODE_MAP_HANDOFF) TestMapHandoff();
  else if (m_Mode == MODE_COMMAND) TestCommandResponse();
  else TestAuto();
 }

 void LogLeaderMarker()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Squad squad = Squad();
  if (!pc || !squad) return;
  RplId leaderId = SCR_EntityHelper.EntityToRplId(squad.GetLeader());
  PrintFormat("[ACR] GROUP TEST %1 authority=%2", pc.ACR_DescribeGroupMarker(leaderId), squad.m_Commander == pc.GetPlayerId());
 }

 void SelectLeaderMarker(SCR_PlayerController pc, ACR_Squad squad)
 {
  bool found = pc.ACR_SelectMarker(SCR_EntityHelper.EntityToRplId(squad.GetLeader()), true);
  InputManager input = GetGame().GetInputManager();
  PrintFormat("[ACR] MAP TEST markerFound=%1 possessed=%2 shortcut=%3 movement=%4", found, pc.IsPossessing(), input.IsActionActive("ACR_Possess"), input.IsActionActive("CharacterForward"));
 }

 // ---------------------------------------------------------------- -acrAutoTest
 void TestAuto()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Squad squad = Squad();
  if (!squad || !m_Range.m_Group) return;
  IEntity leaderEntity = squad.GetLeader();
  if (leaderEntity)
  {
   vector position = leaderEntity.GetOrigin();
   if (!m_Ready)
   {
    m_Ready = true;
    m_Start = position;
    PrintFormat("[ACR] READY soldiers=%1 position=%2", m_Range.m_Group.GetAgentsCount(), position);
   }
   if (m_Ticks % 10 == 0) PrintFormat("[ACR] Squad distance travelled=%1 target distance=%2", vector.Distance(position, m_Start), vector.Distance(position, m_Range.m_Target));
  }
  if (!pc || !pc.GetControlledEntity()) return;
  if (m_Ticks == 15 || m_Ticks == 32 || m_Ticks == 34 || m_Ticks == 38 || m_Ticks == 52 || m_Ticks == 61)
  {
   InputManager input = GetGame().GetInputManager();
   PrintFormat("[ACR] SHORTCUTS possessing=%1 F7=%2 F8=%3 F9=%4", pc.IsPossessing(), input.IsActionActive("ACR_Possess"), input.IsActionActive("ACR_Return"), input.IsActionActive("ACR_Map"));
   PrintFormat("[ACR] INPUT possessing=%1 forward=%2 aim=%3 fire=%4", pc.IsPossessing(), input.IsActionActive("CharacterForward"), input.IsActionActive("CharacterAimUp"), input.IsActionActive("CharacterFire"));
  }
  if (m_Ticks == 12) pc.ACR_Request(ACR_ERequest.ADVANCE);
  if (m_Ticks == 20) pc.ACR_Request(ACR_ERequest.MOVE_TO, RplId.Invalid(), Vector(1370, 0, 2832));
  if (m_Ticks == 22)
  {
   ChimeraCharacter testBody = ChimeraCharacter.Cast(pc.GetControlledEntity());
   CharacterInputContext testInput = testBody.GetCharacterController().GetInputContext();
   testInput.SetMovement(1, Vector(0, 0, 1));
   ACR_MapHandoff.ClearCharacterInput(testBody);
   float speed;
   vector direction;
   testInput.GetMovement(speed, direction);
   PrintFormat("[ACR] MOVEMENT RESET speed=%1 direction=%2", speed, direction);
  }
  if (m_Ticks == 25 && leaderEntity)
  {
   vector start = leaderEntity.GetOrigin() + Vector(0, 1.2, -3);
   PrintFormat("[ACR] AIM ray selected intended soldier=%1", pc.ACR_TraceSoldier(start, Vector(0, 0, 1)) == leaderEntity);
  }
  if (m_Ticks == 30)
  {
   m_TestOriginal = pc.GetControlledEntity();
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   if (agents.Count() > 0)
   {
    m_TestFirst = agents[0].GetControlledEntity();
    pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(m_TestFirst));
   }
  }
  if (m_Ticks == 31)
  {
   SCR_PlayerControllerGroupComponent groupComp = SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
   SCR_CommandingManagerComponent commandManager = SCR_CommandingManagerComponent.GetInstance();
   PrintFormat("[ACR] NATIVE TEST leader=%1 menuMove=%2", groupComp.IsPlayerLeaderOwnGroup(), commandManager.CanShowCommand("waypointMoveForced"));
   SCR_PlayerControllerCommandingComponent commanding = SCR_PlayerControllerCommandingComponent.GetLocalPlayerControllerCommandingComponent();
   commanding.PrepareExecuteCommand("waypointMoveForced", Ground("1390 0 2852"));
  }
  if (m_Ticks == 32)
  {
   array<AIWaypoint> nativeOrders = {};
   m_Range.m_Group.GetWaypoints(nativeOrders);
   bool foundNativeOrder;
   foreach (AIWaypoint order : nativeOrders)
    if (vector.Distance(order.GetOrigin(), Ground("1390 0 2852")) < 2) foundNativeOrder = true;
   PrintFormat("[ACR] NATIVE TEST waypointAtRequestedPosition=%1", foundNativeOrder);
  }
  if (m_Ticks == 33)
  {
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity next = agent.GetControlledEntity();
    if (next != m_TestFirst)
    {
     pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(next));
     PrintFormat("[ACR] CHAIN different=%1 originalPreserved=%2", pc.GetControlledEntity() == next, pc.GetMainEntity() == m_TestOriginal);
     break;
    }
   }
  }
  if (m_Ticks == 34)
  {
   PrintFormat("[ACR] NATIVE TEST released=%1 members=%2", squad.m_Commander == 0, m_Range.m_Group.GetAgentsCount());
   SCR_CommandingManagerComponent commands = SCR_CommandingManagerComponent.GetInstance();
   commands.RequestCommandExecution(commands.FindCommandIndex("waypointMoveForced"), RplId.Invalid(), SCR_EntityHelper.EntityToRplId(m_Range.m_Group), Ground("1420 0 2862"), pc.GetPlayerId(), 0);
  }
  if (m_Ticks == 35)
  {
   PrintFormat("[ACR] CHECK possessing=%1", pc.IsPossessing());
   pc.ACR_Request(ACR_ERequest.RETURN);
  }
  if (m_Ticks == 40)
  {
   PrintFormat("[ACR] CHECK returned=%1 exactOriginal=%2", !pc.IsPossessing(), pc.GetControlledEntity() == m_TestOriginal);
   pc.ACR_Request(ACR_ERequest.RALLY);
  }
  if (m_Ticks == 44 || m_Ticks == 54) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 47 || m_Ticks == 56) SelectLeaderMarker(pc, squad);
  if (m_Ticks == 48 || m_Ticks == 57) GetGame().GetCallqueue().CallLater(LogLeaderMarker, 100, false);
  if (m_Ticks == 49 || m_Ticks == 58) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 51 || m_Ticks == 60) pc.ACR_Request(ACR_ERequest.RETURN);
 }

 // ---------------------------------------------------------------- -acrMapHandoffTest
 void TestMapHandoff()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Squad squad = Squad();
  if (!pc || !pc.GetControlledEntity() || !squad) return;
  if (m_Ticks == 12)
  {
   m_MapTestBody = squad.GetLeader();
   pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(m_MapTestBody));
  }
  if (m_Ticks == 15 || m_Ticks == 50)
  {
   SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(pc.GetControlledEntity());
   IEntity item = gadgets.GetGadgetByType(EGadgetType.MAP);
   PrintFormat("[ACR] MAP HANDOFF equip mapExists=%1", item != null);
   gadgets.SetGadgetMode(item, EGadgetMode.IN_HAND, true);
  }
  if (m_Ticks == 19 || m_Ticks == 21 || m_Ticks == 45 || m_Ticks == 53 || m_Ticks == 56 || m_Ticks == 61)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(m_MapTestBody);
   CharacterControllerComponent controller = body.GetCharacterController();
   CharacterControllerComponent current = ChimeraCharacter.Cast(pc.GetControlledEntity()).GetCharacterController();
   PrintFormat("[ACR] MAP FLAGS tick=%1 oldView=%2 oldMove=%3 newView=%4", m_Ticks, controller.GetDisableViewControls(), controller.GetDisableMovementControls(), current.GetDisableViewControls());
   PrintFormat("[ACR] MAP HANDOFF tick=%1 held=%2 wanted=%3 forcedLook=%4 inputLook=%5 playerFocus=%6 mapOpen=%7", m_Ticks, controller.IsGadgetInHands(), controller.GetInputContext().GetWantedLefHandGadgetEntity() != null, controller.IsFreeLookForced(), controller.GetInputContext().CameraIsFreeLook(), pc.GetGadgetFocus(), SCR_MapEntity.GetMapInstance().IsOpen());
  }
  if (m_Ticks == 20)
  {
   m_MapTestYaw = m_MapTestBody.GetYawPitchRoll()[0];
   m_MapTestPosition = m_MapTestBody.GetOrigin();
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity next = agent.GetControlledEntity();
    if (next && next != m_MapTestBody)
    {
     if (!pc.ACR_SelectMarker(SCR_EntityHelper.EntityToRplId(next), false)) Print("[ACR] MAP HANDOFF missing test marker");
     break;
    }
   }
  }
  if (m_Ticks == 22 || m_Ticks == 54) pc.ACR_Request(ACR_ERequest.RETURN);
  if (m_Ticks == 23) pc.ACR_Request(ACR_ERequest.MOVE_TO, RplId.Invalid(), Ground("1375 0 2832"));
  if (m_Ticks == 43)
   PrintFormat("[ACR] MAP HANDOFF AI distance=%1 yawBefore=%2 yawAfter=%3", vector.Distance(m_MapTestBody.GetOrigin(), m_MapTestPosition), m_MapTestYaw, m_MapTestBody.GetYawPitchRoll()[0]);
  if (m_Ticks == 44)
  {
   // The leader's plain marker is hidden behind its group marker.
   if (!pc.ACR_SelectMarker(SCR_EntityHelper.EntityToRplId(m_MapTestBody), true)) Print("[ACR] MAP HANDOFF missing leader marker");
  }
  if (m_Ticks == 48 || m_Ticks == 60) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
 }

 // ---------------------------------------------------------------- -acrCommandTest
 void TestCommandResponse()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Squad squad = Squad();
  if (!pc || !pc.GetControlledEntity() || !squad) return;
  SCR_PlayerControllerCommandingComponent commands = SCR_PlayerControllerCommandingComponent.GetLocalPlayerControllerCommandingComponent();
  SCR_CommandingManagerComponent manager = SCR_CommandingManagerComponent.GetInstance();
  if (m_Ticks == 12 || m_Ticks == 40) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(squad.GetLeader()));
  if (m_Ticks == 15 || m_Ticks == 43 || m_Ticks == 65)
  {
   m_OrderTarget = Ground("1370 0 2832");
   if (m_Ticks == 43) m_OrderTarget = Ground("1335 0 2867");
   if (m_Ticks == 65) m_OrderTarget = pc.GetControlledEntity().GetOrigin();
   m_OrderUnits.Clear();
   m_OrderDistances.Clear();
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || ACR_Squad.FindController(body)) continue;
    m_OrderUnits.Insert(body);
    m_OrderDistances.Insert(vector.Distance(body.GetOrigin(), m_OrderTarget));
   }
   PrintFormat("[ACR] RESPONSE order=%1 menu=%2 allowed=%3 ai=%4", m_Ticks, manager.CanShowCommand("waypointMove"), commands.ACR_HasCommandAuthority(), m_OrderUnits.Count());
   if (m_Ticks == 65) commands.PrepareExecuteCommand("follow");
   else commands.PrepareExecuteCommand("waypointMove", m_OrderTarget);
  }
  if (m_Ticks == 35 || m_Ticks == 63 || m_Ticks == 85)
  {
   int responded;
   foreach (int i, IEntity body : m_OrderUnits)
   {
    float remaining = vector.Distance(body.GetOrigin(), m_OrderTarget);
    float progress = m_OrderDistances[i] - remaining;
    if (progress > 8) responded++;
    PrintFormat("[ACR] RESPONSE soldier=%1 progress=%2 remaining=%3", i, progress, remaining);
   }
   PrintFormat("[ACR] RESPONSE movedTowardOrder=%1 total=%2", responded, m_OrderUnits.Count());
  }
  if (m_Ticks == 36 && m_OrderUnits.Count() > 0)
   pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(m_OrderUnits[0]));
  if (m_Ticks == 38 || m_Ticks == 88)
  {
   PrintFormat("[ACR] RESPONSE nonLeader menu=%1 allowed=%2 members=%3", manager.CanShowCommand("waypointMove"), commands.ACR_HasCommandAuthority(), m_Range.m_Group.GetAgentsCount());
   commands.PrepareExecuteCommand("waypointMove", Ground("1420 0 2862"));
   manager.RequestCommandExecution(manager.FindCommandIndex("waypointMove"), RplId.Invalid(), SCR_EntityHelper.EntityToRplId(m_Range.m_Group), Ground("1420 0 2862"), pc.GetPlayerId(), 0);
  }
  if (m_Ticks == 50) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 52) LogLeaderMarker();
  if (m_Ticks == 54) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 87) pc.ACR_Request(ACR_ERequest.RETURN);
 }

 // ---------------------------------------------------------------- -acrVehicleTest
 // Alpha boards the truck, Bravo boards as passengers, Alpha drives, unloads
 // Bravo, then dismounts itself. Logs how many of each squad are in the truck.
 protected vector m_TruckStart;
 void LogVehicle(string step)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !m_Range.m_Truck) return;
  ACR_Squad alpha = manager.FindSquadByGroup(m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(m_Range.m_Bravo);
  PrintFormat("[ACR] VEHICLE %1 alphaIn=%2 bravoIn=%3 truckMoved=%4", step, CountInTruck(alpha), CountInTruck(bravo), vector.Distance(m_Range.m_Truck.GetOrigin(), m_TruckStart));
 }
 int CountInTruck(ACR_Squad squad)
 {
  if (!squad) return -1;
  int count;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle()) continue;
   CompartmentAccessComponent compartment = character.GetCompartmentAccessComponent();
   if (compartment && compartment.GetVehicleIn(character) == m_Range.m_Truck) count++;
  }
  return count;
 }
 void TestVehicle()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || !pc.GetControlledEntity() || !m_Range.m_Truck || !m_Range.m_Group || !m_Range.m_Bravo) return;
  RplId alpha = SCR_EntityHelper.EntityToRplId(m_Range.m_Group);
  RplId bravo = SCR_EntityHelper.EntityToRplId(m_Range.m_Bravo);
  vector truckPos = m_Range.m_Truck.GetOrigin();
  if (m_Ticks == 12)
  {
   m_TruckStart = truckPos;
   pc.ACR_Request(ACR_ERequest.ORDER_GET_IN, alpha, truckPos);
  }
  if (m_Ticks == 35) { LogVehicle("alphaBoarded"); pc.ACR_Request(ACR_ERequest.ORDER_GET_IN, bravo, truckPos); }
  if (m_Ticks == 60) { LogVehicle("bravoBoarded"); pc.ACR_Request(ACR_ERequest.MOVE_TO, alpha, m_TruckStart + Vector(80, 0, 0)); }
  if (m_Ticks == 95) { LogVehicle("drove"); pc.ACR_Request(ACR_ERequest.ORDER_UNLOAD, alpha); }
  if (m_Ticks == 115) { LogVehicle("unloaded"); pc.ACR_Request(ACR_ERequest.ORDER_GET_OUT, alpha); }
  if (m_Ticks == 135) LogVehicle("dismounted");
 }

 // ---------------------------------------------------------------- -acrBasesTest
 // Alpha and Bravo walk to the unclaimed Crossroads and capture it; logs the
 // state of every base and the economy every 10 s.
 void TestBases()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager || !m_Range.m_Group || !m_Range.m_Bravo) return;
  if (m_Ticks == 8)
  {
   PrintFormat("[ACR] BASES count=%1", ACR_Base.s_All.Count());
   pc.ACR_Request(ACR_ERequest.MODE_FAST, SCR_EntityHelper.EntityToRplId(m_Range.m_Group));
  }
  if (m_Ticks == 10) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(m_Range.m_Group), "1480 0 2860".ToVector());
  if (m_Ticks == 12) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(m_Range.m_Bravo), "1485 0 2855".ToVector());
  if (m_Ticks % 10 == 0)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
    PrintFormat("[ACR] BASE %1: %2", base.m_sName, manager.m_Bases.Describe(base));
   PrintFormat("[ACR] ECONOMY US rp=%1 income=%2 next=%3s", manager.m_Bases.GetRP("US"), manager.m_Bases.GetIncome("US"), manager.m_Bases.SecondsToIncome());
  }
  if (m_Ticks == 200) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrAssaultTest
 // Alpha and Bravo assault the enemy Outpost (garrison 3) while the player stays
 // at spawn ~600 m away. Checks the garrison fights, doesn't respawn, and the base flips.
 protected bool m_AssaultDone;
 void TestAssault()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager || !m_Range.m_Group || !m_Range.m_Bravo) return;
  ACR_Base outpost;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sName == "Outpost") outpost = base;
  if (!outpost) return;
  if (m_Ticks == 10) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(m_Range.m_Group), "1298 0 2192".ToVector());
  if (m_Ticks == 12) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(m_Range.m_Bravo), "1305 0 2187".ToVector());
  if (m_Ticks % 10 == 0)
  {
   ACR_Squad alpha = manager.FindSquadByGroup(m_Range.m_Group);
   ACR_Squad bravo = manager.FindSquadByGroup(m_Range.m_Bravo);
   string a = "gone";
   string b = "gone";
   if (alpha) a = alpha.Describe();
   if (bravo) b = bravo.Describe();
   PrintFormat("[ACR] ASSAULT t=%1 outpost: %2 | alpha: %3 | bravo: %4", m_Ticks, manager.m_Bases.Describe(outpost), a, b);
   PrintFormat("[ACR] ASSAULT LOD garrison=%1 alpha=%2 max=%3", LodList(outpost.m_GarrisonGroup), LodList(m_Range.m_Group), AIAgent.GetMaxLOD());
  }
  if (!m_AssaultDone && outpost.m_sOwner == "US")
  {
   m_AssaultDone = true;
   PrintFormat("[ACR] ASSAULT captured at t=%1", m_Ticks);
   GetGame().GetCallqueue().CallLater(PrintComplete, 5000, false);
  }
  if (m_Ticks == 150) Print("[ACR] TEST COMPLETE");
 }
 string LodList(AIGroup group)
 {
  if (!group) return "none";
  string text = group.GetLOD().ToString() + ":";
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) text += " " + agent.GetLOD().ToString();
  return text;
 }
 void PrintComplete() { Print("[ACR] TEST COMPLETE"); }

 // ---------------------------------------------------------------- -acrRequisitionTest
 void TestRequisition()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  array<ref ACR_RequisitionEntry> entries = manager.m_Bases.GetFaction("US").m_aRequisitions;
  if (m_Ticks == 8)
  {
   PrintFormat("[ACR] REQ rp=%1 entries=%2 garrisonPool=%3", manager.m_Bases.GetRP("US"), entries.Count(), manager.m_Bases.GetFaction("US").m_aGarrison.Count());
   foreach (int i, ACR_RequisitionEntry e : entries)
    PrintFormat("[ACR] REQ %1: %2 | %3 RP | stock %4 | vehicle=%5", i, e.m_sName, e.m_iCost, e.m_iStock, !e.m_sVehicle.IsEmpty());
  }
  if (m_Ticks == 10)
  {
   foreach (int i, ACR_RequisitionEntry e : entries)
    if (e.m_sVehicle.IsEmpty()) { pc.ACR_TestPurchase(i, "Farm [1]"); break; }
  }
  if (m_Ticks == 12)
  {
   int pick = -1;
   foreach (int i, ACR_RequisitionEntry e : entries)
   {
    if (e.m_sVehicle.IsEmpty()) continue;
    if (pick < 0) pick = i;
    if (e.m_sVehicle.Contains("M923")) { pick = i; break; }
   }
   if (pick >= 0) pc.ACR_TestPurchase(pick, "Farm [1]");
  }
  if (m_Ticks == 14) pc.ACR_TestPurchase(0, "Outpost"); // enemy base: must be refused
  if (m_Ticks == 30 || m_Ticks == 65)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
    PrintFormat("[ACR] REQ squad %1 (%2): %3", squad.GetName(), squad.GetFaction().GetFactionKey(), squad.Describe());
   PrintFormat("[ACR] REQ rp=%1", manager.m_Bases.GetRP("US"));
  }
  if (m_Ticks == 35 && m_Range.m_Group)
  {
   array<AIAgent> agents = {};
   m_Range.m_Group.GetAgents(agents);
   if (agents.Count() > 1) SCR_EntityHelper.DeleteEntityAndChildren(agents[agents.Count() - 1].GetControlledEntity());
  }
  if (m_Ticks == 40) pc.ACR_Request(ACR_ERequest.REINFORCE, SCR_EntityHelper.EntityToRplId(m_Range.m_Group));
  if (m_Ticks == 70) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrGameMasterTest
 void TestGameMaster()
 {
  SCR_EditorManagerEntity editor = SCR_EditorManagerEntity.GetInstance();
  if (m_Ticks == 10)
  {
   if (!editor) { Print("[ACR] GM no editor manager"); return; }
   PrintFormat("[ACR] GM hasEdit=%1 modes=%2", editor.HasMode(EEditorMode.EDIT), editor.GetModes());
   editor.Open();
  }
  if (m_Ticks == 14 && editor) PrintFormat("[ACR] GM opened=%1", editor.IsOpened());
  if (m_Ticks == 16 && editor) editor.Close();
  if (m_Ticks == 18 && editor)
  {
   PrintFormat("[ACR] GM closed=%1", !editor.IsOpened());
   Print("[ACR] TEST COMPLETE");
  }
 }

 // ---------------------------------------------------------------- -acrAITest
 // The Soviet AI commander runs on its own for ~2.5 minutes; the log shows
 // purchases, postings, defences and attacks. US squads stay idle at spawn.
 void TestAI()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (m_Ticks % 20 == 0)
  {
   PrintFormat("[ACR] AITEST t=%1 USSR rp=%2 aiActive=%3", m_Ticks, manager.m_Bases.GetRP("USSR"), manager.IsAIInCommand("USSR"));
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad.GetFaction().GetFactionKey() == "USSR")
     PrintFormat("[ACR] AITEST   %1 task=%2 %3", squad.GetName(), squad.m_iAITask, squad.Describe());
   foreach (ACR_Base base : ACR_Base.s_All)
    PrintFormat("[ACR] AITEST   base %1: %2", base.m_sName, manager.m_Bases.Describe(base));
  }
  if (m_Ticks == 150) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrGunnerTest
 IEntity m_GunVehicle;
 ChimeraCharacter m_Gunner;
 vector m_GunStart;
 float m_GunYaw;

 string GunState()
 {
  if (!m_GunVehicle) return "no vehicle";
  vector angles = m_GunVehicle.GetYawPitchRoll();
  CarControllerComponent car = CarControllerComponent.Cast(m_GunVehicle.FindComponent(CarControllerComponent));
  string sim = "";
  if (car && car.GetSimulation())
  {
   VehicleWheeledSimulation s = car.GetSimulation();
   sim = string.Format("gear=%1 (n=%2) rpm=%3 thr=%4 brk=%5 steer=%6 engine=%7", s.GetGear(), s.GearboxGearsCount() - s.GearboxForwardGearsCount() - 1, (int)s.EngineGetRPM(), s.GetThrottle(), s.GetBrake(), s.GetSteering(), car.IsEngineOn());
  }
  vector offset = m_GunVehicle.GetOrigin() - m_GunStart;
  float forward = vector.Dot(offset, Vector(Math.Sin(m_GunYaw * Math.DEG2RAD), 0, Math.Cos(m_GunYaw * Math.DEG2RAD)));
  float speed = 0;
  if (m_GunVehicle.GetPhysics()) speed = m_GunVehicle.GetPhysics().GetVelocity().Length();
  return string.Format("moved=%1 forward=%2 yaw=%3 speed=%4 %5", offset.Length(), forward, angles[0], speed, sim);
 }

 void TestGunner()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (m_Ticks == 5) manager.SpawnUnit("Gun Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", Ground("1390 0 2795"), Ground("1395 0 2790"));
  if (m_Ticks == 14)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() != "Gun Truck") continue;
    array<IEntity> vehicles = {};
    squad.GetVehicles(vehicles);
    if (!vehicles.IsEmpty()) m_GunVehicle = vehicles[0];
   }
   if (!m_GunVehicle) { Print("[ACR] GUNNER FAIL: no gun truck"); return; }
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_GunVehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> turrets = {};
   if (compartments) compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
   foreach (BaseCompartmentSlot turret : turrets)
    if (turret.GetOccupant()) m_Gunner = ChimeraCharacter.Cast(turret.GetOccupant());
   PrintFormat("[ACR] GUNNER turrets=%1 gunner=%2", turrets.Count(), m_Gunner);
   if (m_Gunner) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(m_Gunner));
  }
  if (!m_GunVehicle) return;
  if (m_Ticks == 17)
  {
   ChimeraCharacter driver;
   IEntity drivable = ACR_GunnerDrive.DrivableFrom(pc.GetControlledEntity(), driver);
   PrintFormat("[ACR] GUNNER possessed=%1 drivable=%2 driver=%3", pc.GetControlledEntity() == m_Gunner, drivable == m_GunVehicle, driver);
   m_GunStart = m_GunVehicle.GetOrigin();
   m_GunYaw = m_GunVehicle.GetYawPitchRoll()[0];
  }
  if (m_Ticks >= 18 && m_Ticks < 24) pc.ACR_TestDrive(m_GunVehicle, 1, 0);
  if (m_Ticks >= 24 && m_Ticks < 26) pc.ACR_TestDrive(m_GunVehicle, 0, 0);
  if (m_Ticks >= 26 && m_Ticks < 31) pc.ACR_TestDrive(m_GunVehicle, 0.5, 1);
  if (m_Ticks >= 31 && m_Ticks < 35) pc.ACR_TestDrive(m_GunVehicle, 0, 0);
  if (m_Ticks == 35) { m_GunStart = m_GunVehicle.GetOrigin(); m_GunYaw = m_GunVehicle.GetYawPitchRoll()[0]; }
  if (m_Ticks >= 35 && m_Ticks < 41) pc.ACR_TestDrive(m_GunVehicle, -1, 0);
  if (m_Ticks >= 41 && m_Ticks < 44) pc.ACR_TestDrive(m_GunVehicle, 0, 0);
  if (m_Ticks >= 17 && m_Ticks <= 45) PrintFormat("[ACR] GUNNER t=%1 drivenByGunner=%2 %3", m_Ticks, manager.IsGunnerDriven(m_GunVehicle), GunState());
  if (m_Ticks == 46)
  {
   ACR_Squad squad = manager.FindSquadBySoldier(m_Gunner);
   if (squad) pc.ACR_Request(ACR_ERequest.MOVE_TO, SCR_EntityHelper.EntityToRplId(squad.m_Group), m_GunVehicle.GetOrigin() + Vector(0, 0, 60));
  }
  if (m_Ticks == 50 || m_Ticks == 60) PrintFormat("[ACR] GUNNER after map order: drivenByGunner=%1 %2", manager.IsGunnerDriven(m_GunVehicle), GunState());
  if (m_Ticks == 62) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrProbeTest: obstacle map (32 m cells)
 static const int PROBE_CELL = 32;
 static const int PROBE_CELLS = 128;
 protected int m_ProbeRow;
 protected int m_ProbeHits;

 bool ProbeCount(IEntity e)
 {
  if (!e || !e.GetPhysics()) return true;
  if (GenericTerrainEntity.Cast(e)) return true;
  m_ProbeHits++;
  return true;
 }

 void TestProbe()
 {
  if (m_Ticks < 5) return;
  BaseWorld world = GetGame().GetWorld();
  for (int n = 0; n < 16 && m_ProbeRow < PROBE_CELLS; n++)
  {
   string row = "";
   string heights = "";
   float z = m_ProbeRow * PROBE_CELL + PROBE_CELL * 0.5;
   for (int c = 0; c < PROBE_CELLS; c++)
   {
    float x = c * PROBE_CELL + PROBE_CELL * 0.5;
    float y = world.GetSurfaceY(x, z);
    heights += ((int)y).ToString() + ",";
    if (y < 1) { row += "~"; continue; }
    m_ProbeHits = 0;
    world.QueryEntitiesBySphere(Vector(x, y + 1, z), PROBE_CELL * 0.5, ProbeCount, null, EQueryEntitiesFlags.STATIC);
    if (m_ProbeHits == 0) row += ".";
    else if (m_ProbeHits > 9) row += "#";
    else row += m_ProbeHits.ToString();
   }
   PrintFormat("[ACR] PROBE %1 %2", m_ProbeRow, row);
   PrintFormat("[ACR] PROBEH %1 %2", m_ProbeRow, heights);
   m_ProbeRow++;
  }
  if (m_ProbeRow >= PROBE_CELLS) { Print("[ACR] TEST COMPLETE"); m_ProbeRow++; }
 }

 // ---------------------------------------------------------------- -acrRespawnTest
 // Farm is handed to nobody and Crossroads to US, so every respawn should land at
 // Crossroads (the owned base closest to the placed spawn point).
 IEntity m_RespawnOldMain;

 string RespawnState(SCR_PlayerController pc)
 {
  ACR_Base crossroads;
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName == "Crossroads") crossroads = base;
  IEntity controlled = pc.GetControlledEntity();
  IEntity main = pc.GetMainEntity();
  float distance = -1;
  if (main && crossroads) distance = vector.Distance(main.GetOrigin(), crossroads.GetOrigin());
  return string.Format("possessing=%1 controlledAlive=%2 mainAlive=%3 mainIsNew=%4 mainToCrossroads=%5", pc.IsPossessing(), ACR_Respawn.IsAlive(controlled), ACR_Respawn.IsAlive(main), main != m_RespawnOldMain, distance);
 }

 ChimeraCharacter AlphaMember(int index)
 {
  if (!m_Range.m_Group) return null;
  array<AIAgent> agents = {};
  m_Range.m_Group.GetAgents(agents);
  if (index >= agents.Count()) return null;
  return ChimeraCharacter.Cast(agents[index].GetControlledEntity());
 }

 void Kill(IEntity entity)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (character && character.GetCharacterController()) character.GetCharacterController().ForceDeath();
 }

 void TestRespawn()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (m_Ticks == 5)
  {
   manager.m_iRespawnSeconds = 6;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sName == "Farm") { base.m_sOwner = ""; base.m_iGarrisonLeft = 0; }
    if (base.m_sName == "Crossroads") base.m_sOwner = "US";
   }
  }
  // 1) Command unit dies while the player controls an Alpha soldier; F8 waits for the new one.
  if (m_Ticks == 10)
  {
   m_RespawnOldMain = pc.GetMainEntity();
   ChimeraCharacter soldier = AlphaMember(1);
   if (soldier) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(soldier));
  }
  if (m_Ticks == 12) { Kill(m_RespawnOldMain); PrintFormat("[ACR] RESPAWN T1 killed main: %1", RespawnState(pc)); }
  if (m_Ticks == 14) pc.ACR_Request(ACR_ERequest.RETURN);
  if (m_Ticks == 16 || m_Ticks == 21) PrintFormat("[ACR] RESPAWN T1 t=%1 %2", m_Ticks, RespawnState(pc));
  // 2) A possessed soldier dies while the command unit lives: back to the command unit.
  if (m_Ticks == 23)
  {
   m_RespawnOldMain = pc.GetMainEntity();
   ChimeraCharacter soldier = AlphaMember(2);
   if (soldier) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(soldier));
  }
  if (m_Ticks == 25) Kill(pc.GetControlledEntity());
  if (m_Ticks == 27) PrintFormat("[ACR] RESPAWN T2 %1 backInMain=%2", RespawnState(pc), pc.GetControlledEntity() == pc.GetMainEntity());
  // 3) Killed in the command unit: native respawn after the delay, at Crossroads.
  if (m_Ticks == 29) { m_RespawnOldMain = pc.GetControlledEntity(); Kill(m_RespawnOldMain); }
  if (m_Ticks == 32 || m_Ticks == 38) PrintFormat("[ACR] RESPAWN T3 t=%1 %2", m_Ticks, RespawnState(pc));
  if (m_Ticks == 40) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrSupportTest
 // Soviet target groups in the open field south of the airfield; each support is
 // called on its own group and the survivors are counted.
 protected ref array<SCR_AIGroup> m_Targets = {};
 protected ref array<string> m_TargetNames = {"mortar HE", "rocket strike", "bomb run", "heli gun run"};
 protected ref array<vector> m_TargetSpots = {"1240 0 2150".ToVector(), "1350 0 2150".ToVector(), "1240 0 2270".ToVector(), "1350 0 2270".ToVector()};

 int SupportIndex(ACR_Manager manager, ACR_ESupportType type)
 {
  foreach (int i, ACR_SupportEntry entry : manager.m_Bases.GetFaction("US").m_aSupports)
   if (entry.m_eType == type) return i;
  return -1;
 }

 int AliveIn(SCR_AIGroup group)
 {
  if (!group) return 0;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  int alive = 0;
  foreach (AIAgent agent : agents) if (ACR_Respawn.IsAlive(agent.GetControlledEntity())) alive++;
  return alive;
 }

 void LogTargets(string when)
 {
  string text = "";
  foreach (int i, SCR_AIGroup group : m_Targets) text += string.Format(" | %1: %2 alive", m_TargetNames[i], AliveIn(group));
  PrintFormat("[ACR] SUPPORTTEST %1%2", when, text);
 }

 void TestSupport()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  if (m_Ticks == 5)
  {
   manager.m_Bases.GetFaction("US").m_iRP = 1000;
   ResourceName groupPrefab;
   foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction("USSR").m_aRequisitions)
    if (entry.m_sVehicle.IsEmpty() && entry.m_sName.Contains("Fire Team")) { groupPrefab = entry.m_sGroup; break; }
   foreach (vector spot : m_TargetSpots)
   {
    SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(groupPrefab, spot));
    m_Targets.Insert(group);
   }
   PrintFormat("[ACR] SUPPORTTEST targets=%1 prefab=%2 supports=%3", m_Targets.Count(), groupPrefab, manager.m_Bases.GetFaction("US").m_aSupports.Count());
  }
  if (m_Ticks == 11) LogTargets("before");
  if (m_Ticks == 12) pc.ACR_TestSupport(SupportIndex(manager, ACR_ESupportType.MORTAR_HE_SMALL), m_TargetSpots[0]);
  if (m_Ticks == 13) pc.ACR_TestSupport(SupportIndex(manager, ACR_ESupportType.MORTAR_SMOKE), "1450 0 2400".ToVector());
  if (m_Ticks == 14) pc.ACR_TestSupport(SupportIndex(manager, ACR_ESupportType.ILLUMINATION), "1300 0 2450".ToVector());
  if (m_Ticks == 15) pc.ACR_TestSupport(SupportIndex(manager, ACR_ESupportType.ROCKET_STRIKE), m_TargetSpots[1]);
  if (m_Ticks == 16) pc.ACR_TestSupport(SupportIndex(manager, ACR_ESupportType.BOMB_RUN), m_TargetSpots[2]);
  if (m_Ticks == 17) pc.ACR_TestSupport(SupportIndex(manager, ACR_ESupportType.HELI_GUN_RUN), m_TargetSpots[3]);
  if (m_Ticks == 18) pc.ACR_TestSupport(0, m_TargetSpots[0]); // on cooldown: must be refused
  if (m_Ticks == 60) pc.ACR_TestSupport(SupportIndex(manager, ACR_ESupportType.HELI_LOITER), "1300 0 2210".ToVector());
  if (m_Ticks % 10 == 0 && m_Ticks >= 20) LogTargets(string.Format("t=%1", m_Ticks));
  if (m_Ticks == 140) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrGunshipTest
 // A US UH-1H and a Soviet Mi-8 loiter over enemy fireteams in the open.
 protected SCR_AIGroup m_UHTarget;
 protected SCR_AIGroup m_MiTarget;

 ResourceName FireTeamOf(ACR_Manager manager, string key)
 {
  foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction(key).m_aRequisitions)
   if (entry.m_sVehicle.IsEmpty() && (entry.m_sName.Contains("Fire Team") || entry.m_sName.Contains("Fire Group"))) return entry.m_sGroup;
  return "";
 }

 void TestGunship()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  vector uhSpot = "1300 0 2210".ToVector();
  vector miSpot = "1650 0 2830".ToVector();
  if (m_Ticks == 5)
  {
   manager.m_Bases.GetFaction("US").m_iRP = 1000;
   manager.m_Bases.GetFaction("USSR").m_iRP = 1000;
   m_UHTarget = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(FireTeamOf(manager, "USSR"), uhSpot));
   m_MiTarget = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(FireTeamOf(manager, "US"), miSpot));
   PrintFormat("[ACR] GUNSHIPTEST targets %1 %2", m_UHTarget, m_MiTarget);
  }
  if (!manager.m_Supports) return;
  if (m_Ticks == 10) PrintFormat("[ACR] GUNSHIPTEST Mi-8: %1", manager.m_Supports.Call("USSR", SupportIndex(manager, ACR_ESupportType.HELI_LOITER), miSpot, true));
  if (m_Ticks == 12) PrintFormat("[ACR] GUNSHIPTEST UH-1H: %1", manager.m_Supports.Call("US", SupportIndex(manager, ACR_ESupportType.HELI_LOITER), uhSpot, true));
  if (m_Ticks % 10 == 0 && m_Ticks >= 20) PrintFormat("[ACR] GUNSHIPTEST t=%1 UH-1H target alive %2 | Mi-8 target alive %3", m_Ticks, AliveIn(m_UHTarget), AliveIn(m_MiTarget));
  if (m_Ticks == 150) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrTacticsTest
 // The US AI commander (player leaves command) attacks the Soviet Outpost with
 // five fireteams: reserve, pincer staging, preparation fire, smoke, base of
 // fire, assault, falling back. The Soviet AI commander is off.
 void TestTactics()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  if (m_Ticks == 2)
  {
   foreach (ACR_Base held : ACR_Base.s_All) if (held.m_sName == "Crossroads") held.m_sOwner = "US";
  }
  if (m_Ticks == 5) pc.ACR_Request(ACR_ERequest.LEAVE_COMMAND);
  if (m_Ticks == 3)
  {
   manager.m_Bases.GetFaction("US").m_iRP = 300;
   ResourceName team = FireTeamOf(manager, "US");
   array<vector> spots = {"1350 0 2540".ToVector(), "1200 0 2520".ToVector(), "1400 0 2560".ToVector()};
   foreach (vector spot : spots) ACR_Manager.SpawnPrefab(team, spot);
  }
  if (m_Ticks % 15 == 0 && m_Ticks >= 15)
  {
   ACR_Base outpost;
   foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName == "Outpost") outpost = base;
   PrintFormat("[ACR] TACTICSTEST t=%1 outpost: %2 | US rp %3", m_Ticks, manager.m_Bases.Describe(outpost), manager.m_Bases.GetRP("US"));
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetFaction().GetFactionKey() != "US" || squad.IsMounted()) continue;
    float d = -1;
    if (squad.GetAnchor()) d = vector.DistanceXZ(squad.GetAnchor().GetOrigin(), outpost.GetOrigin());
    PrintFormat("[ACR] TACTICSTEST   %1 task=%2 %3 | %4 m from Outpost | enemies known %5", squad.GetName(), squad.m_iAITask, squad.Describe(), (int)d, squad.KnownEnemies(400));
   }
  }
  if (m_Ticks == 175) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrTransportTest
 // Alpha and Bravo board a crewed UH-1H. Alpha gets a queued LZ: nothing while
 // the helicopter hovers high, out once it sets down. Bravo is dropped with F6
 // from a 10 m hover (warning first, second press drops them).
 protected IEntity m_Huey;
 protected ChimeraCharacter m_HueyPilot;
 protected vector m_HoverAt;

 void Hover()
 {
  if (!m_Huey) return;
  vector transform[4];
  Math3D.AnglesToMatrix(Vector(90, 0, 0), transform);
  transform[3] = m_HoverAt;
  m_Huey.SetTransform(transform);
  Physics physics = m_Huey.GetPhysics();
  if (physics)
  {
   physics.EnableGravity(false);
   physics.SetVelocity(vector.Zero);
   physics.SetAngularVelocity(vector.Zero);
  }
 }

 int Aboard(SCR_AIGroup group)
 {
  if (!group) return 0;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  int aboard;
  foreach (AIAgent agent : agents) if (ACR_Transport.VehicleOf(agent.GetControlledEntity()) == m_Huey) aboard++;
  return aboard;
 }

 void TestTransport()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  vector lz = Ground("1600 0 2830");
  if (m_Ticks == 5) manager.SpawnUnit("Huey", "", "{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et", true, "", Ground("1420 0 2830"), Ground("1425 0 2820"));
  if (m_Ticks == 12)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() != "Huey") continue;
    array<IEntity> vehicles = {};
    squad.GetVehicles(vehicles);
    if (!vehicles.IsEmpty()) m_Huey = vehicles[0];
   }
   if (!m_Huey) { Print("[ACR] TRANSPORT FAIL: no helicopter"); return; }
   pc.ACR_Request(ACR_ERequest.ORDER_GET_IN, SCR_EntityHelper.EntityToRplId(m_Range.m_Group), m_Huey.GetOrigin());
  }
  if (m_Ticks == 13 && m_Huey) pc.ACR_Request(ACR_ERequest.ORDER_GET_IN, SCR_EntityHelper.EntityToRplId(m_Range.m_Bravo), m_Huey.GetOrigin());
  if (!m_Huey) return;
  if (m_Ticks >= 14 && m_Ticks <= 50 && m_Ticks % 6 == 0) PrintFormat("[ACR] TRANSPORT t=%1 aboard: Alpha %2 Bravo %3", m_Ticks, Aboard(m_Range.m_Group), Aboard(m_Range.m_Bravo));
  if (m_Ticks == 52) pc.ACR_Request(ACR_ERequest.ORDER_SET_LZ, SCR_EntityHelper.EntityToRplId(m_Range.m_Group), lz);
  if (m_Ticks == 54) m_HoverAt = lz + Vector(0, 25, 0);
  if (m_Ticks >= 54 && m_Ticks < 60) Hover();
  if (m_Ticks == 59) PrintFormat("[ACR] TRANSPORT hovering 25 m over the LZ: Alpha aboard %1 (expect all)", Aboard(m_Range.m_Group));
  if (m_Ticks == 60) m_HoverAt = lz + Vector(0, 1.5, 0);
  if (m_Ticks >= 60 && m_Ticks < 80) Hover();
  if (m_Ticks == 66) PrintFormat("[ACR] TRANSPORT set down at the LZ: Alpha aboard %1 (expect 0), Bravo aboard %2", Aboard(m_Range.m_Group), Aboard(m_Range.m_Bravo));
  if (m_Ticks == 67)
  {
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Huey.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> pilots = {};
   if (compartments) compartments.GetCompartmentsOfType(pilots, ECompartmentType.PILOT);
   foreach (BaseCompartmentSlot seat : pilots) if (seat.GetOccupant()) { m_HueyPilot = ChimeraCharacter.Cast(seat.GetOccupant()); break; }
   if (m_HueyPilot) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(m_HueyPilot));
  }
  if (m_Ticks == 70) m_HoverAt = lz + Vector(40, 10, 0);
  if (m_Ticks >= 70 && m_Ticks < 90) Hover();
  if (m_Ticks == 74) PrintFormat("[ACR] TRANSPORT possessed pilot=%1, Bravo aboard %2", pc.GetControlledEntity() == m_HueyPilot, Aboard(m_Range.m_Bravo));
  if (m_Ticks == 75 || m_Ticks == 76) pc.ACR_Request(ACR_ERequest.UNLOAD_HERE);
  if (m_Ticks == 82)
  {
   PrintFormat("[ACR] TRANSPORT after F6 x2 at 10 m: Bravo aboard %1 (expect 0), Bravo alive %2", Aboard(m_Range.m_Bravo), AliveIn(m_Range.m_Bravo));
   foreach (ACR_Squad squad : manager.m_Squads) PrintFormat("[ACR] TRANSPORT   %1: %2", squad.GetName(), squad.Describe());
  }
  if (m_Ticks == 88) PrintFormat("[ACR] TRANSPORT final: Bravo aboard %1, alive %2", Aboard(m_Range.m_Bravo), AliveIn(m_Range.m_Bravo));
  if (m_Ticks == 89) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrStallTest
 // Stuck Truck: its driver "freezes" (AI off) on a move order, and again after each
 // back-up: two tries, then the crew bails and walks on. Lone Truck: gunner killed,
 // made too heavy to move, enemies close: the crew bails at once and takes cover.
 protected ACR_Squad m_StuckSquad;
 protected ACR_Squad m_LoneSquad;
 protected IEntity m_StuckVehicle;
 protected IEntity m_LoneVehicle;
 protected bool m_bWasUnsticking;
 protected vector m_UnstickFrom;
 protected vector m_StallStart;
 protected vector m_LoneStart;

 ACR_Squad NamedSquad(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad.GetName() == name) return squad;
  return null;
 }

 void Freeze(IEntity vehicle)
 {
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (!driver) return;
  AIControlComponent ai = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();
 }

 string StallState(ACR_Squad squad, IEntity vehicle, vector from)
 {
  if (!squad) return "no squad";
  int alive;
  int aboard;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   alive++;
   if (ACR_Transport.VehicleOf(body)) aboard++;
  }
  float moved;
  if (vehicle) moved = vector.DistanceXZ(vehicle.GetOrigin(), from);
  float walked;
  IEntity anchor = squad.GetAnchor();
  if (anchor && vehicle) walked = vector.DistanceXZ(anchor.GetOrigin(), vehicle.GetOrigin());
  return string.Format("alive=%1 aboard=%2 order='%3' tries=%4 unsticking=%5 vehicleMoved=%6m squadFromVehicle=%7m combat=%8", alive, aboard, squad.m_Order, squad.UnstickTries(), squad.IsUnsticking(), (int)moved, (int)walked, squad.IsCombatUnit());
 }

 void TestStall()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  ResourceName gunTruck = "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et";
  if (m_Ticks == 5)
  {
   manager.SpawnUnit("Stuck Truck", "", gunTruck, true, "", Ground("1390 0 2795"), Ground("1395 0 2790"));
   manager.SpawnUnit("Lone Truck", "", gunTruck, true, "", Ground("1300 0 2530"), Ground("1305 0 2535"));
  }
  if (m_Ticks == 14)
  {
   m_StuckSquad = NamedSquad(manager, "Stuck Truck");
   m_LoneSquad = NamedSquad(manager, "Lone Truck");
   if (!m_StuckSquad || !m_LoneSquad) { Print("[ACR] STALL FAIL: trucks missing"); return; }
   m_StuckVehicle = ACR_Transport.CrewedVehicle(m_StuckSquad);
   m_LoneVehicle = ACR_Transport.CrewedVehicle(m_LoneSquad);
   if (!m_StuckVehicle || !m_LoneVehicle) { Print("[ACR] STALL FAIL: crews not aboard"); return; }
   array<AIAgent> agents = {};
   m_LoneSquad.GetAgents(agents);
   foreach (AIAgent agent : agents)
    if (ACR_Transport.SeatOf(agent.GetControlledEntity()) == ECompartmentType.TURRET) Kill(agent.GetControlledEntity());
   Physics heavy = m_LoneVehicle.GetPhysics();
   if (heavy) heavy.SetMass(heavy.GetMass() * 200);
   ResourceName team = FireTeamOf(manager, "USSR");
   if (!team.IsEmpty()) ACR_Manager.SpawnPrefab(team, Ground("1300 0 2400"));
   m_StuckSquad.MoveTo(Ground("1700 0 2800"));
   m_LoneSquad.MoveTo(Ground("1300 0 2250"));
   m_StallStart = m_StuckVehicle.GetOrigin();
   m_LoneStart = m_LoneVehicle.GetOrigin();
   Freeze(m_StuckVehicle);
   PrintFormat("[ACR] STALL start: enemy team=%1", team);
  }
  if (!m_StuckSquad || !m_LoneSquad || m_Ticks <= 14) return;
  bool unsticking = m_StuckSquad.IsUnsticking();
  if (unsticking && !m_bWasUnsticking)
  {
   m_UnstickFrom = m_StuckVehicle.GetOrigin();
   PrintFormat("[ACR] STALL unstick try %1 began at t=%2", m_StuckSquad.UnstickTries(), m_Ticks);
  }
  if (!unsticking && m_bWasUnsticking)
  {
   PrintFormat("[ACR] STALL unstick ended at t=%1: backed %2 m", m_Ticks, (int)vector.DistanceXZ(m_StuckVehicle.GetOrigin(), m_UnstickFrom));
   Freeze(m_StuckVehicle); // still "stuck"
  }
  m_bWasUnsticking = unsticking;
  if (m_Ticks % 5 == 0)
  {
   PrintFormat("[ACR] STALL t=%1 stuck: %2", m_Ticks, StallState(m_StuckSquad, m_StuckVehicle, m_StallStart));
   PrintFormat("[ACR] STALL t=%1 lone: %2", m_Ticks, StallState(m_LoneSquad, m_LoneVehicle, m_LoneStart));
  }
  if (m_Ticks == 110) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrRideTest
 // Bravo takes the "Ride" order to a point 560 m south; a crewed truck 70 m away
 // fetches it. The fetch limits are checked directly. Alpha walks east along the
 // runway with two men dragged 90 m behind, and must regroup before moving on.
 protected ACR_Squad m_RideSquad;
 protected ACR_Squad m_WalkSquad;
 protected ACR_Squad m_TaxiSquad;
 protected vector m_RideDest;

 string RideState()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  string state = "none";
  ACR_Ride ride;
  if (manager.m_Rides) ride = manager.m_Rides.RideOf(m_RideSquad);
  if (ride) state = typename.EnumToString(ACR_ERideState, ride.m_State);
  float toDest = -1;
  IEntity anchor = m_RideSquad.GetAnchor();
  if (anchor) toDest = vector.DistanceXZ(anchor.GetOrigin(), m_RideDest);
  float taxiMoved = -1;
  IEntity truck = ACR_Transport.CrewedVehicle(m_TaxiSquad);
  if (truck) taxiMoved = vector.DistanceXZ(truck.GetOrigin(), Ground("1400 0 2850"));
  return string.Format("ride=%1 bravo='%2' aboard=%3/%4 toDest=%5m | taxi='%6' truckFromStart=%7m", state, m_RideSquad.m_Order, Aboard(m_Range.m_Bravo), AliveIn(m_Range.m_Bravo), (int)toDest, m_TaxiSquad.m_Order, (int)taxiMoved);
 }

 void TestRide()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  if (m_Ticks == 5)
   manager.SpawnUnit("Taxi 1", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", true, "", Ground("1400 0 2850"), Ground("1405 0 2855"));
  if (m_Ticks == 14)
  {
   m_TaxiSquad = NamedSquad(manager, "Taxi 1");
   m_RideSquad = manager.FindSquadByGroup(m_Range.m_Bravo);
   m_WalkSquad = Squad();
   if (!m_TaxiSquad || !m_RideSquad || !m_WalkSquad || !manager.m_Rides) { Print("[ACR] RIDE FAIL: setup incomplete"); return; }
   float d;
   ACR_Squad nearOk = manager.m_Rides.FindTransport("US", Ground("1300 0 2250"), 2000, 3, d);
   ACR_Squad tooFar = manager.m_Rides.FindTransport("US", Ground("1300 0 2250"), 1000, 3, d);
   ACR_Squad veryFar = manager.m_Rides.FindTransport("US", Ground("400 0 2250"), 5000, 3, d);
   PrintFormat("[ACR] RIDE limits: 600 m away on a 2 km trip -> %1 (expect Taxi 1) | 600 m on a 1 km trip -> %2 (expect none) | 1 km away -> %3 (expect none) | free=%4 seats=%5",
    nearOk, tooFar, veryFar, manager.m_Rides.IsFreeTransport(m_TaxiSquad, "US"), ACR_Rides.FreeSeats(ACR_Transport.CrewedVehicle(m_TaxiSquad)));
   // Land routing: a few long lines across the island; each leg of a route must be dry.
   array<vector> ends = {"1320 0 2832".ToVector(), "1450 0 1150".ToVector(), "3200 0 2700".ToVector(), "2500 0 1300".ToVector(), "600 0 1500".ToVector()};
   for (int e = 0; e + 1 < ends.Count(); e++)
   {
    vector from = Ground(string.Format("%1 0 %2", ends[e][0], ends[e][2]));
    vector to = Ground(string.Format("%1 0 %2", ends[e + 1][0], ends[e + 1][2]));
    array<vector> legs = {};
    int started = System.GetTickCount();
    bool routed = ACR_Terrain.LandRoute(from, to, legs);
    int took = System.GetTickCount() - started;
    float worst;
    vector prev = from;
    foreach (vector leg : legs)
    {
     worst = Math.Max(worst, ACR_Terrain.WaterAlong(prev, leg));
     prev = leg;
    }
    PrintFormat("[ACR] ROUTETEST %1 -> %2: straight water %3 m, routed=%4 legs=%5 worst leg water %6 m, %7 ms", from, to, (int)ACR_Terrain.WaterAlong(from, to), routed, legs.Count(), (int)worst, took);
   }
   m_RideDest = Ground("1290 0 2300");
   pc.ACR_Request(ACR_ERequest.ORDER_RIDE, SCR_EntityHelper.EntityToRplId(m_Range.m_Bravo), m_RideDest);
   m_WalkSquad.MoveTo(Ground("1750 0 2832"));
  }
  if (!m_TaxiSquad || !m_RideSquad || !m_WalkSquad || m_Ticks < 15) return;
  if (m_Ticks == 22)
  {
   // Drag two of Alpha's men 90 m back along the route.
   IEntity leader = m_WalkSquad.GetLeader();
   array<AIAgent> agents = {};
   m_WalkSquad.GetAgents(agents);
   int moved;
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || body == leader || moved >= 2 || !leader) continue;
    body.SetOrigin(Ground(string.Format("%1 0 %2", leader.GetOrigin()[0] - 90, leader.GetOrigin()[2] + moved * 3)));
    body.Update();
    moved++;
   }
   PrintFormat("[ACR] RIDE cohesion: dragged %1 back, stragglers(60)=%2", moved, m_WalkSquad.Stragglers(60));
  }
  if (m_Ticks % 5 == 0)
  {
   PrintFormat("[ACR] RIDE t=%1 %2", m_Ticks, RideState());
   PrintFormat("[ACR] RIDE t=%1 alpha='%2' stragglers(60)=%3 stragglers(25)=%4", m_Ticks, m_WalkSquad.m_Order, m_WalkSquad.Stragglers(60), m_WalkSquad.Stragglers(25));
  }
  if (m_Ticks == 170) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrLogisticsTest
 // Alpha's spare magazines are taken away, then it walks to an ammo truck and
 // must be topped up (the truck's stock goes down). A gun truck is damaged and
 // drained of fuel next to a repair truck and a tanker, and must be fixed up.
 protected ACR_Squad m_GunCrew;
 protected IEntity m_AmmoTruck;

 void TestLogistics()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  if (m_Ticks == 5)
  {
   manager.SpawnUnit("Ammo Truck", "", "{92264FF932676C13}Prefabs/Vehicles/Wheeled/M923A1/M923A1_ammo.et", true, "", Ground("1400 0 2832"), Ground("1405 0 2836"));
   manager.SpawnUnit("Repair Truck", "", "{A042ACE5C2B13206}Prefabs/Vehicles/Wheeled/M923A1/M923A1_repair.et", true, "", Ground("1450 0 2800"), Ground("1455 0 2805"));
   manager.SpawnUnit("Fuel Truck", "", "{2BE1F8B9299B67C1}Prefabs/Vehicles/Wheeled/M923A1/M923A1_tanker.et", true, "", Ground("1450 0 2815"), Ground("1455 0 2820"));
   manager.SpawnUnit("Gun Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", Ground("1465 0 2807"), Ground("1470 0 2810"));
  }
  ACR_Squad alpha = Squad();
  if (m_Ticks == 14)
  {
   m_GunCrew = NamedSquad(manager, "Gun Truck");
   ACR_Squad ammoCrew = NamedSquad(manager, "Ammo Truck");
   if (ammoCrew) m_AmmoTruck = ACR_Transport.CrewedVehicle(ammoCrew);
   PrintFormat("[ACR] LOGI service flags: ammo=%1 repair=%2 fuel=%3", ACR_Logistics.ServiceOf(m_AmmoTruck), ACR_Logistics.ServiceOf(ACR_Transport.CrewedVehicle(NamedSquad(manager, "Repair Truck"))), ACR_Logistics.ServiceOf(ACR_Transport.CrewedVehicle(NamedSquad(manager, "Fuel Truck"))));
   // Take Alpha's spare magazines.
   int taken;
   array<AIAgent> agents = {};
   alpha.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(soldier.FindComponent(InventoryStorageManagerComponent));
    if (!inventory) continue;
    array<IEntity> items = {};
    inventory.GetItems(items);
    foreach (IEntity item : items)
    {
     if (!item.FindComponent(BaseMagazineComponent)) continue;
     if (item.GetParent() && item.GetParent().FindComponent(BaseWeaponComponent)) continue; // loaded
     if (inventory.TryDeleteItem(item)) taken++;
    }
   }
   // Damage and drain the gun truck.
   IEntity gun = ACR_Transport.CrewedVehicle(m_GunCrew);
   if (gun)
   {
    SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(gun.FindComponent(SCR_DamageManagerComponent));
    if (damage) damage.SetHealthScaled(0.35);
    SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(gun.FindComponent(SCR_FuelManagerComponent));
    if (fuel) fuel.SetTotalFuelPercentage(0.1);
    array<BaseMagazineComponent> belts = {};
    ACR_Logistics.TurretMagazines(gun, belts);
    foreach (BaseMagazineComponent belt : belts) belt.SetAmmoCount(0);
   }
   PrintFormat("[ACR] LOGI drained: %1 magazines taken from Alpha; gun truck health %2 fuel %3 gun ammo %4", taken, ACR_Logistics.Health(gun), ACR_Logistics.Fuel(gun), ACR_Logistics.VehicleAmmo(gun));
  }
  if (m_Ticks == 22 && alpha && m_AmmoTruck) alpha.MoveTo(m_AmmoTruck.GetOrigin() + Vector(-8, 0, 0));
  if (m_Ticks == 30)
  {
   // The gun truck drives up to the ammo truck too, for its M2 belt.
   IEntity gunVehicle = ACR_Transport.CrewedVehicle(m_GunCrew);
   PrintFormat("[ACR] LOGI gun truck after repair/fuel stop: health %1 fuel %2", ACR_Logistics.Health(gunVehicle), ACR_Logistics.Fuel(gunVehicle));
   if (m_GunCrew && m_AmmoTruck) m_GunCrew.MoveTo(m_AmmoTruck.GetOrigin() + Vector(12, 0, 8));
  }
  if (m_Ticks % 5 == 0 && m_Ticks >= 15 && alpha)
  {
   IEntity gunNow = ACR_Transport.CrewedVehicle(m_GunCrew);
   PrintFormat("[ACR] LOGI t=%1 alpha: %2 | gun truck: health %3 fuel %4 gunAmmo %5 | ammo truck stock %6%%", m_Ticks, alpha.Describe(), ACR_Logistics.Health(gunNow), ACR_Logistics.Fuel(gunNow), ACR_Logistics.VehicleAmmo(gunNow), manager.m_Logistics.StockPercent(m_AmmoTruck));
  }
  if (m_Ticks == 100) Print("[ACR] TEST COMPLETE");
 }
}
