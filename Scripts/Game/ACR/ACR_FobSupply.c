// Paid construction supplies. A separate SELF-only resource container feeds the building
// interface without exposing paid stock to truck loading or vanilla passive generation.
class ACR_FobProviderComponentClass : SCR_CampaignBuildingProviderComponentClass {}
class ACR_FobProviderComponent : SCR_CampaignBuildingProviderComponent
{
 [RplProp()]
 protected bool m_bBlocked;

 void SetBlocked(bool blocked)
 {
  if (m_bBlocked == blocked) return;
  m_bBlocked = blocked;
  Replication.BumpMe();
 }

 override int GetBudgetValue(EEditableEntityBudget type, out SCR_CampaignBuildingProviderComponent componentToUse)
 {
  if (type != EEditableEntityBudget.CAMPAIGN) return super.GetBudgetValue(type, componentToUse);
  componentToUse = this;
  if (m_bBlocked) return 0;
  return ACR_FobSupply.Stock(GetOwner());
 }

 override bool IsThereEnoughBudgetToSpawn(notnull array<ref SCR_EntityBudgetValue> budgetCosts)
 {
  if (m_bBlocked) return false;
  return super.IsThereEnoughBudgetToSpawn(budgetCosts);
 }
}

class ACR_FobSupply
{
 static const ResourceName PROVIDER = "{AC230926A001007D}Prefabs/ACR/ACR_FobSupply.et";
 static const int CAPACITY = 1000; // matches the vanilla command post's bundled storage
 static const int DELIVERY_AMOUNT = 500;
 static const int DELIVERY_COST = 20;
 static const float DELIVERY_MS = 60000;

 static int Stock(IEntity provider)
 {
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(provider);
  if (!container) return 0;
  return Math.Floor(container.GetResourceValue());
 }

 static void Create(ACR_Base base)
 {
  if (!base || !base.m_bBuilt || base.m_FobSupply) return;
  base.m_FobSupply = ACR_Manager.SpawnPrefab(PROVIDER, base.GetOrigin());
  if (!base.m_FobSupply) return;
  SCR_FactionAffiliationComponent faction = SCR_FactionAffiliationComponent.Cast(base.m_FobSupply.FindComponent(SCR_FactionAffiliationComponent));
  if (faction) faction.SetAffiliatedFaction(base.GetOwnerFaction());
 }

 static ACR_Base AtProvider(IEntity provider)
 {
  if (!provider) return null;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_bRetired && base.m_bBuilt && base.m_FobSupply == provider) return base;
  return null;
 }

 static bool SpendBuild(IEntity provider, int cost)
 {
  if (!Replication.IsServer() || cost < 0) return false;
  ACR_Base base = AtProvider(provider);
  if (!base || base.m_bContested) return false;
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(provider);
  if (!container || Stock(provider) < cost) return false;
  container.SetResourceValue(Stock(provider) - cost);
  return true;
 }

 // All decisions are repeated on the server; base names are only lookup keys.
 static string Request(SCR_PlayerController pc, string label, bool build)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  if (!ACR_Setup.Started()) return "Wait for the match to start";
  if (!manager.IsCommander(pc.GetPlayerId())) return "Take command to manage FOB supplies";
  IEntity body = pc.GetControlledEntity();
  if (!body || ACR_Logistics.Health(body) <= 0) return "Wait until you have spawned";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "You have no faction";
  string key = faction.GetFactionKey();
  ACR_Base base = manager.m_Requisition.FindSpawnBase(key, label);
  if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_FobSupply) return "Select one of your built FOBs";
  if (base.m_bContested) return "FOB is contested";
  if (!build)
  {
   int before = manager.m_Bases.GetRP(key);
   string result = Order(manager, key, base);
   if (manager.m_Bases.GetRP(key) < before) manager.LogSpend(pc, key, before, "construction supplies for " + base.m_sName);
   return result;
  }
  if (!body || ACR_Logistics.Health(body) <= 0 || vector.DistanceXZ(body.GetOrigin(), base.GetOrigin()) > base.m_fRadius) return "Move inside the FOB to build";
  SCR_CampaignBuildingManagerComponent building = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
  if (!building) return "Construction unavailable";
  if (Stock(base.m_FobSupply) <= 0) return "Order construction supplies first";
  building.EnterEditorMode(pc.GetPlayerId(), base.m_FobSupply, true, true);
  return "FOB construction opened";
 }

 static string Order(ACR_Manager manager, string key, ACR_Base base)
 {
  if (!Replication.IsServer() || !manager || !base || base.m_bRetired || !ACR_Base.s_All.Contains(base) || !base.m_bBuilt || base.m_sOwner != key || !base.m_FobSupply) return "Select one of your built FOBs";
  if (base.m_bContested) return "FOB is contested";
  if (base.m_iSupplyDelivery > 0) return "A supply delivery is already on order";
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(base.m_FobSupply);
  if (!container || container.GetMaxResourceValue() - Stock(base.m_FobSupply) < DELIVERY_AMOUNT) return "FOB needs room for 500 supplies";
  if (!manager.m_Bases.Spend(key, DELIVERY_COST)) return "Not enough RP (20 needed)";
  base.m_iSupplyDelivery = DELIVERY_AMOUNT;
  base.m_fSupplyDeliveryLeft = DELIVERY_MS;
  base.m_fSupplyDeliveryUpdated = GetGame().GetWorld().GetWorldTime();
  base.m_sSupplyDeliveryFaction = key;
  return "500 construction supplies ordered: 20 RP, 60 seconds; pauses while contested";
 }

 static void Tick(ACR_Base base)
 {
  if (!base) return;
  float now = GetGame().GetWorld().GetWorldTime();
  float elapsed = Math.Max(0, now - base.m_fSupplyDeliveryUpdated);
  base.m_fSupplyDeliveryUpdated = now;
  Advance(base, elapsed);
 }

 static void Advance(ACR_Base base, float elapsed)
 {
  if (!base || base.m_bRetired || !base.m_FobSupply) return;
  SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(base.m_FobSupply.FindComponent(SCR_FactionAffiliationComponent));
  if (affiliation && affiliation.GetAffiliatedFaction() != base.GetOwnerFaction())
   affiliation.SetAffiliatedFaction(base.GetOwnerFaction());
  if (base.m_iSupplyDelivery > 0 && base.m_sSupplyDeliveryFaction != base.m_sOwner)
  {
   base.m_iSupplyDelivery = 0;
   base.m_fSupplyDeliveryLeft = 0;
  }
  ACR_FobProviderComponent provider = ACR_FobProviderComponent.Cast(base.m_FobSupply.FindComponent(ACR_FobProviderComponent));
  if (provider) provider.SetBlocked(base.m_bContested);
  if (base.m_iSupplyDelivery <= 0 || base.m_bContested) return;
  base.m_fSupplyDeliveryLeft = Math.Max(0, base.m_fSupplyDeliveryLeft - Math.Max(0, elapsed));
  if (base.m_fSupplyDeliveryLeft > 0) return;
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(base.m_FobSupply);
  if (!container) return;
  container.SetResourceValue(Math.Min(container.GetMaxResourceValue(), Stock(base.m_FobSupply) + base.m_iSupplyDelivery));
  base.m_iSupplyDelivery = 0;
  PrintFormat("[ACR] FOB %1 received 500 paid construction supplies", base.m_sName);
 }

 static string Describe(ACR_Base base)
 {
  string detail = string.Format("\nConstruction supplies: %1 / %2", Stock(base.m_FobSupply), CAPACITY);
  if (base.m_iSupplyDelivery > 0)
  {
   string state = "";
   if (base.m_bContested) state = " (paused: contested)";
   detail += string.Format("\nDelivery: %1 supplies, %2 s%3", base.m_iSupplyDelivery, Math.Ceil(base.m_fSupplyDeliveryLeft / 1000), state);
  }
  return detail;
 }
}
