// Per-match settings. Chosen on ACA's setup screen ("Field engineering settings..." page),
// or with launch options (-acafeMode realistic, -acafeBuilders engineers ...), or a mission
// header; read through ACA's own ACR_Options, so the setup screen wins over flags. Anything
// not chosen follows the preset (-acafeMode / the Preset row), then ACAFE_Config.
//
//   Preset          standard | realistic       (realistic = the four rules below at their strict value)
//   Builders        any | engineers            only sappers, engineers or a squad with a construction truck
//   Materials       any | truck                roadblocks/checkpoints: RP or truck supplies | truck supplies only
//   Mines           paid | kit                 bought (RP or truck) | the mines the soldiers carry, then the truck's
//   Own mines       ai | side | real           spare our AI | spare our whole side | go off for anyone
//   Costs           1 | 0.5 | 2 | 0            multiplier (0 = free)
//   Work time       1 | 0.5 | 2                multiplier
//   Enemy AI        0 | 1                      the ACA AI commander uses field engineering
class ACAFE_Settings
{
 static const string MODE = "acafeMode";
 static const string ENABLED = "acafe";
 static const string BUILDERS = "acafeBuilders";
 static const string MATERIALS = "acafeMaterials";
 static const string MINES = "acafeMines";
 static const string OWN_MINES = "acafeOwnMines";
 static const string COST = "acafeCost";
 static const string TIME = "acafeTime";
 static const string AI = "acafeAI";

 // ---------------------------------------------------------------- effective values

 static bool Enabled() { return Value(ENABLED) != "0"; }
 static bool EngineersOnly() { return Value(BUILDERS) == "engineers"; }
 static bool TruckMaterials() { return Value(MATERIALS) == "truck"; }
 static bool KitMines() { return Value(MINES) == "kit"; }
 static string OwnMines() { return Value(OWN_MINES); }
 static bool AIEnabled() { return Value(AI) == "1" || ACAFE_Config.AITest(); }

 static float CostScale()
 {
  float scale = Value(COST).ToFloat();
  if (scale < 0) scale = 1;
  return scale;
 }

 static float TimeScale()
 {
  float scale = Value(TIME).ToFloat();
  if (scale <= 0) scale = 1;
  return Math.Clamp(scale, 0.1, 10);
 }

 // The chosen value, else the preset's, else ACAFE_Config's.
 static string Value(string option)
 {
  string value;
  if (ACR_Options.Get(option, value))
  {
   value.ToLower();
   // A bare flag (-acafeAI, -acafe) means on.
   if (value.IsEmpty() && (option == AI || option == ENABLED)) value = "1";
   if (!value.IsEmpty()) return value;
  }
  if (option != MODE && Preset() == "realistic")
  {
   string strict = Realistic(option);
   if (!strict.IsEmpty()) return strict;
  }
  return Default(option);
 }

 static string Preset()
 {
  string value;
  if (ACR_Options.Get(MODE, value))
  {
   value.ToLower();
   if (value == "realistic") return value;
  }
  return "standard";
 }

 static string Realistic(string option)
 {
  if (option == BUILDERS) return "engineers";
  if (option == MATERIALS) return "truck";
  if (option == MINES) return "kit";
  if (option == OWN_MINES) return "real";
  return "";
 }

 // The standard preset: ACAFE_Config.
 static string Default(string option)
 {
  if (option == ENABLED)
  {
   if (ACAFE_Config.ENABLED) return "1";
   return "0";
  }
  if (option == BUILDERS)
  {
   if (ACAFE_Config.ENGINEERS_ONLY) return "engineers";
   return "any";
  }
  if (option == MATERIALS)
  {
   if (ACAFE_Config.TRUCK_MATERIALS_ONLY) return "truck";
   return "any";
  }
  if (option == MINES)
  {
   if (ACAFE_Config.KIT_MINES) return "kit";
   return "paid";
  }
  if (option == OWN_MINES)
  {
   if (ACAFE_Config.REALISTIC_MINES) return "real";
   if (ACAFE_Config.OWN_PLAYERS_SAFE) return "side";
   return "ai";
  }
  if (option == COST) return ScaleText(ACAFE_Config.COST_SCALE);
  if (option == TIME) return ScaleText(ACAFE_Config.TIME_SCALE);
  if (option == AI)
  {
   if (ACAFE_Config.AI_USES_ENGINEERING) return "1";
   return "0";
  }
  return "";
 }

 // The row values' spelling ("1", "0.5"...) for a config multiplier.
 static string ScaleText(float scale)
 {
  if (scale == 0) return "0";
  if (scale == 0.5) return "0.5";
  if (scale == 1) return "1";
  if (scale == 2) return "2";
  return scale.ToString();
 }

 // "standard", "realistic", or "custom" when the rules match neither preset.
 static string PresetShown()
 {
  array<string> rules = {BUILDERS, MATERIALS, MINES, OWN_MINES};
  bool realistic = true;
  bool standard = true;
  foreach (string rule : rules)
  {
   if (Value(rule) != Realistic(rule)) realistic = false;
   if (Value(rule) != Default(rule)) standard = false;
  }
  if (realistic) return "realistic";
  if (standard) return "standard";
  return "custom";
 }

 // ---------------------------------------------------------------- setup screen rows

 // Row keys on ACA's setup dialog: "acafe:<option>".
 static string Rows()
 {
  array<string> options = {MODE, ENABLED, BUILDERS, MATERIALS, MINES, OWN_MINES, COST, TIME, AI};
  string rows;
  foreach (string option : options)
  {
   string shown = Value(option);
   if (option == MODE) shown = PresetShown();
   rows += "acafe:" + option + "\t" + Label(option) + ": " + ValueName(option, shown) + "\n";
  }
  return rows;
 }

 static string Label(string option)
 {
  if (option == MODE) return "Preset";
  if (option == ENABLED) return "Field engineering";
  if (option == BUILDERS) return "Builders";
  if (option == MATERIALS) return "Roadblock materials";
  if (option == MINES) return "Mines";
  if (option == OWN_MINES) return "Own mines";
  if (option == COST) return "Costs";
  if (option == TIME) return "Work time";
  if (option == AI) return "Enemy AI commander uses it";
  return option;
 }

 static string ValueName(string option, string value)
 {
  if (option == MODE)
  {
   if (value == "realistic") return "realistic";
   if (value == "custom") return "custom";
   return "standard";
  }
  if (option == ENABLED || option == AI)
  {
   if (value == "0") return "off";
   return "on";
  }
  if (option == BUILDERS)
  {
   if (value == "engineers") return "engineers or truck";
   return "any squad";
  }
  if (option == MATERIALS)
  {
   if (value == "truck") return "truck supplies only";
   return "RP or truck supplies";
  }
  if (option == MINES)
  {
   if (value == "kit") return "soldiers' kit, then truck";
   return "RP or truck supplies";
  }
  if (option == OWN_MINES)
  {
   if (value == "real") return "dangerous to all";
   if (value == "side") return "safe for our side";
   return "safe for our AI";
  }
  if (option == COST || option == TIME)
  {
   float scale = value.ToFloat();
   if (option == COST && scale == 0) return "free";
   if (scale == 1) return "normal";
   if (scale == 0.5) return "half";
   if (scale == 2) return "double";
   return "x" + value;
  }
  return value;
 }

 static array<string> Values(string option)
 {
  array<string> values = {};
  if (option == MODE) values = {"standard", "realistic"};
  else if (option == ENABLED || option == AI) values = {"1", "0"};
  else if (option == BUILDERS) values = {"any", "engineers"};
  else if (option == MATERIALS) values = {"any", "truck"};
  else if (option == MINES) values = {"paid", "kit"};
  else if (option == OWN_MINES) values = {"ai", "side", "real"};
  else if (option == COST) values = {"1", "0.5", "2", "0"};
  else if (option == TIME) values = {"1", "0.5", "2"};
  return values;
 }

 // A click on one of our rows (server). Returns the message for the player.
 static string Handle(int playerId, string key)
 {
  if (!ACR_Setup.Wanted() || ACR_Setup.Started()) return "The match has already started";
  if (!ACR_Manager.IsHostOrAdmin(playerId)) return "Only the host or an admin can change the match setup";
  if (!key.StartsWith("acafe:")) return "Unknown option";
  string option = key.Substring(6, key.Length() - 6);
  array<string> values = Values(option);
  if (values.IsEmpty()) return "Unknown option";
  string current = Value(option);
  if (option == MODE) current = PresetShown();
  int index = values.Find(current);
  string next = values[(index + 1) % values.Count()];
  if (option == MODE)
  {
   // A preset sets the four rules at once (each can still be changed afterwards).
   ACR_Options.Set(MODE, next);
   ACR_Options.Unset(BUILDERS);
   ACR_Options.Unset(MATERIALS);
   ACR_Options.Unset(MINES);
   ACR_Options.Unset(OWN_MINES);
  }
  else ACR_Options.Set(option, next);
  string shown = next;
  if (option == MODE) shown = PresetShown();
  PrintFormat("[ACAFE] SETUP player %1: %2 = %3", playerId, option, next);
  return Label(option) + ": " + ValueName(option, shown);
 }

 // One line for the log when the match starts.
 static string Describe()
 {
  return string.Format("on=%1 preset=%2 builders=%3 materials=%4 mines=%5 ownMines=%6 cost=x%7 time=x%8 ai=%9",
   Enabled(), PresetShown(), Value(BUILDERS), Value(MATERIALS), Value(MINES), Value(OWN_MINES), CostScale(), TimeScale(), AIEnabled());
 }
}
