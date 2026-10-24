// The engineering orders: names, prices, timers and the vanilla prefabs each side builds.
enum ACAFE_EOrder
{
 NONE,
 ROADBLOCK,
 CHECKPOINT_S,
 CHECKPOINT_M,
 CHECKPOINT_L,
 MINES_AP,
 MINES_AT,
 MINES_MIXED,
 CLEAR_MINES
}

class ACAFE_Orders
{
 static const int COUNT = 9;          // ACAFE_EOrder values, NONE included
 static const int PANEL_BASE = 900;   // command panel action ids: PANEL_BASE + order (PANEL_BASE itself: open/close)
 static const int AI_TASK = 40;       // ACR_Squad.m_iAITask while the AI commander's squad works for us

 // US for west / RHS US, USSR for east / RHS AFRF, log barricades for FIA.
 static const ResourceName CHECKPOINT_S_US = "{A870787967D4D18D}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadSmall/E_Checkpoint_S_US_01.et";
 static const ResourceName CHECKPOINT_M_US = "{7DBED4D2E5586BEA}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadMedium/E_Checkpoint_M_US_01.et";
 static const ResourceName CHECKPOINT_L_US = "{07B8390A85FAAF82}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadLarge/E_Checkpoint_L_US_01.et";
 static const ResourceName CHECKPOINT_S_USSR = "{5AF4E84CA11058F4}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadSmall/E_Checkpoint_S_USSR_01.et";
 static const ResourceName CHECKPOINT_M_USSR = "{1DF9835399B603EB}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadMedium/E_Checkpoint_M_USSR_01.et";
 static const ResourceName CHECKPOINT_L_USSR = "{612AB9E420809989}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadLarge/E_Checkpoint_L_USSR_01.et";
 static const ResourceName BARRICADE_S_US = "{D7B8408B96F4DF79}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadSmall/E_Barricade_S_US_01.et";
 static const ResourceName BARRICADE_M_US = "{6FE74C9C75DA8A52}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadMedium/E_Barricade_M_US_01.et";
 static const ResourceName BARRICADE_L_US = "{DAEC7898BE11C117}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadLarge/E_Barricade_L_US_01.et";
 static const ResourceName BARRICADE_S_USSR = "{17B68DA54A970140}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadSmall/E_Barricade_S_USSR_01.et";
 static const ResourceName BARRICADE_M_USSR = "{1236588FCEA4848F}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadMedium/E_Barricade_M_USSR_01.et";
 static const ResourceName BARRICADE_L_USSR = "{3FB42D0A5589A530}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadLarge/E_Barricade_L_USSR_01.et";
 static const ResourceName LOGS_1 = "{B95DE10716B00052}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Barricade_Logs_01.et";
 static const ResourceName LOGS_2 = "{D509D5E7AA748F7C}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Barricade_Logs_02.et";
 static const ResourceName LOGS_3 = "{3095991EA6911817}PrefabsEditable/Auto/Compositions/Misc/SubCompositions/E_Barricade_Logs_03.et";
 static const ResourceName HEDGEHOG = "{E6AE368A342034CB}PrefabsEditable/Auto/Props/Military/Fortification/E_CzechHedgehog_01_rusty.et";

 static const ResourceName MINE_AP_US = "{C72C9E090CED8B05}PrefabsEditable/Mines/E_Mine_M14.et";
 static const ResourceName MINE_AT_US = "{B63D38376558C9DD}PrefabsEditable/Mines/E_Mine_M15AT.et";
 static const ResourceName MINE_AP_USSR = "{4F6673E18A5A5974}PrefabsEditable/Mines/E_Mine_PMN4.et";
 static const ResourceName MINE_AT_USSR = "{44DCCCB56D42C43B}PrefabsEditable/Mines/E_Mine_TM62M.et";
 static const ResourceName MINEFIELD_SIGN = "{BF4F9228DD61216B}PrefabsEditable/Auto/Structures/Signs/Military/SignMinefield_01/E_SignMinefield_01.et";

 static bool IsStructure(int order) { return order >= ACAFE_EOrder.ROADBLOCK && order <= ACAFE_EOrder.CHECKPOINT_L; }
 static bool IsMinefield(int order) { return order >= ACAFE_EOrder.MINES_AP && order <= ACAFE_EOrder.MINES_MIXED; }

 static bool IsEast(string factionKey)
 {
  return factionKey == "USSR" || factionKey == "RHS_AFRF" || factionKey.Contains("USSR") || factionKey.Contains("AFRF");
 }

 static bool IsFia(string factionKey)
 {
  return factionKey == "FIA" || factionKey.Contains("FIA");
 }

 // e.g. "medium checkpoint" (the squad label and the feed use it)
 static string Name(int order)
 {
  switch (order)
  {
   case ACAFE_EOrder.ROADBLOCK: return "roadblock";
   case ACAFE_EOrder.CHECKPOINT_S: return "small checkpoint";
   case ACAFE_EOrder.CHECKPOINT_M: return "medium checkpoint";
   case ACAFE_EOrder.CHECKPOINT_L: return "large checkpoint";
   case ACAFE_EOrder.MINES_AP: return "AP minefield";
   case ACAFE_EOrder.MINES_AT: return "AT minefield";
   case ACAFE_EOrder.MINES_MIXED: return "mixed minefield";
   case ACAFE_EOrder.CLEAR_MINES: return "mine clearing";
  }
  return "engineering";
 }

 // With its article: "a roadblock", "an AP minefield".
 static string AName(int order)
 {
  string name = Name(order);
  if (name.StartsWith("A") || name.StartsWith("a") || name.StartsWith("e") || name.StartsWith("i") || name.StartsWith("o") || name.StartsWith("u")) return "an " + name;
  return "a " + name;
 }

 // Short form for the map ("Checkpoint M").
 static string ShortName(int order)
 {
  switch (order)
  {
   case ACAFE_EOrder.ROADBLOCK: return "Roadblock";
   case ACAFE_EOrder.CHECKPOINT_S: return "Checkpoint S";
   case ACAFE_EOrder.CHECKPOINT_M: return "Checkpoint M";
   case ACAFE_EOrder.CHECKPOINT_L: return "Checkpoint L";
   case ACAFE_EOrder.MINES_AP: return "Minefield AP";
   case ACAFE_EOrder.MINES_AT: return "Minefield AT";
   case ACAFE_EOrder.MINES_MIXED: return "Minefield mixed";
   case ACAFE_EOrder.CLEAR_MINES: return "Mine clearing";
  }
  return "Engineering";
 }

 static int CostRP(int order)
 {
  switch (order)
  {
   case ACAFE_EOrder.ROADBLOCK: return ACAFE_Config.ROADBLOCK_RP;
   case ACAFE_EOrder.CHECKPOINT_S: return ACAFE_Config.CHECKPOINT_S_RP;
   case ACAFE_EOrder.CHECKPOINT_M: return ACAFE_Config.CHECKPOINT_M_RP;
   case ACAFE_EOrder.CHECKPOINT_L: return ACAFE_Config.CHECKPOINT_L_RP;
   case ACAFE_EOrder.MINES_AP: return ACAFE_Config.MINES_AP_RP;
   case ACAFE_EOrder.MINES_AT: return ACAFE_Config.MINES_AT_RP;
   case ACAFE_EOrder.MINES_MIXED: return ACAFE_Config.MINES_MIXED_RP;
   case ACAFE_EOrder.CLEAR_MINES: return ACAFE_Config.CLEAR_MINES_RP;
  }
  return 0;
 }

 static int CostSupplies(int order)
 {
  switch (order)
  {
   case ACAFE_EOrder.ROADBLOCK: return ACAFE_Config.ROADBLOCK_SUPPLIES;
   case ACAFE_EOrder.CHECKPOINT_S: return ACAFE_Config.CHECKPOINT_S_SUPPLIES;
   case ACAFE_EOrder.CHECKPOINT_M: return ACAFE_Config.CHECKPOINT_M_SUPPLIES;
   case ACAFE_EOrder.CHECKPOINT_L: return ACAFE_Config.CHECKPOINT_L_SUPPLIES;
   case ACAFE_EOrder.MINES_AP: return ACAFE_Config.MINES_AP_SUPPLIES;
   case ACAFE_EOrder.MINES_AT: return ACAFE_Config.MINES_AT_SUPPLIES;
   case ACAFE_EOrder.MINES_MIXED: return ACAFE_Config.MINES_MIXED_SUPPLIES;
  }
  return 0;
 }

 // Seconds of work at the site (engineers halve it). Mine clearing depends on what it finds.
 static float Duration(int order)
 {
  switch (order)
  {
   case ACAFE_EOrder.ROADBLOCK: return ACAFE_Config.ROADBLOCK_SECONDS;
   case ACAFE_EOrder.CHECKPOINT_S: return ACAFE_Config.CHECKPOINT_S_SECONDS;
   case ACAFE_EOrder.CHECKPOINT_M: return ACAFE_Config.CHECKPOINT_M_SECONDS;
   case ACAFE_EOrder.CHECKPOINT_L: return ACAFE_Config.CHECKPOINT_L_SECONDS;
   case ACAFE_EOrder.MINES_AP: return ACAFE_Config.MINES_AP_SECONDS;
   case ACAFE_EOrder.MINES_AT: return ACAFE_Config.MINES_AT_SECONDS;
   case ACAFE_EOrder.MINES_MIXED: return ACAFE_Config.MINES_MIXED_SECONDS;
   case ACAFE_EOrder.CLEAR_MINES: return ACAFE_Config.CLEAR_MIN_SECONDS;
  }
  return 30;
 }

 // "6 RP or 40 supplies from a construction truck, 30 s" (the panel hint)
 // What the order costs and takes with this match's settings (the panel hint).
 static string PriceText(int order)
 {
  float scale = ACAFE_Settings.CostScale();
  int rp = (int)Math.Round(CostRP(order) * scale);
  int supplies = (int)Math.Round(CostSupplies(order) * scale);
  string text;
  if (IsMinefield(order) && ACAFE_Settings.KitMines())
  {
   int perAP = (int)Math.Round(ACAFE_Config.KIT_TRUCK_SUPPLIES_AP * scale);
   int perAT = (int)Math.Round(ACAFE_Config.KIT_TRUCK_SUPPLIES_AT * scale);
   text = string.Format("the mines its soldiers carry, the rest from a construction truck at %1 / %2 supplies per AP / AT mine", perAP, perAT);
  }
  else if (IsStructure(order) && ACAFE_Settings.TruckMaterials()) text = string.Format("%1 supplies from a construction truck with the squad", supplies);
  else if (rp <= 0 && supplies <= 0) text = "free";
  else
  {
   text = string.Format("%1 RP", rp);
   if (ACAFE_Config.TRUCK_SUPPLIES_FIRST && supplies > 0) text += string.Format(" or %1 supplies from a construction truck", supplies);
  }
  if (order == ACAFE_EOrder.CLEAR_MINES) return text + string.Format(", %1 s per mine", (int)(ACAFE_Config.CLEAR_SECONDS_PER_MINE * ACAFE_Settings.TimeScale()));
  return text + string.Format(", %1 s", (int)(Duration(order) * ACAFE_Settings.TimeScale()));
 }

 static string Hint(int order)
 {
  string who = "engineers twice as fast";
  if (ACAFE_Settings.EngineersOnly()) who = "engineers or sappers only, or a squad with a construction truck";
  if (IsStructure(order)) return string.Format("Right-click on or next to a road: build a %1 across it (%2; %3). Click the button again to cancel.", Name(order), PriceText(order), who);
  if (IsMinefield(order))
  {
   string where = "AP mines off the roads";
   if (order == ACAFE_EOrder.MINES_AT) where = "AT mines on the road through it (anywhere when there is none)";
   if (order == ACAFE_EOrder.MINES_MIXED) where = "AT mines on the road, AP mines round it";
   return string.Format("Right-click the area: lay a %1 m %2 (%3; %4; %5). Click the button again to cancel.", (int)ACAFE_Config.MINEFIELD_SIZE, Name(order), where, PriceText(order), who);
  }
  if (order == ACAFE_EOrder.CLEAR_MINES) return string.Format("Right-click the area: sweep %1 m round it and lift every mine found (%2; %3). Click the button again to cancel.", (int)ACAFE_Config.CLEAR_RADIUS, PriceText(order), who);
  return "";
 }

 static ResourceName MineFor(string factionKey, bool antiTank)
 {
  // FIA and the east lay Soviet mines; everyone else American ones.
  bool soviet = IsEast(factionKey) || IsFia(factionKey);
  if (antiTank)
  {
   if (soviet) return MINE_AT_USSR;
   return MINE_AT_US;
  }
  if (soviet) return MINE_AP_USSR;
  return MINE_AP_US;
 }

 // The pieces of a road structure in its own frame (x across the road, z along it; the
 // vanilla road compositions are laid out that way) and the footprint radius they need.
 static float Pieces(int order, string factionKey, float roadWidth, notnull array<ResourceName> prefabs, notnull array<vector> offsets)
 {
  prefabs.Clear();
  offsets.Clear();
  bool east = IsEast(factionKey);
  if (IsFia(factionKey))
  {
   switch (order)
   {
    case ACAFE_EOrder.ROADBLOCK:
     if (roadWidth <= 5) prefabs.Insert(LOGS_1);
     else if (roadWidth <= 8) prefabs.Insert(LOGS_2);
     else prefabs.Insert(LOGS_3);
     offsets.Insert(vector.Zero);
     return 6;
    case ACAFE_EOrder.CHECKPOINT_S:
     prefabs.Insert(LOGS_3);
     offsets.Insert(vector.Zero);
     return 7;
    case ACAFE_EOrder.CHECKPOINT_M:
     prefabs.Insert(LOGS_3);
     offsets.Insert(Vector(0, 0, 4));
     prefabs.Insert(LOGS_2);
     offsets.Insert(Vector(1.5, 0, -4));
     return 9;
    case ACAFE_EOrder.CHECKPOINT_L:
     prefabs.Insert(LOGS_3);
     offsets.Insert(Vector(0, 0, 7));
     prefabs.Insert(LOGS_2);
     offsets.Insert(Vector(-1.5, 0, 0));
     prefabs.Insert(LOGS_3);
     offsets.Insert(Vector(0, 0, -7));
     float side = roadWidth * 0.5 + 1.5;
     prefabs.Insert(HEDGEHOG);
     offsets.Insert(Vector(side, 0, 7));
     prefabs.Insert(HEDGEHOG);
     offsets.Insert(Vector(-side, 0, -7));
     return 11;
   }
   return 6;
  }
  switch (order)
  {
   case ACAFE_EOrder.ROADBLOCK:
    // The smallest barricade that spans the road (vanilla widths 7 / 10 / 15 m).
    if (roadWidth <= 6)
    {
     if (east) prefabs.Insert(BARRICADE_S_USSR);
     else prefabs.Insert(BARRICADE_S_US);
     offsets.Insert(vector.Zero);
     return 5;
    }
    if (roadWidth <= 9)
    {
     if (east) prefabs.Insert(BARRICADE_M_USSR);
     else prefabs.Insert(BARRICADE_M_US);
     offsets.Insert(vector.Zero);
     return 6;
    }
    if (east) prefabs.Insert(BARRICADE_L_USSR);
    else prefabs.Insert(BARRICADE_L_US);
    offsets.Insert(vector.Zero);
    return 8;
   case ACAFE_EOrder.CHECKPOINT_S:
    if (east) prefabs.Insert(CHECKPOINT_S_USSR);
    else prefabs.Insert(CHECKPOINT_S_US);
    offsets.Insert(vector.Zero);
    return 6;
   case ACAFE_EOrder.CHECKPOINT_M:
    if (east) prefabs.Insert(CHECKPOINT_M_USSR);
    else prefabs.Insert(CHECKPOINT_M_US);
    offsets.Insert(vector.Zero);
    return 8;
   case ACAFE_EOrder.CHECKPOINT_L:
    if (east) prefabs.Insert(CHECKPOINT_L_USSR);
    else prefabs.Insert(CHECKPOINT_L_US);
    offsets.Insert(vector.Zero);
    return 11;
  }
  return 6;
 }
}
