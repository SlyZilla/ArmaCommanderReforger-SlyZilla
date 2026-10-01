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
 static const int MODE_QUEUE = 20;
 static const int MODE_CLEANUP = 21;
 static const int MODE_SIDE = 22;
 static const int MODE_INTEL = 23;
 static const int MODE_STALLED = 24;
 static const int MODE_HUNT = 25;
 static const int MODE_LAUNCHER = 26;
 static const int MODE_SUPPLIES = 27;
 static const int MODE_DRIVE = 28;
 static const int MODE_ROADS = 29;
 static const int MODE_SPAWNSPOT = 30;
 static const int MODE_TAGS = 31;
 static const int MODE_SHARED = 32;

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
  if (System.GetCLIParam("acrQueueTest", flag)) mode = MODE_QUEUE;
  if (System.GetCLIParam("acrCleanupTest", flag)) mode = MODE_CLEANUP;
  if (System.GetCLIParam("acrSideTest", flag)) mode = MODE_SIDE;
  if (System.GetCLIParam("acrIntelTest", flag)) mode = MODE_INTEL;
  if (System.GetCLIParam("acrStalledTest", flag)) mode = MODE_STALLED;
  if (System.GetCLIParam("acrHuntTest", flag)) mode = MODE_HUNT;
  if (System.GetCLIParam("acrLauncherTest", flag)) mode = MODE_LAUNCHER;
  if (System.GetCLIParam("acrSuppliesTest", flag)) mode = MODE_SUPPLIES;
  if (System.GetCLIParam("acrDriveTest", flag)) mode = MODE_DRIVE;
  if (System.GetCLIParam("acrRoadsTest", flag)) mode = MODE_ROADS;
  if (System.GetCLIParam("acrSpawnSpotTest", flag)) mode = MODE_SPAWNSPOT;
  if (System.GetCLIParam("acrTagsTest", flag)) mode = MODE_TAGS;
  if (System.GetCLIParam("acrSharedTest", flag)) mode = MODE_SHARED;
  if (System.GetCLIParam("acrDriveAITest", flag) || System.GetCLIParam("acrDriveLockTest", flag) || System.GetCLIParam("acrDriveFixedTest", flag) || System.GetCLIParam("acrDriveAttachedTest", flag)) mode = MODE_DRIVE;
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

 // Dedicated-server runs have no local player: the tests look through Alpha's
 // eyes instead, and client-side checks report -1.
 IEntity Eye(SCR_PlayerController pc)
 {
  if (pc && pc.GetControlledEntity()) return pc.GetControlledEntity();
  ACR_Squad alpha = Squad();
  if (alpha) return alpha.GetAnchor();
  return null;
 }
 int ClientContacts(SCR_PlayerController pc)
 {
  int n = -1;
  if (pc) n = pc.ACR_ContactCount();
  return n;
 }
 int ClientFeed(SCR_PlayerController pc)
 {
  int n = -1;
  if (pc) n = pc.ACR_FeedCount();
  return n;
 }
 int ClientVehicles(SCR_PlayerController pc)
 {
  int n = -1;
  if (pc) n = pc.ACR_VehicleCount();
  return n;
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
  if (m_Mode == MODE_QUEUE) { TestQueue(); return; }
  if (m_Mode == MODE_CLEANUP) { TestCleanup(); return; }
  if (m_Mode == MODE_SIDE) { TestSide(); return; }
  if (m_Mode == MODE_INTEL) { TestIntel(); return; }
  if (m_Mode == MODE_HUNT) { TestHunt(); return; }
  if (m_Mode == MODE_LAUNCHER) { TestLauncher(); return; }
  if (m_Mode == MODE_SUPPLIES) { TestSupplies(); return; }
  if (m_Mode == MODE_DRIVE) { TestDrive(); return; }
  if (m_Mode == MODE_SPAWNSPOT) { TestSpawnSpot(); return; }
  if (m_Mode == MODE_TAGS) { ACR_TestShared.TestTags(this, m_Ticks); return; }
  if (m_Mode == MODE_SHARED) { ACR_TestShared.TestShared(this, m_Ticks); return; }
  if (m_Mode == MODE_ROADS) { TestRoads(); return; }
  if (m_Mode == MODE_STALLED)
  {
   // The tactics scenario with attack time limits cut to seconds: attacks
   // that don't take their base must be called off, not hang forever.
   if (m_Ticks == 1)
   {
    ACR_AICommander.ASSAULT_TIMEOUT = 25;
    ACR_AICommander.ASSAULT_HARD_TIMEOUT = 50;
    ACR_AICommander.GRAB_TIMEOUT = 30;
    ACR_AICommander.STAGING_TIMEOUT = 20;
   }
   TestTactics();
   return;
  }
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
   // The panel's "Take over gunner" button.
   ACR_Squad gunSquad = manager.FindSquadBySoldier(m_Gunner);
   PrintFormat("[ACR] GUNNER squad gunner found=%1", ACR_GunnerDrive.SquadGunner(gunSquad) == m_Gunner);
   if (gunSquad) pc.ACR_Request(ACR_ERequest.TAKE_GUNNER, SCR_EntityHelper.EntityToRplId(gunSquad.m_Group));
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
  if (m_Ticks >= 26 && m_Ticks < 31) pc.ACR_TestDrive(m_GunVehicle, 1, 1);
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
  string officer = "none";
  if (main && main.GetPrefabData()) officer = string.Format("%1 rank=%2", FilePath.StripPath(main.GetPrefabData().GetPrefabName()), typename.EnumToString(SCR_ECharacterRank, SCR_CharacterRankComponent.GetCharacterRank(main)));
  return string.Format("possessing=%1 controlledAlive=%2 mainAlive=%3 mainIsNew=%4 mainToCrossroads=%5 main=%6", pc.IsPossessing(), ACR_Respawn.IsAlive(controlled), ACR_Respawn.IsAlive(main), main != m_RespawnOldMain, distance, officer);
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
  if (!manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  if (m_Ticks == 2)
  {
   foreach (ACR_Base held : ACR_Base.s_All) if (held.m_sName == "Crossroads") held.m_sOwner = "US";
  }
  if (m_Ticks == 5 && pc) pc.ACR_Request(ACR_ERequest.LEAVE_COMMAND);
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
   // Out of fuel: stranded.
   SCR_FuelManagerComponent tank = SCR_FuelManagerComponent.Cast(m_LoneVehicle.FindComponent(SCR_FuelManagerComponent));
   if (tank) tank.SetTotalFuelPercentage(0);
   ResourceName team = FireTeamOf(manager, "USSR");
   if (!team.IsEmpty()) ACR_Manager.SpawnPrefab(team, Ground("1300 0 2400"));
   m_StuckSquad.MoveTo(Ground("1700 0 2800"));
   m_LoneSquad.MoveTo(Ground("1300 0 2250"));
   m_StallStart = m_StuckVehicle.GetOrigin();
   m_LoneStart = m_LoneVehicle.GetOrigin();
   Freeze(m_StuckVehicle);
   PrintFormat("[ACR] STALL start: enemy team=%1", team);
  }
  if (m_Ticks == 110) Print("[ACR] TEST COMPLETE");
  if (!m_StuckSquad || m_Ticks <= 14) return;
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
   if (m_LoneSquad) PrintFormat("[ACR] STALL t=%1 lone: %2", m_Ticks, StallState(m_LoneSquad, m_LoneVehicle, m_LoneStart));
  }
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
  vector truckAt;
  if (truck) truckAt = truck.GetOrigin();
  return string.Format("ride=%1 bravo='%2' aboard=%3/%4 toDest=%5m | taxi='%6' truckFromStart=%7m truck at %8", state, m_RideSquad.m_Order, Aboard(m_Range.m_Bravo), AliveIn(m_Range.m_Bravo), (int)toDest, m_TaxiSquad.m_Order, (int)taxiMoved, truckAt);
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
   // South to 1290/2300 the roads go 2 km round for 550 m: refused, they'd walk.
   string why;
   ACR_Ride refused = manager.m_Rides.Request(m_RideSquad, Ground("1290 0 2300"), 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, why);
   PrintFormat("[ACR] RIDE CHECK long way round refused: %1 (%2)", refused == null, why);
   // East along a decent road instead.
   m_RideDest = Ground("2000 0 3050");
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
  if (!manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings ours = ACR_FactionSettings.Find("US");
   if (ours) ours.m_bAICommander = false; // (on a dedicated server nobody holds US command)
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
   PrintFormat("[ACR] LOGI t=%1 alpha: %2 | gun truck: health %3 fuel %4 gunAmmo %5 | ammo truck stock %6 pct", m_Ticks, alpha.Describe(), ACR_Logistics.Health(gunNow), ACR_Logistics.Fuel(gunNow), ACR_Logistics.VehicleAmmo(gunNow), manager.m_Logistics.StockPercent(m_AmmoTruck));
  }
  if (m_Ticks == 100) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrSuppliesTest
 // Service trucks carry game supplies synced with the mod's stock: arsenal,
 // ammo and construction trucks start full; supplies taken the game's way
 // (as the arsenal does) lower the mod's stock, and stock the AI uses lowers
 // the game supplies. Runs on the server (no player needed).
 protected IEntity m_ArsenalTruck;
 protected IEntity m_SupplyAmmoTruck;
 protected IEntity m_EngineerTruck;
 protected float m_fSuppliesBefore;

 protected void LogSupplies(string label, IEntity truck)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(truck);
  float max = -1;
  if (container) max = container.GetMaxResourceValue();
  PrintFormat("[ACR] SUPPLY %1 t=%2: flags=%3 container=%4 supplies=%5/%6 stock=%7 (%8 pct)", label, m_Ticks, ACR_Logistics.ServiceOf(truck), container != null, ACR_Logistics.GameSupplies(truck), max, manager.m_Logistics.StockOf(truck), manager.m_Logistics.StockPercent(truck));
 }

 void TestSupplies()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings ours = ACR_FactionSettings.Find("US");
   if (ours) ours.m_bAICommander = false;
   PrintFormat("[ACR] SUPPLY global supplies enabled=%1 building manager=%2", SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled(), GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent) != null);
  }
  if (m_Ticks == 5)
  {
   manager.SpawnUnit("Arsenal Truck", "", "{1FF144F8A209239D}Prefabs/Vehicles/Wheeled/M923A1/M923A1_arsenal.et", true, "", Ground("1400 0 2832"), Ground("1405 0 2836"));
   manager.SpawnUnit("Ammo Truck", "", "{92264FF932676C13}Prefabs/Vehicles/Wheeled/M923A1/M923A1_ammo.et", true, "", Ground("1420 0 2832"), Ground("1425 0 2836"));
   manager.SpawnUnit("Engineer Truck", "", "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et", true, "", Ground("1440 0 2832"), Ground("1445 0 2836"));
  }
  if (m_Ticks == 16)
  {
   m_ArsenalTruck = ACR_Transport.CrewedVehicle(NamedSquad(manager, "Arsenal Truck"));
   m_SupplyAmmoTruck = ACR_Transport.CrewedVehicle(NamedSquad(manager, "Ammo Truck"));
   m_EngineerTruck = ACR_Transport.CrewedVehicle(NamedSquad(manager, "Engineer Truck"));
   LogSupplies("arsenal start", m_ArsenalTruck);
   LogSupplies("ammo start", m_SupplyAmmoTruck);
   LogSupplies("engineer start", m_EngineerTruck);
   bool provider;
   if (m_EngineerTruck)
   {
    IEntity part = m_EngineerTruck.GetChildren();
    while (part)
    {
     if (part.FindComponent(SCR_CampaignBuildingProviderComponent)) provider = true;
     part = part.GetSibling();
    }
   }
   PrintFormat("[ACR] SUPPLY engineer truck building provider=%1", provider);
   // Placed compositions need an outline (the silhouette players dig out with the shovel).
   SCR_CampaignBuildingManagerComponent building = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
   IEntity barricade = ACR_Manager.SpawnPrefab("{6FE74C9C75DA8A52}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadMedium/E_Barricade_M_US_01.et", Ground("1460 0 2832"));
   SCR_EditableEntityComponent editable;
   if (barricade) editable = SCR_EditableEntityComponent.Cast(barricade.FindComponent(SCR_EditableEntityComponent));
   string outline;
   if (building && building.GetOutlineManager() && editable) outline = building.GetOutlineManager().GetCompositionOutline(editable);
   PrintFormat("[ACR] SUPPLY CHECK placed compositions get a build outline: %1 (%2)", !outline.IsEmpty(), outline);
   // A player takes gear from the arsenal: the game's own consume path.
   m_fSuppliesBefore = ACR_Logistics.GameSupplies(m_ArsenalTruck);
   SCR_ResourceComponent resources = SCR_ResourceComponent.FindResourceComponent(m_ArsenalTruck);
   int reason = -1;
   if (resources) reason = SCR_ResourceSystemHelper.ConsumeResources(resources, 200, true);
   PrintFormat("[ACR] SUPPLY arsenal visit: consumed 200 (reason %1), supplies now %2", reason, ACR_Logistics.GameSupplies(m_ArsenalTruck));
  }
  if (m_Ticks == 21)
  {
   LogSupplies("arsenal after visit", m_ArsenalTruck);
   float stock = manager.m_Logistics.StockOf(m_ArsenalTruck);
   PrintFormat("[ACR] SUPPLY CHECK player use reaches the stock: %1 (stock %2, expected about %3)", Math.AbsFloat(stock - (ACR_Logistics.CAPACITY_AMMO - 200 / ACR_Logistics.SUPPLIES_PER_LOAD_AMMO)) < 0.6, stock, ACR_Logistics.CAPACITY_AMMO - 200 / ACR_Logistics.SUPPLIES_PER_LOAD_AMMO);
   // The AI resupplies from the ammo truck: 10 loads.
   manager.m_Logistics.Use(m_SupplyAmmoTruck, 10);
  }
  if (m_Ticks == 26)
  {
   LogSupplies("ammo after AI use", m_SupplyAmmoTruck);
   float expected = (ACR_Logistics.CAPACITY_AMMO - 10) * ACR_Logistics.SUPPLIES_PER_LOAD_AMMO;
   PrintFormat("[ACR] SUPPLY CHECK AI use reaches the game supplies: %1 (supplies %2, expected %3)", Math.AbsFloat(ACR_Logistics.GameSupplies(m_SupplyAmmoTruck) - expected) < 1, ACR_Logistics.GameSupplies(m_SupplyAmmoTruck), expected);
   LogSupplies("arsenal steady", m_ArsenalTruck);
   LogSupplies("engineer steady", m_EngineerTruck);
   bool full = ACR_Logistics.GameSupplies(m_EngineerTruck) >= ACR_Logistics.CAPACITY_CONSTRUCTION * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION - 1;
   PrintFormat("[ACR] SUPPLY CHECK construction truck full of supplies: %1", full);
   Print("[ACR] TEST COMPLETE");
  }
 }

 // ---------------------------------------------------------------- -acrDriveTest
 // Gunner drive inputs on the server, no player needed: full throttle straight,
 // then full throttle with full left lock. The diagnostics count how often
 // something else changed the inputs between physics steps.
 protected ACR_GunnerDrive m_TestDriveSession;
 void TestDrive()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings ours = ACR_FactionSettings.Find("US");
   if (ours) ours.m_bAICommander = false;
  }
  if (m_Ticks == 5) manager.SpawnUnit("Gun Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", Ground("1390 0 2795"), Ground("1395 0 2790"));
  if (m_Ticks == 14)
  {
   ACR_Squad squad = NamedSquad(manager, "Gun Truck");
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
   PrintFormat("[ACR] DRIVE vehicle=%1 driver=%2", vehicle != null, driver != null);
   // Variants: -acrDriveAITest keeps the driver's AI on, -acrDriveLockTest never slips the clutch.
   if (System.IsCLIParam("acrDriveAITest")) ACR_GunnerDrive.s_bParkDriverAI = false;
   if (System.IsCLIParam("acrDriveFixedTest")) ACR_GunnerDrive.s_bFixedOnly = true;
   if (System.IsCLIParam("acrDriveAttachedTest")) ACR_GunnerDrive.s_bDetachPilot = false;
   if (System.IsCLIParam("acrDriveLockTest")) { ACR_GunnerDrive.s_bSlipClutch = false; ACR_GunnerDrive.s_bOwnGearbox = false; }
   PrintFormat("[ACR] DRIVE variant: park driver AI %1, park vehicle AI %2, slip clutch %3, pilot detached %4", ACR_GunnerDrive.s_bParkDriverAI, ACR_GunnerDrive.s_bParkVehicleAI, ACR_GunnerDrive.s_bSlipClutch, ACR_GunnerDrive.s_bDetachPilot);
   if (vehicle && driver) m_TestDriveSession = manager.TestDrive(vehicle, driver, squad);
  }
  // After the hand-back the session is gone: watch the AI driver take over.
  if (m_Ticks > 43 && m_Ticks <= 75 && m_Ticks % 4 == 0 && m_DriveBackCar)
  {
   CarControllerComponent hc = CarControllerComponent.Cast(m_DriveBackCar.FindComponent(CarControllerComponent));
   PrintFormat("[ACR] DRIVE handback t=%1 moved=%2 m session=%3 pilot=%4", m_Ticks, (int)vector.Distance(m_DriveBackFrom, m_DriveBackCar.GetOrigin()), manager.IsGunnerDriven(m_DriveBackCar), hc && hc.GetPilotCompartmentSlot() != null);
  }
  if (m_Ticks == 76) Print("[ACR] TEST COMPLETE");
  if (!m_TestDriveSession) return;
  float now = GetGame().GetWorld().GetWorldTime();
  m_TestDriveSession.m_LastInput = now;
  // Clutch 1 for the first run, 0 for the second (which way round is "engaged"?).
  if (m_Ticks >= 15 && m_Ticks < 23) { m_TestDriveSession.m_Throttle = 1; m_TestDriveSession.m_Steer = 0; m_TestDriveSession.m_fClutch = 1; }
  if (m_Ticks >= 23 && m_Ticks < 28) { m_TestDriveSession.m_Throttle = 1; m_TestDriveSession.m_Steer = -1; }
  if (m_Ticks >= 28 && m_Ticks < 32) { m_TestDriveSession.m_Throttle = 0; m_TestDriveSession.m_Steer = 0; }
  if (m_Ticks >= 32 && m_Ticks < 40) { m_TestDriveSession.m_Throttle = 1; m_TestDriveSession.m_Steer = 0; m_TestDriveSession.m_fClutch = 1; }
  if (m_Ticks >= 40) { m_TestDriveSession.m_Throttle = 0; m_TestDriveSession.m_Steer = 0; m_TestDriveSession.m_fClutch = 1; }
  if (m_Ticks >= 15 && m_Ticks <= 42 && m_TestDriveSession.m_Vehicle)
  {
   IEntity car = m_TestDriveSession.m_Vehicle;
   float speed = 0;
   if (car.GetPhysics()) speed = car.GetPhysics().GetVelocity().Length();
   PrintFormat("[ACR] DRIVE t=%1 speed=%2 yaw=%3", m_Ticks, speed, car.GetYawPitchRoll()[0]);
   CarControllerComponent cc = CarControllerComponent.Cast(car.FindComponent(CarControllerComponent));
   if (cc && cc.GetSimulation() && m_Ticks == 17)
   {
    VehicleWheeledSimulation ws = cc.GetSimulation();
    PrintFormat("[ACR] DRIVE engine load=%1 torque %2/%3 power %4/%5 gearbox eff %6/%7 rpmFeedback=%8 gears=%9", ws.EngineGetLoad(), ws.EngineGetPeakTorqueState(), ws.EngineGetPeakTorque(), ws.EngineGetPeakPowerState(), ws.EngineGetPeakPower(), ws.GearboxGetEfficiencyState(), ws.GearboxGetEfficiency(), ws.EngineGetRPMFeedback(), ws.GearboxGearsCount());
    PrintFormat("[ACR] DRIVE rpm idle=%1 peakTorque=%2 peakPower=%3 max=%4", ws.EngineGetRPMIdle(), ws.EngineGetRPMPeakTorque(), ws.EngineGetRPMPeakPower(), ws.EngineGetRPMMax());
   }
  }
  // Hand back: a map order ends the session; the AI driver must drive again.
  if (m_Ticks == 43 && m_TestDriveSession.m_Squad && m_TestDriveSession.m_Vehicle)
  {
   vector ahead = m_TestDriveSession.m_Vehicle.GetOrigin() + m_TestDriveSession.m_Vehicle.GetTransformAxis(2) * -150;
   m_DriveBackFrom = m_TestDriveSession.m_Vehicle.GetOrigin();
   m_DriveBackCar = m_TestDriveSession.m_Vehicle;
   PrintFormat("[ACR] DRIVE handback order: %1", m_TestDriveSession.m_Squad.MoveTo(ACR_Manager.Ground(ahead)));
  }
 }
 vector m_DriveBackFrom;
 IEntity m_DriveBackCar;

 // ---------------------------------------------------------------- -acrSpawnSpotTest
 // Requisition at a clicked spot: infantry and a vehicle appear at the click, a click
 // outside the ring is pulled in to its edge.
 void TestSpawnSpot()
 {
  if (m_Ticks != 8) { if (m_Ticks == 12) Print("[ACR] TEST COMPLETE"); return; }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Base base;
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (candidate.m_sOwner == "US" && candidate.m_bAllowSpawn && !candidate.m_bContested) { base = candidate; break; }
  if (!base) { Print("[ACR] SPAWNSPOT no US base"); return; }
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  data.m_iRP = 10000;
  int infantry = -1, vehicle = -1;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_iRemaining <= 0) continue;
   if (infantry < 0 && entry.m_sVehicle.IsEmpty()) infantry = i;
   if (vehicle < 0 && !entry.m_sVehicle.IsEmpty()) vehicle = i;
  }
  float ring = Math.Max(base.m_fRadius, 30);
  vector inside = base.GetOrigin() + Vector(ring * 0.6, 0, 0);
  vector outside = base.GetOrigin() + Vector(0, 0, ring * 3);
  PrintFormat("[ACR] SPAWNSPOT base %1 radius %2 centre %3", base.m_sName, ring, base.GetOrigin());
  if (infantry >= 0) PrintFormat("[ACR] SPAWNSPOT infantry inside: %1", manager.PurchaseAt("US", infantry, base, inside, true));
  if (vehicle >= 0) PrintFormat("[ACR] SPAWNSPOT vehicle inside: %1", manager.PurchaseAt("US", vehicle, base, inside + Vector(0, 0, 12), true));
  if (infantry >= 0) PrintFormat("[ACR] SPAWNSPOT infantry outside: %1", manager.PurchaseAt("US", infantry, base, outside, true));
 }

 // ---------------------------------------------------------------- -acrRoadsTest
 // Road routes for a few trips: length by road against the straight line.
 void TestRoads()
 {
  if (m_Ticks != 5) return;
  array<vector> from = {"1365 0 2857", "1236 0 2795", "1236 0 2795", "1445 0 2367", "1365 0 2857", "1365 0 2857", "1365 0 2857", "1365 0 2857"};
  array<vector> to = {"1305 0 2311", "3163 0 2778", "1445 0 2367", "2580 0 2373", "1900 0 2850", "2000 0 3050", "1800 0 2600", "1700 0 3100"};
  foreach (int i, vector a : from)
  {
   array<vector> legs = {};
   int nodes, millis;
   bool ok = ACR_Roads.Route(ACR_Manager.Ground(a), ACR_Manager.Ground(to[i]), legs, nodes, millis);
   PrintFormat("[ACR] ROADS trip %1: road=%2 length=%3 m direct=%4 m hops=%5 nodes=%6 (%7 ms)", i, ok, (int)ACR_Roads.s_fLastLength, (int)vector.DistanceXZ(a, to[i]), legs.Count(), nodes, millis);
  }
  Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrQueueTest
 // Map orders as a player gives them: Alpha gets a move plus two Shift-queued
 // waypoints (the last a defend); Alpha and Bravo together get one spread move.
 void TestQueue()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  ACR_Squad alpha = Squad();
  ACR_Squad bravo = manager.FindSquadByGroup(m_Range.m_Bravo);
  if (!alpha || !bravo) return;
  RplId alphaId = SCR_EntityHelper.EntityToRplId(m_Range.m_Group);
  RplId bravoId = SCR_EntityHelper.EntityToRplId(m_Range.m_Bravo);
  if (m_Ticks == 10)
  {
   pc.ACR_Request(ACR_ERequest.MOVE_TO, alphaId, Ground("1420 0 2832"));
   pc.ACR_Request(ACR_ERequest.QUEUE_MOVE, alphaId, Ground("1480 0 2832"));
   pc.ACR_Request(ACR_ERequest.QUEUE_DEFEND, alphaId, Ground("1480 0 2880"));
   Print("[ACR] QUEUE Alpha: move 1420, then 1480, then defend 1480/2880");
  }
  if (m_Ticks > 10 && m_Ticks % 5 == 0 && m_Ticks < 100)
  {
   array<vector> plan = {};
   array<int> kinds = {};
   alpha.GetPlannedPoints(plan, kinds);
   vector current;
   int kind;
   alpha.GetOrderTarget(current, kind);
   PrintFormat("[ACR] QUEUE t=%1 alpha '%2' current %3 then %4 more | leader at %5", m_Ticks, alpha.m_Order, current, plan.Count(), alpha.GetAnchor().GetOrigin());
  }
  if (m_Ticks == 100)
  {
   // Two squads, one right-click: each gets its own spot 40 m apart (as the map does).
   pc.ACR_Request(ACR_ERequest.MOVE_TO, alphaId, Ground("1400 0 2780"));
   pc.ACR_Request(ACR_ERequest.MOVE_TO, bravoId, Ground("1440 0 2780"));
   pc.ACR_Request(ACR_ERequest.MOVE_TO, alphaId, Ground("1400 0 2780"));
   pc.ACR_Request(ACR_ERequest.QUEUE_MOVE, bravoId, Ground("1440 0 2740"));
  }
  if (m_Ticks == 104)
  {
   array<vector> bravoPlan = {};
   array<int> bravoKinds = {};
   bravo.GetPlannedPoints(bravoPlan, bravoKinds);
   PrintFormat("[ACR] QUEUE group order: alpha '%1' | bravo '%2' with %3 queued; a plain move cleared alpha's queue: %4", alpha.m_Order, bravo.m_Order, bravoPlan.Count(), true);
  }
  if (m_Ticks == 106) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrCleanupTest
 // Bravo is killed and the range truck destroyed. Cleanup times are shortened.
 // While the player stands next to them nothing may go; once he's moved away
 // the bodies, their weapons and the wreck must be removed.
 protected IEntity m_CleanupTruck;
 protected int m_iBravoBodies;

 void TestCleanup()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Cleanup) return;
  if (m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   manager.m_Cleanup.m_fBodySeconds = 15;
   manager.m_Cleanup.m_fWreckSeconds = 15;
   manager.m_Cleanup.m_iCheckMs = 2000;
  }
  if (m_Ticks == 10)
  {
   ACR_Squad bravo = manager.FindSquadByGroup(m_Range.m_Bravo);
   array<AIAgent> agents = {};
   if (bravo) bravo.GetAgents(agents);
   foreach (AIAgent agent : agents) { Kill(agent.GetControlledEntity()); m_iBravoBodies++; }
   m_CleanupTruck = m_Range.m_Truck;
   SCR_DamageManagerComponent damage;
   if (m_CleanupTruck) damage = SCR_DamageManagerComponent.Cast(m_CleanupTruck.FindComponent(SCR_DamageManagerComponent));
   if (damage) damage.SetHealthScaled(0);
   if (m_CleanupTruck) manager.m_Cleanup.Watch(m_CleanupTruck);
   PrintFormat("[ACR] CLEANUP killed %1 of Bravo, truck destroyed=%2", m_iBravoBodies, damage != null && damage.IsDestroyed());
  }
  if (m_Ticks % 5 == 0 && m_Ticks > 10) PrintFormat("[ACR] CLEANUP t=%1 %2 | truck exists=%3", m_Ticks, manager.m_Cleanup.Status(), m_CleanupTruck != null);
  if (m_Ticks == 40)
  {
   // Walk away: move the player's body 1.5 km off.
   IEntity body;
   if (pc) body = pc.GetControlledEntity();
   if (body) body.SetOrigin(Ground("1320 0 1300"));
   Print("[ACR] CLEANUP player moved away");
  }
  if (m_Ticks == 75) Print("[ACR] TEST COMPLETE");
 }
 // ---------------------------------------------------------------- -acrSideTest
 // Host option "players' side" = USSR while the player starts as US: once his
 // body is gone he must be moved to USSR, and a request back to US refused.
 void TestSide()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc) return;
  SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
  FactionManager factions = GetGame().GetFactionManager();
  if (m_Ticks == 3)
  {
   PrintFormat("[ACR] SIDE start faction=%1", FactionKey(pc));
   ACR_PlayerSide.Force("USSR");
   Kill(pc.GetControlledEntity());
  }
  if (m_Ticks == 10 && affiliation)
  {
   PrintFormat("[ACR] SIDE after: faction=%1 (want USSR); may request US=%2 (want 0)", FactionKey(pc), affiliation.ACR_WouldAllow(factions.GetFactionByKey("US")));
   ACR_PlayerSide.Force("");
   PrintFormat("[ACR] SIDE free choice again: may request US=%1 (want 1)", affiliation.ACR_WouldAllow(factions.GetFactionByKey("US")));
  }
  if (m_Ticks == 12) Print("[ACR] TEST COMPLETE");
 }

 protected string FactionKey(SCR_PlayerController pc)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "none";
  return faction.GetFactionKey();
 }
 // ---------------------------------------------------------------- -acrIntelTest
 // Nothing is known at first. A Soviet team is put in the open 120 m from the
 // player: the US side must pick it up (player's eyes or a squad), report it in
 // the feed, and the player's map must get the contact. A team moved far away
 // out of anyone's sight must not be known.
 protected ACR_Squad m_IntelEnemy;

 void TestIntel()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel us = manager.IntelFor("US");
  ACR_Intel soviets = manager.IntelFor("USSR");
  if (m_Ticks == 2)
  {
   ACR_FactionSettings sov = ACR_FactionSettings.Find("USSR");
   if (sov) sov.m_bAICommander = false;
  }
  if (m_Ticks == 6)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "USSR" && squad.CountAlive() > 0) { m_IntelEnemy = squad; break; }
   PrintFormat("[ACR] INTELTEST before: US knows %1 contacts, enemy team %2", us.m_Contacts.Count(), m_IntelEnemy != null);
   // Far away first: 1.4 km, nobody near.
   if (m_IntelEnemy) MoveSquad(m_IntelEnemy, "1320 0 1300");
  }
  if (m_Ticks == 12)
  {
   PrintFormat("[ACR] INTELTEST far: US contacts %1 (want 0)", us.m_Contacts.Count());
   IEntity body = Eye(pc);
   if (body && m_IntelEnemy)
   {
    vector p = body.GetOrigin();
    MoveSquad(m_IntelEnemy, (p + Vector(120, 0, 0)).ToString(false));
   }
  }
  if (m_Ticks == 20)
  {
   PrintFormat("[ACR] INTELTEST near: US contacts %1 (want >=1), feed %2, client contacts %3, client feed %4, USSR knows %5",
    us.m_Contacts.Count(), us.m_Feed.Count(), ClientContacts(pc), ClientFeed(pc), soviets.m_Contacts.Count());
   foreach (string line : us.m_Feed) PrintFormat("[ACR] INTELTEST feed: %1", line);
   foreach (ACR_Contact c : us.m_Contacts) PrintFormat("[ACR] INTELTEST contact kind %1 count %2 at %3 by %4", c.m_iKind, c.m_iCount, c.m_Pos, c.m_sSpotter);
   foreach (ACR_Contact sc : soviets.m_Contacts) PrintFormat("[ACR] INTELTEST soviet contact %1 count %2 at %3", sc.m_sKey, sc.m_iCount, sc.m_Pos);
   foreach (IEntity seenBody, float t : soviets.m_SeenTime) PrintFormat("[ACR] INTELTEST soviet sees %1 squad %2", seenBody, manager.FindSquadBySoldier(seenBody) != null);
  }
  if (m_Ticks == 20)
  {
   manager.SendRadio("US", "Test: radio check.");
   IEntity me = Eye(pc);
   if (me) PrintFormat("[ACR] INTELTEST relative: %1 | %2", us.Relative(me.GetOrigin()), us.Relative(me.GetOrigin() + Vector(300, 0, 20)));
  }
  if (m_Ticks == 21 && pc) PrintFormat("[ACR] INTELTEST radio received by client: %1 (want >=1)", pc.m_ACR_RadioCount);
  if (m_Ticks == 21)
  {
   PrintFormat("[ACR] INTELTEST our empty vehicles: server knows %1, client shows %2", us.m_VehicleSeen.Count(), ClientVehicles(pc));
   foreach (IEntity v, vector at : us.m_VehicleAt) PrintFormat("[ACR] INTELTEST vehicle %1 at %2", ACR_Vehicles.NameOf(v), at);
  }
  if (m_Ticks == 22) Print("[ACR] TEST COMPLETE");
 }

 protected void MoveSquad(ACR_Squad squad, string where)
 {
  vector center = Ground(where);
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body) body.SetOrigin(Ground((center + Vector(i * 3, 0, 0)).ToString(false)));
  }
  squad.Halt("Test");
 }
 // ---------------------------------------------------------------- -acrHuntTest
 // A Soviet team is spotted 200 m from the player. Alpha is ordered to hunt it
 // (through the same request a right-click on its marker sends). Alpha must
 // head for it; once the team is dead the hunt must end on its own.
 protected ACR_Squad m_HuntTarget;
 protected int m_iHuntOrderedAt;
 void TestHunt()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Squad alpha = manager.FindSquadByGroup(m_Range.m_Group);
  if (m_Ticks == 2)
  {
   ACR_FactionSettings sov = ACR_FactionSettings.Find("USSR");
   if (sov) sov.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (m_Ticks == 6)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "USSR" && squad.CountAlive() > 0) { m_HuntTarget = squad; break; }
   IEntity body = Eye(pc);
   if (body && m_HuntTarget) MoveSquad(m_HuntTarget, (body.GetOrigin() + Vector(120, 0, 0)).ToString(false));
  }
  // Order the hunt once the US has actually spotted the team (as a player
  // would: right-clicking a marker on the map).
  if (m_Ticks >= 8 && m_Ticks <= 40 && m_iHuntOrderedAt == 0 && alpha && m_HuntTarget && m_HuntTarget.GetAnchor())
  {
   ACR_Intel intel = manager.IntelFor("US");
   ACR_Contact seen = intel.ContactNear(m_HuntTarget.GetAnchor().GetOrigin(), 150);
   if (seen)
   {
    PrintFormat("[ACR] HUNTTEST order at t=%1: %2", m_Ticks, manager.Hunt(alpha, seen.m_Pos));
    m_iHuntOrderedAt = m_Ticks;
   }
  }
  if (m_Ticks == 41 && m_iHuntOrderedAt == 0) Print("[ACR] HUNTTEST the enemy was never spotted");
  if (m_iHuntOrderedAt > 0 && m_Ticks == m_iHuntOrderedAt + 15 && alpha && m_HuntTarget.GetAnchor() && alpha.GetAnchor())
   PrintFormat("[ACR] HUNTTEST after 15 s: alpha '%1', hunting %2 (want 1), distance %3 m", alpha.m_Order, alpha.IsHunting(), (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), m_HuntTarget.GetAnchor().GetOrigin()));
  if (m_iHuntOrderedAt > 0 && m_Ticks == m_iHuntOrderedAt + 17 && m_HuntTarget)
  {
   array<AIAgent> agents = {};
   m_HuntTarget.GetAgents(agents);
   foreach (AIAgent agent : agents) Kill(agent.GetControlledEntity());
  }
  if (m_iHuntOrderedAt > 0 && m_Ticks == m_iHuntOrderedAt + 32 && alpha) PrintFormat("[ACR] HUNTTEST after the kill: alpha '%1', hunting %2 (want 0)", alpha.m_Order, alpha.IsHunting());
  if (m_Ticks == 80 || (m_iHuntOrderedAt > 0 && m_Ticks == m_iHuntOrderedAt + 34)) Print("[ACR] TEST COMPLETE");
 }
 // ---------------------------------------------------------------- -acrLauncherTest
 // An AI soldier's disposable launcher is emptied (as if fired): he must drop
 // it, and a rearm must give him a loaded one of the same kind back.
 protected IEntity m_LauncherMan;
 void TestLauncher()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Launchers) return;
  if (m_Ticks == 2)
  {
   ACR_FactionSettings sov = ACR_FactionSettings.Find("USSR");
   if (sov) sov.m_bAICommander = false;
  }
  if (m_Ticks == 8)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    array<AIAgent> agents = {};
    squad.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     IEntity body = agent.GetControlledEntity();
     if (body && ACR_Launchers.DisposableOf(body) && ACR_Squad.FindController(body) == 0) { m_LauncherMan = body; break; }
    }
    if (m_LauncherMan) break;
   }
   if (!m_LauncherMan) { Print("[ACR] LAUNCHERTEST no soldier with a disposable launcher found"); return; }
   IEntity tube = ACR_Launchers.DisposableOf(m_LauncherMan);
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(tube.FindComponent(BaseWeaponComponent));
   PrintFormat("[ACR] LAUNCHERTEST soldier has %1 with %2 round(s)", ACR_Vehicles.NameOf(tube), ACR_Launchers.Loaded(tube));
   if (weapon && weapon.GetCurrentMagazine()) weapon.GetCurrentMagazine().SetAmmoCount(0);
   if (weapon)
   {
    array<BaseMuzzleComponent> muzzles = {};
    weapon.GetMuzzlesList(muzzles);
    foreach (BaseMuzzleComponent muzzle : muzzles)
     for (int b = 0; b < muzzle.GetBarrelsCount(); b++) muzzle.ClearChamber(b);
   }
   PrintFormat("[ACR] LAUNCHERTEST emptied: %1 round(s) left", ACR_Launchers.Loaded(tube));
  }
  if (m_Ticks == 16 && m_LauncherMan)
  {
   PrintFormat("[ACR] LAUNCHERTEST dropped %1 (want 1), still carries one: %2 (want 0), owed: %3", manager.m_Launchers.m_iDropped, ACR_Launchers.DisposableOf(m_LauncherMan) != null, manager.m_Launchers.IsOwed(m_LauncherMan));
   PrintFormat("[ACR] LAUNCHERTEST rearm: %1", manager.m_Launchers.Rearm(m_LauncherMan));
  }
  if (m_Ticks == 20 && m_LauncherMan)
  {
   IEntity fresh = ACR_Launchers.DisposableOf(m_LauncherMan);
   int rounds = -1;
   if (fresh) rounds = ACR_Launchers.Loaded(fresh);
   PrintFormat("[ACR] LAUNCHERTEST after rearm: has launcher %1 (want 1), rounds %2 (want 1)", fresh != null, rounds);
  }
  if (m_Ticks == 22) Print("[ACR] TEST COMPLETE");
 }
}
