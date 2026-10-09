// Command units are officers: the player loadout list holds only the US/USSR
// officer loadouts, whatever the world's loadout manager lists. Applied the same
// way on server and clients (loadouts replicate as list indexes). Ranks: see
// ACR_Respawn.MakeColonel.
modded class SCR_LoadoutManager
{
 protected bool m_bACR_Officers;

 protected void ACR_UseOfficers()
 {
  if (m_bACR_Officers) return;
  m_bACR_Officers = true;
  array<ResourceName> confs = {
   "{FD9337B1D5432C5A}Configs/Loadouts/US/Regular/Loadout_US_Officer.conf",
   "{4CAE966D675071B3}Configs/Loadouts/USSR/Regular/Loadout_USSR_Officer.conf"
  };
  array<ref SCR_BasePlayerLoadout> officers = {};
  foreach (ResourceName conf : confs)
  {
   Resource resource = BaseContainerTools.LoadContainer(conf);
   if (!resource || !resource.IsValid()) continue;
   SCR_BasePlayerLoadout loadout = SCR_BasePlayerLoadout.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
   if (loadout) officers.Insert(loadout);
  }
  if (officers.Count() != confs.Count())
  {
   Print("[ACR] Officer loadouts not found, keeping the world's loadouts", LogLevel.WARNING);
   return;
  }
  // RHS sides (when loaded): their standard subunit's squad leader (RHS's catalogs have
  // no officer). Same list on server and clients: both load the same mods.
  ACR_AddOfficer(officers, "RHS_USAF", "USMC Officer", "{0B0F9BA9E9A6E118}Prefabs/Characters/Factions/BLUFOR/RHS_USAF/RHS_USAF_USMC_MEF/Character_RHS_USAF_USMC_SL.et");
  ACR_AddOfficer(officers, "RHS_AFRF", "MSV Officer", "{963BD8F3900F27B4}Prefabs/Characters/Factions/OPFOR/RHS_AFRF/MSV/VKPO_Summer/Character_RHS_RF_MSV_VKPO_S_SL.et");
  m_aPlayerLoadouts = officers;
  PrintFormat("[ACR] Player loadouts: officers only (%1)", officers.Count());
 }

 protected void ACR_AddOfficer(array<ref SCR_BasePlayerLoadout> officers, string factionKey, string name, ResourceName prefab)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource || !resource.IsValid()) return;
  SCR_FactionPlayerLoadout loadout = new SCR_FactionPlayerLoadout();
  loadout.m_sLoadoutName = name;
  loadout.m_sLoadoutResource = prefab;
  loadout.m_sAffiliatedFaction = factionKey;
  officers.Insert(loadout);
 }

 override array<ref SCR_BasePlayerLoadout> GetPlayerLoadouts() { ACR_UseOfficers(); return super.GetPlayerLoadouts(); }
 override int GetLoadoutCount() { ACR_UseOfficers(); return super.GetLoadoutCount(); }
 override int GetLoadoutIndex(SCR_BasePlayerLoadout loadout) { ACR_UseOfficers(); return super.GetLoadoutIndex(loadout); }
 override SCR_BasePlayerLoadout GetLoadoutByIndex(int index) { ACR_UseOfficers(); return super.GetLoadoutByIndex(index); }
 override SCR_BasePlayerLoadout GetLoadoutByName(string name, FactionKey faction = string.Empty) { ACR_UseOfficers(); return super.GetLoadoutByName(name, faction); }
 override SCR_BasePlayerLoadout GetRandomFactionLoadout(Faction faction) { ACR_UseOfficers(); return super.GetRandomFactionLoadout(faction); }
 override int GetPlayerLoadoutsByFaction(Faction faction, out notnull array<ref SCR_BasePlayerLoadout> outLoadouts) { ACR_UseOfficers(); return super.GetPlayerLoadoutsByFaction(faction, outLoadouts); }
}
