// Fortification planner (Jacob, 24 Oct): where sandbags make sense around a held base, worked
// out from the world itself, so it runs on any map. Four kinds of position:
//   CORNER     a short high sandbag wall (1-2 pieces, sometimes an end cap bent back) off a
//              building corner into a street that leads towards the enemy: the building covers
//              one flank, the bags face up the street, a vehicle-wide lane stays open.
//   SQUARE     a horseshoe redoubt beside an urban junction (3+ road arms with buildings round
//              it), on the friendly side, looking down the arms that lead towards the enemy.
//   EDGE       an L of sandbags just off a house corner on the town edge, facing open ground
//              towards the enemy (villages whose houses stand back from the road get these).
//   PERIMETER  a horseshoe fighting position on open ground at the base edge, facing an
//              approach with a long view.
// Every position is dressed so it looks dug in by people (Jacob, 24 Oct: "humanize" them):
// duckboards, a lantern, a chair, folded canvas, crates, wire out front, a camo net, picked
// per spot from a generator seeded by its position, so a spot always looks the same.
// Only replicated props without inventories are used (no loot, visible to every client).
// Threat directions are the nearest bases the owner does not hold. Nothing here changes the
// game: the only consumer so far is the debug preview (/acr fortify), which spawns the props
// (no navmesh rebuild, no structure registration) so the spots can be judged.

enum ACR_EFortifyKind
{
	CORNER,
	SQUARE,
	PERIMETER,
	EDGE
}

class ACR_FortifyPos : Managed
{
	ACR_EFortifyKind m_eKind;
	vector m_vPos;         // anchor: the building corner, redoubt centre or position
	vector m_vFacing;      // unit XZ direction the bags face
	float m_fScore;
	float m_fThreat;       // ThreatDot of the facing
	float m_fSight;        // clear view along the facing, m
	float m_fLane;         // CORNER: open road left beside the wall, m
	string m_sNote;
	ref array<vector> m_aPiecePos = {};
	ref array<float> m_aPieceYaw = {};
	ref array<ResourceName> m_aPiecePrefab = {};
	ref array<bool> m_aPieceWall = {};     // sandbags (false: dressing)

	void Add(vector pos, float yaw, ResourceName prefab, bool wall)
	{
		if (prefab.IsEmpty()) return;
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		m_aPiecePos.Insert(pos);
		m_aPieceYaw.Insert(yaw);
		m_aPiecePrefab.Insert(prefab);
		m_aPieceWall.Insert(wall);
	}

	// A sandbag piece facing a direction, set by hand: a couple of degrees and centimetres off
	// (more and the pieces of one wall visibly step).
	void AddWall(vector pos, vector facing, ResourceName prefab)
	{
		vector at = pos + facing * (ACR_Fortify.Rand() * 0.16 - 0.08);
		Add(at, ACR_Fortify.Bearing(facing) + ACR_Fortify.Rand() * 4 - 2, prefab, true);
	}

	int Walls()
	{
		int n = 0;
		foreach (bool wall : m_aPieceWall) if (wall) n++;
		return n;
	}
}

// The props one side builds with (US plastic sandbags and kit, Soviet and FIA burlap).
class ACR_FortifyKit : Managed
{
	ResourceName m_sLong;
	ResourceName m_sEnd;
	ResourceName m_sRound;
	ResourceName m_sLight;
	ResourceName m_sChair;
	ResourceName m_sNet;
	ref array<ResourceName> m_aClutter = {};
}

class ACR_FortifySeg : Managed
{
	vector m_vA;
	vector m_vB;
	float m_fWidth;
	int m_iRoad;
	bool m_bFirst;
	bool m_bLast;
}

class ACR_FortifyNode : Managed
{
	vector m_vPos;
	ref array<vector> m_aArms = {};
	ref array<int> m_aRoads = {};
	int m_iBuildings;
	float m_fOpen;
}

class ACR_Fortify : Managed
{
	static const ResourceName LONG_PLASTIC = "{31A455F7FD8DCE84}Prefabs/Props/Military/Sandbags/Sandbag_01_long_high_plastic.et";
	static const ResourceName LONG_BURLAP = "{BE16EE8FAA315FE2}Prefabs/Props/Military/Sandbags/Sandbag_01_long_high_burlap.et";
	static const ResourceName END_PLASTIC = "{35F2A674E602198F}Prefabs/Props/Military/Sandbags/Sandbag_01_end_high_plastic.et";
	static const ResourceName END_BURLAP = "{B547CF929FCC6BB7}Prefabs/Props/Military/Sandbags/Sandbag_01_end_high_burlap.et";
	static const ResourceName ROUND_PLASTIC = "{B6307C189CCCA0B9}Prefabs/Props/Military/Sandbags/Sandbag_01_round_high_plastic.et";
	static const ResourceName ROUND_BURLAP = "{7AF4B627D5C90235}Prefabs/Props/Military/Sandbags/Sandbag_01_round_high_burlap.et";
	static const ResourceName DUCKBOARD = "{CC66836BF4E211F9}Prefabs/Props/Military/Camps/Duckboard_01.et";
	static const ResourceName LANTERN_US = "{50E6A3845C9F20FD}Prefabs/Props/Military/Camps/LanternMilitary_US_01.et";
	static const ResourceName LAMP_EAST = "{2CB0AA2A87842C18}Prefabs/Props/Civilian/LampKerosene_01/LampKerosene_01_military_USSR.et";
	static const ResourceName CANVAS = "{31E97EEAE2DA9C8F}Prefabs/Structures/Military/Camps/Canvas_Covers/US/CanvasCover_Folded_US.et";
	static const ResourceName CHAIR_US = "{55973C5B0950D4AB}Prefabs/Props/Military/Furniture/ChairMilitary_US_01.et";
	static const ResourceName CHAIR_EAST = "{172DD50ACF177B9E}Prefabs/Props/Military/Furniture/ChairMilitary_USSR_01.et";
	static const ResourceName NET_US = "{C06AF25AD6AD95D1}Prefabs/Structures/Military/CamoNets/US/CamoNet_Wall_medium_US.et";
	static const ResourceName NET_USSR = "{08313D18309DAC02}Prefabs/Structures/Military/CamoNets/Soviet/CamoNet_Wall_medium_Soviet.et";
	static const ResourceName NET_FIA = "{7E1711BD38AB2AE6}Prefabs/Structures/Military/CamoNets/FIA/CamoNet_Wall_medium_FIA_01_V1.et";
	static const ResourceName COIL = "{BE7B5BAFE1A16A50}Prefabs/Props/Military/Fortification/BarbedTape_Coil.et";
	static const ResourceName WHEELBARROW = "{6DD0648BB7A52420}Prefabs/Props/Civilian/Wheelbarrow_01.et";
	static const ResourceName CRATES = "{AA1E1DD1D69055EC}Prefabs/Props/Military/MilitaryCrates/CrateStack_01/CrateStack_01_green.et";
	static const ResourceName LOG = "{32654927F2657E8A}Prefabs/Props/Forest/WoodPile/WoodLog_01_A.et";
	static const ResourceName TIRES = "{CEA239D9E1899899}Prefabs/Props/Garbage/Tires/TirePile_01_small.et";
	static const ResourceName JEEP_TIRES = "{5CA44F8D9F7491ED}Prefabs/Props/VehicleParts/Tires/Tire_M151A2_pile.et";
	static const ResourceName LADDER = "{7BB30759D09C70B5}Prefabs/Props/Civilian/StepladderWooden_01.et";
	static const ResourceName PALLET = "{D7F69BF7D2DCA0A8}Prefabs/Props/Industrial/Pallets/Pallet_01.et";
	static const ResourceName BARREL = "{F4BB6B6A0486F797}Prefabs/Props/Industrial/BarrelMetal_01_military.et";

	static const float LONG_LEN = 3.4;        // m along a long piece's local X (model 3.45 m)
	static const float END_LEN = 2.1;         // m along an end piece's local X (model 2.13 m)
	static const float ROUND_HALF = 1.6;      // m from a round piece's centre to its tips
	static const float LANE = 3.5;            // m of road a corner wall must leave open
	static const float MIN_ROAD_WIDTH = 3.0;  // narrower splines are paths
	static const float MIN_THREAT = 0.3;      // a corner's street must lead roughly enemy-wards
	static const float MIN_CORNER_SIGHT = 15.0;
	static const float MIN_PERIMETER_SIGHT = 30.0;
	static const float MIN_EDGE_SIGHT = 40.0;
	static const int MAX_PERIMETER = 3;

	// Rejection counters of the last Plan, for tuning (printed by the preview).
	static int s_iCornersSeen;
	static int s_iNoRoad;
	static int s_iNoThreat;
	static int s_iNoFit;
	static int s_iBlocked;
	static int s_iNoSight;

	static ref array<IEntity> s_aPreview = {};
	protected static ref array<IEntity> s_aFound = {};
	protected static int s_iSeed = 1;

	// ---------------------------------------------------------------- variety

	// Each position seeds the generator from where it stands, so its look never changes.
	static void Seed(vector p)
	{
		s_iSeed = Math.AbsInt((int)(p[0] * 10) * 31 + (int)(p[2] * 10) * 17) % 65521 + 1;
		Rand();
		Rand();
	}

	// 0..1
	static float Rand()
	{
		s_iSeed = (s_iSeed * 75 + 74) % 65537;
		return 1.0 * s_iSeed / 65537.0;
	}

	static int RandIndex(int count)
	{
		int idx = (int)(Rand() * count);
		if (idx >= count) idx = count - 1;
		return idx;
	}

	static ACR_FortifyKit Kit(string factionKey)
	{
		ACR_FortifyKit kit = new ACR_FortifyKit();
		bool east = factionKey == "USSR" || factionKey == "RHS_AFRF";
		bool fia = factionKey == "FIA";
		kit.m_sLong = LONG_PLASTIC;
		kit.m_sEnd = END_PLASTIC;
		kit.m_sRound = ROUND_PLASTIC;
		kit.m_sLight = LANTERN_US;
		kit.m_sChair = CHAIR_US;
		kit.m_sNet = NET_US;
		if (east || fia)
		{
			kit.m_sLong = LONG_BURLAP;
			kit.m_sEnd = END_BURLAP;
			kit.m_sRound = ROUND_BURLAP;
			kit.m_sLight = LAMP_EAST;
			kit.m_sChair = CHAIR_EAST;
			kit.m_sNet = NET_USSR;
		}
		if (fia) kit.m_sNet = NET_FIA;
		// Regulars bring crates and canvas; FIA make do with what the town has.
		kit.m_aClutter.Insert(kit.m_sChair);
		kit.m_aClutter.Insert(CANVAS);
		kit.m_aClutter.Insert(PALLET);
		kit.m_aClutter.Insert(BARREL);
		if (fia)
		{
			kit.m_aClutter.Insert(TIRES);
			kit.m_aClutter.Insert(LOG);
			kit.m_aClutter.Insert(WHEELBARROW);
			kit.m_aClutter.Insert(LADDER);
		}
		else
		{
			kit.m_aClutter.Insert(CRATES);
			kit.m_aClutter.Insert(CRATES);
			kit.m_aClutter.Insert(JEEP_TIRES);
			kit.m_aClutter.Insert(WHEELBARROW);
		}
		return kit;
	}

	// The facing of a piece whose long axis runs along axis, front towards the enemy side.
	static vector FacingForAxis(vector axis, vector enemyward)
	{
		vector g = Vector(-axis[2], 0, axis[0]);
		if (DotXZ(g, enemyward) < 0) g = g * -1;
		return g;
	}

	static vector RightOf(vector facing)
	{
		return Vector(facing[2], 0, -facing[0]);
	}

	// Lived-in clutter behind a position: count items spread along `along` (0..span) from
	// origin, back..back+depth behind the facing. Skips spots against buildings, in water,
	// on another item and (offRoad) on a road.
	static void Dress(ACR_FortifyPos pos, ACR_FortifyKit kit, vector origin, vector facing, vector along, float span, float back, float depth, int count, array<ref ACR_FortifySeg> segs, array<IEntity> buildings, bool offRoad)
	{
		for (int i = 0; i < count; i++)
		{
			ResourceName prefab = kit.m_aClutter[RandIndex(kit.m_aClutter.Count())];
			vector p = origin + along * (Rand() * span) - facing * (back + Rand() * depth);
			float yaw = Rand() * 360 - 180;
			if (BuildingGap(buildings, p, 6) < 0.6 || BBB_Terrain.IsWaterAt(p)) continue;
			if (offRoad && RoadEdgeGap(segs, p) < 0.5) continue;
			bool crowded = false;
			foreach (int k, vector other : pos.m_aPiecePos)
				if (!pos.m_aPieceWall[k] && vector.DistanceXZ(other, p) < 1.2) crowded = true;
			if (crowded) continue;
			pos.Add(p, yaw, prefab, false);
		}
	}

	// Drops pieces that ended up against a building (end caps, boards and nets are placed
	// without their own check; jitter can push a wall piece a little closer).
	static void Prune(ACR_FortifyPos pos, array<IEntity> buildings)
	{
		for (int i = pos.m_aPiecePos.Count() - 1; i >= 0; i--)
		{
			float need = 0.5;
			if (pos.m_aPieceWall[i]) need = 0.25;
			if (BuildingGap(buildings, pos.m_aPiecePos[i], 8) >= need) continue;
			pos.m_aPiecePos.Remove(i);
			pos.m_aPieceYaw.Remove(i);
			pos.m_aPiecePrefab.Remove(i);
			pos.m_aPieceWall.Remove(i);
		}
	}

	// A light at the foot of the bags, some of the time.
	static void Light(ACR_FortifyPos pos, ACR_FortifyKit kit, vector at, array<IEntity> buildings)
	{
		if (Rand() < 0.4 || BuildingGap(buildings, at, 6) < 0.4) return;
		pos.Add(at, Rand() * 360 - 180, kit.m_sLight, false);
	}

	// Barbed wire coil out in front, across the facing, where it blocks no road or door.
	static void Wire(ACR_FortifyPos pos, vector at, vector facing, array<ref ACR_FortifySeg> segs, array<IEntity> buildings)
	{
		if (RoadEdgeGap(segs, at) < 1.5 || BuildingGap(buildings, at, 8) < 2 || BBB_Terrain.IsWaterAt(at)) return;
		pos.Add(at, Bearing(facing) + 90 + Rand() * 20 - 10, COIL, false);
	}

	// End pieces bent back from both tips of a round piece: a horseshoe. A flank that would
	// stand on a road or against a building is left out.
	static void Horseshoe(ACR_FortifyPos pos, ACR_FortifyKit kit, vector roundAt, vector facing, array<ref ACR_FortifySeg> segs, array<IEntity> buildings, bool offRoad)
	{
		vector right = RightOf(facing);
		for (int side = -1; side <= 1; side += 2)
		{
			float bend = (40 + Rand() * 25) * Math.DEG2RAD;
			vector axis = right * (Math.Cos(bend) * side) - facing * Math.Sin(bend);
			vector at = roundAt + right * (ROUND_HALF * side) - facing * 0.5 + axis * (END_LEN * 0.5);
			if (BuildingGap(buildings, at, 6) < 0.4) continue;
			if (offRoad && RoadEdgeGap(segs, at) < 0.3) continue;
			pos.AddWall(at, FacingForAxis(axis, facing), kit.m_sEnd);
		}
	}

	// ---------------------------------------------------------------- geometry helpers

	static float Bearing(vector dir)
	{
		float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
		while (yaw > 180) yaw -= 360;
		while (yaw <= -180) yaw += 360;
		return yaw;
	}

	static vector FlatDir(vector from, vector to)
	{
		vector d = to - from;
		d[1] = 0;
		float len = d.Length();
		if (len < 0.001) return vector.Zero;
		return d * (1.0 / len);
	}

	static float DotXZ(vector a, vector b)
	{
		return a[0] * b[0] + a[2] * b[2];
	}

	static string Compass(vector dir)
	{
		array<string> names = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
		float b = Bearing(dir);
		if (b < 0) b += 360;
		int idx = (int)Math.Round(b / 45.0);
		if (idx >= 8) idx = 0;
		return names[idx];
	}

	// Distance in XZ from p to a road segment's centreline, with the closest point.
	static float SegDistance(ACR_FortifySeg s, vector p, out vector proj)
	{
		vector seg = s.m_vB - s.m_vA;
		seg[1] = 0;
		float lenSq = seg.LengthSq();
		float t = 0;
		if (lenSq > 0.0001)
		{
			vector v = p - s.m_vA;
			t = (v[0] * seg[0] + v[2] * seg[2]) / lenSq;
			t = Math.Clamp(t, 0.0, 1.0);
		}
		proj = s.m_vA + seg * t;
		proj[1] = p[1];
		return vector.DistanceXZ(p, proj);
	}

	static int NearestSeg(array<ref ACR_FortifySeg> segs, vector p, out vector proj, out float dist)
	{
		int best = -1;
		dist = float.MAX;
		foreach (int i, ACR_FortifySeg s : segs)
		{
			vector q;
			float d = SegDistance(s, p, q);
			if (d < dist)
			{
				dist = d;
				proj = q;
				best = i;
			}
		}
		return best;
	}

	// How far p is from the nearest road's edge (negative: on the road).
	static float RoadEdgeGap(array<ref ACR_FortifySeg> segs, vector p)
	{
		float gap = float.MAX;
		foreach (ACR_FortifySeg s : segs)
		{
			vector q;
			float d = SegDistance(s, p, q) - s.m_fWidth * 0.5;
			if (d < gap) gap = d;
		}
		return gap;
	}

	static float BuildingGap(array<IEntity> buildings, vector p, float within)
	{
		float gap = float.MAX;
		foreach (IEntity b : buildings)
		{
			if (!b || vector.DistanceXZ(b.GetOrigin(), p) > within + 40) continue;
			float d = ACR_Garrison.DistanceToBox(b, p);
			if (d < gap) gap = d;
		}
		return gap;
	}

	// Free line of sight (characters ignored), as a distance along dir.
	static float Sight(vector from, vector dir, float range)
	{
		vector start = from;
		start[1] = GetGame().GetWorld().GetSurfaceY(start[0], start[2]) + 1.3;
		TraceParam trace = new TraceParam();
		trace.Start = start;
		trace.End = start + dir * range;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		float fraction = GetGame().GetWorld().TraceMove(trace, ACR_CombatFlank.StaticObstacle);
		return fraction * range;
	}

	// Nothing solid along a wall line at knee height.
	static bool LineClear(vector a, vector b)
	{
		BaseWorld world = GetGame().GetWorld();
		vector start = a;
		start[1] = world.GetSurfaceY(a[0], a[2]) + 0.6;
		vector end = b;
		end[1] = world.GetSurfaceY(b[0], b[2]) + 0.6;
		TraceParam trace = new TraceParam();
		trace.Start = start;
		trace.End = end;
		trace.Flags = TraceFlags.ENTS;
		return world.TraceMove(trace, ACR_CombatFlank.StaticObstacle) >= 1;
	}

	static bool FlatEnough(vector a, vector b, float maxRise)
	{
		BaseWorld world = GetGame().GetWorld();
		return Math.AbsFloat(world.GetSurfaceY(a[0], a[2]) - world.GetSurfaceY(b[0], b[2])) <= maxRise;
	}

	// ---------------------------------------------------------------- inputs

	// Directions to the nearest (up to three) bases the owner does not hold. Enemy bases
	// weigh 1, neutral ones 0.7, and further ones less.
	static void Threats(ACR_Base base, string owner, notnull array<vector> dirs, notnull array<float> weights)
	{
		vector center = base.GetOrigin();
		array<ACR_Base> picks = {};
		float nearest = 0;
		for (int pick = 0; pick < 3; pick++)
		{
			ACR_Base best = null;
			float bestDist = float.MAX;
			foreach (ACR_Base other : ACR_Base.s_All)
			{
				if (!other || other == base || other.m_bRetired || picks.Contains(other)) continue;
				if (!owner.IsEmpty() && other.m_sOwner == owner) continue;
				float d = vector.DistanceXZ(center, other.GetOrigin());
				if (d < 50 || d >= bestDist) continue;
				best = other;
				bestDist = d;
			}
			if (!best) break;
			if (picks.IsEmpty()) nearest = bestDist;
			picks.Insert(best);
			float w = 0.7;
			if (!best.m_sOwner.IsEmpty()) w = 1.0;
			w = w * Math.Clamp(nearest / bestDist, 0.6, 1.0);
			dirs.Insert(FlatDir(center, best.GetOrigin()));
			weights.Insert(w);
		}
	}

	// 1 = straight at a threat; with no threats known every direction counts.
	static float ThreatDot(array<vector> dirs, array<float> weights, vector dir)
	{
		if (dirs.IsEmpty()) return 1;
		float best = -1;
		foreach (int i, vector t : dirs)
		{
			float v = weights[i] * DotXZ(t, dir);
			if (v > best) best = v;
		}
		return best;
	}

	static void Roads(vector center, float radius, notnull array<ref ACR_FortifySeg> segs)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return;
		array<BaseRoad> roads = {};
		aiWorld.GetRoadNetworkManager().GetRoadsInAABB(center - Vector(radius, 500, radius), center + Vector(radius, 500, radius), roads);
		foreach (int ri, BaseRoad road : roads)
		{
			if (!road) continue;
			float width = road.GetWidth();
			if (width < MIN_ROAD_WIDTH) continue;
			array<vector> pts = {};
			road.GetPoints(pts);
			int count = pts.Count();
			for (int pi = 0; pi + 1 < count; pi++)
			{
				if (vector.DistanceXZ(pts[pi], center) > radius + 30 && vector.DistanceXZ(pts[pi + 1], center) > radius + 30) continue;
				ACR_FortifySeg seg = new ACR_FortifySeg();
				seg.m_vA = pts[pi];
				seg.m_vB = pts[pi + 1];
				seg.m_fWidth = width;
				seg.m_iRoad = ri;
				seg.m_bFirst = pi == 0;
				seg.m_bLast = pi + 2 == count;
				segs.Insert(seg);
			}
		}
	}

	protected static bool CollectBuilding(IEntity e)
	{
		if (!e || e.GetParent()) return true;
		if (!SCR_DestructibleBuildingEntity.Cast(e) && !BaseBuilding.Cast(e)) return true;
		vector mins, maxs;
		e.GetBounds(mins, maxs);
		vector size = maxs - mins;
		if (size[0] < 3.5 || size[2] < 3.5 || size[1] < 2.5) return true;
		s_aFound.Insert(e);
		return true;
	}

	static void Buildings(vector center, float radius, notnull array<IEntity> found)
	{
		s_aFound.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, CollectBuilding, null, EQueryEntitiesFlags.STATIC);
		foreach (IEntity e : s_aFound) found.Insert(e);
		s_aFound.Clear();
	}

	static void AddArm(ACR_FortifyNode node, vector dir, int road)
	{
		if (dir == vector.Zero) return;
		foreach (vector arm : node.m_aArms)
			if (DotXZ(arm, dir) > 0.9) return;
		node.m_aArms.Insert(dir);
		if (!node.m_aRoads.Contains(road)) node.m_aRoads.Insert(road);
	}

	// Road ends that meet, plus roads passing through a road end: junctions with their arms.
	static void Junctions(array<ref ACR_FortifySeg> segs, array<IEntity> buildings, notnull array<ref ACR_FortifyNode> nodes)
	{
		foreach (ACR_FortifySeg s : segs)
		{
			if (s.m_bFirst) AddEnd(nodes, s.m_vA, FlatDir(s.m_vA, s.m_vB), s.m_iRoad);
			if (s.m_bLast) AddEnd(nodes, s.m_vB, FlatDir(s.m_vB, s.m_vA), s.m_iRoad);
		}
		foreach (ACR_FortifyNode node : nodes)
		{
			foreach (ACR_FortifySeg through : segs)
			{
				if (node.m_aRoads.Contains(through.m_iRoad)) continue;
				vector q;
				if (SegDistance(through, node.m_vPos, q) > through.m_fWidth * 0.5 + 3) continue;
				vector t = FlatDir(through.m_vA, through.m_vB);
				AddArm(node, t, through.m_iRoad);
				AddArm(node, t * -1, through.m_iRoad);
			}
			node.m_fOpen = BuildingGap(buildings, node.m_vPos, 45);
			foreach (IEntity b : buildings)
				if (b && ACR_Garrison.DistanceToBox(b, node.m_vPos) < 45) node.m_iBuildings++;
		}
	}

	protected static void AddEnd(array<ref ACR_FortifyNode> nodes, vector pos, vector dir, int road)
	{
		foreach (ACR_FortifyNode n : nodes)
		{
			if (vector.DistanceXZ(n.m_vPos, pos) > 10) continue;
			AddArm(n, dir, road);
			return;
		}
		ACR_FortifyNode node = new ACR_FortifyNode();
		node.m_vPos = pos;
		AddArm(node, dir, road);
		nodes.Insert(node);
	}

	// ---------------------------------------------------------------- the three kinds

	static void Corners(ACR_Base base, ACR_FortifyKit kit, array<vector> dirs, array<float> weights, array<ref ACR_FortifySeg> segs, array<IEntity> buildings, array<ref ACR_FortifyNode> nodes, notnull array<ref ACR_FortifyPos> found)
	{
		foreach (IEntity b : buildings)
		{
			if (!b) continue;
			vector mins, maxs;
			b.GetBounds(mins, maxs);
			vector bc = b.CoordToParent((mins + maxs) * 0.5);
			vector axisX = b.VectorToParent(Vector(1, 0, 0));
			vector axisZ = b.VectorToParent(Vector(0, 0, 1));
			array<vector> local = {Vector(mins[0], mins[1], mins[2]), Vector(maxs[0], mins[1], mins[2]), Vector(maxs[0], mins[1], maxs[2]), Vector(mins[0], mins[1], maxs[2])};
			foreach (vector lc : local)
			{
				s_iCornersSeen++;
				vector c = b.CoordToParent(lc);
				vector proj;
				float dist;
				int si = NearestSeg(segs, c, proj, dist);
				if (si < 0)
				{
					s_iNoRoad++;
					continue;
				}
				ACR_FortifySeg seg = segs[si];
				float width = seg.m_fWidth;
				float edgeGap = dist - width * 0.5;
				if (edgeGap < -1 || edgeGap > 5)
				{
					s_iNoRoad++;
					continue;
				}
				// The street must run along a face of the building, not diagonally past it.
				vector n = FlatDir(c, proj);
				if (n == vector.Zero || (Math.AbsFloat(DotXZ(n, axisX)) < 0.8 && Math.AbsFloat(DotXZ(n, axisZ)) < 0.8))
				{
					s_iNoRoad++;
					continue;
				}
				vector f = FlatDir(seg.m_vA, seg.m_vB);
				if (ThreatDot(dirs, weights, f * -1) > ThreatDot(dirs, weights, f)) f = f * -1;
				float threat = ThreatDot(dirs, weights, f);
				// The building must stand behind the corner, seen from the enemy end.
				if (threat < MIN_THREAT || DotXZ(bc - c, f) >= 0)
				{
					s_iNoThreat++;
					continue;
				}
				// As many pieces as fit while a lane stays open across the road.
				vector start = c + n * 0.4 + f * 0.6;
				float roadLeft = dist + width * 0.5 - 0.4;
				int pieces = (int)Math.Floor((roadLeft - LANE) / LONG_LEN);
				int byCentre = (int)Math.Floor((dist + 0.5) / LONG_LEN);
				if (byCentre < pieces) pieces = byCentre;
				if (pieces > 2) pieces = 2;
				if (pieces < 1)
				{
					s_iNoFit++;
					continue;
				}
				vector end = start + n * (LONG_LEN * pieces);
				// The trace misses a wall it starts inside (a neighbour's box), so check the pieces too.
				bool pieceInside = false;
				for (int pk = 0; pk < pieces; pk++)
					if (BuildingGap(buildings, start + n * (LONG_LEN * (0.5 + pk)), 10) < 0.3) pieceInside = true;
				if (pieceInside || !LineClear(start, end) || !FlatEnough(start, end, 1.0) || BBB_Terrain.IsWaterAt(end) || ACR_FobSlots.VehicleOnSpot((start + end) * 0.5, 3.0))
				{
					s_iBlocked++;
					continue;
				}
				float sight = Sight((start + end) * 0.5 + f * 0.8, f, 60);
				if (sight < MIN_CORNER_SIGHT)
				{
					s_iNoSight++;
					continue;
				}
				ACR_FortifyPos pos = new ACR_FortifyPos();
				pos.m_eKind = ACR_EFortifyKind.CORNER;
				pos.m_vPos = c;
				pos.m_vFacing = f;
				pos.m_fThreat = threat;
				pos.m_fSight = sight;
				pos.m_fLane = roadLeft - LONG_LEN * pieces;
				pos.m_fScore = threat * 40 + sight * 0.5;
				foreach (ACR_FortifyNode node : nodes)
				{
					if (node.m_aArms.Count() >= 3 && vector.DistanceXZ(node.m_vPos, c) < 35)
					{
						pos.m_fScore += 12;
						pos.m_sNote = "near junction";
						break;
					}
				}
				Seed(c);
				for (int k = 0; k < pieces; k++)
					pos.AddWall(start + n * (LONG_LEN * (0.5 + k)), f, kit.m_sLong);
				// Sometimes an end piece bent back at the street end, if the lane still allows it.
				float span = LONG_LEN * pieces;
				float bend = (30 + Rand() * 30) * Math.DEG2RAD;
				vector capAxis = n * Math.Cos(bend) - f * Math.Sin(bend);
				if (Rand() < 0.6 && pos.m_fLane - END_LEN * Math.Cos(bend) >= LANE)
				{
					pos.AddWall(start + n * span + capAxis * (END_LEN * 0.5), FacingForAxis(capAxis, f), kit.m_sEnd);
					pos.m_fLane = pos.m_fLane - END_LEN * Math.Cos(bend);
				}
				Light(pos, kit, start + n * (Rand() * span) - f * 0.9, buildings);
				if (Rand() < 0.5) pos.Add(start + n * (span * 0.5) - f * 1.4, Bearing(f) + 90 + Rand() * 10 - 5, DUCKBOARD, false);
				Dress(pos, kit, start, f, n, span, 1.4, 1.2, 1 + RandIndex(3), segs, buildings, false);
				pos.m_sNote = string.Format("%1 %2 pieces, road %3 m", pos.m_sNote, pieces, width.ToString(-1, 1));
				Prune(pos, buildings);
				found.Insert(pos);
			}
		}
	}

	static void Squares(ACR_Base base, ACR_FortifyKit kit, array<vector> dirs, array<float> weights, array<ref ACR_FortifySeg> segs, array<IEntity> buildings, array<ref ACR_FortifyNode> nodes, notnull array<ref ACR_FortifyPos> found)
	{
		vector center = base.GetOrigin();
		foreach (ACR_FortifyNode node : nodes)
		{
			int armCount = node.m_aArms.Count();
			if (armCount < 3 || node.m_iBuildings < 4) continue;
			float halfRoad = 2.5;
			vector q;
			float nodeDist;
			int si = NearestSeg(segs, node.m_vPos, q, nodeDist);
			if (si >= 0) halfRoad = segs[si].m_fWidth * 0.5;
			// Arms in bearing order; redoubt candidates sit on the bisectors between them.
			array<float> bearings = {};
			foreach (vector arm : node.m_aArms) bearings.Insert(Bearing(arm));
			bearings.Sort();
			vector bestSpot = vector.Zero;
			float bestScore = -1000000;
			for (int ai = 0; ai < bearings.Count(); ai++)
			{
				float b1 = bearings[ai];
				float b2 = bearings[(ai + 1) % bearings.Count()];
				if (b2 <= b1) b2 += 360;
				float mid = (b1 + b2) * 0.5 * Math.DEG2RAD;
				vector bis = Vector(Math.Sin(mid), 0, Math.Cos(mid));
				array<float> offsets = {halfRoad + 3, halfRoad + 5, halfRoad + 8};
				foreach (float off : offsets)
				{
					vector p = node.m_vPos + bis * off;
					p[1] = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
					if (RoadEdgeGap(segs, p) < 1.5 || BuildingGap(buildings, p, 10) < 1.5) continue;
					if (BBB_Terrain.IsWaterAt(p) || !FlatEnough(p - bis * 2, p + bis * 2, 1.0) || ACR_FobSlots.VehicleOnSpot(p, 4.0)) continue;
					// Friendly side of the junction, seeing down the threatening arms.
					float score = -10 * ThreatDot(dirs, weights, bis) - off;
					foreach (vector armDir : node.m_aArms)
					{
						if (ThreatDot(dirs, weights, armDir) < 0.2) continue;
						vector target = node.m_vPos + armDir * 35;
						if (Sight(p, FlatDir(p, target), vector.DistanceXZ(p, target)) >= vector.DistanceXZ(p, target) - 1) score += 10;
					}
					if (score > bestScore)
					{
						bestScore = score;
						bestSpot = p;
					}
					break;
				}
			}
			if (bestSpot == vector.Zero) continue;
			// Face the two most threatening arms (one piece when they are close together).
			vector first = vector.Zero;
			vector second = vector.Zero;
			float t1 = -1000000;
			float t2 = -1000000;
			foreach (vector armWay : node.m_aArms)
			{
				vector face = FlatDir(bestSpot, node.m_vPos + armWay * 25);
				float t = ThreatDot(dirs, weights, armWay);
				if (t > t1)
				{
					second = first;
					t2 = t1;
					first = face;
					t1 = t;
				}
				else if (t > t2)
				{
					second = face;
					t2 = t;
				}
			}
			ACR_FortifyPos pos = new ACR_FortifyPos();
			pos.m_eKind = ACR_EFortifyKind.SQUARE;
			pos.m_vPos = bestSpot;
			pos.m_vFacing = first;
			pos.m_fThreat = t1;
			pos.m_fSight = Sight(bestSpot, first, 60);
			pos.m_fScore = bestScore + armCount * 5 + Math.Min(node.m_fOpen, 20) - vector.DistanceXZ(node.m_vPos, center) * 0.05;
			Seed(bestSpot);
			vector mainAt = RedoubtPiece(bestSpot, first, segs, buildings);
			pos.AddWall(mainAt, first, kit.m_sRound);
			Horseshoe(pos, kit, mainAt, first, segs, buildings, true);
			if (second != vector.Zero && t2 >= 0.2 && DotXZ(first, second) < 0.5) pos.AddWall(RedoubtPiece(bestSpot, second, segs, buildings), second, kit.m_sRound);
			vector across = RightOf(first);
			if (Rand() < 0.7) pos.Add(bestSpot - first * 0.9, Bearing(first) + 90 + Rand() * 10 - 5, DUCKBOARD, false);
			Light(pos, kit, mainAt - first * 0.9 + across * (Rand() * 2 - 1), buildings);
			Dress(pos, kit, bestSpot - across * 1.5, first, across, 3.0, 1.6, 1.5, 2 + RandIndex(2), segs, buildings, true);
			if (Rand() < 0.5) Wire(pos, mainAt + first * (5 + Rand() * 2), first, segs, buildings);
			int openM = (int)Math.Min(node.m_fOpen, 99);
			pos.m_sNote = string.Format("%1 arms, %2 buildings, open %3 m", armCount, node.m_iBuildings, openM);
			Prune(pos, buildings);
			found.Insert(pos);
		}
	}

	// A redoubt piece stands 1.2 m out towards its arm, unless that puts it on the road or
	// against a building; then it stays at the redoubt centre.
	static vector RedoubtPiece(vector spot, vector facing, array<ref ACR_FortifySeg> segs, array<IEntity> buildings)
	{
		vector p = spot + facing * 1.2;
		if (RoadEdgeGap(segs, p) < 0.5 || BuildingGap(buildings, p, 10) < 0.5) return spot;
		return p;
	}

	static void Perimeter(ACR_Base base, ACR_FortifyKit kit, array<ref ACR_FortifySeg> segs, array<vector> dirs, array<float> weights, array<IEntity> buildings, notnull array<ref ACR_FortifyPos> found)
	{
		vector center = base.GetOrigin();
		float ring = Math.Clamp(base.m_fRadius, 60, 200);
		array<vector> axes = {};
		if (dirs.IsEmpty())
		{
			axes.Insert(Vector(0, 0, 1));
			axes.Insert(Vector(1, 0, 0));
			axes.Insert(Vector(0, 0, -1));
			axes.Insert(Vector(-1, 0, 0));
		}
		else
		{
			foreach (vector d : dirs) axes.Insert(d);
		}
		array<float> scales = {0.9, 1.0, 1.15};
		foreach (vector axis : axes)
		{
			ACR_FortifyPos best = null;
			float axisBearing = Bearing(axis);
			for (float shift = -40; shift <= 40; shift += 8)
			{
				float rad = (axisBearing + shift) * Math.DEG2RAD;
				vector outward = Vector(Math.Sin(rad), 0, Math.Cos(rad));
				foreach (float scale : scales)
				{
					vector p = center + outward * (ring * scale);
					p[1] = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
					string why;
					if (!ACR_FobBuild.FootprintClear(p, 2.5, null, why, true, 1.5)) continue;
					float sight = Sight(p, outward, 120);
					if (sight < MIN_PERIMETER_SIGHT) continue;
					float threat = ThreatDot(dirs, weights, outward);
					float score = threat * 30 + sight * 0.25;
					if (BuildingGap(buildings, p - outward * 10, 15) < 15) score += 10;
					if (best && best.m_fScore >= score) continue;
					best = new ACR_FortifyPos();
					best.m_eKind = ACR_EFortifyKind.PERIMETER;
					best.m_vPos = p;
					best.m_vFacing = outward;
					best.m_fThreat = threat;
					best.m_fSight = sight;
					best.m_fScore = score;
				}
			}
			if (!best) continue;
			Seed(best.m_vPos);
			vector f = best.m_vFacing;
			vector across = RightOf(f);
			best.AddWall(best.m_vPos, f, kit.m_sRound);
			Horseshoe(best, kit, best.m_vPos, f, segs, buildings, true);
			if (Rand() < 0.6) best.Add(best.m_vPos - f * 1.4, Bearing(f) + 90 + Rand() * 10 - 5, DUCKBOARD, false);
			if (Rand() < 0.4) best.Add(best.m_vPos - f * 2.6, Bearing(f), kit.m_sNet, false);
			Light(best, kit, best.m_vPos - f * 0.9 + across * (Rand() * 2 - 1), buildings);
			Dress(best, kit, best.m_vPos - across * 1.5, f, across, 3.0, 1.8, 1.5, 1 + RandIndex(2), segs, buildings, true);
			if (Rand() < 0.6) Wire(best, best.m_vPos + f * (6 + Rand() * 3), f, segs, buildings);
			best.m_sNote = string.Format("ring %1 m", (int)vector.DistanceXZ(center, best.m_vPos));
			Prune(best, buildings);
			found.Insert(best);
		}
	}

	// House corners on the side facing the enemy with open ground in front: an L of sandbags
	// just off the corner, square to the house front, the end piece covering the flank.
	static void Edges(ACR_Base base, ACR_FortifyKit kit, array<vector> dirs, array<float> weights, array<ref ACR_FortifySeg> segs, array<IEntity> buildings, notnull array<ref ACR_FortifyPos> found)
	{
		foreach (IEntity b : buildings)
		{
			if (!b) continue;
			vector mins, maxs;
			b.GetBounds(mins, maxs);
			vector bc = b.CoordToParent((mins + maxs) * 0.5);
			array<vector> normals = {b.VectorToParent(Vector(1, 0, 0)), b.VectorToParent(Vector(-1, 0, 0)), b.VectorToParent(Vector(0, 0, 1)), b.VectorToParent(Vector(0, 0, -1))};
			array<vector> local = {Vector(mins[0], mins[1], mins[2]), Vector(maxs[0], mins[1], mins[2]), Vector(maxs[0], mins[1], maxs[2]), Vector(mins[0], mins[1], maxs[2])};
			foreach (vector lc : local)
			{
				vector c = b.CoordToParent(lc);
				vector diag = FlatDir(bc, c);
				// The two faces meeting here: the front is the one that faces the enemy most.
				vector front = vector.Zero;
				vector side = vector.Zero;
				float frontThreat = -1;
				foreach (vector nrm : normals)
				{
					vector flat = FlatDir(vector.Zero, nrm);
					if (DotXZ(flat, diag) <= 0) continue;
					float t = ThreatDot(dirs, weights, flat);
					if (t > frontThreat)
					{
						if (front != vector.Zero) side = front;
						front = flat;
						frontThreat = t;
					}
					else side = flat;
				}
				if (front == vector.Zero || side == vector.Zero || frontThreat < 0.35) continue;
				vector at = c + front * 1.4 + side * 0.4;
				at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
				if (RoadEdgeGap(segs, at) < 2.0 || BuildingGap(buildings, at, 10) < 0.8) continue;
				if (BBB_Terrain.IsWaterAt(at) || !FlatEnough(at - side * 1.7, at + side * 1.7, 0.8)) continue;
				float sight = Sight(at + front * 0.8, front, 120);
				if (sight < MIN_EDGE_SIGHT) continue;
				ACR_FortifyPos pos = new ACR_FortifyPos();
				pos.m_eKind = ACR_EFortifyKind.EDGE;
				pos.m_vPos = c;
				pos.m_vFacing = front;
				pos.m_fThreat = frontThreat;
				pos.m_fSight = sight;
				pos.m_fScore = frontThreat * 30 + sight * 0.3;
				Seed(c);
				pos.AddWall(at, front, kit.m_sLong);
				// The end piece turns back along the side face, covering the flank.
				vector flankAt = at + side * (LONG_LEN * 0.5 + 0.3) - front * (END_LEN * 0.5);
				if (BuildingGap(buildings, flankAt, 6) >= 0.4 && RoadEdgeGap(segs, flankAt) >= 1.0) pos.AddWall(flankAt, side, kit.m_sEnd);
				if (Rand() < 0.3) pos.Add(at - front * 1.6 - side * 1.2, Bearing(front), kit.m_sNet, false);
				Light(pos, kit, at - front * 0.9 + side * (Rand() * 2 - 1), buildings);
				Dress(pos, kit, at - side * 1.5, front, side, 3.0, 1.2, 1.2, 1 + RandIndex(2), segs, buildings, true);
				pos.m_sNote = string.Format("house corner, view %1 m", (int)sight);
				Prune(pos, buildings);
				found.Insert(pos);
			}
		}
	}

	// Two positions whose sandbags come within 2.5 m (walls off neighbouring buildings can
	// reach into the same street even when their corners are far apart).
	static bool Clash(ACR_FortifyPos a, ACR_FortifyPos b)
	{
		foreach (int i, vector pa : a.m_aPiecePos)
		{
			if (!a.m_aPieceWall[i]) continue;
			foreach (int j, vector pb : b.m_aPiecePos)
				if (b.m_aPieceWall[j] && vector.DistanceXZ(pa, pb) < 2.5) return true;
		}
		return false;
	}

	// Best first, at most max of them, spacing m apart.
	static void Pick(array<ref ACR_FortifyPos> candidates, int max, float spacing, notnull array<ref ACR_FortifyPos> result)
	{
		array<ref ACR_FortifyPos> chosen = {};
		while (chosen.Count() < max)
		{
			ACR_FortifyPos best = null;
			foreach (ACR_FortifyPos c : candidates)
			{
				if (chosen.Contains(c)) continue;
				if (best && best.m_fScore >= c.m_fScore) continue;
				bool tooClose = false;
				foreach (ACR_FortifyPos have : chosen)
					if (vector.DistanceXZ(have.m_vPos, c.m_vPos) < spacing || Clash(have, c)) tooClose = true;
				foreach (ACR_FortifyPos taken : result)
					if (vector.DistanceXZ(taken.m_vPos, c.m_vPos) < 8 || Clash(taken, c)) tooClose = true;
				if (!tooClose) best = c;
			}
			if (!best) break;
			chosen.Insert(best);
		}
		foreach (ACR_FortifyPos keep : chosen) result.Insert(keep);
	}

	// ---------------------------------------------------------------- plan

	static float SearchRadius(ACR_Base base)
	{
		return Math.Clamp(base.m_fRadius + 50, 100, 220);
	}

	static void Plan(ACR_Base base, string owner, notnull array<ref ACR_FortifyPos> result, out string summary)
	{
		s_iCornersSeen = 0; s_iNoRoad = 0; s_iNoThreat = 0; s_iNoFit = 0; s_iBlocked = 0; s_iNoSight = 0;
		summary = "";
		if (!base) return;
		vector center = base.GetOrigin();
		float radius = SearchRadius(base);
		array<vector> dirs = {};
		array<float> weights = {};
		Threats(base, owner, dirs, weights);
		array<ref ACR_FortifySeg> segs = {};
		Roads(center, radius, segs);
		// Buildings a little beyond the search radius too: pieces near its edge must clear them.
		array<IEntity> buildings = {};
		Buildings(center, radius + 30, buildings);
		array<ref ACR_FortifyNode> nodes = {};
		Junctions(segs, buildings, nodes);

		ACR_FortifyKit kit = Kit(owner);
		// Bigger towns get more: about one street corner per 7 buildings, one redoubt per 40.
		int townSize = 0;
		foreach (IEntity house : buildings)
			if (vector.DistanceXZ(house.GetOrigin(), center) <= radius) townSize++;
		array<ref ACR_FortifyPos> squares = {};
		Squares(base, kit, dirs, weights, segs, buildings, nodes, squares);
		Pick(squares, Math.ClampInt(1 + townSize / 40, 1, 3), 60, result);
		array<ref ACR_FortifyPos> corners = {};
		Corners(base, kit, dirs, weights, segs, buildings, nodes, corners);
		Pick(corners, Math.ClampInt(townSize / 7, 4, 14), 15, result);
		array<ref ACR_FortifyPos> houses = {};
		Edges(base, kit, dirs, weights, segs, buildings, houses);
		Pick(houses, Math.ClampInt(townSize / 10, 0, 6), 20, result);
		array<ref ACR_FortifyPos> edges = {};
		Perimeter(base, kit, segs, dirs, weights, buildings, edges);
		Pick(edges, MAX_PERIMETER, 40, result);

		string threatText = "";
		foreach (int i, vector d : dirs)
			threatText += string.Format("%1%2 ", Compass(d), weights[i].ToString(-1, 1));
		if (threatText.IsEmpty()) threatText = "none known ";
		summary = string.Format("%1: threats %2| %3 roads segs, %4 buildings, %5 junctions | corners %6 of %7 (no road %8, not enemy-facing %9", base.m_sName, threatText, segs.Count(), townSize, nodes.Count(), corners.Count(), s_iCornersSeen, s_iNoRoad, s_iNoThreat);
		summary += string.Format(", no fit %1, blocked %2, no view %3), squares %4, house corners %5, perimeter %6", s_iNoFit, s_iBlocked, s_iNoSight, squares.Count(), houses.Count(), edges.Count());
	}

	static string KindName(ACR_EFortifyKind kind)
	{
		switch (kind)
		{
			case ACR_EFortifyKind.CORNER: return "corner";
			case ACR_EFortifyKind.SQUARE: return "square";
			case ACR_EFortifyKind.PERIMETER: return "perimeter";
			case ACR_EFortifyKind.EDGE: return "house";
		}
		return "?";
	}

	static void Log(ACR_Base base, array<ref ACR_FortifyPos> plan)
	{
		foreach (ACR_FortifyPos p : plan)
		{
			PrintFormat("[ACR] FORTIFY %1 %2 at %3 %4 facing %5 (%6) pieces %7 score %8 | threat %9", base.m_sName, KindName(p.m_eKind), (int)p.m_vPos[0], (int)p.m_vPos[2], Compass(p.m_vFacing), (int)Bearing(p.m_vFacing), p.m_aPiecePos.Count(), (int)p.m_fScore, p.m_fThreat.ToString(-1, 2));
			PrintFormat("[ACR] FORTIFY   view %1 m, lane %2 m, %3", (int)p.m_fSight, p.m_fLane.ToString(-1, 1), p.m_sNote);
		}
	}

	// ---------------------------------------------------------------- debug preview

	static int SpawnPreview(array<ref ACR_FortifyPos> plan)
	{
		int spawned = 0;
		foreach (ACR_FortifyPos p : plan)
		{
			foreach (int i, vector at : p.m_aPiecePos)
			{
				Resource resource = Resource.Load(p.m_aPiecePrefab[i]);
				if (!resource.IsValid()) continue;
				EntitySpawnParams spawnParams = new EntitySpawnParams();
				spawnParams.TransformMode = ETransformMode.WORLD;
				Math3D.AnglesToMatrix(Vector(p.m_aPieceYaw[i], 0, 0), spawnParams.Transform);
				spawnParams.Transform[3] = at;
				IEntity ent = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
				if (!ent) continue;
				s_aPreview.Insert(ent);
				spawned++;
			}
		}
		return spawned;
	}

	static int ClearPreview()
	{
		int removed = 0;
		foreach (IEntity ent : s_aPreview)
		{
			if (!ent) continue;
			SCR_EntityHelper.DeleteEntityAndChildren(ent);
			removed++;
		}
		s_aPreview.Clear();
		return removed;
	}

	// /acr fortify [base]: replaces any earlier preview with this base's plan.
	static string Preview(ACR_Base base, string owner)
	{
		ClearPreview();
		if (!base) return "No base: name one, select it on the map or stand near it";
		if (!base.m_sOwner.IsEmpty()) owner = base.m_sOwner;
		array<ref ACR_FortifyPos> plan = {};
		string summary;
		int started = System.GetTickCount();
		Plan(base, owner, plan, summary);
		int ms = System.GetTickCount() - started;
		int spawned = SpawnPreview(plan);
		PrintFormat("[ACR] FORTIFY plan %1 (%2 ms)", summary, ms);
		Log(base, plan);
		int corners = 0;
		int squares = 0;
		int houses = 0;
		int edges = 0;
		foreach (ACR_FortifyPos p : plan)
		{
			if (p.m_eKind == ACR_EFortifyKind.CORNER) corners++;
			else if (p.m_eKind == ACR_EFortifyKind.SQUARE) squares++;
			else if (p.m_eKind == ACR_EFortifyKind.EDGE) houses++;
			else edges++;
		}
		return string.Format("[ACR] Fortify preview %1 (for %2): %3 street corners, %4 redoubts, %5 house corners, %6 perimeter posts, %7 props (%8 ms). Details in the server log (FORTIFY). /acr fortify clear removes them.", base.m_sName, owner, corners, squares, houses, edges, spawned, ms);
	}
}
