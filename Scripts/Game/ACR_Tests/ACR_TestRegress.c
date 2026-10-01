// -acrRegressTest 1 (server-capable): regression checks for bugs fixed after
// the 1 Oct review. Each check prints "[ACR] REGRESS PASS|FAIL <name>".
class ACR_TestRegress
{
 static void Check(string name, bool ok, string detail = "")
 {
  string verdict = "FAIL";
  if (ok) verdict = "PASS";
  PrintFormat("[ACR] REGRESS %1 %2 %3", verdict, name, detail);
 }

 static string Verdict(bool ok)
 {
  if (ok) return "PASS";
  return "FAIL";
 }

 static ACR_Squad s_Defender;

 static void Run(ACR_TestHarness harness, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !harness.m_Range) return;
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  ACR_Squad bravo = manager.FindSquadByGroup(harness.m_Range.m_Bravo);
  if (!bravo) return;
  IEntity truck = harness.m_Range.m_Truck;

  // 1. Two dismount hand-overs in a row: the resumed order must stand.
  if (tick == 5 && truck)
  {
   int placed = bravo.BoardCargo(truck, 200, true);
   PrintFormat("[ACR] REGRESS info: Bravo placed in the truck: %1", placed);
  }
  if (tick == 9)
  {
   Check("bravo mounted for the dismount check", bravo.IsMounted(), "");
   vector target = harness.Ground("1460 0 2900");
   bravo.DismountAndContinue(ACR_Squad.WP_MOVE, target, -1, "Moving", ACR_EOrderKind.MOVE);
  }
  if (tick == 10) bravo.DismountAndContinue(ACR_Squad.WP_MOVE, harness.Ground("1460 0 2900"), -1, "Moving", ACR_EOrderKind.MOVE);
  if (tick == 20)
  {
   array<AIWaypoint> waypoints = {};
   bravo.m_Group.GetWaypoints(waypoints);
   Check("second dismount keeps the resumed order", bravo.m_Order == "Moving" && !waypoints.IsEmpty() && !bravo.IsMounted(), string.Format("order '%1' waypoints %2 mounted %3", bravo.m_Order, waypoints.Count(), bravo.IsMounted()));
  }

  // 2. Helicopter type per side, and a helicopter that can't launch refunds.
  if (tick == 22)
  {
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   Check("USSR gets the Mi-8", manager.m_Supports.HelicopterFor("USSR") == ACR_Supports.HELI_EAST, manager.m_Supports.HelicopterFor("USSR"));
   Check("door-gunner helicopters exist", Resource.Load(ACR_Supports.HELI_TRANSPORT_WEST).IsValid() && Resource.Load(ACR_Supports.HELI_TRANSPORT_EAST).IsValid(), "");
   Check("US gets the UH-1H", manager.m_Supports.HelicopterFor("US") == ACR_Supports.HELI_WEST, manager.m_Supports.HelicopterFor("US"));
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   string saved = soviets.m_sSupportHelicopter;
   soviets.m_sSupportHelicopter = "{0000000000000000}Prefabs/ACR/NoSuchHelicopter.et";
   ACR_FactionData data = manager.m_Bases.GetFaction("USSR");
   data.m_iRP = 500;
   int index = harness.SupportIndex(manager, ACR_ESupportType.HELI_LOITER);
   string result = manager.m_Supports.Call("USSR", index, harness.Ground("1400 0 2832"), true);
   ACR_SupportEntry entry = data.m_aSupports[index];
   Check("failed helicopter launch refunds RP and cooldown", data.m_iRP == 500 && entry.m_fReadyAt <= GetGame().GetWorld().GetWorldTime(), result + string.Format(" rp %1", data.m_iRP));
   soviets.m_sSupportHelicopter = saved;
  }

  // 3. World bounds for client-supplied points.
  if (tick == 23)
  {
   Check("point on the map", ACR_Manager.OnMap(harness.Ground("1400 0 2832")), "");
   Check("point off the map", !ACR_Manager.OnMap(Vector(9000000, 0, 9000000)), "");
  }
  // 5. (1 Oct MP) The AI commander waits while players load in, outside test runs.
  if (tick == 24)
  {
   float savedStart = manager.m_fAIHoldStart;
   manager.m_fAIHoldStart = GetGame().GetWorld().GetWorldTime();
   ACR_Perf.s_bChecks = false;
   bool held = manager.HoldAIFor("US");
   ACR_Perf.s_bChecks = true;
   bool free = !manager.HoldAIFor("US");
   manager.m_fAIHoldStart = savedStart;
   Check("AI commander held at scenario start, not in test runs", held && free, string.Format("held %1 free in tests %2", held, free));
  }
  // 6. (1 Oct MP) Rides leave once most riders are aboard.
  if (tick == 25)
   Check("ride leaves with 7 of 9 aboard, not 6", ACR_Rides.MostlyAboard(7, 9) && !ACR_Rides.MostlyAboard(6, 9), "");
  // 7. (1 Oct MP) Our full-screen map config (denser contours) is the one in use and loads.
  if (tick == 27)
  {
   SCR_MapConfigComponent mapConfig = SCR_MapConfigComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_MapConfigComponent));
   ResourceName used;
   if (mapConfig) used = mapConfig.GetGadgetMapConfig();
   Check("map uses the dense-contour config", used.Contains("ACR_MapFullscreen") && Resource.Load(used).IsValid(), used);
  }
  // 4. An AI commander leaves the other side's defenders alone (with both
  //    sides on AI it used to set them free every 15 s).
  if (tick == 26)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != "USSR") continue;
    foreach (ACR_Base base : ACR_Base.s_All)
     if (base.m_sOwner == "USSR") { squad.m_iAITask = ACR_EAITask.DEFEND; squad.m_AITarget = base; s_Defender = squad; break; }
    if (s_Defender) break;
   }
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = true;
   PrintFormat("[ACR] REGRESS info: Soviet defender %1, US AI on", s_Defender != null);
  }
  if (tick == 50)
  {
   bool kept = s_Defender && s_Defender.m_iAITask == ACR_EAITask.DEFEND;
   Check("AI commander leaves the other side's defenders alone", kept, "");
   PrintFormat("[ACR] REGRESS invariant errors: %1", ACR_Perf.s_iInvariantErrors);
   Print("[ACR] TEST COMPLETE");
  }
 }
}
