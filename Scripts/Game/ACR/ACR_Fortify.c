// Fortification planner (Jacob, 24 Oct): where sandbags make sense around a held base, worked
// out from the world itself, so it runs on any map. Three kinds of position:
//   CORNER     a short sandbag wall (1-2 pieces) off a building corner into a street that leads towards the
//              enemy: the building covers one flank, the bags face up the street, and a
//              vehicle-wide lane stays open on the far side.
//   SQUARE     a redoubt beside an urban junction (3+ road arms with buildings round it), on
//              the friendly side, looking down the arms that lead towards the enemy.
//   PERIMETER  a fighting position on open ground at the base edge, facing an approach with a
//              long view.
// Threat directions are the nearest bases the owner does not hold. Nothing here changes the
// game: the only consumer so far is the debug preview (/acr fortify), which spawns plain
// sandbag props (no navmesh rebuild, no structure registration) so the spots can be judged.

enum ACR_EFortifyKind
{
	CORNER,
	SQUARE,
	PERIMETER
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
	ref array<bool> m_aPieceRound = {};

	void AddPiece(vector pos, vector facing, bool round)
	{
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		m_aPiecePos.Insert(pos);
		m_aPieceYaw.Insert(ACR_Fortify.Bearing(facing));
		m_aPieceRound.Insert(round);
	}
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
	static const ResourceName LONG_PLASTIC = "{78AB783E28EDEBD4}Prefabs/Props/Military/Sandbags/Sandbag_01_long_plastic.et";
	static const ResourceName LONG_BURLAP = "{21D83950DF36F250}Prefabs/Props/Military/Sandbags/Sandbag_01_long_burlap.et";
	static const ResourceName ROUND_PLASTIC = "{64A3A595C0A833AC}Prefabs/Props/Military/Sandbags/Sandbag_01_round_plastic.et";
	static const ResourceName ROUND_BURLAP = "{B31A14BC1A0E7776}Prefabs/Props/Military/Sandbags/Sandbag_01_round_burlap.et";

	static const float LONG_LEN = 3.4;        // m along a long piece's local X (model 3.45 m)
	static const float LANE = 3.5;            // m of road a corner wall must leave open
	static const float MIN_ROAD_WIDTH = 3.0;  // narrower splines are paths
	static const float MIN_THREAT = 0.3;      // a corner's street must lead roughly enemy-wards
	static const float MIN_CORNER_SIGHT = 15.0;
	static const float MIN_PERIMETER_SIGHT = 30.0;
	static const int MAX_CORNERS = 8;
	static const int MAX_SQUARES = 2;
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

	static void Corners(ACR_Base base, array<vector> dirs, array<float> weights, array<ref ACR_FortifySeg> segs, array<IEntity> buildings, array<ref ACR_FortifyNode> nodes, notnull array<ref ACR_FortifyPos> found)
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
				for (int k = 0; k < pieces; k++)
					pos.AddPiece(start + n * (LONG_LEN * (0.5 + k)), f, false);
				pos.m_sNote = string.Format("%1 %2 pieces, road %3 m", pos.m_sNote, pieces, width.ToString(-1, 1));
				found.Insert(pos);
			}
		}
	}

	static void Squares(ACR_Base base, array<vector> dirs, array<float> weights, array<ref ACR_FortifySeg> segs, array<IEntity> buildings, array<ref ACR_FortifyNode> nodes, notnull array<ref ACR_FortifyPos> found)
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
			pos.AddPiece(RedoubtPiece(bestSpot, first, segs, buildings), first, true);
			if (second != vector.Zero && t2 >= 0.2 && DotXZ(first, second) < 0.5) pos.AddPiece(RedoubtPiece(bestSpot, second, segs, buildings), second, true);
			int openM = (int)Math.Min(node.m_fOpen, 99);
			pos.m_sNote = string.Format("%1 arms, %2 buildings, open %3 m", armCount, node.m_iBuildings, openM);
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

	static void Perimeter(ACR_Base base, array<vector> dirs, array<float> weights, array<IEntity> buildings, notnull array<ref ACR_FortifyPos> found)
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
			best.AddPiece(best.m_vPos, best.m_vFacing, true);
			best.m_sNote = string.Format("ring %1 m", (int)vector.DistanceXZ(center, best.m_vPos));
			found.Insert(best);
		}
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
					if (vector.DistanceXZ(have.m_vPos, c.m_vPos) < spacing) tooClose = true;
				foreach (ACR_FortifyPos taken : result)
					if (vector.DistanceXZ(taken.m_vPos, c.m_vPos) < 8) tooClose = true;
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
		array<IEntity> buildings = {};
		Buildings(center, radius, buildings);
		array<ref ACR_FortifyNode> nodes = {};
		Junctions(segs, buildings, nodes);

		array<ref ACR_FortifyPos> squares = {};
		Squares(base, dirs, weights, segs, buildings, nodes, squares);
		Pick(squares, MAX_SQUARES, 60, result);
		array<ref ACR_FortifyPos> corners = {};
		Corners(base, dirs, weights, segs, buildings, nodes, corners);
		Pick(corners, MAX_CORNERS, 15, result);
		array<ref ACR_FortifyPos> edges = {};
		Perimeter(base, dirs, weights, buildings, edges);
		Pick(edges, MAX_PERIMETER, 40, result);

		string threatText = "";
		foreach (int i, vector d : dirs)
			threatText += string.Format("%1%2 ", Compass(d), weights[i].ToString(-1, 1));
		if (threatText.IsEmpty()) threatText = "none known ";
		summary = string.Format("%1: threats %2| %3 roads segs, %4 buildings, %5 junctions | corners %6 of %7 (no road %8, not enemy-facing %9", base.m_sName, threatText, segs.Count(), buildings.Count(), nodes.Count(), corners.Count(), s_iCornersSeen, s_iNoRoad, s_iNoThreat);
		summary += string.Format(", no fit %1, blocked %2, no view %3), squares %4, perimeter %5", s_iNoFit, s_iBlocked, s_iNoSight, squares.Count(), edges.Count());
	}

	static string KindName(ACR_EFortifyKind kind)
	{
		switch (kind)
		{
			case ACR_EFortifyKind.CORNER: return "corner";
			case ACR_EFortifyKind.SQUARE: return "square";
			case ACR_EFortifyKind.PERIMETER: return "perimeter";
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

	static ResourceName PiecePrefab(bool round, string factionKey)
	{
		bool burlap = factionKey == "USSR" || factionKey == "RHS_AFRF" || factionKey == "FIA";
		if (round)
		{
			if (burlap) return ROUND_BURLAP;
			return ROUND_PLASTIC;
		}
		if (burlap) return LONG_BURLAP;
		return LONG_PLASTIC;
	}

	static int SpawnPreview(array<ref ACR_FortifyPos> plan, string factionKey)
	{
		int spawned = 0;
		foreach (ACR_FortifyPos p : plan)
		{
			foreach (int i, vector at : p.m_aPiecePos)
			{
				Resource resource = Resource.Load(PiecePrefab(p.m_aPieceRound[i], factionKey));
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
		int spawned = SpawnPreview(plan, owner);
		PrintFormat("[ACR] FORTIFY plan %1 (%2 ms)", summary, ms);
		Log(base, plan);
		int corners = 0;
		int squares = 0;
		int edges = 0;
		foreach (ACR_FortifyPos p : plan)
		{
			if (p.m_eKind == ACR_EFortifyKind.CORNER) corners++;
			else if (p.m_eKind == ACR_EFortifyKind.SQUARE) squares++;
			else edges++;
		}
		return string.Format("[ACR] Fortify preview %1 (for %2): %3 corner walls, %4 square redoubts, %5 perimeter posts, %6 sandbag pieces (%7 ms). Details in the server log (FORTIFY). /acr fortify clear removes them.", base.m_sName, owner, corners, squares, edges, spawned, ms);
	}
}
