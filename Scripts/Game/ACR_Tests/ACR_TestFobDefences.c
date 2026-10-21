// ACR_TestFobDefences: the FOB defences upgrade (Jacob, 20 Oct) on its own FOB fixture.
// Ring layout against the building footprints, real prefabs per side, the three tiers (cost,
// one-build lock, pieces placed and replaced, panel states), capture, partial and total loss,
// and the AI build order. Called by srv:structures (tick 50).
class ACR_TestFobDefences
{
	static const int CHECKS = 14; // (one per Check below)

	static ACR_Base s_Fob;
	static IEntity s_Hq;

	static void Run(ACR_Manager manager, ACR_Base fob1, ACR_Squad alpha)
	{
		if (!manager || !fob1 || !alpha || !alpha.GetAnchor())
		{
			ACR_TestRegress.Abort("structures", "FOB defences test fixture missing");
			return;
		}
		Layout();
		Prefabs();
		if (!MakeFob(manager, alpha.GetAnchor().GetOrigin(), fob1))
		{
			ACR_TestRegress.Abort("structures", "no spot for the FOB defences fixture");
			return;
		}
		// Spot checks have their own tests; this fixture stands on unprepared terrain.
		ACR_FobSlots.s_bSkipSpotChecksForTest = true;
		Tiers(manager);
		Losses(manager);
		AiOrder(manager);
		ACR_FobSlots.s_bSkipSpotChecksForTest = false;
		ACR_FobSlots.Cleanup(s_Fob);
		if (s_Hq) SCR_EntityHelper.DeleteEntityAndChildren(s_Hq);
		manager.m_Structures.ACR_TestForceTick();
	}

	static void Layout()
	{
		bool apart = true;
		string detail = "";
		float r = ACR_FobDefences.PIECE_RADIUS;
		for (int d = 0; d < ACR_FobDefences.SPOT_COUNT; d++)
		{
			vector pd = ACR_FobDefences.SpotOffset(d);
			if (pd.Length() < ACR_FobBuild.HQ_RADIUS + r || pd.Length() + r > ACR_FobBuild.FOB_SPACING * 0.5) { apart = false; detail += string.Format(" spot %1 vs HQ/spacing", d); }
			for (int s = 0; s < ACR_FobSlots.SLOT_COUNT; s++)
			{
				float rs = ACR_FobSlots.SlotRadius(s, ACR_FobSlots.MaxTier(s));
				if (vector.DistanceXZ(pd, ACR_FobSlots.SlotOffset(s)) < r + rs) { apart = false; detail += string.Format(" %1-slot%2", d, s); }
			}
			for (int e = d + 1; e < ACR_FobDefences.SPOT_COUNT; e++)
				if (vector.DistanceXZ(pd, ACR_FobDefences.SpotOffset(e)) < r + r) { apart = false; detail += string.Format(" %1-%2", d, e); }
		}
		ACR_TestRegress.Check("fobdefences: spots clear every building footprint, the HQ and FOB spacing", apart, detail);
	}

	static void Prefabs()
	{
		bool ok = true;
		string detail = "";
		array<string> sides = {"US", "USSR", "FIA"};
		foreach (string side : sides)
		{
			for (int tier = 0; tier <= 2; tier++)
			{
				for (int spot = 0; spot < ACR_FobDefences.SPOT_COUNT; spot++)
				{
					ResourceName prefab = ACR_FobDefences.PrefabFor(spot, side, tier);
					if (prefab.IsEmpty()) continue;
					if (!Resource.Load(prefab).IsValid() || ACR_Structures.KindOfName(prefab) != ACR_EStructure.DEFENCES) { ok = false; detail += string.Format(" %1/%2/%3", side, tier, spot); }
				}
			}
		}
		ACR_TestRegress.Check("fobdefences: every side's pieces load and count as fortifications", ok, detail);
	}

	static bool MakeFob(ACR_Manager manager, vector home, ACR_Base fob1)
	{
		vector spot;
		bool found = false;
		for (int ring = 0; ring < 3 && !found; ring++)
		{
			for (int i = 0; i < 24; i++)
			{
				float angle = (i * 15 + 7) * Math.DEG2RAD;
				float dist = 1500 + ring * 200;
				vector candidate = ACR_Manager.Ground(home + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist));
				if (!ACR_Manager.OnMap(candidate) || BBB_Terrain.IsWaterAt(candidate) || ACR_TestStructures.InsideABase(candidate, 200)) continue;
				if (vector.DistanceXZ(candidate, fob1.GetOrigin()) < 400) continue;
				spot = candidate;
				found = true;
				break;
			}
		}
		if (!found) return false;
		s_Hq = ACR_TestStructures.Place(ACR_FobBuild.HQ_US, spot);
		manager.m_Structures.ACR_TestForceTick();
		foreach (ACR_Base base : ACR_Base.s_All)
			if (base && base.m_bBuilt && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), spot) < 5) s_Fob = base;
		return s_Fob && s_Fob.m_FobSupply && s_Fob.m_FobDefences;
	}

	static bool Finish(ACR_Manager manager)
	{
		ACR_FobSlot defences = s_Fob.m_FobDefences;
		ACR_FobSlots.Advance(s_Fob, defences.m_fDuration + 1);
		manager.m_Structures.ACR_TestForceTick();
		return defences.m_eState == ACR_EFobSlotState.BUILT;
	}

	static bool PieceIs(int spot, string fragment)
	{
		IEntity piece = s_Fob.m_FobDefences.m_aPieces[spot];
		if (!piece || !piece.GetPrefabData()) return false;
		string name = piece.GetPrefabData().GetPrefabName();
		name.ToLower();
		return name.Contains(fragment);
	}

	static void Tiers(ACR_Manager manager)
	{
		string key = s_Fob.m_sOwner;
		ACR_FobSlot defences = s_Fob.m_FobDefences;
		SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(s_Fob.m_FobSupply);
		if (store) store.SetResourceValue(1000);
		string why;
		string ordered = ACR_FobSlots.OrderUpgradeBase(manager, key, s_Fob, ACR_FobSlots.SLOT_DEFENCES, why);
		bool charged = store && store.GetResourceValue() == 1000 - ACR_FobDefences.Cost(0);
		int building = ACR_FobSlots.FeedState(s_Fob, ACR_FobSlots.SLOT_DEFENCES);
		ACR_TestRegress.Check("fobdefences: Defences I ordered, charged 400 and shown building", !ordered.IsEmpty() && charged && defences.m_eState == ACR_EFobSlotState.BUILDING && building >= 10 && building < 110, why + " " + building.ToString());

		string lockWhy;
		bool locked = ACR_FobSlots.OrderUpgradeBase(manager, key, s_Fob, ACR_FobSlots.SLOT_HOSPITAL, lockWhy) == "" && lockWhy.Contains("Already building");
		ACR_TestRegress.Check("fobdefences: a building slot waits while defences are built", locked, lockWhy);

		int queuedBefore = ACR_Navmesh.s_iQueued;
		bool built = Finish(manager);
		bool posts = PieceIs(0, "sandbagposition") && PieceIs(3, "sandbagposition") && !s_Fob.m_FobDefences.m_aPieces[4];
		ACR_Structure record = manager.m_Structures.Find(defences.m_aPieces[0]);
		bool registered = record && record.m_eKind == ACR_EStructure.DEFENCES && record.m_sFaction == key;
		ACR_TestRegress.Check("fobdefences: Defences I places 4 sandbag positions, registered for its side", built && posts && ACR_FobDefences.Standing(defences) == 4 && registered && ACR_FobSlots.FeedState(s_Fob, ACR_FobSlots.SLOT_DEFENCES) == 300, string.Format("built=%1 standing=%2 registered=%3", built, ACR_FobDefences.Standing(defences), registered));

		array<ref Tuple2<vector, vector>> areas = {};
		array<bool> redoRoads = {};
		bool onLayer = defences.m_aPieces[0] && ACR_Navmesh.Collect(defences.m_aPieces[0], areas, redoRoads);
		int queued = ACR_Navmesh.s_iQueued - queuedBefore;
		ACR_TestRegress.Check("fobdefences: placed pieces queue a navmesh rebuild round their bounds", onLayer && queued >= 4, string.Format("areas=%1 queued=%2", areas.Count(), queued));

		float yawDelta = Math.AbsFloat(defences.m_aPieces[1].GetYawPitchRoll()[0] - ACR_FobDefences.SpotYaw(1));
		ACR_TestRegress.Check("fobdefences: pieces face outwards", yawDelta < 1 || yawDelta > 359, yawDelta.ToString());

		if (store) store.SetResourceValue(1000);
		ACR_FobSlots.OrderUpgradeBase(manager, key, s_Fob, ACR_FobSlots.SLOT_DEFENCES, why);
		bool tier1Charged = store && store.GetResourceValue() == 1000 - ACR_FobDefences.Cost(1);
		built = Finish(manager);
		bool nests = PieceIs(4, "machinegunnest") && PieceIs(6, "machinegunnest") && PieceIs(0, "sandbagposition");
		ACR_TestRegress.Check("fobdefences: Defences II adds 3 MG nests and keeps the sandbags", tier1Charged && built && nests && defences.m_iTier == 1 && ACR_FobDefences.Standing(defences) == 7 && ACR_FobSlots.FeedState(s_Fob, ACR_FobSlots.SLOT_DEFENCES) == 400, why);

		if (store) store.SetResourceValue(1000);
		IEntity oldPost = defences.m_aPieces[0];
		ACR_FobSlots.OrderUpgradeBase(manager, key, s_Fob, ACR_FobSlots.SLOT_DEFENCES, why);
		int building2 = ACR_FobSlots.FeedState(s_Fob, ACR_FobSlots.SLOT_DEFENCES);
		built = Finish(manager);
		bool replaced = oldPost == null && PieceIs(0, "bunker_s_") && PieceIs(1, "guardtower_s_") && PieceIs(5, "machinegunnest");
		ACR_TestRegress.Check("fobdefences: Defences III turns the posts into bunkers and towers", built && replaced && defences.m_iTier == 2 && ACR_FobDefences.Standing(defences) == 7 && building2 >= 500 && building2 < 600 && ACR_FobSlots.FeedState(s_Fob, ACR_FobSlots.SLOT_DEFENCES) == 600, string.Format("building=%1 standing=%2", building2, ACR_FobDefences.Standing(defences)));

		string maxWhy;
		ACR_TestRegress.Check("fobdefences: no fourth tier", ACR_FobSlots.OrderUpgradeBase(manager, key, s_Fob, ACR_FobSlots.SLOT_DEFENCES, maxWhy) == "" && maxWhy.Contains("already built"), maxWhy);

		int tier;
		int pct;
		bool nextBlocked;
		bool decoded = ACR_FobSlots.FeedBuilding(505, tier, pct) && tier == 2 && pct == 5;
		decoded = decoded && ACR_FobSlots.FeedBuilt(401, tier, nextBlocked) && tier == 1 && nextBlocked && !ACR_FobSlots.FeedBuilding(600, tier, pct);
		ACR_TestRegress.Check("fobdefences: panel decodes tier-2 and blocked tier-1 states", decoded && ACR_FobSlots.Describe(s_Fob).Contains("Defences III"), ACR_FobSlots.Describe(s_Fob));
	}

	static void Losses(ACR_Manager manager)
	{
		ACR_FobSlot defences = s_Fob.m_FobDefences;
		string other = "USSR";
		if (s_Fob.m_sOwner == "USSR") other = "US";
		string owner = s_Fob.m_sOwner;
		manager.m_Structures.TransferFob(s_Fob, other);
		SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(defences.m_aPieces[4].FindComponent(SCR_CampaignBuildingCompositionComponent));
		ACR_Structure record = manager.m_Structures.Find(defences.m_aPieces[4]);
		ACR_TestRegress.Check("fobdefences: captured defences change hands with the FOB", defences.m_sOwner == other && comp && comp.ACR_Faction() == other && record && record.m_sFaction == other, "");
		manager.m_Structures.TransferFob(s_Fob, owner);
		s_Fob.m_sOwner = owner;

		SCR_EntityHelper.DeleteEntityAndChildren(defences.m_aPieces[2]);
		ACR_FobSlots.Advance(s_Fob, 0.1);
		ACR_TestRegress.Check("fobdefences: losing one position keeps the tier", defences.m_eState == ACR_EFobSlotState.BUILT && defences.m_iTier == 2 && ACR_FobDefences.Standing(defences) == 6, ACR_FobDefences.Standing(defences).ToString());

		foreach (IEntity piece : defences.m_aPieces)
			if (piece) SCR_EntityHelper.DeleteEntityAndChildren(piece);
		ACR_FobSlots.Advance(s_Fob, 0.1);
		ACR_TestRegress.Check("fobdefences: losing every position resets the defences", defences.m_eState == ACR_EFobSlotState.EMPTY && defences.m_iTier == 0 && ACR_FobDefences.Standing(defences) == 0 && ACR_FobSlots.FeedState(s_Fob, ACR_FobSlots.SLOT_DEFENCES) == 0, "");
	}

	// The AI builds Defences I once the base buildings stand, before the tier-1 buildings.
	static void AiOrder(ACR_Manager manager)
	{
		ACR_AICommander ai = new ACR_AICommander(manager, s_Fob.m_sOwner);
		foreach (ACR_FobSlot slot : s_Fob.m_aFobSlots)
		{
			if (slot.m_iSlotIndex == ACR_FobSlots.SLOT_HELIPAD) continue;
			slot.m_eState = ACR_EFobSlotState.BUILT;
			slot.m_sOwner = s_Fob.m_sOwner;
		}
		int tier;
		int first = ai.BestUpgradeSlotFor(s_Fob, tier);
		bool defencesFirst = first == ACR_FobSlots.SLOT_DEFENCES && tier == 0;
		s_Fob.m_FobDefences.m_eState = ACR_EFobSlotState.BUILT;
		s_Fob.m_FobDefences.m_sOwner = s_Fob.m_sOwner;
		int second = ai.BestUpgradeSlotFor(s_Fob, tier);
		bool barracksNext = second == ACR_FobSlots.SLOT_BARRACKS && tier == 1;
		s_Fob.m_FobDefences.m_eState = ACR_EFobSlotState.EMPTY;
		foreach (ACR_FobSlot reset : s_Fob.m_aFobSlots) reset.m_eState = ACR_EFobSlotState.EMPTY;
		ACR_TestRegress.Check("fobdefences: AI builds Defences I after the base buildings, then large barracks", defencesFirst && barracksNext, string.Format("first=%1 second=%2", first, second));
	}
}
