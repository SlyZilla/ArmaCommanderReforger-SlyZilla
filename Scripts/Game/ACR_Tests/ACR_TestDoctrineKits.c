// -acrDoctrineKitsTest 1 (server-capable, ~3 min): the company starting kits (ACR_DoctrineKits, #38).
// First the swap and the sides: a mechanized company (Blitz's kit) leaves the side's transport truck
// out (once), a rifle company's truck and a rifle squad aren't swapped; every side gets its company's
// kit at Start with no AI commander on either side, and a rifle company (or none picked) gets none.
// Then each of US, USSR and FIA gets each company's kit in turn (by its kit key),
// with RP_START each time (FIA has no base on the test range: it gets the USSR side's settings and
// borrows the empty Crossroads to deploy at). What each faction gets is logged ("DOCTRINEKITS ...").
// - Balanced gives nothing; Blitz the truck's price back and an APC or IFV, or keeps its truck when
//   its list has no carrier; Firepower a mortar team and an observer team; Fortress two tier I
//   positions at its front; Raider two recon teams and its doctrine intel a head start (a hint
//   once there is an enemy doctrine to learn);
// - every unit a kit bought appears, and per faction the RP left plus the kit's value at list
//   price is RP_START for every doctrine: the same starting value. The value is worked out from
//   the buy list, not from what the kit noted: each purchase at the price its entry stood at
//   (stepped pricing climbs with each), the positions at Cost(1), less the truck's price.
class ACR_TestDoctrineKits
{
 static const int CHECKS = 26;
 static const int RP_START = 400;
 static const int STEP_TICKS = 10;
 static const int DOCTRINES = 5;
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const ResourceName URAL = "{16C1F16C9B053801}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et";
 static const ResourceName SQUAD = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 protected static int s_iStep = -1;
 protected static int s_iStepAt;
 protected static int s_iPositionsBefore;
 protected static ref array<int> s_Bought = {}; // each entry's purchases before the kit
 protected static int s_iTruck;         // the truck price given back (Blitz, when it swapped)
 protected static string s_sCarrier = "";
 protected static bool s_bHeadStart;    // Raider: its intel had the head start right after the kit
 protected static string s_sValues = "";
 protected static bool s_bEqual = true;
 protected static int s_iValue;         // RP left + the kit at list price, right after the kit
 protected static int s_iKit;
 protected static int s_iRPAfter;
 protected static ACR_Base s_Borrowed;  // a base lent to a faction without one (FIA)

 static string DoctrineAt(int step)
 {
  int d = step % DOCTRINES;
  if (d == 0) return "balanced";
  if (d == 1) return "blitz";
  if (d == 2) return "firepower";
  if (d == 3) return "fortress";
  return "raider";
 }

 static string FactionAt(int step)
 {
  int f = step / DOCTRINES;
  if (f == 0) return "US";
  if (f == 1) return "USSR";
  return "FIA";
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   // The AI commanders buy as the scenario starts, before this first tick: what they bought goes
   // back on the lists, stock and price, so the kits see a new match's lists (pn11 q0, 27 Oct: the
   // AI had bought the only LAV, and the mechanized kit swapped nothing).
   array<string> sides = {"US", "USSR", "FIA"};
   foreach (string side : sides) RestockList(manager.m_Bases.GetFaction(side));
   // The checks name the kit's exact picks: no variety (ACR_Variety).
   ACR_Variety.s_bOff = true;
   ACR_DoctrineKits.ResetForTest();
   return;
  }
  if (tick == 6)
  {
   CheckSwapAndSides(manager);
   CheckNoTruck(manager);
   s_iStep = 0;
   StartStep(manager, tick);
   return;
  }
  if (s_iStep < 0 || tick - s_iStepAt < STEP_TICKS) return;
  FinishStep(manager);
  s_iStep++;
  if (s_iStep < 3 * DOCTRINES)
  {
   StartStep(manager, tick);
   return;
  }
  CheckWaitsForSquads(manager);
  ACR_DoctrineKits.ResetForTest();
  s_iStep = -1;
  ACR_TestRegress.Complete("doctrinekits", CHECKS);
 }

 // pn12d (27 Oct, Everon): the starting squads registered 9 s after spawning, after the fixed 5 s the
 // kit used to wait, and Combat Engineers got no positions. A kit now waits for its side's starting
 // squads, and goes anyway after KIT_WAIT_MAX_MS. Driven by WaitStep's clock, not the real one.
 protected static void CheckWaitsForSquads(ACR_Manager manager)
 {
  ACR_DoctrineKits.ResetForTest();
  manager.m_Bases.GetFaction("US").m_iRP = RP_START;
  ACR_DoctrineKits.DueForTest("US", "fortress");
  int have = ACR_DoctrineKits.SquadsOf(manager, "US");
  ACR_DoctrineKits.SetStartingForTest("US", have + 1); // one of its starting squads still out
  float t0 = GetGame().GetWorld().GetWorldTime();
  ACR_DoctrineKits.BeginWaitForTest(t0);
  bool waits = ACR_DoctrineKits.WaitStep(t0 + 6000) && ACR_DoctrineKits.Pending("US");
  ACR_DoctrineKits.SetStartingForTest("US", have); // it registers
  bool given = !ACR_DoctrineKits.WaitStep(t0 + 7000) && !ACR_DoctrineKits.Pending("US");
  ACR_KitRecord record;
  ACR_DoctrineKits.s_Given.Find("US", record);
  int positions;
  if (record) positions = record.m_iPositions;
  ACR_TestRegress.Check("doctrinekits: a kit waits for its side's starting squads (one still out at 6 s), then digs its positions in once they're in", waits && given && positions > 0,
   string.Format("waiting at 6 s %1, given at 7 s %2, positions %3 (%4 squads)", waits, given, positions, have));
  // Squads that never all come in: the kit goes anyway after KIT_WAIT_MAX_MS.
  ACR_DoctrineKits.ResetForTest();
  ACR_DoctrineKits.DueForTest("US", "raider");
  ACR_DoctrineKits.SetStartingForTest("US", have + 3);
  float t1 = GetGame().GetWorld().GetWorldTime();
  ACR_DoctrineKits.BeginWaitForTest(t1);
  bool still = ACR_DoctrineKits.WaitStep(t1 + ACR_DoctrineKits.KIT_WAIT_MAX_MS - 1000);
  bool capped = !ACR_DoctrineKits.WaitStep(t1 + ACR_DoctrineKits.KIT_WAIT_MAX_MS) && !ACR_DoctrineKits.Pending("US");
  ACR_TestRegress.Check("doctrinekits: a kit whose side's squads never all come in is given anyway after a minute", still && capped, string.Format("waiting at 59 s %1, given at 60 s %2", still, capped));
 }

 protected static void RestockList(ACR_FactionData data)
 {
  if (!data) return;
  foreach (ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (!entry || entry.m_iBought <= 0) continue;
   entry.m_iBought = 0;
   if (entry.m_iBaseCost > 0) entry.m_iCost = entry.m_iBaseCost;
   ACR_Pricing.Setup(entry, entry.m_bStepped);
  }
 }

 // The side's starting truck as the generator's default force has it: the M923A1 for the west,
 // the Ural for the east (and FIA), through the faction swaps.
 protected static ResourceName Truck(string faction)
 {
  ResourceName truck = TRUCK;
  if (faction != "US") truck = URAL;
  return ACR_FactionCatalog.ResolvePrefab(faction, truck, EEntityCatalogType.VEHICLE);
 }

 // A mechanized company with no starting truck to swap (Red's Arland start: two rifle squads, pn10,
 // 26 Oct, the kit came out empty) gets its carrier at list price, one that seats a squad over a
 // scout car (the USSR list has both BTR-70 and BRDM-2 at the same price).
 protected static void CheckNoTruck(ACR_Manager manager)
 {
  string name = "doctrinekits: a mechanized company with no starting truck gets a squad carrier at list price";
  ACR_FactionData data = manager.m_Bases.GetFaction("USSR");
  if (!data || !ACR_DoctrineKits.HQOf("USSR"))
  {
   ACR_TestRegress.Skip(name, "no USSR data or base");
   return;
  }
  data.m_iRP = RP_START;
  int index = ACR_DoctrineKits.CarrierIndex(data);
  bool squadCarrierListed = false;
  foreach (ACR_RequisitionEntry entry : data.m_aRequisitions)
   if (entry.m_bSquadCarrier && entry.m_iRemaining > 0) squadCarrierListed = true;
  string pick = "none";
  bool pickSeatsSquad = false;
  if (index >= 0)
  {
   pick = data.m_aRequisitions[index].m_sName;
   pickSeatsSquad = data.m_aRequisitions[index].m_bSquadCarrier;
  }
  ACR_DoctrineKits.DueForTest("USSR", "blitz");
  ACR_KitRecord record = ACR_DoctrineKits.Give(manager, "USSR", "blitz");
  int spent = RP_START - data.m_iRP;
  bool bought = record.m_aUnits.Count() == 1 && record.m_iCredit == 0 && record.m_aPaid.Count() == 1 && spent == record.m_aPaid[0] && spent > 0;
  ACR_TestRegress.Check(name, index >= 0 && bought && pickSeatsSquad == squadCarrierListed, string.Format("pick %1 (seats a squad %2, one listed %3); %4; spent %5 RP", pick, pickSeatsSquad, squadCarrierListed, record.Describe(), spent));
  ACR_DoctrineKits.ResetForTest();
 }

 protected static void CheckSwapAndSides(ACR_Manager manager)
 {
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  data.m_iRP = RP_START;
  vector at = KitSpot("US");
  ACR_DoctrineKits.DueForTest("US", "blitz");
  bool blitzSwaps = ACR_DoctrineKits.SwapsTruck(manager, "US", "Kit truck", "", Truck("US"), true, "", at, at);
  bool once = !ACR_DoctrineKits.SwapsTruck(manager, "US", "Kit truck 2", "", Truck("US"), true, "", at, at);
  ACR_DoctrineKits.DueForTest("US", "blitz");
  bool squadKept = !ACR_DoctrineKits.SwapsTruck(manager, "US", "Kit squad", SQUAD, "", true, "", at, at);
  ACR_DoctrineKits.DueForTest("US", "balanced");
  bool balancedKeeps = !ACR_DoctrineKits.SwapsTruck(manager, "US", "Kit truck", "", Truck("US"), true, "", at, at);
  int truck = ACR_DoctrineKits.TruckPrice(data, "US", Truck("US"));
  int carrier = ACR_DoctrineKits.CarrierIndex(data);
  string carrierName = "none";
  if (carrier >= 0) carrierName = string.Format("%1 (%2 RP)", data.m_aRequisitions[carrier].m_sName, data.m_aRequisitions[carrier].m_iCost);
  ACR_TestRegress.Check("doctrinekits: a mechanized company leaves the transport truck out once; a rifle squad and a rifle company's truck stay", blitzSwaps && once && squadKept && balancedKeeps, string.Format("Blitz swaps %1, a second truck kept %2, rifle squad kept %3, Balanced keeps %4; truck %5 RP, carrier %6", blitzSwaps, once, squadKept, balancedKeeps, truck, carrierName));

  // Every side's company kit at Start, whoever plays it: no AI commander on either side here.
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  ACR_Options.Set("acrCompanyWest", "engineers");
  ACR_Options.Set("acrCompanyEast", "recon");
  ACR_DoctrineKits.Plan(manager);
  string westKit = ACR_DoctrineKits.DueKit(west);
  string eastKit = ACR_DoctrineKits.DueKit(east);
  ACR_Options.Set("acrCompanyWest", "rifle");
  ACR_Options.Unset("acrCompanyEast");
  bool off = !ACR_DoctrineKits.Enabled();
  ACR_DoctrineKits.Plan(manager);
  string rifleKit = ACR_DoctrineKits.DueKit(west);
  string unsetKit = ACR_DoctrineKits.DueKit(east);
  ACR_Options.Unset("acrCompanyWest");
  ACR_TestRegress.Check("doctrinekits: every side gets its company's kit at Start, with no AI commander on either; a rifle company (or none picked) gets none", westKit == "fortress" && eastKit == "raider" && rifleKit == "" && unsetKit == "" && off, string.Format("%1 engineers -> '%2', %3 recon -> '%4'; rifle -> '%5', none picked -> '%6', kits off %7", west, westKit, east, eastKit, rifleKit, unsetKit, off));
  ACR_DoctrineKits.ResetForTest();
 }

 protected static vector KitSpot(string faction)
 {
  ACR_Base hq = ACR_DoctrineKits.HQOf(faction);
  if (!hq) return vector.Zero;
  return ACR_Manager.Ground(hq.GetOrigin() + Vector(20, 0, 20));
 }

 // A faction's turn starts: its settings and a base to deploy at (FIA borrows the empty
 // Crossroads), and what its list offers the kits.
 protected static void StartFaction(ACR_Manager manager, string faction)
 {
  ACR_FactionSettings.EnsureFor(faction, "USSR");
  ACR_FactionSettings settings = ACR_FactionSettings.Find(faction);
  if (settings) settings.m_bAICommander = false;
  s_bEqual = true;
  s_sValues = "";
  s_Borrowed = null;
  if (!ACR_DoctrineKits.HQOf(faction))
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (!base || base.m_bRetired || !base.m_sOwner.IsEmpty() || !base.m_bAllowSpawn) continue;
    base.m_sOwner = faction;
    if (ACR_LogisticsRules.CanDeploy(base)) { s_Borrowed = base; break; }
    base.m_sOwner = "";
   }
  }
  ACR_Base hq = ACR_DoctrineKits.HQOf(faction);
  string at = "none";
  if (hq) at = hq.m_sName;
  string borrowed = "";
  if (s_Borrowed) borrowed = " (borrowed)";
  ACR_FactionData data = manager.m_Bases.GetFaction(faction);
  PrintFormat("[ACR] DOCTRINEKITS %1: deploys at %2%3, %4 entries in its list", faction, at, borrowed, data.m_aRequisitions.Count());
  LogList(faction, data);
 }

 protected static void EndFaction(ACR_Manager manager, string faction)
 {
  ACR_TestRegress.Check("doctrinekits: " + faction + ": every doctrine starts with the same value (RP left + its kit at list price)", s_bEqual, s_sValues);
  if (s_Borrowed) s_Borrowed.m_sOwner = "";
  s_Borrowed = null;
 }

 // What the kits can draw on, and what each would pick, for reading what a faction gets.
 protected static void LogList(string faction, ACR_FactionData data)
 {
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_bCarrier || entry.m_bTransport || entry.m_bMortarTeam || entry.m_bObserverTeam || ACR_AICommander.IsSmallTeamEntry(entry))
    PrintFormat("[ACR] DOCTRINEKITS %1 list %2: %3 %4 RP, %5 left (carrier %6, transport %7, mortar %8, observer %9)", faction, i, entry.m_sName, entry.m_iCost, entry.m_iRemaining, entry.m_bCarrier, entry.m_bTransport, entry.m_bMortarTeam, entry.m_bObserverTeam);
  }
  PrintFormat("[ACR] DOCTRINEKITS %1 picks: carrier %2, truck %3 (%4 RP), mortar %5, observer %6, recon %7", faction, EntryName(data, ACR_DoctrineKits.CarrierIndex(data)), FilePath.StripPath(Truck(faction)), ACR_DoctrineKits.TruckPrice(data, faction, Truck(faction)), EntryName(data, ACR_DoctrineKits.MortarIndex(data)), EntryName(data, ACR_DoctrineKits.ObserverIndex(data)), EntryName(data, ACR_DoctrineKits.ReconIndex(data)));
 }

 protected static string EntryName(ACR_FactionData data, int index)
 {
  if (index < 0) return "none";
  return string.Format("%1 (%2 RP)", data.m_aRequisitions[index].m_sName, data.m_aRequisitions[index].m_iCost);
 }

 protected static void StartStep(ACR_Manager manager, int tick)
 {
  s_iStepAt = tick;
  string faction = FactionAt(s_iStep);
  string doctrine = DoctrineAt(s_iStep);
  if (s_iStep % DOCTRINES == 0) StartFaction(manager, faction);
  ACR_FactionData data = manager.m_Bases.GetFaction(faction);
  data.m_iRP = RP_START;
  s_iPositionsBefore = ACR_Fortify.CountPositions(faction);
  s_iTruck = 0;
  s_sCarrier = "";
  ACR_DoctrineKits.DueForTest(faction, doctrine);
  s_Bought.Clear();
  foreach (ACR_RequisitionEntry entry : data.m_aRequisitions) s_Bought.Insert(entry.m_iBought);
  if (doctrine == "blitz")
  {
   vector at = KitSpot(faction);
   bool swapped = ACR_DoctrineKits.SwapsTruck(manager, faction, "Kit truck", "", Truck(faction), true, "", at, at);
   if (swapped) s_iTruck = ACR_DoctrineKits.TruckPrice(data, faction, Truck(faction));
   int carrier = ACR_DoctrineKits.CarrierIndex(data);
   if (carrier >= 0) s_sCarrier = data.m_aRequisitions[carrier].m_sName;
  }
  ACR_KitRecord record = ACR_DoctrineKits.Give(manager, faction, doctrine);
  // Its value now: the purchases and positions are made, before any income or refund.
  s_iRPAfter = data.m_iRP;
  s_iKit = BoughtValue(data) + (ACR_Fortify.CountPositions(faction) - s_iPositionsBefore) * ACR_Fortify.Cost(1) - s_iTruck;
  s_iValue = s_iRPAfter + s_iKit;
  s_bHeadStart = false;
  ACR_Intel intel = manager.IntelFor(faction);
  if (doctrine == "raider" && intel) s_bHeadStart = intel.Doctrine().m_fHeadStart == ACR_DoctrineIntel.HINT;
  PrintFormat("[ACR] DOCTRINEKITS %1 %2 given: %3 (%4 RP left)", faction, doctrine, record.Describe(), data.m_iRP);
 }

 // What the purchases since StartStep cost at list price: each at the price its entry stood at
 // then (ACR_Pricing: a stepped entry climbs with every purchase).
 protected static int BoughtValue(ACR_FactionData data)
 {
  int value;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (i >= s_Bought.Count()) break;
   for (int b = s_Bought[i]; b < entry.m_iBought; b++)
   {
    if (entry.m_bStepped) value += ACR_Pricing.PriceAfter(entry.m_iBaseCost, b);
    else value += entry.m_iBaseCost;
   }
  }
  return value;
 }

 protected static void FinishStep(ACR_Manager manager)
 {
  string faction = FactionAt(s_iStep);
  string doctrine = DoctrineAt(s_iStep);
  string name = "doctrinekits: " + faction + ": ";
  ACR_FactionData data = manager.m_Bases.GetFaction(faction);
  ACR_KitRecord record;
  ACR_DoctrineKits.s_Given.Find(faction, record);
  if (!record)
  {
   ACR_TestRegress.Check(name + doctrine + "'s kit", false, "no record");
   if (doctrine == "raider") ACR_TestRegress.Check(name + "Raider's doctrine intel", false, "no record");
   s_bEqual = false;
   if (s_iStep % DOCTRINES == DOCTRINES - 1) EndFaction(manager, faction);
   return;
  }
  int appeared = 0;
  foreach (string unit : record.m_aUnits)
  {
   if (Appeared(manager, faction, unit)) appeared++;
  }
  int positions = ACR_Fortify.CountPositions(faction) - s_iPositionsBefore;
  if (s_iValue != RP_START) s_bEqual = false;
  if (!s_sValues.IsEmpty()) s_sValues += ", ";
  s_sValues += string.Format("%1 %2 (%3 RP left + %4)", doctrine, s_iValue, s_iRPAfter, s_iKit);
  string detail = string.Format("%1; %2 of %3 units appeared, %4 positions, %5 RP left", record.Describe(), appeared, record.m_aUnits.Count(), positions, data.m_iRP);
  bool ok = appeared == record.m_aUnits.Count();
  string what = "";
  if (doctrine == "balanced")
  {
   ok = ok && record.m_aUnits.IsEmpty() && positions == 0 && s_iRPAfter == RP_START;
   what = "a rifle company gets nothing";
  }
  else if (doctrine == "blitz")
  {
   // With a carrier in its list: the carrier for the truck. Without: it keeps its truck.
   if (!s_sCarrier.IsEmpty()) ok = ok && record.m_aUnits.Count() == 1 && record.m_iCredit == s_iTruck && s_iTruck > 0 && record.m_aUnits[0].StartsWith(s_sCarrier);
   else ok = ok && record.m_aUnits.IsEmpty() && record.m_iCredit == 0 && s_iTruck == 0;
   what = "a mechanized company gets an APC or IFV for its truck (or keeps the truck without one)";
  }
  else if (doctrine == "firepower")
  {
   ok = ok && record.m_aUnits.Count() == 2;
   what = "a weapons company gets a mortar team and an observer team";
  }
  else if (doctrine == "fortress")
  {
   ok = ok && positions == ACR_DoctrineKits.POSITIONS && record.m_iPositions == ACR_DoctrineKits.POSITIONS;
   what = "combat engineers dig two tier I positions in at their front";
  }
  else
  {
   // Once there is an enemy doctrine to learn, the head start is a hint: the intel tick gives it
   // as soon as an enemy AI commander is in command, else this test's own enemy does.
   ACR_Intel intel = manager.IntelFor(faction);
   bool hint = false;
   bool byTick = false;
   int level = -1;
   if (intel)
   {
    ACR_DoctrineIntel doctrineIntel = intel.Doctrine();
    byTick = doctrineIntel.m_fHeadStart == 0;
    if (!byTick)
    {
     doctrineIntel.m_EnemyForTest = ACR_AIDoctrine.Find("blitz");
     doctrineIntel.Tick();
     doctrineIntel.m_EnemyForTest = null;
    }
    level = doctrineIntel.m_iLevel;
    hint = level == 1 && doctrineIntel.m_fHeadStart == 0;
   }
   ok = ok && record.m_aUnits.Count() == ACR_DoctrineKits.RECON_TEAMS;
   ACR_TestRegress.Check(name + "a recon company's doctrine intel starts at a hint once there is an enemy doctrine to learn", s_bHeadStart && hint, string.Format("head start after the kit %1, given by the intel tick %2, then level %3", s_bHeadStart, byTick, level));
   what = "a recon company gets two recon teams";
  }
  ACR_TestRegress.Check(name + what + ", and every unit it bought appears", ok, detail);
  if (s_iStep % DOCTRINES == DOCTRINES - 1) EndFaction(manager, faction);
 }

 protected static bool Appeared(ACR_Manager manager, string faction, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.GetName() == name) return true;
  }
  array<ACR_MortarTeam> teams = {};
  manager.Mortars().TeamsOf(faction, teams);
  foreach (ACR_MortarTeam team : teams)
  {
   if (team.m_sName == name) return true;
  }
  return false;
 }
}
