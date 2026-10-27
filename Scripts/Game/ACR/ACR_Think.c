// AC:A Think (26 Oct 2026, Jacob): what each AI commander is thinking, live, for the host and
// admins, instead of reading "[ACR] AI <side>" lines out of script.log. Opened from the map's
// Debug box ("AC:A Think") or chat "/acr think [west|east|off] [log]". A card on the left of
// the screen (map and 3D) shows one side at a time: the status line (squads free / defending /
// attacking, attacks with phase, staging grids and base of fire, refits, RP, support and build
// budget, doctrine), then either every squad of that side (task, order, target, how long since
// its last order and how many in 5 min, waypoint recovery) or its last 20 decisions.
// The AI runs on the server: while the card is open the client asks every POLL_MS and the
// server answers that one player (admin check and rate limit on every request), nothing else.
class ACR_Think
{
 static const int POLL_MS = 3000;
 static const int MAX_SQUADS = 30;
 static const int MAX_CHARS = 150; // longer lines are cut, the card stays readable
 static const float LOOP_WINDOW_MS = 300000;
 static const int LOOP_ORDERS = 8;   // this many new orders in 5 min reads as a loop

 // ---------------------------------------------------------------- server

 // Match clock for the decision log, e.g. "1:04:09" or "12:30".
 static string Clock()
 {
  int seconds = (int)(GetGame().GetWorld().GetWorldTime() / 1000);
  int hours = seconds / 3600;
  int minutes = (seconds / 60) - hours * 60;
  seconds = seconds - (seconds / 60) * 60;
  string mm = minutes.ToString();
  if (hours > 0 && minutes < 10) mm = "0" + mm;
  string ss = seconds.ToString();
  if (seconds < 10) ss = "0" + ss;
  if (hours > 0) return string.Format("%1:%2:%3", hours, mm, ss);
  return string.Format("%1:%2", mm, ss);
 }

 static string SideKey(int side)
 {
  if (side == 1) return ACR_FactionSelection.East();
  return ACR_FactionSelection.West();
 }

 // side 0 West, 1 East; page 0 squads, 1 decisions. Lines joined by "\n".
 static string Report(ACR_Manager manager, int side, int page)
 {
  array<string> lines = {};
  string key = SideKey(side);
  string pageName = "squads";
  if (page == 1) pageName = "decisions";
  lines.Insert(string.Format("AC:A THINK  %1  [%2]  %3", key, pageName, Clock()));
  ACR_AICommander ai = manager.AICommanderFor(key);
  if (!ai) lines.Insert("No AI commander for this side");
  else
  {
   if (!ai.m_bActive) lines.Insert("AI not in command (a player commands this side)");
   if (manager.m_Debug && manager.m_Debug.IsPaused(key)) lines.Insert("PAUSED from the debug menu: squads keep their orders");
   string status = ai.StatusText();
   string head = string.Format("[ACR] AI %1 status: ", key);
   if (status.StartsWith(head)) status = status.Substring(head.Length(), status.Length() - head.Length());
   array<string> parts = {};
   status.Split(" | ", parts, true);
   foreach (string part : parts) lines.Insert(part.Trim());
   array<string> attacks = {};
   ai.ThinkAttacks(attacks);
   foreach (string attack : attacks) lines.Insert("  " + attack);
  }
  lines.Insert("");
  if (page == 1) Decisions(ai, lines);
  else Squads(manager, key, lines);
  string text;
  foreach (int i, string line : lines)
  {
   if (line.Length() > MAX_CHARS) line = line.Substring(0, MAX_CHARS - 3) + "...";
   if (i > 0) text += "\n";
   text += line;
  }
  return text;
 }

 protected static void Decisions(ACR_AICommander ai, notnull array<string> lines)
 {
  array<string> said = {};
  if (ai) ai.ThinkDecisions(said);
  lines.Insert(string.Format("Last %1 decisions, newest first:", said.Count()));
  for (int i = said.Count() - 1; i >= 0; i--) lines.Insert(said[i]);
 }

 protected static void Squads(ACR_Manager manager, string key, notnull array<string> lines)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  int shown, total;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != key) continue;
   total++;
   if (shown >= MAX_SQUADS) continue;
   shown++;
   lines.Insert(SquadLine(squad, now));
  }
  lines.Insert(string.Format("%1 squad(s)", total));
  if (total > shown) lines.Insert(string.Format("(%1 more not shown)", total - shown));
 }

 static string SquadLine(ACR_Squad squad, float now)
 {
  string task = typename.EnumToString(ACR_EAITask, squad.m_iAITask);
  if (squad.HasPlayer()) task = "PLAYER";
  else if (squad.m_bMortar) task = "MORTAR";
  if (squad.m_AITarget) task += " " + squad.m_AITarget.m_sName;
  string ordered = "AI";
  if (squad.m_iOrderedBy > 0) ordered = "player";
  int recent = squad.OrdersSince(now - LOOP_WINDOW_MS);
  string loop;
  if (recent >= LOOP_ORDERS) loop = " LOOP?";
  string order = squad.m_Order;
  if (squad.m_MoveTarget != vector.Zero) order += " @" + ACR_Manager.Grid(squad.m_MoveTarget);
  string when = "no order yet";
  if (squad.m_fOrderedAt > 0) when = string.Format("%1 s ago by %2, %3 in 5 min%4", (int)((now - squad.m_fOrderedAt) / 1000), ordered, recent, loop);
  string line = string.Format("%1 %2%% | %3 | %4 | %5", squad.GetName(), (int)(squad.StrengthRatio() * 100), task, order, when);
  string movement = squad.ThinkMovement();
  if (!movement.IsEmpty()) line += " | " + movement;
  return line;
 }

 // ---------------------------------------------------------------- client
 // 0 off; otherwise 1 + side * 2 + page.
 protected static int s_iMode;
 protected static Widget s_Root;
 protected static ref array<TextWidget> s_Texts = {};
 protected static const int MAX_LINES = 50;
 protected static const ResourceName TEXT_LAYOUT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";

 static bool IsOpen() { return s_iMode > 0; }

 // Off -> West squads -> West decisions -> East squads -> East decisions -> off.
 static void Cycle()
 {
  Open((s_iMode + 1) % 5);
 }

 // "/acr think [west|east|off] [log|squads]"; no argument cycles.
 static void Command(array<string> args)
 {
  if (args.IsEmpty()) { Cycle(); return; }
  int side = -1;
  int page = 0;
  foreach (string arg : args)
  {
   arg.ToLower();
   if (arg == "off" || arg == "close") { Open(0); return; }
   if (arg == "west" || arg == "w") side = 0;
   else if (arg == "east" || arg == "e") side = 1;
   else if (arg == "log" || arg == "decisions") page = 1;
   else if (arg == "squads") page = 0;
  }
  if (side < 0 && s_iMode > 0) side = (s_iMode - 1) / 2;
  if (side < 0) side = 0;
  Open(1 + side * 2 + page);
 }

 protected static void Open(int mode)
 {
  s_iMode = mode;
  GetGame().GetCallqueue().Remove(Poll);
  if (mode == 0)
  {
   if (s_Root) s_Root.SetVisible(false);
   return;
  }
  Show("AC:A THINK: asking the server...");
  Poll();
  GetGame().GetCallqueue().CallLater(Poll, POLL_MS, true);
 }

 protected static void Poll()
 {
  if (s_iMode == 0) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_RequestThink((s_iMode - 1) / 2, (s_iMode - 1) % 2);
 }

 // The server's answer (or its refusal, which closes the card).
 static void Received(bool allowed, string text)
 {
  if (s_iMode == 0) return;
  if (!allowed)
  {
   Open(0);
   SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
   if (chat) chat.OnNewMessage("[ACR] " + text);
   return;
  }
  Show(text);
 }

 protected static void Show(string text)
 {
  if (!Build()) return;
  array<string> lines = {};
  text.Split("\n", lines, false);
  foreach (int i, TextWidget widget : s_Texts)
  {
   bool used = i < lines.Count();
   widget.SetVisible(used);
   if (used) widget.SetText(lines[i]);
  }
  s_Root.SetVisible(true);
 }

 protected static bool Build()
 {
  if (s_Root) return true;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws) return false;
  s_Root = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, ws);
  FrameSlot.SetAnchorMin(s_Root, 0, 0);
  FrameSlot.SetAnchorMax(s_Root, 0, 0);
  FrameSlot.SetAlignment(s_Root, 0, 0);
  FrameSlot.SetPos(s_Root, 16, 64);
  FrameSlot.SetSizeToContent(s_Root, true);
  s_Root.SetZOrder(1100); // over the map and its panels
  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(0.02, 0.13, 0.30, 0.80), 0, s_Root);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, s_Root);
  AlignableSlot.SetPadding(column, 8, 6, 8, 6);
  for (int i = 0; i < MAX_LINES; i++)
  {
   TextWidget text = TextWidget.Cast(ws.CreateWidgets(TEXT_LAYOUT, column));
   if (!text) continue;
   text.SetFlags(WidgetFlags.IGNORE_CURSOR);
   text.SetExactFontSize(12);
   s_Texts.Insert(text);
  }
  return true;
 }

 // Visible state (tests read it).
 static string LineText(int index)
 {
  if (!s_Root || index < 0 || index >= s_Texts.Count() || !s_Texts[index].IsVisible()) return "";
  return s_Texts[index].GetText();
 }
}

modded class SCR_PlayerController
{
 protected float m_fACR_ThinkAt;

 void ACR_RequestThink(int side, int page)
 {
  Rpc(ACR_ThinkServer, side, page);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_ThinkServer(int side, int page)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_fACR_ThinkAt < 1000) return;
  m_fACR_ThinkAt = now;
  if (!ACR_Manager.IsHostOrAdmin(GetPlayerId()))
  {
   Rpc(ACR_ThinkOwner, false, "AC:A Think: host and admins only");
   return;
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  Rpc(ACR_ThinkOwner, true, ACR_Think.Report(manager, side, page));
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_ThinkOwner(bool allowed, string text)
 {
  ACR_Think.Received(allowed, text);
 }
}
