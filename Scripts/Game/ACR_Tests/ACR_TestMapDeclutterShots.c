// -acrMapDeclutterShotsTest 1 (a game client, not srv; ~1.5 min): the calm command map as the player
// sees it, one in-engine shot per sketch board ("Declutter - 1 Map view", "4 Zoomed out", "5 Dark
// map"), plus the UI size Normal and the classic map with -acrMapDeclutter 0 for comparison. Three
// more US squads sit at our nearest base (one icon, "×N"), Alpha moves off (a moving mark), INTEL
// gets a run of purchases to merge.
class ACR_TestMapDeclutterShots
{
 static const ResourceName RIFLE_SQUAD = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 static const int CHECKS = 11;
 protected static vector s_GroupCentre;
 protected static vector s_Sighting;
 protected static vector s_Jumped;
 protected static vector s_Clicked;
 protected static vector s_Base;
 protected static bool s_bPicker;
 protected static bool s_bHover;
 protected static RplId s_Picked = RplId.Invalid();
 protected static int s_iFrames;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (tick == 2)
  {
   ACR_Options.Unset(ACR_MapDeclutter.OPTION);
   ACR_MapDeclutter.ResetForTest();
   ACR_Base own = Nearest(h.m_Range.m_Rally, true);
   if (!own) return;
   s_Base = own.GetOrigin();
   PrintFormat("[ACR] DECLSHOTS squads at %1", own.m_sName);
   SCR_AIGroup first = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(RIFLE_SQUAD, ACR_Manager.Ground(s_Base + Vector(-20, 0, 15))));
   if (first) first.SetCustomName("1st Squad", 0);
   SCR_AIGroup second = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(RIFLE_SQUAD, ACR_Manager.Ground(s_Base + Vector(25, 0, -10))));
   if (second) second.SetCustomName("2nd Squad", 0);
   SCR_AIGroup third = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(FIRE_TEAM, ACR_Manager.Ground(s_Base + Vector(5, 0, 30))));
   if (third) third.SetCustomName("Light Fire Team 1", 0);
   return;
  }
  if (!pc || tick < 4) return;
  if (tick == 4) Feed(manager, pc);
  if (tick == 12)
  {
   ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
   if (alpha) alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(0, 0, -400));
  }
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  WorkspaceWidget ws = GetGame().GetWorkspace();
  // 1: the map view near our base.
  if (tick == 20) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (tick == 22 && mapEntity) mapEntity.ZoomPanSmooth(ws.GetWidth() / 2400.0, s_Base[0], s_Base[2], 0.1);
  if (tick == 26)
  {
   ACR_TestRegress.Check("mapdeclutter shots: the calm map is drawn and the panels are dark-styled", ACR_OrderDisplay.s_iDeclFrames > 0 && pc.ACR_DeclPanelStyled(), string.Format("frames %1, styled %2", ACR_OrderDisplay.s_iDeclFrames, pc.ACR_DeclPanelStyled()));
   ACR_TestRegress.Check("mapdeclutter shots: squads at our base are one icon with ×N", pc.ACR_DeclBadgesShown() > 0, string.Format("badges %1", pc.ACR_DeclBadgesShown()));
  }
  if (tick == 27)
  {
   // The soldiers of the squads at our base and the squads under its "×N" are hidden: off the
   // screen, not just a hidden widget the game shows again (30 Oct).
   int hidden;
   int onScreen = pc.ACR_DeclHiddenOnScreenForTest(hidden);
   ACR_TestRegress.Check("mapdeclutter shots: what the calm map hides at a base stays off the screen (soldiers, squads under a ×N)", hidden > 0 && onScreen == 0, string.Format("hidden %1, still on screen %2", hidden, onScreen));
   Shot("declutter_1_map");
  }
  // The "×N" icon's list of its squads (playtest 30 Oct: picking the right squad was hard).
  if (tick == 28) s_bPicker = pc.ACR_SquadPickerForTest();
  if (tick == 29)
  {
   ACR_TestRegress.Check("mapdeclutter shots: a click on the ×N icon lists its squads to pick from", s_bPicker && pc.ACR_SquadPickerOpen(), string.Format("opened %1, open %2", s_bPicker, pc.ACR_SquadPickerOpen()));
   Shot("declutter_7_cluster_list");
   // Hovering the second row rings where that squad really stands.
   s_bHover = pc.ACR_SquadPickerHoverForTest(1);
  }
  if (tick == 30)
  {
   Shot("declutter_8_row_hover");
   // Its number key (2) picks it.
   s_Picked = pc.ACR_SquadPickerIdForTest(1);
   pc.ACR_SquadPickerKeyForTest(1);
  }
  if (tick == 31)
  {
   bool picked = s_Picked.IsValid() && pc.ACR_GetSelectedGroup() == s_Picked && !pc.ACR_SquadPickerOpen();
   ACR_TestRegress.Check("mapdeclutter shots: hovering a row rings its squad; its number key picks it and the list closes", s_bHover && picked, string.Format("hover %1, picked %2, open %3", s_bHover, picked, pc.ACR_SquadPickerOpen()));
  }
  if (tick == 31) pc.ACR_CloseSquadPicker();
  // Nothing selected again: the zoomed-out check below wants the panel free to fold.
  if (tick == 31) pc.ACR_ClearSelectionForTest();
  // 4: zoomed out: the bars.
  if (tick == 30 && mapEntity) ZoomFar(mapEntity);
  if (tick == 33) ACR_TestRegress.Check("mapdeclutter shots: zoomed out, the ACA panel and INTEL fold to one-line bars", pc.ACR_DeclPanelCollapsed() && pc.ACR_DeclIntelCollapsed(), string.Format("panel %1, intel %2, zoom %3 px/m", pc.ACR_DeclPanelCollapsed(), pc.ACR_DeclIntelCollapsed(), mapEntity.GetCurrentZoom()));
  if (tick == 34) Shot("declutter_4_far");
  // 5: the dark map, by the moon button.
  if (tick == 37 && mapEntity) mapEntity.ZoomPanSmooth(ws.GetWidth() / 2400.0, s_Base[0], s_Base[2], 0.1);
  if (tick == 38)
  {
   SCR_MapToolEntry moon = SCR_MapToolMenuUI.s_ACR_MoonEntry;
   if (moon) moon.m_OnClick.Invoke();
   else pc.ACR_DeclToggleDark();
  }
  if (tick == 41)
  {
   SCR_MapToolEntry button = SCR_MapToolMenuUI.s_ACR_MoonEntry;
   bool lit = button && button.IsEntryActive();
   ACR_TestRegress.Check("mapdeclutter shots: the moon button darkens the map", pc.ACR_DeclTintShown() && ACR_MapDeclutter.Dark() && lit, string.Format("tint %1, dark %2, button %3 lit %4", pc.ACR_DeclTintShown(), ACR_MapDeclutter.Dark(), button != null, lit));
  }
  if (tick == 42) Shot("declutter_5_dark");
  // UI size Normal.
  if (tick == 45)
  {
   pc.ACR_DeclToggleDark();
   pc.ACR_DeclSetCompact(false);
  }
  if (tick == 48) Shot("declutter_6_normal");
  // The classic map with the option off (the server says so within a second).
  if (tick == 50)
  {
   pc.ACR_DeclSetCompact(true);
   ACR_Options.Set(ACR_MapDeclutter.OPTION, "0");
   GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  }
  if (tick == 53) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (tick == 55 && mapEntity) mapEntity.ZoomPanSmooth(ws.GetWidth() / 2400.0, s_Base[0], s_Base[2], 0.1);
  if (tick == 57) s_iFrames = ACR_OrderDisplay.s_iDeclFrames;
  if (tick == 59)
  {
   bool classic = !ACR_MapDeclutter.On() && !pc.ACR_DeclPanelStyled() && !pc.ACR_DeclTintShown() && ACR_OrderDisplay.s_iDeclFrames == s_iFrames && pc.ACR_DeclBadgesShown() == 0;
   ACR_TestRegress.Check("mapdeclutter shots: with -acrMapDeclutter 0 the classic map is back", classic, string.Format("on %1, styled %2, frames %3 -> %4", ACR_MapDeclutter.On(), pc.ACR_DeclPanelStyled(), s_iFrames, ACR_OrderDisplay.s_iDeclFrames));
  }
  if (tick == 60) Shot("declutter_0_classic");
  // A click on the sighting's INTEL line centres the map on it; a double-click on a squad centres
  // and zooms in on it.
  if (tick == 61) s_Jumped = pc.ACR_FeedJumpForTest();
  if (tick == 62)
  {
   float off = MapCentreOff(s_Sighting);
   ACR_TestRegress.Check("mapdeclutter shots: a click on an INTEL line that names a place centres the map there", s_Jumped != vector.Zero && vector.DistanceXZ(s_Jumped, s_Sighting) < 1 && off >= 0 && off < 100, string.Format("spot %1, sighting %2, map centre %3 m off", s_Jumped, s_Sighting, (int)off));
   s_Clicked = pc.ACR_DoubleClickForTest();
  }
  if (tick == 63)
  {
   float squadOff = MapCentreOff(s_Clicked);
   float zoom = 0;
   if (mapEntity) zoom = mapEntity.GetCurrentZoom();
   ACR_TestRegress.Check("mapdeclutter shots: a double-click on a squad centres the map on it and zooms in", s_Clicked != vector.Zero && squadOff >= 0 && squadOff < 100 && zoom >= ws.GetWidth() / 1300.0, string.Format("squad at %1, map centre %2 m off, zoom %3", s_Clicked, (int)squadOff, zoom));
   // Control groups: 1st and 2nd Squad kept as group 1 (Ctrl + 1), the selection cleared, then 1
   // twice: both selected again, the map on them, and their labels carry [1].
   s_GroupCentre = pc.ACR_KeepGroupForTest(0, h.NamedSquad(manager, "1st Squad"), h.NamedSquad(manager, "2nd Squad"));
   pc.ACR_ClearSelectionForTest();
  }
  if (tick == 64)
  {
   pc.ACR_RecallControlGroup(0, false);
   pc.ACR_RecallControlGroup(0, false);
  }
  if (tick == 65)
  {
   ACR_Squad first = h.NamedSquad(manager, "1st Squad");
   ACR_Squad second = h.NamedSquad(manager, "2nd Squad");
   bool both = first && second && pc.ACR_IsSelected(SCR_EntityHelper.EntityToRplId(first.m_Group)) && pc.ACR_IsSelected(SCR_EntityHelper.EntityToRplId(second.m_Group));
   float groupOff = MapCentreOff(s_GroupCentre);
   string label = pc.ACR_GroupMarkerLabelForTest(SCR_EntityHelper.EntityToRplId(second.m_Group));
   ACR_TestRegress.Check("mapdeclutter shots: control groups: Ctrl + 1 keeps the selection, 1 selects it again, 1 twice centres the map on it, labels show [1]", both && pc.ACR_ControlGroupSizeForTest(0) == 2 && groupOff >= 0 && groupOff < 150 && label.Contains("[1]"), string.Format("selected %1, group %2, map centre %3 m off, label '%4'", both, pc.ACR_ControlGroupSizeForTest(0), (int)groupOff, label));
   Shot("declutter_9_control_group");
  }
  if (tick == 66)
  {
   ACR_Options.Unset(ACR_MapDeclutter.OPTION);
   ACR_TestRegress.Complete("mapdeclutter shots", CHECKS);
  }
 }

 // Zoomed out: the map's widest zoom (ACR_MapDeclutter.IsFar).
 protected static void ZoomFar(SCR_MapEntity mapEntity)
 {
  mapEntity.ZoomOut();
 }

 // A run of purchases by the player (his tag) and a repeated sighting, for INTEL to merge.
 protected static void Feed(ACR_Manager manager, SCR_PlayerController pc)
 {
  ACR_Intel intel = manager.IntelFor("US");
  if (!intel) return;
  string tag = manager.Command().TagOf(pc.GetPlayerId());
  if (tag.IsEmpty()) tag = "Yung";
  float now = GetGame().GetWorld().GetWorldTime();
  intel.AddFeed(now - 9000, tag + " is in command");
  intel.AddFeed(now - 8000, "Popp requisitioned Rifle Squad at Lamentin (-15 RP)");
  intel.AddFeed(now - 7500, "Popp requisitioned Transport Truck at Lamentin (-30 RP)");
  // A sighting names a place: its INTEL line jumps the map there.
  s_Sighting = ACR_Manager.Ground(s_Base + Vector(500, 0, 400));
  intel.AddFeed(now - 6000, "Alpha 1: enemy rifle squad near Le Moule", s_Sighting);
  intel.AddFeed(now - 5000, "Alpha 1: enemy rifle squad near Le Moule", s_Sighting);
  intel.AddFeed(now - 3000, tag + " requisitioned Grenadier Team at Lamentin (-10 RP)");
  intel.AddFeed(now - 2500, tag + " requisitioned MG Team at Lamentin (-10 RP)");
  intel.AddFeed(now - 2000, tag + " requisitioned Grenadier Team at Lamentin (-9 RP)");
  intel.AddFeed(now - 1000, tag + " requisitioned MG Team at Lamentin (-10 RP)");
 }

 // How far the map's centre is from 'at' (m; -1: no map or no spot).
 protected static float MapCentreOff(vector at)
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || at == vector.Zero) return -1;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float cx, cz;
  mapEntity.ScreenToWorld(ws.GetWidth() / 2, ws.GetHeight() / 2, cx, cz);
  return vector.DistanceXZ(Vector(cx, 0, cz), at);
 }

 protected static ACR_Base Nearest(vector pos, bool ours)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   if (ours != (base.m_sOwner == "US")) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), pos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = base;
  }
  return best;
 }

 // In the engine, to $profile:acr-shots/<name> (the last frame drawn, so a tick after the state to
 // show). Its folder is made and checked first: a write that fails would put the shot on the
 // clipboard instead.
 static void Shot(string name)
 {
  FileIO.MakeDirectory("$profile:acr-shots");
  if (!FileIO.FileExists("$profile:acr-shots"))
  {
   PrintFormat("[ACR] DECLSHOTS %1 skipped: no shots folder", name);
   return;
  }
  System.MakeScreenshot("$profile:acr-shots/" + name);
  PrintFormat("[ACR] DECLSHOTS ready %1", name);
 }
}
