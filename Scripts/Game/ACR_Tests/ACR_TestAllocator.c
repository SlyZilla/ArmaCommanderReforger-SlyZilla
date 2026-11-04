// -acrAllocatorTest 1 (server-capable, ~15 s): the AI job allocator (ACR_AIAllocator) on made-up
// fronts, no squads in the world: an attack takes its nearest squads and leaves the far one out,
// an attack that can't reach its bar takes no one, a far member is swapped for a near free squad,
// the job a squad has is kept unless another is clearly better, locked squads stay, an attack
// needs a squad that can take the base, and the value curves.
class ACR_TestAllocator
{
 static const int CHECKS = 13;

 static ACR_AIJobSquad Squad(string name, float strength, int current, array<float> etas, array<float> fits = null)
 {
  ACR_AIJobSquad squad = new ACR_AIJobSquad();
  squad.m_sName = name;
  squad.m_fStrength = strength;
  squad.m_iCurrent = current;
  foreach (int i, float eta : etas)
  {
   squad.m_aEta.Insert(eta);
   float fit = 1;
   if (fits && i < fits.Count()) fit = fits[i];
   squad.m_aFit.Insert(fit);
  }
  return squad;
 }

 static array<float> E(float a, float b)
 {
  array<float> pair = {};
  pair.Insert(a);
  pair.Insert(b);
  return pair;
 }

 static ACR_AIJob Attack(string name, float value, float need, float bar = 1)
 {
  ACR_AIJob job = new ACR_AIJob();
  job.m_sName = name;
  job.m_iKind = ACR_EAITask.ATTACK;
  job.m_fValue = value;
  job.m_fNeed = need;
  job.m_bTeam = true;
  job.m_bNeedsTaker = true;
  job.m_fBar = bar;
  job.m_iMaxSquads = 6;
  return job;
 }

 static ACR_AIJob Hold(string name, float value, int most = 4)
 {
  ACR_AIJob job = new ACR_AIJob();
  job.m_sName = name;
  job.m_iKind = ACR_EAITask.POSTED;
  job.m_fValue = value;
  job.m_iMaxSquads = most;
  job.m_fPatience = 1200;
  return job;
 }

 static string Picks(array<ref ACR_AIJobSquad> squads, array<ref ACR_AIJob> jobs)
 {
  string text;
  foreach (ACR_AIJobSquad squad : squads)
  {
   string where = "-";
   if (squad.m_iPicked >= 0) where = jobs[squad.m_iPicked].m_sName;
   text += string.Format(" %1->%2", squad.m_sName, where);
  }
  return text;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 3) return;

  // Value curves.
  bool curves = Math.AbsFloat(ACR_AIAllocator.TimeFactor(600, 600) - 0.5) < 0.001 && ACR_AIAllocator.TimeFactor(0, 600) == 1;
  ACR_TestRegress.Check("allocator: value halves at the job's patience", curves, string.Format("%1", ACR_AIAllocator.TimeFactor(600, 600)));
  bool ready = ACR_AIAllocator.Readiness(9, 10, 1) == 0 && ACR_AIAllocator.Readiness(12.5, 10, 1) == 1 && ACR_AIAllocator.Readiness(10, 10, 1) > 0.7;
  ACR_TestRegress.Check("allocator: a team below its bar is worth nothing, past 1.25x need no more", ready, string.Format("%1 %2 %3", ACR_AIAllocator.Readiness(9, 10, 1), ACR_AIAllocator.Readiness(10, 10, 1), ACR_AIAllocator.Readiness(12.5, 10, 1)));

  // 1. Nearest first, the far squad left out (it would hold the whole assault back).
  array<ref ACR_AIJob> jobs = {};
  jobs.Insert(Attack("Ajvan", 40, 10));
  jobs.Insert(Hold("Szilis", 2));
  array<ref ACR_AIJobSquad> squads = {};
  squads.Insert(Squad("A", 6, -1, E(60, 300)));
  squads.Insert(Squad("B", 6, -1, E(90, 300)));
  squads.Insert(Squad("Far", 6, -1, E(3000, 100)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool near = squads[0].m_iPicked == 0 && squads[1].m_iPicked == 0 && squads[2].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: an attack takes its nearest squads and leaves the far one to hold", near, Picks(squads, jobs));

  // 2. Not enough for the bar: the attack takes no one.
  jobs[0].m_fNeed = 20;
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool unfunded = squads[0].m_iPicked == 1 && squads[1].m_iPicked == 1 && jobs[0].m_aMembers.IsEmpty();
  ACR_TestRegress.Check("allocator: an attack short of its bar takes no one", unfunded, Picks(squads, jobs));

  // 3. The 3 Nov Ajvan case with a near free squad: the far member goes, the near one joins.
  jobs.Clear();
  ACR_AIJob running = Attack("Ajvan", 40, 7.4, 0.87);
  running.m_bRunning = true;
  jobs.Insert(running);
  jobs.Insert(Hold("Szilis", 2));
  squads.Clear();
  squads.Insert(Squad("Naval", 2, 0, E(0, 2000)));
  squads.Insert(Squad("Squad2", 6, 0, E(2860, 1500)));
  squads.Insert(Squad("Near", 6, -1, E(120, 900)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool swap = squads[0].m_iPicked == 0 && squads[2].m_iPicked == 0 && squads[1].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: a member 48 min out is swapped for a free squad 2 min out", swap, Picks(squads, jobs));
  // Without the near squad the attack keeps both: nothing better to do with them.
  squads.Remove(2);
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool keeps = squads[0].m_iPicked == 0 && squads[1].m_iPicked == 0;
  ACR_TestRegress.Check("allocator: with no one nearer the slow team is kept rather than sent in at 2 men", keeps, Picks(squads, jobs) + string.Format(" value %1", jobs[0].m_fScore));

  // 4. Incumbency: a slightly better job doesn't move a squad, a clearly better one does.
  jobs.Clear();
  jobs.Insert(Hold("Here", 10));
  jobs.Insert(Hold("There", 11.5));
  squads.Clear();
  squads.Insert(Squad("S", 6, 0, E(0, 0)));
  ACR_AIAllocator.Allocate(jobs, squads, 0);
  bool stays = squads[0].m_iPicked == 0;
  jobs[1].m_fValue = 14;
  ACR_AIAllocator.Allocate(jobs, squads, 0);
  bool moves = squads[0].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: a squad keeps its job for a 15% better one", stays, Picks(squads, jobs));
  ACR_TestRegress.Check("allocator: a squad moves for a 40% better one", moves, Picks(squads, jobs));

  // 5. A locked squad stays even for a much better job.
  jobs[1].m_fValue = 100;
  squads[0].m_bLocked = true;
  ACR_AIAllocator.Allocate(jobs, squads, 0);
  bool locked = squads[0].m_iPicked == 0 && jobs[0].m_aMembers.Count() == 1;
  ACR_TestRegress.Check("allocator: a locked squad stays on its job", locked, Picks(squads, jobs));

  // 6. An attack needs a squad that can take the base; a squad that can't do a job isn't given it.
  jobs.Clear();
  jobs.Insert(Attack("Ajvan", 40, 6));
  jobs.Insert(Hold("Szilis", 2));
  squads.Clear();
  ACR_AIJobSquad mortar = Squad("Medics", 8, -1, E(60, 60));
  mortar.m_bTaker = false;
  squads.Insert(mortar);
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool taker = squads[0].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: an attack with no squad able to take the base isn't made", taker, Picks(squads, jobs));
  squads.Clear();
  squads.Insert(Squad("Rifles", 8, -1, E(60, 60), E(0, 1)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool unfit = squads[0].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: a squad with fit 0 for a job never gets it", unfit, Picks(squads, jobs));

  // 7. The team cap: two attacks worth doing, one allowed.
  jobs.Clear();
  jobs.Insert(Attack("North", 40, 6));
  jobs.Insert(Attack("South", 30, 6));
  jobs[0].m_iMaxSquads = 1;
  jobs[1].m_iMaxSquads = 1;
  squads.Clear();
  squads.Insert(Squad("A", 6, -1, E(60, 60)));
  squads.Insert(Squad("B", 6, -1, E(60, 60)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool capped = !jobs[0].m_aMembers.IsEmpty() && jobs[1].m_aMembers.IsEmpty();
  ACR_TestRegress.Check("allocator: no more team jobs than allowed, the most valuable first", capped, Picks(squads, jobs));
  ACR_AIAllocator.Allocate(jobs, squads, 2);
  bool both = jobs[0].m_aMembers.Count() == 1 && jobs[1].m_aMembers.Count() == 1;
  ACR_TestRegress.Check("allocator: with two allowed, both attacks are made", both, Picks(squads, jobs));

  ACR_TestRegress.Complete("allocator", CHECKS);
 }
}
