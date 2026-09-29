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
  if (m_Ticks == 20) pc.ACR_Request(ACR_ERequest.MOVE_TO, RplId.Invalid(), Vector(2450, 0, 2400));
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
   commanding.PrepareExecuteCommand("waypointMoveForced", Ground("2470 0 2420"));
  }
  if (m_Ticks == 32)
  {
   array<AIWaypoint> nativeOrders = {};
   m_Range.m_Group.GetWaypoints(nativeOrders);
   bool foundNativeOrder;
   foreach (AIWaypoint order : nativeOrders)
    if (vector.Distance(order.GetOrigin(), Ground("2470 0 2420")) < 2) foundNativeOrder = true;
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
   commands.RequestCommandExecution(commands.FindCommandIndex("waypointMoveForced"), RplId.Invalid(), SCR_EntityHelper.EntityToRplId(m_Range.m_Group), Ground("2500 0 2450"), pc.GetPlayerId(), 0);
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
  if (m_Ticks == 23) pc.ACR_Request(ACR_ERequest.MOVE_TO, RplId.Invalid(), Ground("2455 0 2400"));
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
   m_OrderTarget = Ground("2450 0 2400");
   if (m_Ticks == 43) m_OrderTarget = Ground("2415 0 2435");
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
   commands.PrepareExecuteCommand("waypointMove", Ground("2500 0 2450"));
   manager.RequestCommandExecution(manager.FindCommandIndex("waypointMove"), RplId.Invalid(), SCR_EntityHelper.EntityToRplId(m_Range.m_Group), Ground("2500 0 2450"), pc.GetPlayerId(), 0);
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
}
