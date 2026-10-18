class ACR_TestLogisticsRules
{
 static const int CHECKS = 19;
 static void Run(ACR_Manager manager, ACR_Base site, ACR_Base fob, int entry)
 {
  ACR_EBaseRole savedRole = site.m_eLogisticsRole;
  bool savedSpawn = site.m_bAllowSpawn;
  bool savedReinforce = site.m_bAllowReinforce;
  site.m_bAllowSpawn = true;
  site.m_bAllowReinforce = true;
  site.m_eLogisticsRole = ACR_EBaseRole.SETTLEMENT;
  ACR_TestRegress.Check("rules: village sends infantry but cannot deploy new formations", ACR_LogisticsRules.CanReinforce(site) && !ACR_LogisticsRules.CanDeploy(site), "");
  int rp = manager.m_Bases.GetRP(site.m_sOwner);
  string refused = manager.PurchaseAt(site.m_sOwner, entry, site);
  ACR_TestRegress.Check("rules: actual village purchase refused before spending", refused.Contains("cannot deploy new units") && manager.m_Bases.GetRP(site.m_sOwner) == rp, refused);
  site.m_eLogisticsRole = ACR_EBaseRole.MILITARY;
  ACR_TestRegress.Check("rules: military bases deploy and reinforce", ACR_LogisticsRules.CanDeploy(site) && ACR_LogisticsRules.CanReinforce(site), "");
  site.m_eLogisticsRole = ACR_EBaseRole.HQ;
  ACR_TestRegress.Check("rules: HQ deploys and reinforces", ACR_LogisticsRules.CanDeploy(site) && ACR_LogisticsRules.CanReinforce(site), "");
  ACR_TestRegress.Check("rules: built FOB deploys and reinforces", ACR_LogisticsRules.CanDeploy(fob) && ACR_LogisticsRules.CanReinforce(fob), "");
  site.m_eLogisticsRole = ACR_EBaseRole.OBJECTIVE;
  ACR_TestRegress.Check("rules: industry is an income objective without replacements", !ACR_LogisticsRules.CanDeploy(site) && !ACR_LogisticsRules.CanReinforce(site), "");
  ACR_Options.Set("acrDeployment", "all");
  ACR_TestRegress.Check("rules: host can allow deployments everywhere", ACR_LogisticsRules.CanDeploy(site), "");
  site.m_bAllowSpawn = false;
  ACR_TestRegress.Check("rules: mission-maker no-deployment flag still wins", !ACR_LogisticsRules.CanDeploy(site), "");
  site.m_bAllowSpawn = true;
  ACR_Options.Set("acrDeployment", "hq");
  site.m_eLogisticsRole = ACR_EBaseRole.MILITARY;
  ACR_TestRegress.Check("rules: HQ-only option excludes captured military sites", !ACR_LogisticsRules.CanDeploy(site) && ACR_LogisticsRules.CanDeploy(fob), "");
  ACR_Options.Unset("acrDeployment");
  site.m_eLogisticsRole = ACR_EBaseRole.SETTLEMENT;
  ACR_Options.Set("acrReinforcement", "deployment");
  ACR_TestRegress.Check("rules: host can disable village replacements", !ACR_LogisticsRules.CanReinforce(site), "");
  ACR_Options.Set("acrReinforcement", "all");
  site.m_eLogisticsRole = ACR_EBaseRole.OBJECTIVE;
  ACR_TestRegress.Check("rules: host can allow replacements at all points", ACR_LogisticsRules.CanReinforce(site), "");
  site.m_bAllowReinforce = false;
  ACR_TestRegress.Check("rules: mission-maker no-reinforcement flag still wins", !ACR_LogisticsRules.CanReinforce(site), "");
  ACR_Options.Unset("acrReinforcement");
  site.m_bAllowReinforce = true;
  site.m_eLogisticsRole = ACR_EBaseRole.AUTHORED;
  ACR_TestRegress.Check("rules: existing authored maps preserve deployment flags", ACR_LogisticsRules.CanDeploy(site) && ACR_LogisticsRules.CanReinforce(site), "");
  site.m_eLogisticsRole = savedRole;
  site.m_bAllowSpawn = savedSpawn;
  site.m_bAllowReinforce = savedReinforce;
  ACR_Options.Set("acrDepotCapacity", "4000");
  ACR_TestRegress.Check("rules: custom depot capacity resolved", ACR_LogisticsRules.DepotBonus() == 4000, "");
  ACR_Options.Unset("acrDepotCapacity");
  ACR_Options.Set("acrServiceSupplyCost", "20");
  ACR_TestRegress.Check("rules: custom service price resolved", ACR_LogisticsRules.ServiceCost() == 20, "");
  ACR_Options.Unset("acrServiceSupplyCost");
  ACR_Options.Set("acrFobRPServiceCost", "0");
  ACR_TestRegress.Check("rules: RP-service fee can be disabled", ACR_LogisticsRules.InstantServiceFee() == 0, "");
  ACR_Options.Unset("acrFobRPServiceCost");
  ACR_TestRegress.Check("rules: logistics options exposed to startup screen", ACR_Setup.Rows().Contains("deployment\t") && ACR_Setup.Rows().Contains("rpservice\t"), "");
  ACR_TestRegress.Check("rules: startup choices cover free and supplied services", ACR_Setup.Values("fobservices").Contains("free") && ACR_Setup.Values("fobservices").Contains("supplies"), "");
  ACR_TestRegress.Check("rules: startup maps every logistics setting", ACR_Setup.Option("deployment") == "acrDeployment" && ACR_Setup.Option("reinforcement") == "acrReinforcement" && ACR_Setup.Option("depotcapacity") == "acrDepotCapacity" && ACR_Setup.Option("servicecost") == "acrServiceSupplyCost" && ACR_Setup.Option("rpservice") == "acrFobRPServiceCost", "");
 }
}
