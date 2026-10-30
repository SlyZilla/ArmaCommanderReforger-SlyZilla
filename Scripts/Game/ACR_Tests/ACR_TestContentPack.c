// -acrContentPackTest 1 (server-capable, ~1 min; preset contentpack): #108, a content pack
// plugged into ACR_ContentSocket. Run it with a pack and its mods loaded (the Cold War pack:
// ACAColdWar and its dependencies). Checks the buy lists keep only the vanilla catalog vehicles
// plus the pack's own, that every listed prefab loads, and that the pack's vehicles bought by
// either side come with that side's crewmen and a driver.
class ACR_TestContentPack
{
 static ref array<ref ACR_PendingPurchase> s_Bought = {};
 static ref array<string> s_Sides = {};
 static int s_iExpected;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   ACR_FactionSettings west = ACR_FactionSettings.Find("US");
   if (west) west.m_bAICommander = false;
   ACR_FactionSettings east = ACR_FactionSettings.Find("USSR");
   if (east) east.m_bAICommander = false;
  }
  if (tick == 8) Lists(manager);
  if (tick == 40) Crews();
 }

 protected static void Lists(ACR_Manager manager)
 {
  ACR_ContentSocket socket = ACR_ContentSocket.Get();
  ACR_TestRegress.Check("content: a pack is plugged in", socket.Plugged(), socket.Name());
  s_iExpected++;
  array<string> sides = {"US", "USSR", "FIA"};
  foreach (string side : sides)
  {
   array<ref ACR_RequisitionEntry> entries = {};
   ACR_FactionCatalog.BuildRequisitions(side, ACR_FactionSettings.Find(side), entries);
   int packed;
   int invalid;
   string report;
   foreach (ACR_RequisitionEntry entry : entries)
   {
    if (entry.m_sVehicle.IsEmpty()) continue;
    Resource resource = Resource.Load(entry.m_sVehicle);
    if (!resource || !resource.IsValid()) { invalid++; report += " bad:" + entry.m_sName; }
    // Not a catalog entry the pack keeps: it must be one the pack added itself.
    if (!ACR_FactionCatalog.Allowed(side, EEntityCatalogType.VEHICLE, entry.m_sVehicle))
    {
     packed++;
     report += " +" + entry.m_sName;
    }
   }
   PrintFormat("[ACR] CONTENTTEST %1: %2 entries, pack vehicles%3", side, entries.Count(), report);
   ACR_TestRegress.Check("content: " + side + " buy list prefabs all load", invalid == 0, report);
   if (side == "FIA") ACR_TestRegress.Check("content: FIA gets no pack vehicles", packed == 0, report);
   else ACR_TestRegress.Check("content: " + side + " sells the pack's vehicles", packed >= 5, string.Format("%1:%2", packed, report));
   s_iExpected += 2;
  }
  // Catalog vehicles a mod added are gone from the lists (the mods add dozens to each side).
  array<ref ACR_RequisitionEntry> usList = {};
  ACR_FactionCatalog.BuildRequisitions("US", ACR_FactionSettings.Find("US"), usList);
  int foreign;
  foreach (ACR_RequisitionEntry item : usList)
  {
   string path = item.m_sVehicle;
   if (path.Contains("Tracked/BFV") || path.Contains("SightDebug") || path.Contains("Leopard1_grn_RS80Z") || path.Contains("1a5MV")) foreign++;
  }
  ACR_TestRegress.Check("content: unvetted mod vehicles stay out of the US list", foreign == 0, foreign.ToString());
  s_iExpected++;

  // Buy the pack's vehicles whose own crews are wrong: Zagoria (USSR riflemen whoever owns it)
  // and WCS (M113 riflemen).
  Buy(manager, "US", "Leopard test", "{A4EA6DD24C1ED74F}Leopard/Leopard1_grn.et", 0);
  Buy(manager, "US", "M113 test", "{953523CFE2DB9769}Prefabs/Vehicles/Tracked/M113/M113_APC_MERDC.et", 1);
  Buy(manager, "USSR", "T-55 test", "{6484A98F55C6AA9E}T-55/T55olive.et", 0);
  Buy(manager, "USSR", "T-72 test", "{BC07885176C7FF8A}Prefabs/Vehicles/Tracked/T72A/T72A_Base.et", 1);
 }

 protected static void Buy(ACR_Manager manager, string side, string name, ResourceName prefab, int slot)
 {
  ACR_Base home;
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (candidate.m_sOwner == side) { home = candidate; break; }
  if (!home) { PrintFormat("[ACR] CONTENTTEST no %1 base", side); return; }
  vector at = manager.ClearSpotNear(home, 20 + 25 * slot, 90, 6);
  ACR_PendingPurchase purchase = new ACR_PendingPurchase();
  purchase.m_sName = name;
  purchase.m_sFaction = side;
  manager.m_sSpawningFaction = side;
  bool ok = manager.m_Requisition.SpawnParts(purchase, "", prefab, true, "", at, at);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] CONTENTTEST bought %1 for %2: %3 %4", name, side, ok, purchase.m_sError);
  s_Bought.Insert(purchase);
  s_Sides.Insert(side);
 }

 protected static void Crews()
 {
  foreach (int i, ACR_PendingPurchase purchase : s_Bought)
  {
   string side = s_Sides[i];
   IEntity vehicle = purchase.m_Vehicle;
   int seated;
   int own;
   bool driver;
   string report;
   SCR_BaseCompartmentManagerComponent compartments;
   if (vehicle) compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   if (compartments)
   {
    array<BaseCompartmentSlot> seats = {};
    compartments.GetCompartments(seats);
    foreach (BaseCompartmentSlot seat : seats)
    {
     IEntity man = seat.GetOccupant();
     if (!man) continue;
     seated++;
     if (seat.GetType() == ECompartmentType.PILOT) driver = true;
     string prefab = man.GetPrefabData().GetPrefabName();
     string faction = ACR_Intel.FactionKeyOf(man);
     bool crewman = prefab.Contains("_Crew") || prefab.Contains("_CC");
     if (faction == side && crewman) own++;
     report += string.Format(" %1:%2", FilePath.StripExtension(FilePath.StripPath(prefab)), faction);
    }
   }
   PrintFormat("[ACR] CONTENTTEST crew %1: %2/%3 own crewmen, driver %4,%5", purchase.m_sName, own, seated, driver, report);
   ACR_TestRegress.Check("content: " + purchase.m_sName + " has a driver and only " + side + " crewmen", vehicle && driver && seated >= 2 && own == seated, string.Format("%1/%2%3", own, seated, report));
   s_iExpected++;
  }
  ACR_TestRegress.Complete("contentpack", s_iExpected);
 }
}
