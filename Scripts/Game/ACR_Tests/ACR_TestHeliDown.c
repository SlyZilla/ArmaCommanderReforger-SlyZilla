// -acrHeliDownTest 1 (server-capable, ~70 s): a support helicopter crippled in the air comes down.
// Script flies it, so with a rotor or the engine shot the game's own loss of lift never applied;
// when the script let go, gravity came back but the engine ran on, and the game's flight model flew
// it off (srv:helipad, 31 Oct: "going down" at 62 m, then 288 m up and 1.8 km away 90 s later). A gun
// run is called; once its rotors are at speed and it is flying on its own lift, 50 m up or more, its
// movement damage goes over the crippled mark. (While spooling up it has no lift and falls anyway.)
class ACR_TestHeliDown
{
 static ref ACR_HeliRun s_Run; // ref: the run is dropped once it is going down
 static IEntity s_Vehicle;
 static int s_iHitAt = -1;
 static float s_fHitUp;
 static float s_fLow = float.MAX;
 static float s_fHigh = -float.MAX;

 protected static float Up(IEntity vehicle)
 {
  vector at = vehicle.GetOrigin();
  return at[1] - GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
  if (tick == 8)
  {
   manager.m_Bases.GetFaction("US").m_iRP = 500;
   ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
   vector target = h.Ground("1135 0 3532");
   if (alpha) target = ACR_Manager.Ground(alpha.m_Home + Vector(-200, 0, 700));
   PrintFormat("[ACR] HELIDOWN call: %1", manager.m_Supports.Call("US", h.SupportIndex(manager, ACR_ESupportType.HELI_GUN_RUN), target, true));
  }
  if (tick > 8 && !s_Run)
  {
   array<ACR_HeliRun> flying = {};
   manager.m_Supports.Helicopters(flying);
   if (!flying.IsEmpty())
   {
    s_Run = flying[0];
    s_Vehicle = s_Run.m_Vehicle;
   }
  }
  if (tick > 100 && s_iHitAt < 0) { ACR_TestRegress.Abort("helidown", "the helicopter never got 50 m up"); return; }
  if (!s_Vehicle) return;
  float up = Up(s_Vehicle);
  if (s_iHitAt < 0)
  {
   if (up < 50 || tick < 40) return; // after the rotors spool up (25 s at most): flying on its own lift
   SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(s_Vehicle.FindComponent(SCR_DamageManagerComponent));
   if (!damage) { ACR_TestRegress.Abort("helidown", "no damage manager"); return; }
   damage.SetMovementDamage(0.8);
   s_iHitAt = tick;
   s_fHitUp = up;
   PrintFormat("[ACR] HELIDOWN crippled at %1 m up, movement damage %2", (int)up, damage.GetMovementDamage());
   return;
  }
  s_fLow = Math.Min(s_fLow, up);
  s_fHigh = Math.Max(s_fHigh, up);
  if ((tick - s_iHitAt) % 5 == 0) PrintFormat("[ACR] HELIDOWN %1 s after: %2 m up", tick - s_iHitAt, (int)up);
  if (tick != s_iHitAt + 40) return;
  ACR_TestRegress.Check("helidown: the script let go of it", s_Run && s_Run.m_bDestroyed, "");
  ACR_TestRegress.Check("helidown: a helicopter crippled in the air comes down and stays down", s_fLow < 5 && up < 15, string.Format("crippled at %1 m up; lowest %2 m, highest %3 m, %4 m up after 40 s", (int)s_fHitUp, (int)s_fLow, (int)s_fHigh, (int)up));
  ACR_TestRegress.Complete("helidown", 2);
 }
}
