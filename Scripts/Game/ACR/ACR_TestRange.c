class ACR_TestRangeClass : GenericEntityClass {}
class ACR_TestRange : GenericEntity
{
 static ACR_TestRange s_Instance;
 SCR_AIGroup m_Group;
 ref ACR_CommandBridge m_CommandBridge;
 AIWaypoint m_Waypoint;
 vector m_Rally;
 vector m_Target;
 vector m_Start;
 bool m_Ready;
 bool m_HintShown;
 bool m_Automated;
 bool m_CommandTest;
 bool m_MapHandoffTest;
 IEntity m_MapTestBody;
 float m_MapTestYaw;
 vector m_MapTestPosition;
 ref array<IEntity> m_OrderUnits = {};
 ref array<float> m_OrderDistances = {};
 vector m_OrderTarget;
 int m_Ticks;
 IEntity m_TestOriginal;
 IEntity m_TestFirst;
 IEntity m_LastLocalBody;
 bool m_WasMovementActive;
 bool m_WasMapOpen;

 void ACR_TestRange(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
 }
 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  s_Instance = this;
  string flag;
  m_Automated = System.GetCLIParam("acrAutoTest", flag);
  m_CommandTest = System.GetCLIParam("acrCommandTest", flag);
  m_MapHandoffTest = System.GetCLIParam("acrMapHandoffTest", flag);
  if (Replication.IsServer())
  {
   IEntity spawn = GetGame().GetWorld().FindEntityByName("ACR_PlayerSpawn");
   if (spawn) spawn.SetOrigin(Ground("2400 0 2400"));
   GetGame().GetCallqueue().CallLater(Setup, 1500, false);
   GetGame().GetCallqueue().CallLater(Tick, 1000, true);
  }
  Print("[ACR] Range initialized");
 }
 vector Ground(vector point)
 {
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 0.15;
  return point;
 }
 IEntity Spawn(ResourceName prefab, vector point)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = Ground(point);
  return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
 }
 void Setup()
 {
  m_Rally = Ground("2415 0 2400");
  m_Target = Ground("2450 0 2400");
  m_Group = SCR_AIGroup.Cast(Spawn("{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et", m_Rally));
  m_CommandBridge = new ACR_CommandBridge();
  m_CommandBridge.Init(m_Group);
  PrintFormat("[ACR] Squad spawn: %1 at %2", m_Group, m_Rally);
 }
 override void EOnFrame(IEntity owner, float timeSlice)
 {
  if (!s_Instance) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || !pc.GetControlledEntity()) return;
  InputManager input = GetGame().GetInputManager();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  bool mapOpen = mapEntity && mapEntity.IsOpen();
  // Activate independently of the character context: possession changes its update order.
  if (mapOpen) input.ActivateContext("ACR_MapContext");
  else input.ActivateContext("ACR_TestContext");
  bool movementActive = input.IsActionActive("CharacterForward") && !mapOpen;
  IEntity localBody = pc.GetControlledEntity();
  if (localBody != m_LastLocalBody || (m_WasMovementActive && !movementActive) || mapOpen != m_WasMapOpen)
  {
   input.ResetAction("CharacterForward");
   input.ResetAction("CharacterRight");
   ACR_ClearCharacterInput(localBody);
  }
  if (localBody != m_LastLocalBody)
  {
   SCR_PlayerControllerCommandingComponent commanding = SCR_PlayerControllerCommandingComponent.GetLocalPlayerControllerCommandingComponent();
   if (commanding) commanding.ACR_CloseCommandMenu();
  }
  m_LastLocalBody = localBody;
  m_WasMovementActive = movementActive;
  m_WasMapOpen = mapOpen;
  pc.ACR_UpdateMap();
  if (!m_HintShown)
  {
   m_HintShown = true;
   PrintFormat("[ACR] Test input active=%1 move action=%2", input.IsContextActive("ACR_TestContext"), input.IsActionActive("ACR_Move"));
   SCR_HintManagerComponent.ShowCustomHint("F5: squad advance | F6: squad rally | F7: possess aimed soldier (or map marker) | F8: return | F9: command map", "Arma Commander test range", 30, true);
  }
  if (input.GetActionTriggered("ACR_Move"))
  {
   if (mapOpen)
   {
    float x, z;
    mapEntity.GetMapCursorWorldPosition(x, z);
    pc.ACR_Request(4, RplId.Invalid(), Vector(x, 0, z));
   }
   else pc.ACR_Request(0);
  }
  if (input.GetActionTriggered("ACR_Rally")) pc.ACR_Request(1);
  if (input.GetActionTriggered("ACR_Possess"))
  {
   if (mapOpen) pc.ACR_PossessMapCursor();
   else pc.ACR_PossessAimed();
  }
  if (input.GetActionTriggered("ACR_Return")) pc.ACR_Request(3);
  if (input.GetActionTriggered("ACR_Map"))
  {
   if (mapOpen) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
   else GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  }
 }
 static void ACR_ClearCharacterInput(IEntity entity)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (!character) return;
  CharacterControllerComponent controller = character.GetCharacterController();
  CharacterInputContext context = controller.GetInputContext();
  controller.SetMovement(0, vector.Zero);
  if (!context) return;
  context.SetMovement(0, vector.Zero);
  context.SetWeaponPullTrigger(false);
  context.SetAimChange(vector.Zero);
 }
 string Command(SCR_PlayerController pc, int command, RplId targetId, vector point)
 {
  if (!Replication.IsServer() || !pc) return "Server unavailable";
  ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
  if (!body || body.GetCharacterController().IsDead()) return "Wait until you have spawned";
  if (command == 3)
  {
   if (!pc.IsPossessing()) return "You are already in your original soldier";
   ACR_MapHandoff.Release(pc, body);
   if (m_CommandBridge && m_CommandBridge.m_Commander == pc.GetPlayerId()) m_CommandBridge.Release();
   pc.SetPossessedEntity(null);
   ACR_ClearCharacterInput(pc.GetControlledEntity());
   PrintFormat("[ACR] RETURN player=%1 controlled=%2", pc.GetPlayerId(), pc.GetControlledEntity());
   return "Returned to your original soldier";
  }
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction() || affiliation.GetAffiliatedFaction().GetFactionKey() != "US") return "Only the US test team may command this squad";
  if (!m_Group || m_Group.GetAgentsCount() == 0) return "Squad is not ready or has been destroyed";
  if (command == 0 || command == 1 || command == 4)
  {
   vector destination = m_Target;
   if (command == 1) destination = m_Rally;
   if (command == 4)
   {
    // Bound the prototype order area; reject invalid/unreasonable coordinates.
    if (!(point[0] >= 1400 && point[0] <= 3400 && point[2] >= 1400 && point[2] <= 3400)) return "Order is outside the test area";
    destination = Ground(point);
   }
   AIWaypoint replacement = AIWaypoint.Cast(Spawn("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", destination));
   if (!replacement) return "Could not create a movement order";
   array<AIWaypoint> previousOrders = {};
   m_Group.GetWaypoints(previousOrders);
   foreach (AIWaypoint previousOrder : previousOrders) m_Group.RemoveWaypoint(previousOrder);
   if (m_Waypoint)
   {
    SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
   }
   m_Waypoint = replacement;
   m_Group.AddWaypoint(m_Waypoint);
   PrintFormat("[ACR] MOVE player=%1 target=%2 command=%3", pc.GetPlayerId(), destination, command);
   return "Squad movement order accepted";
  }
  if (command != 2) return "Unknown command";
  IEntity target = SCR_EntityHelper.RplIdToEntity(targetId);
  if (!target) return "Aim directly at a squad soldier, or select its map marker";
  if (target == body) return "You already control this soldier";
  array<AIAgent> agents = {};
  GetSquadAgents(agents);
  foreach (int unitIndex, AIAgent agent : agents)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!soldier || soldier != target || soldier.GetCharacterController().IsDead()) continue;
   if (ACR_CommandBridge.FindController(soldier) != 0) continue;
   SCR_PossessingManagerComponent manager = SCR_PossessingManagerComponent.GetInstance();
   if (manager && manager.GetIdFromMainEntity(soldier) != 0) continue;
   // The engine supports chained possession and preserves the original body.
   ACR_MapHandoff.Release(pc, body);
   if (m_CommandBridge && m_CommandBridge.m_Commander == pc.GetPlayerId()) m_CommandBridge.Release();
   pc.SetPossessedEntity(soldier);
   if (m_CommandBridge) m_CommandBridge.Refresh();
   ACR_ClearCharacterInput(soldier);
   PrintFormat("[ACR] POSSESS player=%1 controlled=%2", pc.GetPlayerId(), pc.GetControlledEntity());
   if (pc.GetControlledEntity() != soldier) return "Control transfer failed; try again";
   if (m_CommandBridge && m_CommandBridge.IsLeader(pc)) return "Now controlling Alpha 1 leader. Native squad commands are available. F7 swaps again; F8 returns.";
   return string.Format("Now controlling US %1 (the selected soldier). F7 swaps again; F8 returns to your original.", unitIndex + 1);
  }
  return "That soldier is dead, occupied, or outside your test squad";
 }
 void Tick()
 {
  m_Ticks++;
  if (m_CommandBridge) m_CommandBridge.Refresh();
  PublishRoster();
  if (m_MapHandoffTest)
  {
   TestMapHandoff();
   return;
  }
  if (m_CommandTest)
  {
   TestCommandResponse();
   return;
  }
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (m_Group && m_Group.GetLeaderEntity())
  {
   vector position = m_Group.GetLeaderEntity().GetOrigin();
   if (!m_Ready)
   {
    m_Ready = true;
    m_Start = position;
    PrintFormat("[ACR] READY soldiers=%1 position=%2", m_Group.GetAgentsCount(), position);
   }
   if (m_Ticks % 10 == 0) PrintFormat("[ACR] Squad soldiers=%1 distance travelled=%2 target distance=%3", m_Group.GetAgentsCount(), vector.Distance(position, m_Start), vector.Distance(position, m_Target));
  }
  if (!m_Automated || !pc || !pc.GetControlledEntity()) return;
  if (m_Ticks == 15 || m_Ticks == 32 || m_Ticks == 34 || m_Ticks == 38 || m_Ticks == 52 || m_Ticks == 61)
  {
   InputManager input = GetGame().GetInputManager();
   PrintFormat("[ACR] SHORTCUTS possessing=%1 F7=%2 F8=%3 F9=%4", pc.IsPossessing(), input.IsActionActive("ACR_Possess"), input.IsActionActive("ACR_Return"), input.IsActionActive("ACR_Map"));
   PrintFormat("[ACR] INPUT possessing=%1 forward=%2 aim=%3 fire=%4", pc.IsPossessing(), input.IsActionActive("CharacterForward"), input.IsActionActive("CharacterAimUp"), input.IsActionActive("CharacterFire"));
  }
  if (m_Ticks == 12) pc.ACR_Request(0);
  if (m_Ticks == 20) pc.ACR_Request(4, RplId.Invalid(), Vector(2450, 0, 2400));
  if (m_Ticks == 22)
  {
   ChimeraCharacter testBody = ChimeraCharacter.Cast(pc.GetControlledEntity());
   CharacterInputContext testInput = testBody.GetCharacterController().GetInputContext();
   testInput.SetMovement(1, Vector(0, 0, 1));
   ACR_ClearCharacterInput(testBody);
   float speed;
   vector direction;
   testInput.GetMovement(speed, direction);
   PrintFormat("[ACR] MOVEMENT RESET speed=%1 direction=%2", speed, direction);
  }
  if (m_Ticks == 44) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 47) pc.ACR_TestMapSelection();
  if (m_Ticks == 49) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 51) pc.ACR_Request(3);
  if (m_Ticks == 54) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 56) pc.ACR_TestMapSelection();
  if (m_Ticks == 58) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 60) pc.ACR_Request(3);
  if (m_Ticks == 25 && m_Group && m_Group.GetLeaderEntity())
  {
   IEntity target = m_Group.GetLeaderEntity();
   vector start = target.GetOrigin() + Vector(0, 1.2, -3);
   PrintFormat("[ACR] AIM ray selected intended soldier=%1", pc.ACR_TraceSoldier(start, Vector(0, 0, 1)) == target);
  }
  if (m_Ticks == 30)
  {
   m_TestOriginal = pc.GetControlledEntity();
   array<AIAgent> agents = {};
   GetSquadAgents(agents);
   if (agents.Count() > 0)
   {
    m_TestFirst = agents[0].GetControlledEntity();
    pc.ACR_Request(2, SCR_EntityHelper.EntityToRplId(m_TestFirst));
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
   m_Group.GetWaypoints(nativeOrders);
   bool foundNativeOrder;
   foreach (AIWaypoint order : nativeOrders)
    if (vector.Distance(order.GetOrigin(), Ground("2470 0 2420")) < 2) foundNativeOrder = true;
   PrintFormat("[ACR] NATIVE TEST waypointAtRequestedPosition=%1", foundNativeOrder);
  }
  if (m_Ticks == 48 || m_Ticks == 57) GetGame().GetCallqueue().CallLater(pc.ACR_TestLeaderMarker, 100, false);
  if (m_Ticks == 34)
  {
   SCR_PlayerControllerGroupComponent groupComp = SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
   PrintFormat("[ACR] NATIVE TEST released=%1 members=%2", m_CommandBridge.m_Commander == 0, m_Group.GetAgentsCount());
   SCR_CommandingManagerComponent commands = SCR_CommandingManagerComponent.GetInstance();
   commands.RequestCommandExecution(commands.FindCommandIndex("waypointMoveForced"), RplId.Invalid(), SCR_EntityHelper.EntityToRplId(m_Group), Ground("2500 0 2450"), pc.GetPlayerId(), 0);
  }
  if (m_Ticks == 33)
  {
   array<AIAgent> agents = {};
   GetSquadAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity next = agent.GetControlledEntity();
    if (next != m_TestFirst)
    {
     pc.ACR_Request(2, SCR_EntityHelper.EntityToRplId(next));
     PrintFormat("[ACR] CHAIN different=%1 originalPreserved=%2", pc.GetControlledEntity() == next, pc.GetMainEntity() == m_TestOriginal);
     break;
    }
   }
  }
  if (m_Ticks == 35)
  {
   PrintFormat("[ACR] CHECK possessing=%1", pc.IsPossessing());
   pc.ACR_Request(3);
  }
  if (m_Ticks == 40)
  {
   PrintFormat("[ACR] CHECK returned=%1 exactOriginal=%2", !pc.IsPossessing(), pc.GetControlledEntity() == m_TestOriginal);
   pc.ACR_Request(1);
  }
 }
 void TestMapHandoff()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || !pc.GetControlledEntity() || !m_CommandBridge) return;
  if (m_Ticks == 12)
  {
   m_MapTestBody = m_CommandBridge.GetLeader();
   pc.ACR_Request(2, SCR_EntityHelper.EntityToRplId(m_MapTestBody));
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
   GetSquadAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity next = agent.GetControlledEntity();
    if (next && next != m_MapTestBody)
    {
     pc.ACR_TestSelectUnit(SCR_EntityHelper.EntityToRplId(next));
     break;
    }
   }
  }
  if (m_Ticks == 22 || m_Ticks == 54) pc.ACR_Request(3);
  if (m_Ticks == 23) pc.ACR_Request(4, RplId.Invalid(), Ground("2455 0 2400"));
  if (m_Ticks == 43)
   PrintFormat("[ACR] MAP HANDOFF AI distance=%1 yawBefore=%2 yawAfter=%3", vector.Distance(m_MapTestBody.GetOrigin(), m_MapTestPosition), m_MapTestYaw, m_MapTestBody.GetYawPitchRoll()[0]);
  if (m_Ticks == 44) pc.ACR_TestSelectUnit(SCR_EntityHelper.EntityToRplId(m_MapTestBody));
  if (m_Ticks == 48) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 60) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
 }
 void TestCommandResponse()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || !pc.GetControlledEntity() || !m_Group || !m_CommandBridge) return;
  SCR_PlayerControllerCommandingComponent commands = SCR_PlayerControllerCommandingComponent.GetLocalPlayerControllerCommandingComponent();
  SCR_CommandingManagerComponent manager = SCR_CommandingManagerComponent.GetInstance();
  if (m_Ticks == 12 || m_Ticks == 40) pc.ACR_Request(2, SCR_EntityHelper.EntityToRplId(m_CommandBridge.GetLeader()));
  if (m_Ticks == 15 || m_Ticks == 43 || m_Ticks == 65)
  {
   m_OrderTarget = Ground("2450 0 2400");
   if (m_Ticks == 43) m_OrderTarget = Ground("2415 0 2435");
   if (m_Ticks == 65) m_OrderTarget = pc.GetControlledEntity().GetOrigin();
   m_OrderUnits.Clear();
   m_OrderDistances.Clear();
   array<AIAgent> agents = {};
   GetSquadAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || ACR_CommandBridge.FindController(body)) continue;
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
  if (m_Ticks == 36)
  {
   pc.ACR_Request(2, SCR_EntityHelper.EntityToRplId(m_OrderUnits[0]));
  }
  if (m_Ticks == 38 || m_Ticks == 88)
  {
   PrintFormat("[ACR] RESPONSE nonLeader menu=%1 allowed=%2 members=%3", manager.CanShowCommand("waypointMove"), commands.ACR_HasCommandAuthority(), m_Group.GetAgentsCount());
   commands.PrepareExecuteCommand("waypointMove", Ground("2500 0 2450"));
   manager.RequestCommandExecution(manager.FindCommandIndex("waypointMove"), RplId.Invalid(), SCR_EntityHelper.EntityToRplId(m_Group), Ground("2500 0 2450"), pc.GetPlayerId(), 0);
  }
  if (m_Ticks == 50) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 52) pc.ACR_TestLeaderMarker();
  if (m_Ticks == 54) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (m_Ticks == 87) pc.ACR_Request(3);
 }
 void GetSquadAgents(out array<AIAgent> agents)
 {
  m_Group.GetAgents(agents);
  if (m_CommandBridge && m_CommandBridge.m_DetachedLeader && !agents.Contains(m_CommandBridge.m_DetachedLeader))
   agents.Insert(m_CommandBridge.m_DetachedLeader);
 }
 void PublishRoster()
 {
  if (!m_Group) return;
  array<RplId> ids = {};
  array<vector> positions = {};
  array<int> states = {};
  array<AIAgent> agents = {};
  GetSquadAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!soldier) continue;
   ids.Insert(SCR_EntityHelper.EntityToRplId(soldier));
   positions.Insert(soldier.GetOrigin());
   int state = 0;
   if (soldier.GetCharacterController().IsDead()) state = 2;
   else if (ACR_CommandBridge.FindController(soldier) != 0) state = 1;
   states.Insert(state);
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc) continue;
   ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
   if (!body) continue;
   FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
   if (!affiliation || !affiliation.GetAffiliatedFaction() || affiliation.GetAffiliatedFaction().GetFactionKey() != "US") continue;
   IEntity original = pc.GetMainEntity();
   vector originalPos;
   if (original) originalPos = original.GetOrigin();
   pc.ACR_SendRoster(ids, positions, states, originalPos);
   ACR_GroupMapFeed.Publish(this, pc);
  }
 }
 void ~ACR_TestRange()
 {
  if (GetGame())
  {
   GetGame().GetCallqueue().Remove(Setup);
   GetGame().GetCallqueue().Remove(Tick);
  }
  if (s_Instance == this) s_Instance = null;
 }
}


