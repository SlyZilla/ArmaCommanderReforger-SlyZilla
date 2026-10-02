// Possession, command handover, Game Master, respawn and players' side tests (split out of ACR_TestHarness, 2 Oct).
class ACR_TestCommand : Managed
{
 protected ACR_TestHarness h;

 void ACR_TestCommand(ACR_TestHarness harness) { h = harness; }

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

 void LogLeaderMarker()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Squad squad = h.Squad();
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
  ACR_Squad squad = h.Squad();
  if (!squad || !h.m_Range.m_Group) return;
  IEntity leaderEntity = squad.GetLeader();
  if (leaderEntity)
  {
   vector position = leaderEntity.GetOrigin();
   if (!m_Ready)
   {
    m_Ready = true;
    m_Start = position;
    PrintFormat("[ACR] READY soldiers=%1 position=%2", h.m_Range.m_Group.GetAgentsCount(), position);
   }
   if (h.m_Ticks % 10 == 0) PrintFormat("[ACR] Squad distance travelled=%1 target distance=%2", vector.Distance(position, m_Start), vector.Distance(position, h.m_Range.m_Target));
  }
  if (!pc || !pc.GetControlledEntity()) return;
  if (h.m_Ticks == 15 || h.m_Ticks == 32 || h.m_Ticks == 34 || h.m_Ticks == 38 || h.m_Ticks == 52 || h.m_Ticks == 61)
  {
   InputManager input = GetGame().GetInputManager();
   PrintFormat("[ACR] SHORTCUTS possessing=%1 F7=%2 F8=%3 F9=%4", pc.IsPossessing(), input.IsActionActive("ACR_Possess"), input.IsActionActive("ACR_Return"), input.IsActionActive("ACR_Map"));
   PrintFormat("[ACR] INPUT possessing=%1 forward=%2 aim=%3 fire=%4", pc.IsPossessing(), input.IsActionActive("CharacterForward"), input.IsActionActive("CharacterAimUp"), input.IsActionActive("CharacterFire"));
  }
  if (h.m_Ticks == 12) pc.ACR_Request(ACR_ERequest.ADVANCE);
  if (h.m_Ticks == 20) pc.ACR_Request(ACR_ERequest.MOVE_TO, RplId.Invalid(), Vector(1370, 0, 2832));
  if (h.m_Ticks == 22)
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
  if (h.m_Ticks == 25 && leaderEntity)
  {
   vector start = leaderEntity.GetOrigin() + Vector(0, 1.2, -3);
   PrintFormat("[ACR] AIM ray selected intended soldier=%1", pc.ACR_TraceSoldier(start, Vector(0, 0, 1)) == leaderEntity);
  }
  if (h.m_Ticks == 30)
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
  if (h.m_Ticks == 31)
  {
   SCR_PlayerControllerGroupComponent groupComp = SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
   SCR_CommandingManagerComponent commandManager = SCR_CommandingManagerComponent.GetInstance();
   PrintFormat("[ACR] NATIVE TEST leader=%1 menuMove=%2", groupComp.IsPlayerLeaderOwnGroup(), commandManager.CanShowCommand("waypointMoveForced"));
   SCR_PlayerControllerCommandingComponent commanding = SCR_PlayerControllerCommandingComponent.GetLocalPlayerControllerCommandingComponent();
   commanding.PrepareExecuteCommand("waypointMoveForced", h.Ground("1390 0 2852"));
  }
  if (h.m_Ticks == 32)
  {
   array<AIWaypoint> nativeOrders = {};
   h.m_Range.m_Group.GetWaypoints(nativeOrders);
   bool foundNativeOrder;
   foreach (AIWaypoint order : nativeOrders)
    if (vector.Distance(order.GetOrigin(), h.Ground("1390 0 2852")) < 2) foundNativeOrder = true;
   PrintFormat("[ACR] NATIVE TEST waypointAtRequestedPosition=%1", foundNativeOrder);
  }
  if (h.m_Ticks == 33)
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
  if (h.m_Ticks == 34)
  {
   PrintFormat("[ACR] NATIVE TEST released=%1 members=%2", squad.m_Commander == 0, h.m_Range.m_Group.GetAgentsCount());
   SCR_CommandingManagerComponent commands = SCR_CommandingManagerComponent.GetInstance();
   commands.RequestCommandExecution(commands.FindCommandIndex("waypointMoveForced"), RplId.Invalid(), SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group), h.Ground("1420 0 2862"), pc.GetPlayerId(), 0);
  }
  if (h.m_Ticks == 35)
  {
   PrintFormat("[ACR] CHECK possessing=%1", pc.IsPossessing());
   pc.ACR_Request(ACR_ERequest.RETURN);
  }
  if (h.m_Ticks == 40)
  {
   PrintFormat("[ACR] CHECK returned=%1 exactOriginal=%2", !pc.IsPossessing(), pc.GetControlledEntity() == m_TestOriginal);
   pc.ACR_Request(ACR_ERequest.RALLY);
  }
  if (h.m_Ticks == 44 || h.m_Ticks == 54) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (h.m_Ticks == 47 || h.m_Ticks == 56) SelectLeaderMarker(pc, squad);
  if (h.m_Ticks == 48 || h.m_Ticks == 57) GetGame().GetCallqueue().CallLater(LogLeaderMarker, 100, false);
  if (h.m_Ticks == 49 || h.m_Ticks == 58) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (h.m_Ticks == 51 || h.m_Ticks == 60) pc.ACR_Request(ACR_ERequest.RETURN);
 }

 // ---------------------------------------------------------------- -acrMapHandoffTest
 void TestMapHandoff()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Squad squad = h.Squad();
  if (!pc || !pc.GetControlledEntity() || !squad) return;
  if (h.m_Ticks == 12)
  {
   m_MapTestBody = squad.GetLeader();
   pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(m_MapTestBody));
  }
  if (h.m_Ticks == 15 || h.m_Ticks == 50)
  {
   SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(pc.GetControlledEntity());
   IEntity item = gadgets.GetGadgetByType(EGadgetType.MAP);
   PrintFormat("[ACR] MAP HANDOFF equip mapExists=%1", item != null);
   gadgets.SetGadgetMode(item, EGadgetMode.IN_HAND, true);
  }
  if (h.m_Ticks == 19 || h.m_Ticks == 21 || h.m_Ticks == 45 || h.m_Ticks == 53 || h.m_Ticks == 56 || h.m_Ticks == 61)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(m_MapTestBody);
   CharacterControllerComponent controller = body.GetCharacterController();
   CharacterControllerComponent current = ChimeraCharacter.Cast(pc.GetControlledEntity()).GetCharacterController();
   PrintFormat("[ACR] MAP FLAGS tick=%1 oldView=%2 oldMove=%3 newView=%4", h.m_Ticks, controller.GetDisableViewControls(), controller.GetDisableMovementControls(), current.GetDisableViewControls());
   PrintFormat("[ACR] MAP HANDOFF tick=%1 held=%2 wanted=%3 forcedLook=%4 inputLook=%5 playerFocus=%6 mapOpen=%7", h.m_Ticks, controller.IsGadgetInHands(), controller.GetInputContext().GetWantedLefHandGadgetEntity() != null, controller.IsFreeLookForced(), controller.GetInputContext().CameraIsFreeLook(), pc.GetGadgetFocus(), SCR_MapEntity.GetMapInstance().IsOpen());
  }
  if (h.m_Ticks == 20)
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
  if (h.m_Ticks == 22 || h.m_Ticks == 54) pc.ACR_Request(ACR_ERequest.RETURN);
  if (h.m_Ticks == 23) pc.ACR_Request(ACR_ERequest.MOVE_TO, RplId.Invalid(), h.Ground("1375 0 2832"));
  if (h.m_Ticks == 43)
   PrintFormat("[ACR] MAP HANDOFF AI distance=%1 yawBefore=%2 yawAfter=%3", vector.Distance(m_MapTestBody.GetOrigin(), m_MapTestPosition), m_MapTestYaw, m_MapTestBody.GetYawPitchRoll()[0]);
  if (h.m_Ticks == 44)
  {
   // The leader's plain marker is hidden behind its group marker.
   if (!pc.ACR_SelectMarker(SCR_EntityHelper.EntityToRplId(m_MapTestBody), true)) Print("[ACR] MAP HANDOFF missing leader marker");
  }
  if (h.m_Ticks == 48 || h.m_Ticks == 60) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
 }

 // ---------------------------------------------------------------- -acrCommandTest
 void TestCommandResponse()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Squad squad = h.Squad();
  if (!pc || !pc.GetControlledEntity() || !squad) return;
  SCR_PlayerControllerCommandingComponent commands = SCR_PlayerControllerCommandingComponent.GetLocalPlayerControllerCommandingComponent();
  SCR_CommandingManagerComponent manager = SCR_CommandingManagerComponent.GetInstance();
  if (h.m_Ticks == 12 || h.m_Ticks == 40) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(squad.GetLeader()));
  if (h.m_Ticks == 15 || h.m_Ticks == 43 || h.m_Ticks == 65)
  {
   m_OrderTarget = h.Ground("1370 0 2832");
   if (h.m_Ticks == 43) m_OrderTarget = h.Ground("1335 0 2867");
   if (h.m_Ticks == 65) m_OrderTarget = pc.GetControlledEntity().GetOrigin();
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
   PrintFormat("[ACR] RESPONSE order=%1 menu=%2 allowed=%3 ai=%4", h.m_Ticks, manager.CanShowCommand("waypointMove"), commands.ACR_HasCommandAuthority(), m_OrderUnits.Count());
   if (h.m_Ticks == 65) commands.PrepareExecuteCommand("follow");
   else commands.PrepareExecuteCommand("waypointMove", m_OrderTarget);
  }
  if (h.m_Ticks == 35 || h.m_Ticks == 63 || h.m_Ticks == 85)
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
  if (h.m_Ticks == 36 && m_OrderUnits.Count() > 0)
   pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(m_OrderUnits[0]));
  if (h.m_Ticks == 38 || h.m_Ticks == 88)
  {
   PrintFormat("[ACR] RESPONSE nonLeader menu=%1 allowed=%2 members=%3", manager.CanShowCommand("waypointMove"), commands.ACR_HasCommandAuthority(), h.m_Range.m_Group.GetAgentsCount());
   commands.PrepareExecuteCommand("waypointMove", h.Ground("1420 0 2862"));
   manager.RequestCommandExecution(manager.FindCommandIndex("waypointMove"), RplId.Invalid(), SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group), h.Ground("1420 0 2862"), pc.GetPlayerId(), 0);
  }
  if (h.m_Ticks == 50) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (h.m_Ticks == 52) LogLeaderMarker();
  if (h.m_Ticks == 54) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  if (h.m_Ticks == 87) pc.ACR_Request(ACR_ERequest.RETURN);
 }

 // ---------------------------------------------------------------- -acrGameMasterTest
 void TestGameMaster()
 {
  SCR_EditorManagerEntity editor = SCR_EditorManagerEntity.GetInstance();
  if (h.m_Ticks == 10)
  {
   if (!editor) { Print("[ACR] GM no editor manager"); return; }
   PrintFormat("[ACR] GM hasEdit=%1 modes=%2", editor.HasMode(EEditorMode.EDIT), editor.GetModes());
   editor.Open();
  }
  if (h.m_Ticks == 14 && editor) PrintFormat("[ACR] GM opened=%1", editor.IsOpened());
  if (h.m_Ticks == 16 && editor) editor.Close();
  if (h.m_Ticks == 18 && editor)
  {
   PrintFormat("[ACR] GM closed=%1", !editor.IsOpened());
   Print("[ACR] TEST COMPLETE");
  }
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

 void TestRespawn()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  if (h.m_Ticks == 5)
  {
   manager.m_iRespawnSeconds = 6;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sName == "Farm") { base.m_sOwner = ""; base.m_iGarrisonLeft = 0; }
    if (base.m_sName == "Crossroads") base.m_sOwner = "US";
   }
  }
  // 1) Command unit dies while the player controls an Alpha soldier; F8 waits for the new one.
  if (h.m_Ticks == 10)
  {
   m_RespawnOldMain = pc.GetMainEntity();
   ChimeraCharacter soldier = h.AlphaMember(1);
   if (soldier) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(soldier));
  }
  if (h.m_Ticks == 12) { h.Kill(m_RespawnOldMain); PrintFormat("[ACR] RESPAWN T1 killed main: %1", RespawnState(pc)); }
  if (h.m_Ticks == 14) pc.ACR_Request(ACR_ERequest.RETURN);
  if (h.m_Ticks == 16 || h.m_Ticks == 21) PrintFormat("[ACR] RESPAWN T1 t=%1 %2", h.m_Ticks, RespawnState(pc));
  // 2) A possessed soldier dies while the command unit lives: back to the command unit.
  if (h.m_Ticks == 23)
  {
   m_RespawnOldMain = pc.GetMainEntity();
   ChimeraCharacter soldier = h.AlphaMember(2);
   if (soldier) pc.ACR_Request(ACR_ERequest.POSSESS, SCR_EntityHelper.EntityToRplId(soldier));
  }
  if (h.m_Ticks == 25) h.Kill(pc.GetControlledEntity());
  if (h.m_Ticks == 27) PrintFormat("[ACR] RESPAWN T2 %1 backInMain=%2", RespawnState(pc), pc.GetControlledEntity() == pc.GetMainEntity());
  // 3) Killed in the command unit: native respawn after the delay, at Crossroads.
  if (h.m_Ticks == 29) { m_RespawnOldMain = pc.GetControlledEntity(); h.Kill(m_RespawnOldMain); }
  if (h.m_Ticks == 32 || h.m_Ticks == 38) PrintFormat("[ACR] RESPAWN T3 t=%1 %2", h.m_Ticks, RespawnState(pc));
  if (h.m_Ticks == 40) Print("[ACR] TEST COMPLETE");
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
  if (h.m_Ticks == 3)
  {
   PrintFormat("[ACR] SIDE start faction=%1", FactionKey(pc));
   ACR_PlayerSide.Force("USSR");
   h.Kill(pc.GetControlledEntity());
  }
  if (h.m_Ticks == 10 && affiliation)
  {
   PrintFormat("[ACR] SIDE after: faction=%1 (want USSR); may request US=%2 (want 0)", FactionKey(pc), affiliation.ACR_WouldAllow(factions.GetFactionByKey("US")));
   ACR_PlayerSide.Force("");
   PrintFormat("[ACR] SIDE free choice again: may request US=%1 (want 1)", affiliation.ACR_WouldAllow(factions.GetFactionByKey("US")));
  }
  if (h.m_Ticks == 12) Print("[ACR] TEST COMPLETE");
 }

 string FactionKey(SCR_PlayerController pc)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "none";
  return faction.GetFactionKey();
 }
}
