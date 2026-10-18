// Real antenna compositions exercise eligibility and RP requests, not just a
// cached relay flag. Called by srv:structures alongside finite FOB services.
modded class SCR_CampaignBuildingCompositionComponent
{
 void ACR_TestRelayCompletion(bool complete) { m_bCompositionIsSpawned = complete; }
}

class ACR_TestFobRelay
{
 static const int CHECKS = 16;
 static const ResourceName RELAY = "{F7DC8BB193BCAF44}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Antenna_S_US_01.et";
 static const ResourceName ENEMY = "{0E9BAF79B9D47993}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Antenna_S_USSR_01.et";
 static IEntity s_Relay, s_Enemy, s_Rebuilt;

 static void Spawn(vector position)
 {
  s_Relay = ACR_TestStructures.Place(RELAY, position + Vector(0, 0, 20));
  s_Enemy = ACR_TestStructures.Place(ENEMY, position + Vector(0, 0, -28));
 }

 static void Guards(ACR_Manager manager, ACR_Base fob, ACR_Squad squad, int entry)
 {
  ACR_TestRegress.Check("relay: completed friendly antenna enables FOB by default", ACR_LogisticsRules.RequireRelay() && manager.m_Structures.HasRelay(fob) && ACR_LogisticsRules.CanDeploy(fob) && ACR_LogisticsRules.CanReinforce(fob), "");
  vector original = s_Relay.GetOrigin();
  s_Relay.SetOrigin(fob.GetOrigin() + Vector(0, 0, 65));
  ACR_TestRegress.Check("relay: enemy inside and friendly outside do not unlock FOB", s_Enemy && !manager.m_Structures.HasRelay(fob), "");
  ACR_TestRegress.Check("relay: loss closes deployment and RP reinforcement", !ACR_LogisticsRules.CanDeploy(fob) && !ACR_LogisticsRules.CanReinforce(fob) && manager.m_Bases.Describe(fob).Contains("RADIO RELAY REQUIRED"), "");
  int rp = manager.m_Bases.GetRP("US");
  string purchase = manager.PurchaseAt("US", entry, fob);
  ACR_TestRegress.Check("relay: actual purchase refused before RP charge", purchase.Contains("radio relay") && manager.m_Bases.GetRP("US") == rp, purchase);
  int savedRadius = ACR_Requisition.REINFORCE_CLEAR_RADIUS;
  ACR_Requisition.REINFORCE_CLEAR_RADIUS = 0;
  string reinforcement = manager.m_Requisition.ReinforceSquad(null, squad);
  ACR_Requisition.REINFORCE_CLEAR_RADIUS = savedRadius;
  ACR_TestRegress.Check("relay: actual RP reinforce refused without healing or spending", reinforcement.Contains("radio relay") && manager.m_Bases.GetRP("US") == rp && ACR_Logistics.Health(squad.GetAnchor()) < 0.9, reinforcement);
  ACR_TestRegress.Check("relay: direct RP service cannot bypass gate", manager.RestockAmmo(squad, fob) == 0 && ACR_Logistics.Health(squad.GetAnchor()) < 0.9 && manager.m_Bases.GetRP("US") == rp, "");
  ACR_TestRegress.Check("relay: replacement selectors exclude unlinked FOB", manager.m_Requisition.DeployBaseAt("US", fob.GetOrigin()) != fob && manager.m_Requisition.NearestSpawnBase("US", fob.GetOrigin()) != fob, "");
  ACR_Options.Set("acrDeployment", "all");
  ACR_Options.Set("acrReinforcement", "all");
  ACR_TestRegress.Check("relay: all-points policies cannot bypass relay", !ACR_LogisticsRules.CanDeploy(fob) && !ACR_LogisticsRules.CanReinforce(fob), "");
  ACR_Options.Unset("acrDeployment");
  ACR_Options.Unset("acrReinforcement");
  ACR_Options.Set("acrFobRelay", "0");
  ACR_TestRegress.Check("relay: mission-maker override enables deployment and RP reinforcement", ACR_LogisticsRules.CanDeploy(fob) && ACR_LogisticsRules.CanReinforce(fob), "");
  ACR_Options.Unset("acrFobRelay");
  s_Relay.SetOrigin(original);
  ACR_TestRegress.Check("relay: friendly relay restored without stale tick state", ACR_LogisticsRules.CanDeploy(fob) && ACR_LogisticsRules.CanReinforce(fob), "");
  SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(s_Relay.FindComponent(SCR_CampaignBuildingCompositionComponent));
  composition.ACR_TestRelayCompletion(false);
  ACR_TestRegress.Check("relay: unfinished blueprint does not count", !ACR_LogisticsRules.CanDeploy(fob) && !ACR_LogisticsRules.CanReinforce(fob), "");
  composition.ACR_TestRelayCompletion(true);
  ACR_TestRegress.Check("relay: completion restores deployment and RP reinforcement", ACR_LogisticsRules.CanDeploy(fob) && ACR_LogisticsRules.CanReinforce(fob), "");
 }

 static void RestoreEnemySide(ACR_Manager manager)
 {
  if (s_Enemy) manager.m_Structures.ACR_TestSetFaction(s_Enemy, "USSR");
 }

 static void RemoveAntenna(ACR_Base fob)
 {
  IEntity antenna = ACR_Structures.RelayAntenna(s_Relay);
  if (antenna) SCR_EntityHelper.DeleteEntityAndChildren(antenna);
  ACR_TestRegress.Check("relay: antenna loss closes gate even with composition root remaining", antenna == null && s_Relay && ACR_LogisticsRules.RelayBlocked(fob), "");
 }

 static void Rebuild(ACR_Manager manager, ACR_Base fob, IEntity fuel)
 {
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(fob.m_FobSupply);
  ACR_Options.Set("acrFobServices", "supplies");
  store.SetResourceValue(10);
  ACR_TestRegress.Check("relay: supplied services work while RP reinforcement is offline", ACR_LogisticsRules.RelayBlocked(fob) && manager.m_Logistics.Use(fuel, 1) && store.GetResourceValue() == 0, "");
  store.SetResourceValue(500);
  bool built = ACR_FobSupply.SpendBuild(fob.m_FobSupply, 1);
  int rp = manager.m_Bases.GetRP("US");
  ACR_FobSupply.Order(manager, "US", fob);
  ACR_FobSupply.Advance(fob, 60000);
  ACR_TestRegress.Check("relay: construction and paid deliveries work without relay", built && store.GetResourceValue() == 999 && manager.m_Bases.GetRP("US") == rp - 20 && fob.m_iSupplyDelivery == 0, "");
  store.SetResourceValue(500);
  s_Rebuilt = ACR_TestStructures.Place(RELAY, fob.GetOrigin() + Vector(0, 0, 20));
 }

 static void CheckRebuilt(ACR_Base fob)
 {
  ACR_TestRegress.Check("relay: rebuilt relay restores eligibility", s_Rebuilt && ACR_LogisticsRules.CanDeploy(fob) && ACR_LogisticsRules.CanReinforce(fob), "");
 }

 static void Cleanup()
 {
  if (s_Relay) SCR_EntityHelper.DeleteEntityAndChildren(s_Relay);
  if (s_Enemy) SCR_EntityHelper.DeleteEntityAndChildren(s_Enemy);
  if (s_Rebuilt) SCR_EntityHelper.DeleteEntityAndChildren(s_Rebuilt);
 }
}
