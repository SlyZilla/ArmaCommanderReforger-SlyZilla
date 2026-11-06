// The attack ground picture (ACR_AttackGround, -acrAttackGround; Jacob 4 Nov), run from srv:regress:
// the option, a reading of a real base's ground on the test world checked against independent sight
// traces (lines of departure, breaks, firing positions), the eye set, and the background build.
class ACR_TestGround
{
 static ref ACR_AttackGround s_Background;

 static void Run(ACR_Manager manager)
 {
  CheckOption();
  ACR_AttackGround ground = BestReading();
  CheckMixed(ground);
  CheckDepartures(ground);
  CheckFiring(ground);
  CheckEyes(ground);
  StartBackground(ground);
 }

 static void CheckOption()
 {
  bool offByDefault = !ACR_AttackGround.On();
  ACR_Options.Set("acrAttackGround", "1");
  bool on = ACR_AttackGround.On() && ACR_AttackGround.Mode() == 1;
  ACR_Options.Set("acrAttackGround", "2");
  bool draw = ACR_AttackGround.Mode() == 2;
  ACR_Options.Unset("acrAttackGround");
  ACR_TestRegress.Check("ground: off by default, -acrAttackGround 1 reads, 2 also draws", offByDefault && on && draw && !ACR_AttackGround.On(), "");
 }

 // Reads the ground of up to six bases, each approached from the nearest other base, and keeps the
 // one with the most lines of departure (the test world's hills, not a flat field).
 static ACR_AttackGround BestReading()
 {
  ACR_AttackGround best;
  int bestDepartures = -1;
  int tried;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (tried >= 6) break;
   if (!base || base.m_bRetired) continue;
   ACR_Base from;
   float nearest = -1;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (!other || other == base || other.m_bRetired) continue;
    float d = vector.DistanceXZ(other.GetOrigin(), base.GetOrigin());
    if (nearest < 0 || d < nearest) { nearest = d; from = other; }
   }
   if (!from) continue;
   tried++;
   ACR_AttackGround ground = new ACR_AttackGround(base, "test", from.GetOrigin() - base.GetOrigin(), nearest);
   array<vector> none = {};
   ground.SetEyes(none);
   ground.BuildNow();
   int departures = Departures(ground);
   if (departures > bestDepartures) { bestDepartures = departures; best = ground; }
  }
  return best;
 }

 static int Departures(ACR_AttackGround ground)
 {
  int count;
  foreach (ACR_GroundSpoke spoke : ground.m_aSpokes)
   if (spoke.m_iDeparture >= 0 && spoke.m_iBreak >= 0) count++;
  return count;
 }

 static int SeenBy(ACR_AttackGround ground, vector pos)
 {
  BaseWorld world = GetGame().GetWorld();
  int seen;
  foreach (vector eye : ground.m_aEyes)
   if (vector.DistanceXZ(eye, pos) <= ACR_AttackGround.EYE_RANGE && ACR_AttackGround.Sees(world, eye, pos + Vector(0, ACR_AttackGround.BODY, 0))) seen++;
  return seen;
 }

 static void CheckMixed(ACR_AttackGround ground)
 {
  bool ok = ground && ground.IsDone() && ground.LandCount() > 50 && ground.DeadCount() > 0 && ground.DeadCount() < ground.LandCount();
  string detail = "no base read";
  if (ground) detail = ground.Summary(new array<vector>());
  ACR_TestRegress.Check("ground: a base's approaches read as both dead and seen ground", ok, detail);
 }

 // Each line of departure: nothing sees it, something sees the break just inside it.
 static void CheckDepartures(ACR_AttackGround ground)
 {
  if (!ground) { ACR_TestRegress.Check("ground: lines of departure are dead ground with the break inside them", false, "no reading"); return; }
  int checked, wrong;
  string first;
  foreach (ACR_GroundSpoke spoke : ground.m_aSpokes)
  {
   if (spoke.m_iDeparture < 0 || spoke.m_iBreak < 0) continue;
   checked++;
   ACR_GroundSample departure = ground.m_aSamples[spoke.m_iDeparture];
   ACR_GroundSample brk = ground.m_aSamples[spoke.m_iBreak];
   bool inside = vector.DistanceXZ(brk.m_vPos, ground.m_vCenter) < vector.DistanceXZ(departure.m_vPos, ground.m_vCenter);
   if (SeenBy(ground, departure.m_vPos) == 0 && SeenBy(ground, brk.m_vPos) > 0 && inside) continue;
   wrong++;
   if (first.IsEmpty()) first = string.Format("departure %1 seen by %2, break %3 seen by %4", ACR_Manager.Grid(departure.m_vPos), SeenBy(ground, departure.m_vPos), ACR_Manager.Grid(brk.m_vPos), SeenBy(ground, brk.m_vPos));
  }
  ACR_TestRegress.Check("ground: lines of departure are dead ground with the break inside them", checked > 0 && wrong == 0, string.Format("%1 checked, %2 wrong %3", checked, wrong, first));
 }

 // Each firing position sees an eye and has dead ground just outside it.
 static void CheckFiring(ACR_AttackGround ground)
 {
  if (!ground) { ACR_TestRegress.Check("ground: firing positions see the enemy with dead ground behind", false, "no reading"); return; }
  int wrong;
  foreach (int index : ground.m_aFiring)
  {
   ACR_GroundSample firing = ground.m_aSamples[index];
   ACR_GroundSample behind = ground.m_aSamples[index + 1];
   bool sameSpoke = behind.m_iSpoke == firing.m_iSpoke && behind.m_iStep == firing.m_iStep + 1;
   if (!sameSpoke || SeenBy(ground, firing.m_vPos) == 0 || SeenBy(ground, behind.m_vPos) != 0) wrong++;
  }
  ACR_TestRegress.Check("ground: firing positions see the enemy with dead ground behind", ground.m_aFiring.Count() > 0 && wrong == 0, string.Format("%1 positions, %2 wrong", ground.m_aFiring.Count(), wrong));
 }

 // A seen enemy somewhere new adds an eye and starts a new reading; the same enemy a few metres on doesn't.
 static void CheckEyes(ACR_AttackGround ground)
 {
  if (!ground) { ACR_TestRegress.Check("ground: a newly seen enemy adds an eye and rereads the ground", false, "no reading"); return; }
  int before = ground.m_aEyes.Count();
  vector enemy = ground.m_vCenter + Vector(30, 0, 30);
  array<vector> seen = {enemy};
  bool reread = ground.SetEyes(seen) && ground.m_aEyes.Count() == before + 1 && !ground.IsDone();
  ground.BuildNow();
  array<vector> moved = {enemy + Vector(5, 0, 0)};
  bool kept = !ground.SetEyes(moved) && ground.IsDone();
  ACR_TestRegress.Check("ground: a newly seen enemy adds an eye and rereads the ground", reread && kept, string.Format("eyes %1 -> %2", before, ground.m_aEyes.Count()));
 }

 // A reading started the normal way is built by the call queue over the next frames (checked by CheckBackground).
 static void StartBackground(ACR_AttackGround ground)
 {
  s_Background = null;
  if (!ground) return;
  s_Background = new ACR_AttackGround(ground.m_Target, "test", ground.m_vApproach, ground.m_fExtent);
  array<vector> none = {};
  s_Background.SetEyes(none);
 }

 static void CheckBackground()
 {
  bool done = s_Background && s_Background.IsDone() && s_Background.m_iTraces > 0;
  string detail = "not started";
  if (s_Background) detail = s_Background.Summary(new array<vector>());
  ACR_TestRegress.Check("ground: a reading is built in the background a little per frame", done, detail);
  s_Background = null;
 }
}
