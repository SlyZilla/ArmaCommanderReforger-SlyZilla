// ACA Field Engineering: every number worth tuning lives here. Edit, save, restart the game
// (Reforger recompiles scripts on every mission load). Costs are paid like ACA's own:
// supplies from a construction truck travelling with the squad when it has enough,
// otherwise RP (ACA sells 500 supplies for 20 RP, so 1 RP is about 25 supplies).
class ACAFE_Config
{
 static const string VERSION = "0.1.1";

 // ---------------------------------------------------------------- per-match settings
 // The "standard" preset. Each can be changed for a match on ACA's setup screen ("Field
 // engineering settings..."), or with the launch option in brackets (see ACAFE_Settings).
 static const bool ENABLED = true;                // field engineering at all (-acafe 0)
 static const bool ENGINEERS_ONLY = false;        // sappers/engineers or a truck only (-acafeBuilders engineers)
 static const bool TRUCK_MATERIALS_ONLY = false;  // structures need truck supplies (-acafeMaterials truck)
 static const bool KIT_MINES = false;             // mines from the soldiers' kit, then a truck (-acafeMines kit)
 static const float COST_SCALE = 1;               // all costs (-acafeCost 0.5); 0 = free
 static const float TIME_SCALE = 1;               // all work times (-acafeTime 2)
 static const int KIT_TRUCK_SUPPLIES_AP = 10;     // kit mines: supplies per AP mine taken from a truck
 static const int KIT_TRUCK_SUPPLIES_AT = 30;     // ... per AT mine

 // ---------------------------------------------------------------- costs (RP / truck supplies)
 static const int ROADBLOCK_RP = 6;
 static const int ROADBLOCK_SUPPLIES = 40;        // vanilla barricade M is 40
 static const int CHECKPOINT_S_RP = 8;
 static const int CHECKPOINT_S_SUPPLIES = 75;     // vanilla prices: 75 / 225 / 350
 static const int CHECKPOINT_M_RP = 15;
 static const int CHECKPOINT_M_SUPPLIES = 225;
 static const int CHECKPOINT_L_RP = 22;
 static const int CHECKPOINT_L_SUPPLIES = 350;
 static const int MINES_AP_RP = 15;
 static const int MINES_AP_SUPPLIES = 300;
 static const int MINES_AT_RP = 20;
 static const int MINES_AT_SUPPLIES = 400;
 static const int MINES_MIXED_RP = 25;
 static const int MINES_MIXED_SUPPLIES = 500;
 static const int CLEAR_MINES_RP = 3;
 static const bool TRUCK_SUPPLIES_FIRST = true;   // a construction truck within 50 m pays instead of RP

 // ---------------------------------------------------------------- build timers (seconds of work)
 static const float ROADBLOCK_SECONDS = 30;
 static const float CHECKPOINT_S_SECONDS = 40;
 static const float CHECKPOINT_M_SECONDS = 60;
 static const float CHECKPOINT_L_SECONDS = 90;
 static const float MINES_AP_SECONDS = 60;
 static const float MINES_AT_SECONDS = 75;
 static const float MINES_MIXED_SECONDS = 90;
 static const float CLEAR_SECONDS_PER_MINE = 8;   // per mine found
 static const float CLEAR_MIN_SECONDS = 30;       // a sweep takes at least this long
 static const float ENGINEER_SPEED = 2.0;         // sappers/engineers or a construction truck: like FOB upgrades
 static const float GIVE_UP_SECONDS = 180;        // no progress at the site this long: abandoned, refunded
 static const float TRAVEL_SPEED = 1.0;           // m/s assumed for the travel time limit (plus 3 min)

 // ---------------------------------------------------------------- sites
 static const float ROAD_SNAP_DISTANCE = 60;      // roadblocks/checkpoints: road this close to the click
 static const float ROAD_YAW_OFFSET = 0;          // degrees: turn the road compositions if they ever sit wrong
 static const float WORK_RADIUS = 40;             // a squad member this close to the site works (FOB circle)
 static const float STAGE_OFFSET = 10;            // m beside the road edge where the builders wait
 static const float MAX_ORDER_DISTANCE = 6000;
 static const bool HOLD_AFTER_BUILD = true;       // player squads stay and hold what they built
 static const bool SHOW_STRUCTURES_ON_MAP = true; // pale rings on the owner's command map
 static const bool USE_RIDES = true;              // squads on foot far away ask ACA for a ride
 static const float RIDE_MIN_DISTANCE = 600;

 // ---------------------------------------------------------------- minefields
 static const float MINEFIELD_SIZE = 30;          // m, square (turned along a road when there is one)
 static const int MINES_AP_COUNT = 30;
 static const int MINES_AT_COUNT = 12;
 static const int MIXED_AT_COUNT = 8;
 static const int MIXED_AP_COUNT = 20;
 static const float AP_SPACING = 2.5;             // m between AP mines
 static const float AT_SPACING = 4.0;             // m between AT mines along a road
 static const bool MINEFIELD_SIGNS = true;        // vanilla minefield signs at the corners
 static const float SIGN_YAW_OFFSET = 0;
 static const float CLEAR_RADIUS = 25;            // "Clear mines" sweeps this far round the click
 static const int MAX_MINES_PER_SIDE = 400;

 // Friendly fire (per-match setting "Own mines", -acafeOwnMines ai|side|real). Default: the
 // owning side's AI never sets off its own mines (they walk and drive through as if they knew
 // the lanes). Players on the owning side still do (they see the marked area).
 // REALISTIC_MINES = true: every mine goes off for everyone.
 static const bool REALISTIC_MINES = false;
 static const bool OWN_PLAYERS_SAFE = false;      // true: our players are spared too

 // ---------------------------------------------------------------- enemy AI commander
 static const bool AI_USES_ENGINEERING = false;   // the ACA AI commander builds and mines too (per match: -acafeAI 1)
 static const float AI_PLAN_SECONDS = 180;        // how often it considers it
 static const float AI_CHANCE = 0.5;              // ... and how often it then does it
 static const int AI_RP_RESERVE = 40;             // RP it always keeps for troops
 static const int AI_MAX_STRUCTURES = 4;
 static const int AI_MAX_MINEFIELDS = 3;
 static const int AI_MAX_JOBS = 1;                // at the same time

 // -acafeAITest (Play-ACA-FieldEngineering.cmd option 5): the AI commander uses field
 // engineering and, for testing, plans every 30 s and always acts, keeps no RP reserve, lays
 // mines by a frontline base when it has no FOB yet, logs why it didn't act, and its work shows
 // on everyone's command map ("AI ..." rings).
 static bool AITest()
 {
  return System.IsCLIParam("acafeAITest");
 }
}
