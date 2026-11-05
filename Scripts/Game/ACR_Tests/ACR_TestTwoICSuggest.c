// -acrTwoICSuggestTest 1 (server-capable, ~2 min): "2IC suggests" and its Establish FOB help (#14 v1
// steps 5 and 6, ACR_TwoICSuggest). No AI commander, the 2IC on. The FOB here card: the HQ, the
// upgrade and defence spots and the verdict, at most every 250 ms a player. The sites behind one of
// our bases, scored as the AI commander scores them: the best three, best first, numbered with their
// reasons. A suggestion: a Suggestions line with Accept and Dismiss and a marker on the map; Accept
// with no construction truck says so and stays; Dismiss ends it, and that front isn't suggested again.
class ACR_TestTwoICSuggest
{
 static const int CHECKS = 9;
 static const ResourceName CONSTRUCTION = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";
 static int s_iStep;
 static int s_iStepAt;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_TwoIC.ResetForTest();
   ACR_Options.Set("acr2IC", "1");
  }
  if (tick > 150) ACR_TestRegress.Abort("twoicsuggest", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!ai || !bravo || !bravo.GetAnchor())
  {
   ACR_TestRegress.Abort("twoicsuggest", "no US AI commander or no Bravo");
   return;
  }
  int waited = tick - s_iStepAt;

  if (s_iStep == 0)
  {
   // FOB here, 300 m from Bravo.
   vector spot = ACR_Manager.Ground(bravo.GetAnchor().GetOrigin() + Vector(300, 0, 0));
   string card = ACR_TwoICSuggest.FobHere(5, spot);
   string again = ACR_TwoICSuggest.FobHere(5, spot);
   bool lines = card.StartsWith("FOB HERE") && card.Contains(string.Format("/%1", ACR_FobSlots.SLOT_COUNT)) && card.Contains(string.Format("/%1", ACR_FobDefences.SPOT_COUNT)) && (card.Contains("HQ fits") || card.Contains("HQ: ")) && card.Contains("ordered here");
   ACR_TestRegress.Check("twoicsuggest: the FOB here card gives the HQ, the upgrade and defence spots and the verdict, at most every 250 ms", lines && again.IsEmpty(), string.Format("'%1' / again '%2'", card, again));
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   // The sites behind one of our bases (the whole search, as a test may).
   ACR_Base front;
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base && !base.m_bRetired && !base.m_bBuilt && base.m_sOwner == "US") { front = base; break; }
   if (!front)
   {
    ACR_TestRegress.Skip("twoicsuggest: the sites behind a base: the best three, best first, numbered with reasons", "no US base");
    ACR_TestRegress.Skip("twoicsuggest: a suggestion is a Suggestions line with Accept and Dismiss and a marker", "no US base");
    ACR_TestRegress.Skip("twoicsuggest: Accept with no construction truck says so and the suggestion stays", "no US base");
    ACR_TestRegress.Skip("twoicsuggest: Dismiss ends it; that front isn't suggested again", "no US base");
    ACR_TestRegress.Skip("twoicsuggest: Send truck", "no US base");
    ACR_TestRegress.Skip("twoicsuggest: not suggested again while the truck is on its way", "no US base");
    ACR_TestRegress.Skip("twoicsuggest: a sent truck's site keeps a dashed ring", "no US base");
    NextStep(tick);
    s_iStep = 3;
    return;
   }
   array<ref ACR_FobSite> sites = {};
   bool complete = ai.TwoICFobSites(front, -1, sites);
   array<ACR_FobSite> best = {};
   ACR_TwoICSuggest.Top(sites, ACR_TwoICSuggest.SITES_SHOWN, best);
   bool ordered = true;
   for (int i = 1; i < best.Count(); i++)
    if (best[i].m_fScore > best[i - 1].m_fScore) ordered = false;
   array<vector> positions = {};
   array<string> labels = {};
   ACR_TwoICSuggest.SitesFor(sites, front.m_sName, positions, labels);
   bool numbered = labels.Count() == best.Count() && (labels.IsEmpty() || (labels[0].StartsWith("1: ") && labels[0].Contains("m behind the front")));
   string detail = string.Format("behind %1: complete %2, %3 sites, best %4", front.m_sName, complete, sites.Count(), best.Count());
   foreach (string label : labels) detail += " | " + label;
   if (sites.IsEmpty()) ACR_TestRegress.Skip("twoicsuggest: the sites behind a base: the best three, best first, numbered with reasons", detail);
   else ACR_TestRegress.Check("twoicsuggest: the sites behind a base: the best three, best first, numbered with reasons", complete && best.Count() == Math.Min(3, sites.Count()) && ordered && numbered, detail);
   if (best.IsEmpty())
   {
    ACR_TestRegress.Skip("twoicsuggest: a suggestion is a Suggestions line with Accept and Dismiss and a marker", "no clear site");
    ACR_TestRegress.Skip("twoicsuggest: Accept with no construction truck says so and the suggestion stays", "no clear site");
    ACR_TestRegress.Skip("twoicsuggest: Dismiss ends it; that front isn't suggested again", "no clear site");
    ACR_TestRegress.Skip("twoicsuggest: Send truck", "no clear site");
    ACR_TestRegress.Skip("twoicsuggest: not suggested again while the truck is on its way", "no clear site");
    ACR_TestRegress.Skip("twoicsuggest: a sent truck's site keeps a dashed ring", "no clear site");
    NextStep(tick);
    s_iStep = 3;
    return;
   }
   // A suggestion for it.
   ACR_TwoICSuggestion s = ACR_TwoICSuggest.Suggest("US", front, best[0]);
   ACR_TwoICLine line;
   if (s) line = ACR_TwoIC.Find("US", s.m_iId);
   array<vector> rings = {};
   array<string> ringLabels = {};
   array<int> kinds = {};
   ACR_TwoICRequests.Rings("US", rings, ringLabels, kinds);
   bool marker = kinds.Contains(ACR_TwoICSuggest.RING_KIND);
   bool suggested = line && line.m_iKind == ACR_ETwoICLine.SUGGESTION && line.m_sStatus == "Suggested" && (line.m_iActions & ACR_TwoICSuggest.ACT_ACCEPT) != 0 && (line.m_iActions & ACR_TwoICRequests.ACT_CANCEL) != 0 && !line.m_sAnswer.IsEmpty();
   ACR_TestRegress.Check("twoicsuggest: a suggestion is a Suggestions line with Accept and Dismiss and a marker", suggested && marker, string.Format("line '%1' '%2' actions %3; marker %4", TextOf(line), AnswerOf(line), ActionsOf(line), marker));
   // Accept: no construction truck on the range (or one: then it goes).
   string accepted = "";
   if (s) accepted = ACR_TwoICSuggest.ActOn(manager, s, ACR_TwoICSuggest.ACT_ACCEPT);
   bool noTruck = s && line && accepted.Contains("Can't send a truck") && ACR_TwoICSuggest.Find(s.m_iId) != null && line.m_sAnswer.Contains("construction truck");
   bool sent = s && line && accepted.Contains("establishing") && !ACR_TwoICSuggest.Find(s.m_iId) && line.m_sStatus == "Truck sent";
   ACR_TestRegress.Check("twoicsuggest: Accept with no construction truck says so and the suggestion stays (with one, it goes)", noTruck || sent, string.Format("'%1'; line '%2' '%3'", accepted, StatusOf(line), AnswerOf(line)));
   // Dismiss.
   if (s && line && ACR_TwoICSuggest.Find(s.m_iId))
   {
    ACR_TwoICSuggest.ActOn(manager, s, ACR_TwoICRequests.ACT_CANCEL);
    bool ended = !ACR_TwoICSuggest.HasLive("US") && line.m_sStatus == "Dismissed";
    ACR_TwoICSuggest.Tick(manager, "US", ai, true);
    ACR_TestRegress.Check("twoicsuggest: Dismiss ends it; that front isn't suggested again", ended && !ACR_TwoICSuggest.HasLive("US"), string.Format("line '%1' '%2', live %3", StatusOf(line), AnswerOf(line), ACR_TwoICSuggest.HasLive("US")));
   }
   else ACR_TestRegress.Skip("twoicsuggest: Dismiss ends it; that front isn't suggested again", "accepted (a truck went)");
   CheckSendTruck(manager, ai, bravo, front, best[0]);
   NextStep(tick);
   return;
  }

  if (s_iStep == 2 || s_iStep == 3)
  {
   // Not running (the AI commander has the side): a live suggestion is withdrawn.
   ACR_Base any;
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base && !base.m_bRetired && base.m_sOwner == "US") { any = base; break; }
   ACR_FobSite fake = new ACR_FobSite();
   fake.m_vPos = bravo.GetAnchor().GetOrigin() + Vector(0, 0, 700);
   ACR_TwoICSuggestion s2;
   if (any) s2 = ACR_TwoICSuggest.Suggest("US", any, fake);
   ACR_TwoICSuggest.Tick(manager, "US", ai, false);
   ACR_TwoICLine line2;
   if (s2) line2 = ACR_TwoIC.Find("US", s2.m_iId);
   ACR_TestRegress.Check("twoicsuggest: with the AI commander in command a suggestion is withdrawn", s2 && !ACR_TwoICSuggest.HasLive("US") && line2 && line2.m_sStatus == "Withdrawn", string.Format("line '%1' '%2'", StatusOf(line2), AnswerOf(line2)));
   ACR_Options.Unset("acr2IC");
   ACR_TwoIC.ResetForTest();
   ACR_TestRegress.Complete("twoicsuggest", CHECKS);
   s_iStep = 4;
  }
 }

 // pn12d (27 Oct): Send truck sent the truck but the card stayed open, and 0.3 s later the same site
 // was suggested again: the 2IC didn't count the build on its way.
 protected static void CheckSendTruck(ACR_Manager manager, ACR_AICommander ai, ACR_Squad bravo, ACR_Base front, ACR_FobSite site)
 {
  IEntity truck = ACR_Manager.SpawnPrefab(CONSTRUCTION, ACR_Manager.Ground(bravo.GetAnchor().GetOrigin() + Vector(10, 0, 0)));
  if (truck)
  {
   ACR_Vehicles.SetOwner(truck, "US");
   manager.m_Logistics.AddStock(truck, ACR_FobBuild.LOADS_RESERVED);
  }
  ACR_TwoICSuggestion s = ACR_TwoICSuggest.Suggest("US", front, site);
  string answer;
  ACR_TwoICLine line;
  if (s)
  {
   answer = ACR_TwoICSuggest.ActOn(manager, s, ACR_TwoICSuggest.ACT_ACCEPT);
   line = ACR_TwoIC.Find("US", s.m_iId);
  }
  ACR_Squad builder;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US" && squad.OrderState() == ACR_EOrderState.BUILD_FOB) builder = squad;
  // The declutter sketch 3: the chip reads "Truck sent", the answer names the truck.
  bool sent = line && builder && line.m_sStatus == "Truck sent" && (line.m_iActions & ACR_TwoICSuggest.ACT_ACCEPT) == 0 && line.m_sAnswer == builder.GetName() + " is on its way to build it";
  ACR_TestRegress.Check("twoicsuggest: Send truck sends the truck; its line reads Truck sent (which truck) with no Send truck left, so the card closes", sent,
   string.Format("'%1'; line '%2' '%3' actions %4; builder %5", answer, StatusOf(line), AnswerOf(line), ActionsOf(line), builder != null));
  // Its site keeps a dashed ring labelled with the truck's time to it (the declutter sketch 3).
  array<vector> rings = {};
  array<string> ringLabels = {};
  array<int> kinds = {};
  ACR_TwoICRequests.Rings("US", rings, ringLabels, kinds);
  int sentRing = -1;
  string sentLabel = "-";
  float off = -1;
  foreach (int r, int kind : kinds)
  {
   if (kind != ACR_TwoICSuggest.RING_SENT || !builder) continue;
   float d = vector.DistanceXZ(rings[r], builder.FobSite());
   if (sentRing >= 0 && d >= off) continue;
   sentRing = r;
   sentLabel = ringLabels[r];
   off = d;
  }
  bool ringOk = sent && sentRing >= 0 && sentLabel.StartsWith("FOB site  ·  truck") && off >= 0 && off < 1;
  ACR_TestRegress.Check("twoicsuggest: a sent truck's site keeps a dashed ring: \"FOB site · truck on the way · about N min\"", ringOk, string.Format("ring %1 '%2', %3 m from the truck's site", sentRing, sentLabel, off));
  // The next looks, with the truck on its way: neither that site nor that front again.
  ACR_TwoICSuggest.LookNowForTest("US");
  for (int i = 0; i < 30 && !ACR_TwoICSuggest.HasLive("US"); i++) ACR_TwoICSuggest.Tick(manager, "US", ai, true);
  ACR_TwoICSuggestion again = ACR_TwoICSuggest.LiveFor("US");
  bool taken = ACR_TwoICSuggest.Taken("US", site.m_vPos);
  bool clear = !again || (again.m_sFront != front.m_sName && vector.DistanceXZ(again.m_vPos, site.m_vPos) > ACR_FobBuild.FOB_SPACING);
  string what = "none";
  if (again) what = string.Format("behind %1, %2 m from the site", again.m_sFront, (int)vector.DistanceXZ(again.m_vPos, site.m_vPos));
  ACR_TestRegress.Check("twoicsuggest: while that truck is on its way, its site and its front aren't suggested again", sent && taken && clear, string.Format("taken %1; suggested again: %2", taken, what));
  if (again) ACR_TwoICSuggest.ActOn(manager, again, ACR_TwoICRequests.ACT_CANCEL);
  if (builder) builder.CancelEstablishFob();
  if (truck) SCR_EntityHelper.DeleteEntityAndChildren(truck);
  // The truck off that build: its ring goes.
  ACR_TwoICSuggest.Tick(manager, "US", ai, true);
  if (ACR_TwoICSuggest.SentCount("US") > 0) PrintFormat("[ACR] twoicsuggest: the sent site's ring stayed after the truck gave up (%1)", ACR_TwoICSuggest.SentCount("US"));
 }

 protected static string TextOf(ACR_TwoICLine line)
 {
  if (!line) return "-";
  return line.m_sText;
 }

 protected static string StatusOf(ACR_TwoICLine line)
 {
  if (!line) return "-";
  return line.m_sStatus;
 }

 protected static string AnswerOf(ACR_TwoICLine line)
 {
  if (!line) return "-";
  return line.m_sAnswer;
 }

 protected static int ActionsOf(ACR_TwoICLine line)
 {
  if (!line) return -1;
  return line.m_iActions;
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
