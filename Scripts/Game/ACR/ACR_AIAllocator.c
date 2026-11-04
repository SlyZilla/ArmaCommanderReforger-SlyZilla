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

 // Gives every squad its job (m_iPicked, -1: nothing worth doing). At most 'maxTeams' team jobs.
 static void Allocate(notnull array<ref ACR_AIJob> jobs, notnull array<ref ACR_AIJobSquad> squads, int maxTeams)
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
    squad.m_iPicked = squad.m_iCurrent;
    jobs[squad.m_iCurrent].m_aMembers.Insert(s);
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
  int teams;
  while (teams < maxTeams)
  {
   int bestJob = -1;
   float bestNet = 0;
   array<int> bestSet = {};
   foreach (int j, ACR_AIJob teamJob : jobs)
   {
    if (teamDone[j]) continue;
    array<int> team = {};
    float paid;
    foreach (int m : teamJob.m_aMembers) team.Insert(m); // locked members stay
    array<int> order = {};
    array<float> etas = {};
    foreach (int c, ACR_AIJobSquad candidate : squads)
    {
     if (taken[c] || candidate.m_aFit[j] <= 0 || candidate.m_aEta[j] < 0) continue;
     int at = 0;
     while (at < etas.Count() && etas[at] <= candidate.m_aEta[j]) at++;
     order.InsertAt(c, at);
     etas.InsertAt(candidate.m_aEta[j], at);
    }
    float strength, latest;
    float net = -1;
    array<int> keep = {};
    if (!team.IsEmpty())
    {
     net = TeamValue(teamJob, j, squads, team, strength, latest);
     keep.Copy(team);
    }
    int tried;
    foreach (int o : order)
    {
     if (tried >= TEAM_CANDIDATES || team.Count() >= teamJob.m_iMaxSquads) break;
     tried++;
     team.Insert(o);
     paid += squads[o].m_fAlt;
     float value = TeamValue(teamJob, j, squads, team, strength, latest);
     if (value > 0 && value - paid > net)
     {
      net = value - paid;
      keep.Copy(team);
     }
    }
    if (net > bestNet && !keep.IsEmpty())
    {
     bestNet = net;
     bestJob = j;
     bestSet.Copy(keep);
    }
   }
   if (bestJob < 0) break;
   ACR_AIJob won = jobs[bestJob];
   teamDone[bestJob] = true;
   teams++;
   won.m_aMembers.Clear();
   foreach (int w : bestSet)
   {
    won.m_aMembers.Insert(w);
    taken[w] = true;
    squads[w].m_iPicked = bestJob;
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
   got.m_aMembers.Insert(pickS);
   got.m_fStrength += picked.m_fStrength;
   got.m_fScore += pickScore;
   got.m_fLatest = Math.Max(got.m_fLatest, picked.m_aEta[pickJ]);
  }
 }
}
