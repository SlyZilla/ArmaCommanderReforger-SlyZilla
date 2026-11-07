// AI commander job allocator (allocator phase 1, 3 Nov 2026; design agreed with Jacob, kept in the
// private handoff notes). The commander describes the jobs worth doing (ACR_AIJob) and the squads
// that could do them (ACR_AIJobSquad, with a cheap travel estimate and a fit per job), and
// Allocate() gives each squad the job where it is worth most, all jobs weighed together:
// - A one-squad job (grab, defend, hold) scores value x fit x cover x time factor.
// - A team job (attack) is valued as a whole by its slowest member: value x readiness (strength
//   against the need, nothing below the bar) x time factor of the LATEST arrival. Squads join it
//   nearest first, and only while the team is worth more than what they'd do elsewhere, so one far
//   squad that would hold the whole assault back isn't added.
// - The job a squad already has counts INCUMBENCY more (no swapping for a small gain); locked
//   squads (assaulting in contact, on an air lift) stay where they are.
// - Running team jobs are weighed first: a new one never takes a running one's slot under the cap.
//   They try their own squads first, and keeping one costs STAY_MARGIN less than taking it fresh.
// - Trucks are counted (soak 6 Nov): a squad whose way there is a truck ride takes one of the side's
//   free trucks, one squad a truck, so a plan never counts on more rides than there are trucks.
//   The air lift too (match 6 Nov): 'lifts' squads at most may count on one.
// Plain data in and out: the commander builds it (ACR_AICommander, "Job allocator"), and the
// tests feed it made-up fronts (ACR_TestAllocator).

class ACR_AIJob
{
 string m_sKey;            // stable between ticks: kind and target
 int m_iKind;              // ACR_EAITask
 string m_sName;           // for logs and Think
 ACR_Base m_Target;
 float m_fValue;           // what doing it now is worth to the side
 float m_fNeed;            // strength wanted; 0: any one squad does it
 int m_iMaxSquads = 1;
 bool m_bTeam;             // attack-style: valued as a whole, by its slowest member
 bool m_bNeedsTaker;       // a team that needs infantry able to take a base
 float m_fBar = 1;         // team jobs: share of m_fNeed below which they are worth nothing
 float m_fPatience = 600;  // s: its value halves when it takes this long
 bool m_bRunning;          // already under way: its members are incumbents
 ACR_AIAttack m_Attack;    // the commander's attack it stands for, when running
 // Filled by Allocate:
 ref array<int> m_aMembers = {};
 float m_fScore;           // what it came to (team: whole team; solo: sum of members)
 float m_fStrength;
 float m_fLatest;          // s: latest arrival among its members
}

class ACR_AIJobSquad
{
 ACR_Squad m_Squad;        // null in tests
 string m_sName;
 float m_fStrength;
 bool m_bTaker = true;     // can take a base
 int m_iCurrent = -1;      // job it has now (index into the jobs), -1 none
 bool m_bLocked;           // stays on m_iCurrent this tick
 ref array<float> m_aEta = {}; // s to each job, < 0 can't get there
 ref array<float> m_aFit = {}; // 0..1 per job, 0 can't do it
 ref array<bool> m_aByTruck = {}; // per job: its m_aEta counts on a free truck (none listed: never)
 ref array<bool> m_aByAir = {};   // per job: its m_aEta counts on the air lift (none listed: never)
 // Filled by Allocate:
 int m_iPicked = -1;
 float m_fScore;           // its share of what its pick is worth
 float m_fAlt;             // the best one-squad job it had instead
}

class ACR_AIAllocator
{
 static const float INCUMBENCY = 0.3;   // a squad's current job counts 30% more
 static const float OVERSTRENGTH = 1.25; // strength past need x this adds nothing
 static const int TEAM_CANDIDATES = 12;  // nearest squads tried per team job
 static const float STAY_MARGIN = 0.3;  // leaving a running team job: elsewhere must be worth 30% more

 // Value kept after 'eta' seconds: 1 now, 1/2 at 'patience'.
 static float TimeFactor(float eta, float patience)
 {
  if (eta <= 0) return 1;
  return patience / (patience + eta);
 }

 // How ready a team of 'strength' is for a job needing 'need': 0 below bar x need, then rising
 // to 1 at OVERSTRENGTH x need.
 static float Readiness(float strength, float need, float bar)
 {
  if (need <= 0) return 1;
  float ratio = strength / need;
  if (ratio < bar) return 0;
  return Math.Min(1, ratio / OVERSTRENGTH);
 }

 // A running attack is judged by its own abort rule: fully ready at the strength it set out with,
 // worth nothing below 'abortRatio' of it (soak 4 Nov: judged against need = start strength it was
 // only 0.8 ready, and attacks launched on a close call were called off 15 s later).
 static void JudgeRunning(ACR_AIJob job, float startStrength, float abortRatio)
 {
  job.m_fNeed = Math.Max(1, startStrength) / OVERSTRENGTH;
  job.m_fBar = abortRatio * OVERSTRENGTH;
 }

 static float SoloScore(ACR_AIJob job, int j, ACR_AIJobSquad squad)
 {
  if (j >= squad.m_aFit.Count() || j >= squad.m_aEta.Count()) return -1;
  float fit = squad.m_aFit[j];
  float eta = squad.m_aEta[j];
  if (fit <= 0 || eta < 0) return -1;
  float cover = 1;
  if (job.m_fNeed > 0) cover = Math.Min(1, squad.m_fStrength / job.m_fNeed);
  float score = job.m_fValue * fit * cover * TimeFactor(eta, job.m_fPatience);
  if (squad.m_iCurrent == j) score = score * (1 + INCUMBENCY);
  return score;
 }

 // A team job with these members: value x readiness x time factor of the latest arrival.
 static float TeamValue(ACR_AIJob job, int j, notnull array<ref ACR_AIJobSquad> squads, notnull array<int> members, out float strength, out float latest)
 {
  strength = 0;
  latest = 0;
  float fit;
  bool taker = !job.m_bNeedsTaker;
  bool incumbent = false;
  foreach (int s : members)
  {
   ACR_AIJobSquad squad = squads[s];
   strength += squad.m_fStrength;
   fit += squad.m_aFit[j] * squad.m_fStrength;
   latest = Math.Max(latest, squad.m_aEta[j]);
   if (squad.m_bTaker) taker = true;
   if (squad.m_iCurrent == j) incumbent = true;
  }
  if (members.IsEmpty() || !taker || strength <= 0) return 0;
  float value = job.m_fValue * (fit / strength) * Readiness(strength, job.m_fNeed, job.m_fBar) * TimeFactor(latest, job.m_fPatience);
  if (incumbent && job.m_bRunning) value = value * (1 + INCUMBENCY);
  return value;
 }

 // Its way to job j counts on a free truck.
 static bool ByTruck(ACR_AIJobSquad squad, int j)
 {
  return j < squad.m_aByTruck.Count() && squad.m_aByTruck[j];
 }

 // Its way to job j counts on the air lift.
 static bool ByAir(ACR_AIJobSquad squad, int j)
 {
  return j < squad.m_aByAir.Count() && squad.m_aByAir[j];
 }

 // The best team for a team job among the squads not yet taken: its locked members, then the
 // candidates in order (nearest first; 'ownFirst': its own squads first), keeping the prefix worth
 // most once what they'd do elsewhere is paid for. Returns that net worth (-1: none worth it).
 // 'trucks': free trucks left for squads that need a ride there (-1: no limit); one that would need
 // more is passed over, and a nearer one on foot may take its place. 'lifts': the same for the air lift.
 static float BestPrefix(ACR_AIJob job, int j, notnull array<ref ACR_AIJobSquad> squads, notnull array<bool> taken, bool ownFirst, notnull array<int> keep, int trucks = -1, int lifts = -1)
 {
  array<int> team = {};
  foreach (int m : job.m_aMembers) team.Insert(m); // locked members stay
  array<int> order = {};
  array<float> ranks = {};
  foreach (int c, ACR_AIJobSquad candidate : squads)
  {
   if (taken[c] || candidate.m_aFit[j] <= 0 || candidate.m_aEta[j] < 0) continue;
   float rank = candidate.m_aEta[j];
   if (ownFirst && candidate.m_iCurrent != j) rank += 1000000;
   int at = 0;
   while (at < ranks.Count() && ranks[at] <= rank) at++;
   order.InsertAt(c, at);
   ranks.InsertAt(rank, at);
  }
  float strength, latest, paid;
  float net = -1;
  keep.Clear();
  if (!team.IsEmpty())
  {
   net = TeamValue(job, j, squads, team, strength, latest);
   keep.Copy(team);
  }
  int tried, riding, flying;
  foreach (int o : order)
  {
   if (tried >= TEAM_CANDIDATES || team.Count() >= job.m_iMaxSquads) break;
   if (ByTruck(squads[o], j))
   {
    if (trucks >= 0 && riding >= trucks) continue;
    riding++;
   }
   if (ByAir(squads[o], j))
   {
    if (lifts >= 0 && flying >= lifts) continue;
    flying++;
   }
   tried++;
   team.Insert(o);
   // Leaving work under way costs (the walk so far, the time): its own squads are cheaper to keep.
   if (job.m_bRunning && squads[o].m_iCurrent == j) paid += squads[o].m_fAlt / (1 + STAY_MARGIN);
   else paid += squads[o].m_fAlt;
   float value = TeamValue(job, j, squads, team, strength, latest);
   if (value > 0 && value - paid > net)
   {
    net = value - paid;
    keep.Copy(team);
   }
  }
  return net;
 }

 // Gives every squad its job (m_iPicked, -1: nothing worth doing). New team jobs only while fewer
 // than 'maxTeams' team jobs are funded (running ones count, but are never cut for the cap).
 // 'trucks': the side's free trucks, one squad each, shared by every job (-1: no limit). 'lifts': the
 // same for the air lift.
 static void Allocate(notnull array<ref ACR_AIJob> jobs, notnull array<ref ACR_AIJobSquad> squads, int maxTeams, int trucks = -1, int lifts = -1)
 {
  foreach (ACR_AIJob job : jobs)
  {
   job.m_aMembers.Clear();
   job.m_fScore = 0;
   job.m_fStrength = 0;
   job.m_fLatest = 0;
  }
  array<bool> taken = {};
  foreach (int s, ACR_AIJobSquad squad : squads)
  {
   squad.m_iPicked = -1;
   squad.m_fScore = 0;
   squad.m_fAlt = 0;
   bool locked = squad.m_bLocked && squad.m_iCurrent >= 0 && squad.m_iCurrent < jobs.Count();
   taken.Insert(locked);
   if (locked)
   {
    ACR_AIJob held = jobs[squad.m_iCurrent];
    squad.m_iPicked = squad.m_iCurrent;
    held.m_aMembers.Insert(s);
    if (!held.m_bTeam)
    {
     // It counts towards its job: no more squads sent to a defence it already covers.
     held.m_fStrength += squad.m_fStrength;
     if (squad.m_iCurrent < squad.m_aEta.Count()) held.m_fLatest = Math.Max(held.m_fLatest, squad.m_aEta[squad.m_iCurrent]);
    }
   }
  }
  // What each free squad would be worth on its own: the price of taking it for a team.
  foreach (int a, ACR_AIJobSquad alt : squads)
  {
   if (taken[a]) continue;
   foreach (int aj, ACR_AIJob soloJob : jobs)
   {
    if (soloJob.m_bTeam) continue;
    alt.m_fAlt = Math.Max(alt.m_fAlt, SoloScore(soloJob, aj, alt));
   }
  }

  // Team jobs, best net worth first. Each tries its nearest squads in order of arrival and keeps
  // the prefix worth most once what they'd do elsewhere is paid for.
  array<bool> teamDone = {};
  foreach (ACR_AIJob doneJob : jobs) teamDone.Insert(!doneJob.m_bTeam);
  // Running team jobs first and outside the cap; new ones only take the slots left (soak 4 Nov: a
  // new attack worth a little more took the one slot, and the running one was called off 15 s
  // after it launched). A running job still has to be worth more than its squads would be elsewhere.
  int teams;
  bool runningPass = true;
  while (runningPass || teams < maxTeams)
  {
   int bestJob = -1;
   float bestNet = 0;
   array<int> bestSet = {};
   foreach (int j, ACR_AIJob teamJob : jobs)
   {
    if (teamDone[j] || teamJob.m_bRunning != runningPass) continue;
    // Nearest first; a running job also tries its own squads first and keeps the better (soak
    // 4 Nov: a nearer outsider worth more elsewhere sat between two members, no nearest-first
    // prefix kept both, and an attack worth more than its squads' alternatives was called off).
    array<int> keep = {};
    float net = BestPrefix(teamJob, j, squads, taken, false, keep, trucks, lifts);
    if (teamJob.m_bRunning)
    {
     array<int> ownKeep = {};
     float ownNet = BestPrefix(teamJob, j, squads, taken, true, ownKeep, trucks, lifts);
     if (ownNet > net)
     {
      net = ownNet;
      keep.Copy(ownKeep);
     }
    }
    if (net > bestNet && !keep.IsEmpty())
    {
     bestNet = net;
     bestJob = j;
     bestSet.Copy(keep);
    }
   }
   if (bestJob < 0)
   {
    if (!runningPass) break;
    runningPass = false;
    continue;
   }
   ACR_AIJob won = jobs[bestJob];
   teamDone[bestJob] = true;
   teams++;
   won.m_aMembers.Clear();
   foreach (int w : bestSet)
   {
    won.m_aMembers.Insert(w);
    taken[w] = true;
    squads[w].m_iPicked = bestJob;
    if (trucks > 0 && ByTruck(squads[w], bestJob) && !squads[w].m_bLocked) trucks--;
    if (lifts > 0 && ByAir(squads[w], bestJob) && !squads[w].m_bLocked) lifts--;
   }
   float wonStrength, wonLatest;
   won.m_fScore = TeamValue(won, bestJob, squads, won.m_aMembers, wonStrength, wonLatest);
   won.m_fStrength = wonStrength;
   won.m_fLatest = wonLatest;
   foreach (int share : won.m_aMembers)
    squads[share].m_fScore = won.m_fScore * squads[share].m_fStrength / Math.Max(0.01, won.m_fStrength);
  }
  // Team jobs left unfunded keep no one, locked members excepted (they go when they can).
  foreach (int u, ACR_AIJob unfunded : jobs)
  {
   if (!unfunded.m_bTeam || unfunded.m_fScore > 0) continue;
   for (int k = unfunded.m_aMembers.Count() - 1; k >= 0; k--)
    if (!squads[unfunded.m_aMembers[k]].m_bLocked) unfunded.m_aMembers.Remove(k);
  }

  // One-squad jobs: the best remaining (squad, job) pair each round.
  while (true)
  {
   int pickS = -1;
   int pickJ = -1;
   float pickScore = 0;
   foreach (int s2, ACR_AIJobSquad idle : squads)
   {
    if (taken[s2]) continue;
    foreach (int j2, ACR_AIJob solo : jobs)
    {
     if (solo.m_bTeam || solo.m_aMembers.Count() >= solo.m_iMaxSquads) continue;
     if (solo.m_fNeed > 0 && solo.m_fStrength >= solo.m_fNeed * OVERSTRENGTH) continue;
     if (trucks == 0 && ByTruck(idle, j2)) continue;
     if (lifts == 0 && ByAir(idle, j2)) continue;
     float score = SoloScore(solo, j2, idle);
     if (score > pickScore)
     {
      pickScore = score;
      pickS = s2;
      pickJ = j2;
     }
    }
   }
   if (pickS < 0) break;
   taken[pickS] = true;
   ACR_AIJobSquad picked = squads[pickS];
   ACR_AIJob got = jobs[pickJ];
   picked.m_iPicked = pickJ;
   picked.m_fScore = pickScore;
   if (trucks > 0 && ByTruck(picked, pickJ)) trucks--;
   if (lifts > 0 && ByAir(picked, pickJ)) lifts--;
   got.m_aMembers.Insert(pickS);
   got.m_fStrength += picked.m_fStrength;
   got.m_fScore += pickScore;
   got.m_fLatest = Math.Max(got.m_fLatest, picked.m_aEta[pickJ]);
  }
 }
}
