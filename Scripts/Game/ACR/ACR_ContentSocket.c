// Socket for an era content pack (#108: Cold War vehicles and kit from Workshop mods). ACA never
// names a pack's mods: a compat add-on that depends on ACA and on exactly the mods it vets plugs
// in with `modded class ACR_ContentSocket` and overrides the hooks below. Unplugged, every hook
// keeps ACA as before.
// Why a filter: vehicle and kit mods add themselves to the vanilla US/USSR/FIA entity catalogs,
// which feed the auto-filled buy lists, garrisons, crewmen and prefab swaps. Loading one put its raw
// prefabs (wrong crews, test variants, out-of-period kit) straight into the buy menu. The pack
// decides which catalog entries stay and adds its own curated entries instead.
enum ACR_ESocketVehicle
{
 ARMED = 1,         // has a weapon the AI can fight with
 TRANSPORT = 2,     // unarmed truck the AI buys to ferry squads
 CARRIER = 4,       // armed APC/IFV (Blitz's starting kit)
 SQUAD_CARRIER = 8, // carrier that seats a whole squad
 AIRCRAFT = 16,     // the AI commander never buys these
 RARE = 32          // fixed stock (armour, aircraft)
}

class ACR_ContentSocket : Managed
{
 protected static ref ACR_ContentSocket s_Instance;
 protected bool m_bLogged;

 static ACR_ContentSocket Get()
 {
  if (!s_Instance) s_Instance = new ACR_ContentSocket();
  if (!s_Instance.m_bLogged && s_Instance.Plugged())
  {
   s_Instance.m_bLogged = true;
   PrintFormat("[ACR] CONTENT pack plugged in: %1", s_Instance.Name());
  }
  return s_Instance;
 }

 // ---------------------------------------------------------------- hooks (override in a compat add-on)

 // The plugged-in pack's name for logs; empty while nothing is plugged in.
 string Name() { return ""; }
 bool Plugged() { return !Name().IsEmpty(); }

 // Whether a faction catalog entry may be used (buy lists, garrisons, crewmen, prefab swaps).
 bool AllowsCatalogEntry(string factionKey, EEntityCatalogType type, ResourceName prefab) { return true; }

 // The pack's own buy-list entries for a side, added after the catalog ones (auto-filled lists only:
 // a mission's own list stays as authored). Use AddVehicle and AddFormation.
 void AddRequisitions(string factionKey, array<ref ACR_RequisitionEntry> result) {}

 // The crewman (or pilot) for a seat of a vehicle this side bought; "" keeps ACA's choice.
 // Also fills crew seats the vehicle leaves without a default occupant.
 ResourceName Crewman(string factionKey, IEntity vehicle, ECompartmentType seat) { return ""; }

 // ---------------------------------------------------------------- helpers for the pack

 // A buyable vehicle, skipped when the list already has one of that prefab or name.
 static ACR_RequisitionEntry AddVehicle(array<ref ACR_RequisitionEntry> result, string name, ResourceName prefab, int cost, int stock, int flags)
 {
  foreach (ACR_RequisitionEntry existing : result)
   if (existing.m_sVehicle == prefab || existing.m_sName == name) return null;
  ACR_RequisitionEntry item = new ACR_RequisitionEntry();
  item.Set(name, "", prefab, cost, stock);
  item.m_bArmed = (flags & ACR_ESocketVehicle.ARMED) != 0;
  item.m_bTransport = (flags & ACR_ESocketVehicle.TRANSPORT) != 0;
  item.m_bCarrier = (flags & ACR_ESocketVehicle.CARRIER) != 0;
  item.m_bSquadCarrier = (flags & ACR_ESocketVehicle.SQUAD_CARRIER) != 0;
  item.m_bAircraft = (flags & ACR_ESocketVehicle.AIRCRAFT) != 0;
  item.m_bRare = (flags & ACR_ESocketVehicle.RARE) != 0 || item.m_bAircraft;
  result.Insert(item);
  return item;
 }

 // A buyable infantry formation: these soldiers, in this order, spawned into the side's base
 // group (as ACR_InfantryCatalog does); they are also its reinforcement recipe. Skipped when the
 // list already has an entry of that name.
 static ACR_RequisitionEntry AddFormation(array<ref ACR_RequisitionEntry> result, string name, ResourceName group, int cost, ACR_EFormation formation, array<ResourceName> soldiers)
 {
  foreach (ACR_RequisitionEntry existing : result)
   if (existing.m_sName == name) return null;
  ACR_RequisitionEntry item = new ACR_RequisitionEntry();
  item.Set(name, group, "", cost, 4);
  item.m_eFormation = formation;
  item.m_aSoldiers = {};
  item.m_aSoldiers.Copy(soldiers);
  result.Insert(item);
  return item;
 }
}
