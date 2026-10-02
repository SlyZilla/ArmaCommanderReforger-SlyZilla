enum ACR_ERequest
{
 ADVANCE,          // test hotkey: selected squad moves 35 m east of its home
 RALLY,            // test hotkey: selected squad returns home
 POSSESS,          // take control of a soldier (targetId = soldier)
 RETURN,           // go back to your command unit
 MOVE_TO,          // squad (targetId = group, or selected) moves to point
 SWITCH_TO_GROUP,  // possess the group's leader if free, else another free soldier
 ORDER_DEFEND,     // defend point
 ORDER_GET_IN,     // board vehicle near point / nearest vehicle
 ORDER_GET_OUT,    // dismount
 ORDER_UNLOAD,     // passengers of this squad's vehicles dismount
 MODE_COMBAT,
 MODE_FAST,
 MODE_SILENT,
 TAKE_COMMAND,
 LEAVE_COMMAND,
 REINFORCE,        // refill the squad's fallen soldiers
 ORDER_SET_LZ,     // passengers dismount on their own when their ride sets down near point
 UNLOAD_HERE,      // player in a vehicle: every passenger squad out now
 ORDER_RIDE,       // a nearby free transport picks the squad up and drives it to point
 QUEUE_MOVE,       // Shift + right-click: move there after the current order
 QUEUE_DEFEND,     // ... defend there after the current order
 ORDER_HUNT,       // pursue the spotted enemy group at point until it's destroyed or lost
 TAKE_GUNNER,      // possess the gunner of the squad's armed vehicle (targetId = group)
 ORDER_HALT        // drop every order and stand still
}

// Arma Commander core. One instance exists on every machine while the mode is
// running; the squad registry and all authority decisions live on the server.
// Clients only use the local-frame input handling.
class ACR_Manager : Managed
{
 protected static ref ACR_Manager s_Instance;

 ref array<ref ACR_Squad> m_Squads = {};
 ref ACR_BaseSystem m_Bases = new ACR_BaseSystem();
 protected ref map<string, ref ACR_AICommander> m_AICommanders = new map<string, ref ACR_AICommander>();
 protected ref map<string, ref ACR_Intel> m_Intel = new map<string, ref ACR_Intel>();
 string m_sSpawningFaction; // side paying for the unit being spawned
 ref map<int, ACR_Squad> m_LastSquad = new map<int, ACR_Squad>();
 protected int m_SquadNumber;

 // Command rules. One commander per faction unless shared command is on.
 bool m_SharedCommand;
 // Game Master for the host and server admins: debugging and fixing stuck units.
 bool m_bHostGameMaster = true;
 int m_iRespawnSeconds = 15;
 int m_iAISkill = 70; // EAISkill.VETERAN
 float m_fAIPerception = 1.6; // how fast AI spots and identifies targets (game default 1)
 ref ACR_Respawn m_Respawn;
 ref ACR_Supports m_Supports;
 ref ACR_Rides m_Rides;
 ref ACR_Logistics m_Logistics;
 ref ACR_Cleanup m_Cleanup;
 ref ACR_Launchers m_Launchers;
 int m_iSoldierCap = 100;   // living AI soldiers per side (purchases stop above it)
 protected bool m_bHostOptionsApplied;
 protected int m_iStatusTick;
 protected ref map<string, ref array<int>> m_Commanders = new map<string, ref array<int>>();
 protected ref array<int> m_LeftCommand = {};   // players who chose to leave command
 protected ref ACR_Command m_Command;   // shared command: tags, last actions, selections
 protected ref ACR_Pings m_Pings;

 ACR_Command Command()
 {
  if (!m_Command) m_Command = new ACR_Command(this);
  return m_Command;
 }

 ACR_Pings Pings()
 {
  if (!m_Pings) m_Pings = new ACR_Pings(this);
  return m_Pings;
 }

 // local input state
 protected IEntity m_LastLocalBody;
 protected bool m_WasMovementActive;
 protected bool m_WasMapOpen;
 protected bool m_HintShown;

 static ACR_Manager GetInstance() { return s_Instance; }

 static ACR_Manager Start()
 {
  if (!s_Instance)
  {
   s_Instance = new ACR_Manager();
   if (Replication.IsServer())
   {
    GetGame().GetCallqueue().CallLater(s_Instance.Tick, 1000, true);
    // Better Battle Buddies: gates, truck beds, spent launchers on its own tick.
    BBB.Start(new ACR_BBBHooks(s_Instance));
   }
   Print("[ACR] Manager started");
  }
  return s_Instance;
 }

 static void Stop()
 {
  ACR_Hud.Clear();
  if (!s_Instance) return;
  if (GetGame()) GetGame().GetCallqueue().Remove(s_Instance.Tick);
  BBB.Stop();
  s_Instance = null;
 }

 static vector Ground(vector point)
 {
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 0.15;
  return point;
 }

 // A point a client sent (an order, a support target, a ping) lies inside the world.
 static bool OnMap(vector point)
 {
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  return point[0] >= mins[0] && point[0] <= maxs[0] && point[2] >= mins[2] && point[2] <= maxs[2];
 }

 // Arma-style 6-digit grid reference (10 m precision).
 static string Grid(vector position)
 {
  int x = position[0] / 10;
  int z = position[2] / 10;
  return string.Format("grid %1-%2", x.ToString(3), z.ToString(3));
 }

 string CallSupport(SCR_PlayerController pc, int index, vector target)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "You have no faction";
  if (!IsCommander(pc.GetPlayerId())) return "Only the commander can call supports";
  if (!OnMap(target)) return "That target is off the map";
  if (!m_Supports) m_Supports = new ACR_Supports(this);
  string key = faction.GetFactionKey();
  int before = m_Bases.GetRP(key);
  string result = m_Supports.Call(key, index, target);
  int spent = before - m_Bases.GetRP(key);
  if (spent > 0)
  {
   string support = m_Bases.GetFaction(key).m_aSupports[index].m_sName;
   string tag = Command().TagOf(pc.GetPlayerId());
   ACR_Intel intel = IntelFor(key);
   string place = "";
   if (intel) place = " " + intel.Place(target);
   Command().Feed(key, string.Format("%1 called %2%3 (-%4 RP)", tag, support, place, spent));
   Command().Note(pc.GetPlayerId(), support + place);
   Pings().OnSupportCalled(key, target, tag, support);
  }
  return result;
 }

 // RP a player spent, in the side's feed ("Popp requisitioned Rifle squad 2 (-150 RP)").
 void LogSpend(SCR_PlayerController pc, string key, int before, string what)
 {
  int spent = before - m_Bases.GetRP(key);
  if (spent <= 0) return;
  string tag = Command().TagOf(pc.GetPlayerId());
  Command().Feed(key, string.Format("%1 %2 (-%3 RP)", tag, what, spent));
  Command().Note(pc.GetPlayerId(), what);
 }

 static IEntity SpawnPrefab(ResourceName prefab, vector point)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = Ground(point);
  return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
 }

 // ================================================================ host options
 // Chosen in the host launcher's setup menu and passed on the server's command
 // line; they override the scenario's own settings for this session.
 //  -acrStartRP N  -acrIncome X  -acrAI both|none|<faction>  -acrSkill N
 //  -acrReaction X  -acrSoldierCap N  -acrUnitCap N  -acrSharedCommand 0|1  (-acrGameMasterAll, -acrRespawnSeconds)
 protected void ApplyHostOptions()
 {
  m_bHostOptionsApplied = true;
  string value;
  if (System.GetCLIParam("acrSkill", value) && value.ToInt() > 0) m_iAISkill = value.ToInt();
  if (System.GetCLIParam("acrReaction", value) && value.ToFloat() > 0) m_fAIPerception = value.ToFloat();
  if (System.GetCLIParam("acrSoldierCap", value) && value.ToInt() > 0) m_iSoldierCap = value.ToInt();
  if (System.GetCLIParam("acrSharedCommand", value)) m_SharedCommand = value.ToInt() != 0;
  string ai;
  bool setAI = System.GetCLIParam("acrAI", ai);
  ai.ToLower();
  string unitCap;
  bool setCap = System.GetCLIParam("acrUnitCap", unitCap);
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
  {
   string key = settings.m_sFaction;
   key.ToLower();
   if (setAI) settings.m_bAICommander = ai == "both" || ai == key;
   if (setCap && unitCap.ToInt() > 0) settings.m_iAIUnitCap = unitCap.ToInt();
  }
  ACR_PlayerSide.Key();
  PrintFormat("[ACR] Host options: skill %1, reaction %2, soldier cap %3, AI %4, unit cap %5, start RP %6, income x%7, shared command %8",
   m_iAISkill, m_fAIPerception, m_iSoldierCap, ai, unitCap, StartRPOption(), IncomeOption(), m_SharedCommand);
 }

 static int StartRPOption()
 {
  string value;
  if (System.GetCLIParam("acrStartRP", value) && value.ToInt() > 0) return value.ToInt();
  return -1;
 }

 static float IncomeOption()
 {
  string value;
  if (System.GetCLIParam("acrIncome", value) && value.ToFloat() > 0) return value.ToFloat();
  return 1;
 }

 // ================================================================ registry (server)

 void Tick()
 {
  int t = System.GetTickCount();
  ScanForSquads();
  t = ACR_Perf.Lap("scan", t);
  EnsureCommanders();
  m_Bases.Tick(this);
  t = ACR_Perf.Lap("bases", t);
  GrantGameMaster();
  ACR_PlayerSide.Enforce();
  TickIntel();
  t = ACR_Perf.Lap("intel", t);
  TickAICommanders();
  t = ACR_Perf.Lap("aicmd", t);
  if (!m_Respawn) m_Respawn = new ACR_Respawn(this);
  m_Respawn.Tick();
  if (!m_Rides) m_Rides = new ACR_Rides(this);
  m_Rides.Tick();
  t = ACR_Perf.Lap("rides", t);
  if (!m_Logistics) m_Logistics = new ACR_Logistics(this);
  m_Logistics.Tick();
  t = ACR_Perf.Lap("logistics", t);
  if (!m_Cleanup) m_Cleanup = new ACR_Cleanup(this);
  m_Cleanup.Tick();
  if (!m_Launchers) m_Launchers = new ACR_Launchers(this);
  Pings().Tick();
  t = ACR_Perf.Lap("cleanup", t);
  if (!m_bHostOptionsApplied) ApplyHostOptions();
  m_iStatusTick++;
  bool statusDue = m_iStatusTick % 5 == 0;
  for (int i = m_Squads.Count() - 1; i >= 0; i--)
  {
   ACR_Squad squad = m_Squads[i];
   if (squad.IsAlive())
   {
    squad.Refresh();
    squad.Tactics(m_iAISkill, GetGame().GetWorld().GetWorldTime());
    if (statusDue) squad.UpdateStatus(m_Logistics);
    continue;
   }
   PrintFormat("[ACR] Squad removed: %1", squad.GetName());
   squad.Dispose();
   m_Squads.RemoveOrdered(i);
  }
  t = ACR_Perf.Lap("squads", t);
  if (ACR_Perf.s_bChecks) foreach (ACR_Squad checked : m_Squads) checked.CheckOrderState();
  ACR_MapFeed.PublishAll(this);
  t = ACR_Perf.Lap("feed", t);
  ACR_Perf.EndTick();
 }

 // Any AI squad in the world, including ones spawned mid-game, is picked up
 // within a second. Player groups and our own command groups are skipped.
 void ScanForSquads()
 {
  array<AIAgent> agents = {};
  GetGame().GetAIWorld().GetAIAgents(agents);
  foreach (AIAgent agent : agents)
  {
   SCR_AIGroup group = SCR_AIGroup.Cast(agent);
   if (!group) group = SCR_AIGroup.Cast(agent.GetParentGroup());
   if (group && CanRegister(group)) Register(group);
  }
 }

 bool CanRegister(SCR_AIGroup group)
 {
  if (group.IsPlayable() || group.IsSlave() || !group.GetFaction()) return false;
  if (m_Bases.IsGarrison(group)) return false;
  if (m_Supports && m_Supports.IsSupportGroup(group)) return false;
  if (group.GetAgentsCount() == 0) return false;
  return !FindSquadByGroup(group) && !FindSquadByCommandGroup(group);
 }

 // Purchased vehicles waiting for their crew group to register, and
 // passenger groups waiting to board.
 ref map<IEntity, string> m_PendingCrewNames = new map<IEntity, string>();
 ref map<SCR_AIGroup, IEntity> m_PendingBoarding = new map<SCR_AIGroup, IEntity>();

 ACR_Squad Register(SCR_AIGroup group)
 {
  m_SquadNumber++;
  ACR_Squad squad = new ACR_Squad(group, m_SquadNumber);
  m_Squads.Insert(squad);
  array<IEntity> vehicles = {};
  squad.GetVehicles(vehicles);
  foreach (IEntity vehicle : vehicles)
  {
   if (!squad.m_OwnVehicle) squad.m_OwnVehicle = vehicle;
   string crewName;
   if (!m_PendingCrewNames.Find(vehicle, crewName)) continue;
   squad.Rename(crewName);
   m_PendingCrewNames.Remove(vehicle);
  }
  squad.Init();
  IEntity ride;
  if (m_PendingBoarding.Find(group, ride))
  {
   m_PendingBoarding.Remove(group);
   if (ride) squad.GetIn(ride.GetOrigin());
  }
  PrintFormat("[ACR] Squad registered: %1 faction=%2 soldiers=%3", squad.GetName(), group.GetFaction().GetFactionKey(), group.GetAgentsCount());
  return squad;
 }

 ACR_Squad FindSquadByGroup(SCR_AIGroup group)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.m_Group == group) return squad;
  return null;
 }

 ACR_Squad FindSquadByCommandGroup(SCR_AIGroup group)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.m_CommandGroup == group) return squad;
  return null;
 }

 ACR_Squad FindSquadBySoldier(IEntity soldier)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.IndexOf(soldier) >= 0) return squad;
  return null;
 }

 ACR_Squad FindSquadLedBy(SCR_PlayerController pc)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.IsLeader(pc)) return squad;
  return null;
 }

 void ReleaseAuthority(SCR_PlayerController pc)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.m_Commander == pc.GetPlayerId()) squad.Release();
 }

 // Fallback for orders sent without a squad (test hotkeys): the squad you are
 // in, else the squad you were last in, else your faction's first.
 ACR_Squad GetSelectedSquad(SCR_PlayerController pc)
 {
  ACR_Squad squad = FindSquadBySoldier(pc.GetControlledEntity());
  if (squad) return squad;
  ACR_Squad last;
  if (m_LastSquad.Find(pc.GetPlayerId(), last) && last && m_Squads.Contains(last)) return last;
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  foreach (ACR_Squad candidate : m_Squads)
   if (candidate.GetFaction() == faction) return candidate;
  return null;
 }

 // ================================================================ Game Master (server)

 static bool IsHostOrAdmin(int playerId)
 {
  if (SCR_Global.IsAdmin(playerId)) return true;
  // Listen server / offline: the server's own player is the host.
  PlayerController local = GetGame().GetPlayerController();
  if (local) return local.GetPlayerId() == playerId;
  // Dedicated server run from someone's PC (no admin configured): the first
  // player still connected counts as the host.
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  int first = int.MAX;
  foreach (int id : players) if (id < first) first = id;
  return playerId == first;
 }

 protected void GrantGameMaster()
 {
  if (!m_bHostGameMaster) return;
  SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
  if (!core) return;
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  bool everyone = System.IsCLIParam("acrGameMasterAll"); // host chose to share it (Host-Arland asks)
  foreach (int id : players)
  {
   if (!everyone && !IsHostOrAdmin(id)) continue;
   SCR_EditorManagerEntity editor = core.GetEditorManager(id);
   if (!editor) continue;
   if (editor.HasMode(EEditorMode.EDIT))
   {
    ResetEditorMode(id, editor);
    continue;
   }
   editor.AddEditorModes(EEditorModeAccess.BASE, EEditorMode.EDIT);
   PrintFormat("[ACR] Game Master granted to player %1", id);
   Notify(id, "Game Master available: press Y");
  }
 }

 // Building from a construction truck switches the editor to building mode and
 // leaves it there (then on photo mode once the truck's mode goes away), so Y
 // no longer opened Game Master. Once the editor has been closed a few seconds,
 // its mode goes back to Game Master; the truck's Build action sets its own.
 protected ref map<int, int> m_EditorClosedFor = new map<int, int>();
 protected void ResetEditorMode(int playerId, SCR_EditorManagerEntity editor)
 {
  if (editor.IsOpened() || editor.GetCurrentMode() == EEditorMode.EDIT)
  {
   m_EditorClosedFor.Set(playerId, 0);
   return;
  }
  int seconds = m_EditorClosedFor.Get(playerId) + 1;
  m_EditorClosedFor.Set(playerId, seconds);
  if (seconds < 3) return;
  editor.SetCurrentMode(EEditorMode.EDIT);
  m_EditorClosedFor.Set(playerId, 0);
  PrintFormat("[ACR] Editor of player %1 set back to Game Master", playerId);
 }

 // ================================================================ AI commanders (server)

 // Every faction with an ACR_FactionSettings entity that allows it gets an AI
 // commander. It only acts while no player of that faction is in command.
 protected void TickAICommanders()
 {
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
  {
   if (!settings.m_bAICommander) continue;
   ACR_AICommander ai;
   if (!m_AICommanders.Find(settings.m_sFaction, ai))
   {
    ai = new ACR_AICommander(this, settings.m_sFaction);
    m_AICommanders.Insert(settings.m_sFaction, ai);
   }
   Faction faction = GetGame().GetFactionManager().GetFactionByKey(settings.m_sFaction);
   array<int> commanders = GetCommanders(faction);
   bool noCommander = commanders == null || commanders.IsEmpty();
   bool hold = noCommander && HoldAIFor(settings.m_sFaction);
   if (hold && !m_AIHeld.Contains(settings.m_sFaction))
   {
    m_AIHeld.Insert(settings.m_sFaction);
    PrintFormat("[ACR] AI commander %1 held: players are still loading in", settings.m_sFaction);
   }
   if (!hold) m_AIHeld.RemoveItem(settings.m_sFaction);
   ai.Tick(noCommander && !hold);
  }
 }

 // The AI commander waits while players may still be loading in, so it doesn't spend
 // the side's RP and send its squads off before a player can take command: the first
 // AI_START_HOLD_MS of the scenario (not in test runs), and AI_JOIN_HOLD_MS after a
 // player who may end up on that side first shows up.
 static const float AI_START_HOLD_MS = 45000;
 static const float AI_JOIN_HOLD_MS = 60000;
 float m_fAIHoldStart = -1; // (regress test resets it)
 protected ref map<int, float> m_PlayerSeenAt = new map<int, float>();
 protected ref array<string> m_AIHeld = {};
 bool HoldAIFor(string factionKey)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fAIHoldStart < 0) m_fAIHoldStart = now;
  if (!ACR_Perf.s_bChecks && now - m_fAIHoldStart < AI_START_HOLD_MS) return true;
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int playerId : players)
  {
   float seen;
   if (!m_PlayerSeenAt.Find(playerId, seen)) { seen = now; m_PlayerSeenAt.Set(playerId, now); }
   if (now - seen >= AI_JOIN_HOLD_MS) continue;
   Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
   if (faction && faction.GetFactionKey() == factionKey) return true;
   if (!faction)
   {
    string forced = ACR_PlayerSide.Key();
    if (forced == "" || forced == factionKey) return true;
   }
  }
  return false;
 }

 // Hunt the known enemy group nearest to point (the side's intel, no map hacks).
 string Hunt(ACR_Squad squad, vector point)
 {
  ACR_Intel intel = IntelFor(squad.GetFaction().GetFactionKey());
  ACR_Contact contact;
  if (intel) contact = intel.ContactNear(point, 120);
  if (!contact) return squad.GetName() + ": no known enemy there";
  string what = ACR_Intel.Describe(contact.m_iKind, contact.m_iCount);
  squad.Hunt(contact.m_Squad, contact.m_Body, contact.m_Pos, what);
  PrintFormat("[ACR] HUNT %1 -> %2 at %3", squad.GetName(), what, contact.m_Pos);
  return squad.GetName() + ": hunting " + what;
 }

 // What each side has seen of the enemy (see ACR_Intel).
 ACR_Intel IntelFor(string factionKey)
 {
  if (factionKey.IsEmpty()) return null;
  ACR_Intel intel;
  if (!m_Intel.Find(factionKey, intel))
  {
   intel = new ACR_Intel(this, factionKey);
   m_Intel.Insert(factionKey, intel);
  }
  return intel;
 }

 protected void TickIntel()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
  {
   ACR_Intel intel = IntelFor(settings.m_sFaction);
   intel.Tick();
   string radio = intel.PopRadio(now);
   if (!radio.IsEmpty()) SendRadio(settings.m_sFaction, radio);
  }
 }

 // A short radio call in the chat of every player on that side.
 void SendRadio(string factionKey, string message)
 {
  PrintFormat("[ACR] RADIO %1: %2", factionKey, message);
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!faction || faction.GetFactionKey() != factionKey) continue;
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (pc) pc.ACR_SendRadio(message);
  }
 }

 bool IsAIInCommand(string factionKey)
 {
  ACR_AICommander ai;
  return m_AICommanders.Find(factionKey, ai) && ai.m_bActive;
 }

 // ================================================================ command rules (server)

 array<int> GetCommanders(Faction faction)
 {
  if (!faction) return null;
  array<int> list;
  if (!m_Commanders.Find(faction.GetFactionKey(), list))
  {
   list = {};
   m_Commanders.Insert(faction.GetFactionKey(), list);
  }
  return list;
 }

 bool IsCommander(int playerId)
 {
  array<int> list = GetCommanders(SCR_FactionManager.SGetPlayerFaction(playerId));
  return list && list.Contains(playerId);
 }

 // Drop commanders who left or switched sides; if a faction has players but
 // no commander, the first one who hasn't opted out takes command.
 void EnsureCommanders()
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (string key, array<int> list : m_Commanders)
  {
   for (int i = list.Count() - 1; i >= 0; i--)
   {
    Faction faction = SCR_FactionManager.SGetPlayerFaction(list[i]);
    if (!players.Contains(list[i]) || !faction || faction.GetFactionKey() != key) list.RemoveOrdered(i);
   }
  }
  for (int i = m_LeftCommand.Count() - 1; i >= 0; i--)
   if (!players.Contains(m_LeftCommand[i])) m_LeftCommand.RemoveOrdered(i);
  foreach (int id : players)
  {
   Faction playerFaction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!playerFaction || m_LeftCommand.Contains(id)) continue;
   array<int> factionCommanders = GetCommanders(playerFaction);
   if (!factionCommanders.IsEmpty()) continue;
   factionCommanders.Insert(id);
   Command().OnTook(id, playerFaction.GetFactionKey(), true);
   Notify(id, "You are in command");
   PrintFormat("[ACR] Commander: %1 takes command of %2", GetGame().GetPlayerManager().GetPlayerName(id), playerFaction.GetFactionKey());
  }
 }

 string CommandStatus(int playerId)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
  array<int> list = GetCommanders(faction);
  if (!list || list.IsEmpty()) return "Command: AI";
  if (m_SharedCommand)
  {
   array<int> ids = {};
   array<string> tags = {};
   Command().TagsOf(faction.GetFactionKey(), ids, tags);
   string names = "";
   foreach (string tag : tags)
   {
    if (!names.IsEmpty()) names += ", ";
    names += tag;
   }
   return "Shared command: " + names;
  }
  if (list.Contains(playerId)) return "You are in command";
  return string.Format("Command: %1", GetGame().GetPlayerManager().GetPlayerName(list[0]));
 }

 string TakeCommand(SCR_PlayerController pc)
 {
  int id = pc.GetPlayerId();
  array<int> list = GetCommanders(SCR_FactionManager.SGetPlayerFaction(id));
  if (!list) return "You have no faction";
  if (list.Contains(id)) return "You are already in command";
  if (!m_SharedCommand && !list.IsEmpty())
   return string.Format("%1 is in command", GetGame().GetPlayerManager().GetPlayerName(list[0]));
  m_LeftCommand.RemoveItem(id);
  list.Insert(id);
  Command().OnTook(id, SCR_FactionManager.SGetPlayerFaction(id).GetFactionKey(), false);
  return "You are in command";
 }

 string LeaveCommand(SCR_PlayerController pc)
 {
  int id = pc.GetPlayerId();
  array<int> list = GetCommanders(SCR_FactionManager.SGetPlayerFaction(id));
  if (!list || !list.Contains(id)) return "You are not in command";
  string tag = Command().TagOf(id);
  list.RemoveItem(id);
  Command().OnLeft(id, SCR_FactionManager.SGetPlayerFaction(id).GetFactionKey(), tag);
  if (!m_LeftCommand.Contains(id)) m_LeftCommand.Insert(id);
  EnsureCommanders();
  return "You left command";
 }

 void NotifyFaction(string factionKey, string message)
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   if (faction && faction.GetFactionKey() == factionKey) Notify(id, message);
  }
 }

 void Notify(int playerId, string message)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (pc) pc.ACR_Notify(message);
 }

 // ================================================================ requests (server)

 // The game stops a possession whenever an editor opens or changes mode. We keep it
 // unless full Game Master (EDIT) is open: building mode and the mode resets after
 // closing it must not drop the possessed soldier.
 static bool EditorStopsPossession(SCR_EditorManagerEntity editor)
 {
  return editor && editor.IsOpened() && editor.GetCurrentMode() == EEditorMode.EDIT;
 }

 static bool EditorOpenFor(int playerId)
 {
  SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
  if (!core) return false;
  SCR_EditorManagerEntity editor = core.GetEditorManager(playerId);
  return editor && editor.IsOpened();
 }


 // ---------------------------------------------------------------- forwarders
 // (requests and requisition live in ACR_Requests / ACR_Requisition)
 ref ACR_Requests m_Requests = new ACR_Requests(this);
 ref ACR_Requisition m_Requisition = new ACR_Requisition(this);
 void CheckLandingZones() { m_Requests.CheckLandingZones(); }
 string HandleRequest(SCR_PlayerController pc, int command, RplId targetId, vector point) { return m_Requests.HandleRequest(pc, command, targetId, point); }
 vector ClearSpotNear(ACR_Base base, float minDistance, float maxDistance, float clearance) { return m_Requisition.ClearSpotNear(base, minDistance, maxDistance, clearance); }
 ACR_Base DeployBaseAt(string factionKey, vector position) { return m_Requisition.DeployBaseAt(factionKey, position); }
 static bool HasShovel(IEntity character) { return ACR_Requisition.HasShovel(character); }
 string Purchase(SCR_PlayerController pc, int entryIndex, string baseLabel, vector spot = "0 0 0", bool hasSpot = false) { return m_Requisition.Purchase(pc, entryIndex, baseLabel, spot, hasSpot); }
 string PurchaseAt(string key, int entryIndex, ACR_Base base, vector spot = "0 0 0", bool hasSpot = false) { return m_Requisition.PurchaseAt(key, entryIndex, base, spot, hasSpot); }
 string Reinforce(SCR_PlayerController pc, RplId groupId) { return m_Requisition.Reinforce(pc, groupId); }
 int RestockAmmo(ACR_Squad squad, ACR_Base base) { return m_Requisition.RestockAmmo(squad, base); }
 void SpawnUnit(string name, ResourceName group, ResourceName vehicleprefab, bool crewed, ResourceName passengers, vector vehiclePos, vector groupPos) { m_Requisition.SpawnUnit(name, group, vehicleprefab, crewed, passengers, vehiclePos, groupPos); }

 // ================================================================ gunner drives (server)

 protected ref map<IEntity, ref ACR_GunnerDrive> m_Drives = new map<IEntity, ref ACR_GunnerDrive>();

 void GunnerDriveInput(SCR_PlayerController pc, IEntity vehicle, float throttle, float steer)
 {
  if (!pc) return;
  GunnerDriveInputFor(pc.GetControlledEntity(), pc.GetPlayerId(), SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId()), vehicle, throttle, steer);
 }

 // (split out so srv:drive can feed it as the AI gunner)
 void GunnerDriveInputFor(IEntity body, int playerId, Faction faction, IEntity vehicle, float throttle, float steer)
 {
  if (!vehicle) return;
  // A running session: the driver seat is unhooked then, so DrivableFrom no longer
  // finds the driver and every input after the first was dropped (the vehicle kept
  // the first key for good; 1 Oct live tests). Check the stored seat instead.
  ACR_GunnerDrive drive = m_Drives.Get(vehicle);
  if (drive)
  {
   if (!drive.StillGunning(body)) return;
  }
  else
  {
   ChimeraCharacter driver;
   if (ACR_GunnerDrive.DrivableFrom(body, driver) != vehicle) return;
   ACR_Squad squad = FindSquadBySoldier(driver);
   if (!squad || squad.GetFaction() != faction) return;
   if (throttle == 0 && steer == 0) return;
   drive = new ACR_GunnerDrive();
   drive.m_Vehicle = vehicle;
   drive.m_Driver = driver;
   drive.m_Squad = squad;
   drive.m_PlayerId = playerId;
   drive.Begin();
   m_Drives.Insert(vehicle, drive);
   PrintFormat("[ACR] GUNNER DRIVE start: %1 (player %2)", squad.GetName(), playerId);
  }
  drive.m_PlayerId = playerId;
  drive.m_Throttle = Math.Clamp(throttle, -1, 1);
  drive.m_Steer = Math.Clamp(steer, -1, 1);
  drive.m_LastInput = GetGame().GetWorld().GetWorldTime();
 }

 bool IsGunnerDriven(IEntity vehicle) { return m_Drives.Contains(vehicle); }

 // Server test hook: a gunner drive without a player.
 ACR_GunnerDrive TestDrive(IEntity vehicle, ChimeraCharacter driver, ACR_Squad squad)
 {
  ACR_GunnerDrive drive = new ACR_GunnerDrive();
  drive.m_Vehicle = vehicle;
  drive.m_Driver = driver;
  drive.m_Squad = squad;
  drive.m_bTest = true;
  drive.Begin();
  m_Drives.Insert(vehicle, drive);
  return drive;
 }

 void SimulateStep(int source = 0)
 {
  if (m_Drives.IsEmpty() || !Replication.IsServer()) return;
  foreach (IEntity vehicle, ACR_GunnerDrive drive : m_Drives) drive.Reapply(source);
 }

 // Every frame on the server (physics inputs must be refreshed continuously).
 void ServerFrame(float timeSlice)
 {
  if (!Replication.IsServer()) return;
  int t = System.GetTickCount();
  if (!m_Supports) m_Supports = new ACR_Supports(this);
  m_Supports.Frame(timeSlice);
  ACR_Perf.Lap("frames", t);
  CheckLandingZones();
  BBB.Frame(); // backing stuck vehicles out (inputs every frame)
  if (m_Drives.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<IEntity> ended = {};
  foreach (IEntity vehicle, ACR_GunnerDrive drive : m_Drives)
   if (!drive.Apply(now)) ended.Insert(vehicle);
  foreach (IEntity vehicle : ended)
  {
   ACR_GunnerDrive drive = m_Drives.Get(vehicle);
   drive.End();
   string squadName = "?";
   if (drive.m_Squad) squadName = drive.m_Squad.GetName();
   PrintFormat("[ACR] GUNNER DRIVE end: %1 (%2)", squadName, drive.m_EndReason);
   // Say so when it stops by itself (after a crash killed the driver, A/D went back to
   // turning the turret with no word why, 1 Oct live test).
   if (drive.m_EndReason == "vehicle or driver lost") Notify(drive.m_PlayerId, "Gunner drive ended: the driver is down.");
   else if (drive.m_EndReason == "idle") Notify(drive.m_PlayerId, "Gunner drive ended (no input for 20 s): the AI driver has the wheel again.");
   m_Drives.Remove(vehicle);
  }
 }

 // ================================================================ local input (every machine)

 protected IEntity m_LastDriveVehicle;
 protected IEntity m_LastRide;

 void OnFrame()
 {
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
   ACR_MapHandoff.ClearCharacterInput(localBody);
  }
  // Narrow view after leaving the map in a possessed soldier: the map gadget's "focus"
  // (which narrows the field of view) stayed on when possession changed outside our
  // own hand-off (the game drops possession itself when build mode opens). Once the
  // map is shut, nothing that legitimately focuses can be in use without a gadget in hand.
  if (!mapOpen && (m_WasMapOpen || localBody != m_LastLocalBody) && pc.GetGadgetFocus())
  {
   ChimeraCharacter focusBody = ChimeraCharacter.Cast(localBody);
   bool gadgetInHand = focusBody && focusBody.GetCharacterController() && focusBody.GetCharacterController().IsGadgetInHands();
   bool mapInHand = false;
   if (gadgetInHand)
   {
    SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(focusBody);
    IEntity mapItem;
    if (gadgets) mapItem = gadgets.GetGadgetByType(EGadgetType.MAP);
    mapInHand = mapItem && focusBody.GetCharacterController().GetAttachedGadgetAtLeftHandSlot() == mapItem;
   }
   if (!gadgetInHand || mapInHand)
   {
    if (mapInHand) ACR_MapHandoff.Release(pc, localBody);
    pc.SetGadgetFocus(false);
    Print("[ACR] Map focus cleared after the map closed");
   }
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
  // Gunner drives: WASD in a turret steers the AI driver (Arma 3 style).
  ChimeraCharacter driveDriver;
  IEntity driveVehicle;
  if (!mapOpen) driveVehicle = ACR_GunnerDrive.DrivableFrom(localBody, driveDriver);
  // Mid-session the seat is unhooked (on a hosting player's own machine too), so
  // DrivableFrom finds no driver: keep driving while still in that vehicle's turret.
  if (!mapOpen && !driveVehicle && m_LastDriveVehicle && ACR_GunnerDrive.TurretVehicleOf(localBody) == m_LastDriveVehicle)
   driveVehicle = m_LastDriveVehicle;
  float driveThrottle, driveSteer;
  if (driveVehicle)
  {
   input.ActivateContext("ACR_GunnerContext");
   driveThrottle = input.GetActionValue("ACR_DriveForward") - input.GetActionValue("ACR_DriveBack");
   driveSteer = input.GetActionValue("ACR_DriveRight") - input.GetActionValue("ACR_DriveLeft");
   if (driveVehicle != m_LastDriveVehicle)
    SCR_HintManagerComponent.ShowCustomHint("Gunner seat: W/S drive forward/back, A/D steer. The AI driver takes over again when you give the squad a map order.", "AC: Actual", 8, true);
  }
  m_LastDriveVehicle = driveVehicle;
  pc.ACR_UpdateGunnerDrive(driveVehicle, driveThrottle, driveSteer);
  pc.ACR_UpdateWorldPing(mapOpen);
  if (!m_HintShown)
  {
   m_HintShown = true;
   SCR_HintManagerComponent.ShowCustomHint("Map: click a squad to select, right-click to order | F7: possess aimed soldier (or hovered marker) | F8: command unit | F9: map", "AC: Actual", 30, true);
  }
  if (input.GetActionTriggered("ACR_Move"))
  {
   if (mapOpen)
   {
    float x, z;
    mapEntity.GetMapCursorWorldPosition(x, z);
    pc.ACR_Request(ACR_ERequest.MOVE_TO, pc.ACR_GetSelectedGroup(), Vector(x, 0, z));
   }
   else pc.ACR_Request(ACR_ERequest.ADVANCE);
  }
  // F6: in a vehicle, drop the passengers; otherwise the old test rally.
  if (input.GetActionTriggered("ACR_Rally"))
  {
   if (ACR_Transport.VehicleOf(localBody)) pc.ACR_Request(ACR_ERequest.UNLOAD_HERE);
   else pc.ACR_Request(ACR_ERequest.RALLY);
  }
  IEntity rideNow = ACR_Transport.VehicleOf(localBody);
  if (rideNow != m_LastRide && rideNow && ACR_Transport.SeatOf(localBody) == ECompartmentType.PILOT)
   SCR_HintManagerComponent.ShowCustomHint("Transporting troops? F6 drops every passenger squad (press twice if you are not down yet). Map: select them, 'Unload at LZ', right-click the LZ to have them jump out on landing.", "AC: Actual", 8, true);
  m_LastRide = rideNow;
  if (input.GetActionTriggered("ACR_Possess"))
  {
   if (mapOpen) pc.ACR_PossessMapCursor();
   else pc.ACR_PossessAimed();
  }
  if (input.GetActionTriggered("ACR_Return")) pc.ACR_Request(ACR_ERequest.RETURN);
  if (input.GetActionTriggered("ACR_Map"))
  {
   if (mapOpen) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
   else GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  }
 }
}
