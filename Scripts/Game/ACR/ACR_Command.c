// Shared command (server): who the commanders are called on the map, what
// each last did, what each has selected, and the side's command feed lines.
// The command rules themselves (who may command) stay in ACR_Manager.
class ACR_Command : Managed
{
 static const int TAG_LENGTH = 4;
 static const float RECENT_MS = 30000;   // an order this fresh is "theirs" (override warning)

 protected ACR_Manager m_Manager;
 protected ref map<int, string> m_LastAction = new map<int, string>();
 protected ref map<int, float> m_LastActionAt = new map<int, float>();
 protected ref map<int, ref array<RplId>> m_Selections = new map<int, ref array<RplId>>();
 protected ref map<int, int> m_TakenAt = new map<int, int>();   // order in which players took command (Cmd1, Cmd2...)
 protected int m_iTakes;

 void ACR_Command(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 // ---------------------------------------------------------------- short names
 // "PoppiPoppins" -> "Popp", "[ACR] xX_Hexxyz_Xx" -> "Hexx", "propst-design" -> "prop".
 // Clan tags in brackets go, digits and symbols split words, "xX"-style
 // decorations are dropped, and the cut is made at whole UTF-8 characters.

 // Byte (or character) code at i, 0..255 for bytes.
 protected static int Code(string text, int i)
 {
  int c = text.ToAscii(i);
  if (c < 0) c += 256;
  return c;
 }

 protected static bool IsLetter(int c)
 {
  return (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c >= 128;
 }

 // A UTF-8 continuation byte (never the start of a character).
 protected static bool IsContinuation(int c)
 {
  return c >= 128 && c < 192;
 }

 // The name with brackets, digits, symbols and decorations removed.
 static string BaseName(string name)
 {
  // 1. Drop anything inside brackets.
  string open = "[({<";
  string close = "])}>";
  string plain = "";
  int depth = 0;
  for (int i = 0; i < name.Length(); i++)
  {
   string ch = name.Get(i);
   if (open.Contains(ch)) { depth++; continue; }
   if (close.Contains(ch)) { if (depth > 0) depth--; continue; }
   if (depth == 0) plain += ch;
  }
  // 2. Words of letters only.
  array<string> words = {};
  string word = "";
  for (int j = 0; j < plain.Length(); j++)
  {
   if (IsLetter(Code(plain, j))) { word += plain.Get(j); continue; }
   if (!word.IsEmpty()) words.Insert(word);
   word = "";
  }
  if (!word.IsEmpty()) words.Insert(word);
  // 3. Without "x", "xX", "Xx" decorations (unless that is all there is).
  array<string> kept = {};
  foreach (string w : words)
  {
   string lower = w;
   lower.ToLower();
   if (lower == "x" || lower == "xx") continue;
   kept.Insert(w);
  }
  if (kept.IsEmpty()) kept = words;
  string joined = "";
  foreach (string k : kept) joined += k;
  return joined;
 }

 // The first 'count' characters, never cutting a multi-byte character in two.
 static string Prefix(string text, int count)
 {
  int seen = 0;
  for (int i = 0; i < text.Length(); i++)
  {
   if (IsContinuation(Code(text, i))) continue;
   if (seen == count) return text.Substring(0, i);
   seen++;
  }
  return text;
 }

 // Characters in the text (whole characters, not bytes).
 static int CharCount(string text)
 {
  int n = 0;
  for (int i = 0; i < text.Length(); i++) if (!IsContinuation(Code(text, i))) n++;
  return n;
 }

 // Tags for one side's commanders, in the same order as names. Clashing tags
 // grow a letter at a time until they differ; identical names get 1, 2...
 // A name with nothing usable becomes Cmd<n>, n = order of taking command.
 static void MakeTags(array<string> names, array<int> takeOrder, notnull array<string> tags)
 {
  tags.Clear();
  array<string> bases = {};
  array<int> lengths = {};
  foreach (int i, string name : names)
  {
   string base = BaseName(name);
   if (base.IsEmpty())
   {
    int n = i + 1;
    if (takeOrder && i < takeOrder.Count()) n = takeOrder[i];
    base = "Cmd" + n.ToString();
   }
   bases.Insert(base);
   lengths.Insert(TAG_LENGTH);
   tags.Insert(Prefix(base, TAG_LENGTH));
  }
  // Grow clashing tags.
  for (int round = 0; round < 32; round++)
  {
   bool grew = false;
   for (int a = 0; a < tags.Count(); a++)
   {
    for (int b = a + 1; b < tags.Count(); b++)
    {
     if (tags[a] != tags[b]) continue;
     if (lengths[a] < CharCount(bases[a])) { lengths[a] = lengths[a] + 1; grew = true; }
     if (lengths[b] < CharCount(bases[b])) { lengths[b] = lengths[b] + 1; grew = true; }
    }
   }
   if (!grew) break;
   for (int t = 0; t < tags.Count(); t++) tags[t] = Prefix(bases[t], lengths[t]);
  }
  // Still the same (same names): number them.
  for (int x = 0; x < tags.Count(); x++)
  {
   int same = 0;
   for (int y = x + 1; y < tags.Count(); y++) if (tags[y] == tags[x]) same++;
   if (same == 0) continue;
   string original = tags[x];
   int number = 1;
   for (int z = x; z < tags.Count(); z++)
   {
    if (tags[z] != original) continue;
    tags[z] = original + number.ToString();
    number++;
   }
  }
 }

 // Commander ids and tags for a side, in command order.
 void TagsOf(string factionKey, notnull array<int> ids, notnull array<string> tags)
 {
  ids.Clear();
  tags.Clear();
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
  array<int> list = m_Manager.GetCommanders(faction);
  if (!list) return;
  array<string> names = {};
  array<int> order = {};
  foreach (int id : list)
  {
   ids.Insert(id);
   names.Insert(GetGame().GetPlayerManager().GetPlayerName(id));
   int taken;
   if (!m_TakenAt.Find(id, taken)) taken = ids.Count();
   order.Insert(taken);
  }
  MakeTags(names, order, tags);
 }

 // One commander's tag ("" if not a commander).
 string TagOf(int playerId)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
  if (!faction) return "";
  array<int> ids = {};
  array<string> tags = {};
  TagsOf(faction.GetFactionKey(), ids, tags);
  int i = ids.Find(playerId);
  if (i >= 0) return tags[i];
  // Not a commander (a ping from the field): the short name on its own.
  string base = BaseName(GetGame().GetPlayerManager().GetPlayerName(playerId));
  if (base.IsEmpty()) return "Player" + playerId.ToString();
  return Prefix(base, TAG_LENGTH);
 }

 // ---------------------------------------------------------------- events
 void OnTook(int playerId, string factionKey, bool automatic)
 {
  m_iTakes++;
  m_TakenAt.Set(playerId, m_iTakes);
  string how = "took command";
  if (automatic) how = "is in command";
  Feed(factionKey, string.Format("%1 %2", TagOf(playerId), how));
 }

 void OnLeft(int playerId, string factionKey, string tag)
 {
  m_Selections.Remove(playerId);
  m_LastAction.Remove(playerId);
  m_LastActionAt.Remove(playerId);
  Feed(factionKey, string.Format("%1 left command", tag));
 }

 void Note(int playerId, string action)
 {
  m_LastAction.Set(playerId, action);
  m_LastActionAt.Set(playerId, GetGame().GetWorld().GetWorldTime());
 }

 // e.g. "Bravo: moving near Hill" (the client adds the name and how long ago)
 string LastAction(int playerId)
 {
  string action;
  m_LastAction.Find(playerId, action);
  return action;
 }

 float LastActionAt(int playerId)
 {
  float at;
  m_LastActionAt.Find(playerId, at);
  return at;
 }

 void Feed(string factionKey, string line)
 {
  ACR_Intel intel = m_Manager.IntelFor(factionKey);
  if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), line);
 }

 // ---------------------------------------------------------------- selections
 void SetSelection(int playerId, array<RplId> groups)
 {
  array<RplId> copy = {};
  int n;
  foreach (RplId id : groups)
  {
   if (n >= 24) break; // a sane limit
   copy.Insert(id);
   n++;
  }
  m_Selections.Set(playerId, copy);
 }

 void SelectionsOf(string factionKey, notnull array<RplId> groups, notnull array<int> owners)
 {
  groups.Clear();
  owners.Clear();
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
  array<int> list = m_Manager.GetCommanders(faction);
  if (!list) return;
  foreach (int id : list)
  {
   array<RplId> picked;
   if (!m_Selections.Find(id, picked)) continue;
   foreach (RplId group : picked)
   {
    groups.Insert(group);
    owners.Insert(id);
   }
  }
 }
}
