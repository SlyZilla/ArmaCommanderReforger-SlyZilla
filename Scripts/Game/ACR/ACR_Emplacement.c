// Gun emplacements built from parts: a gun on its low mount of bags, framed by loose pieces, in the shape the
// place asks for, instead of the game's one ready-made nest. The kernel is what the game's own small gun position
// is made of (Sandbag_MG_US_01_M2HB): the tripod 0.63 m up on a short low piece and a round low piece, the two
// tapered high ends meeting in front of it. Around that, whatever joins it to the line:
//   NOTCH    in the line's own run, its sides turned back. One long piece of the line makes way.
//   BASTION  pushed out ahead of the line on pieces angled back to it: it sees along the line. Two or three long
//            pieces make way; two give a shallow bastion on short pieces, three a deep one on long ones.
//   CORNER   the same at a bend, turned to the bisector.
//   RING     on its own at a line's end: three high round pieces round the gun.
// Which shape and whether at all come from the ground at the spot, weighted and seeded (ACR_BaseLines.PartsFor).
// -acrEmplacements 0: only the game's ready-made nests.
class ACR_EmplacementPiece : Managed
{
 ResourceName m_sPrefab;
 vector m_vLocal;     // x across, z towards the enemy
 float m_fTurn;       // from the emplacement's facing
 bool m_bOnGround;    // lies on the slope, as a line's piece does
}

class ACR_Emplacement : Managed
{
 static const int NOTCH = 0;
 static const int BASTION = 1;
 static const int CORNER = 2;
 static const int RING = 3;
 static const int SHAPES = 4;

 static const ResourceName GUN_US = "{73530808E4B455BA}Prefabs/Weapons/Tripods/Tripod_M3_M2HB.et";
 static const ResourceName GUN_EAST = "{29F0CC704A582154}Prefabs/Weapons/Tripods/6T7/Tripod_6T7_NSV.et";
 static const ResourceName LOW_SHORT_PLASTIC = "{3F5EE4D69DBC478C}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_short_plastic.et";
 static const ResourceName LOW_ROUND_PLASTIC = "{3C9FB6869EB635BB}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_round_plastic.et";
 static const ResourceName LOW_SHORT_BURLAP = "{D8C2B968F3C757C0}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_short_burlap.et";
 static const ResourceName LOW_ROUND_BURLAP = "{33748AC23F947B6E}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_round_burlap.et";
 static const float GUN_UP = 0.634;    // the tripod over the ground, on its mount (the game's small position)
 static const float GUN_X = -0.106;
 static const float GUN_Z = -0.592;
 static const float TIP_X = 2.2;       // m from the middle to the outer end of a tapered front
 static const float TIP_Z = -0.16;     // ... and where those fronts stand from the kernel's origin
 static const float SHORT_REACH = 2.2; // m a short piece spans at most; over that a long one
 static const float MIN_LATERAL = 0.6; // m a side must have left to take a piece at all

 int m_iShape;
 vector m_vPos;
 float m_fYaw;
 ResourceName m_sGun;
 ref array<ref ACR_EmplacementPiece> m_aPieces = {};

 static bool On() { return ACR_LogisticsRules.Choice("acrEmplacements", "1") != "0"; }

 static string ShapeWord(int shape)
 {
  switch (shape)
  {
   case NOTCH: return "notch";
   case BASTION: return "bastion";
   case CORNER: return "corner";
   case RING: return "ring";
  }
  return "?";
 }

 static bool East(string faction)
 {
  return faction == "USSR" || faction == "FIA" || faction.Contains("AFRF") || faction.Contains("RU") || faction.Contains("SLA");
 }

 protected void Add(ResourceName prefab, float x, float z, float turn, bool onGround = false)
 {
  ACR_EmplacementPiece piece = new ACR_EmplacementPiece();
  piece.m_sPrefab = prefab;
  piece.m_vLocal = Vector(x, 0, z);
  piece.m_fTurn = turn;
  piece.m_bOnGround = onGround;
  m_aPieces.Insert(piece);
 }

 // The mount and the two tapered fronts, as the game's small position has them.
 protected void Kernel(string faction, ACR_BaseDefenceKit kit)
 {
  m_sGun = GUN_US;
  ResourceName lowShort = LOW_SHORT_PLASTIC;
  ResourceName lowRound = LOW_ROUND_PLASTIC;
  if (East(faction))
  {
   m_sGun = GUN_EAST;
   lowShort = LOW_SHORT_BURLAP;
   lowRound = LOW_ROUND_BURLAP;
  }
  Add(lowShort, -0.068, -0.519, -88);
  Add(lowRound, -0.131, -0.454, 0);
  Add(kit.m_sEnd, 1.144, -0.208, 180);
  Add(kit.m_sEnd, -1.188, -0.118, 0);
 }

 // A piece from a front's tip to a point in the emplacement's own frame: long or short by the distance, turned
 // along it, its firing side towards the enemy. side: 1 right, -1 left.
 protected void Connector(ACR_BaseDefenceKit kit, vector tip, vector end, int side)
 {
  vector along = end - tip;
  along[1] = 0;
  float length = along.Length();
  if (length < 0.5) return;
  along = along * (1.0 / length);
  ResourceName prefab = kit.m_sLong;
  if (length <= SHORT_REACH) prefab = kit.m_sShort;
  vector facing = ACR_BaseLiner.FacingOf(along, side);
  vector middle = (tip + end) * 0.5;
  Add(prefab, middle[0], middle[2], Math.Atan2(facing[0], facing[2]) * Math.RAD2DEG, true);
 }

 // Between a line's two open ends, 'facing' the enemy. Null if the ends are too far apart for its pieces.
 static ACR_Emplacement Between(string faction, vector left, vector right, vector facing)
 {
  vector across = right - left;
  across[1] = 0;
  float half = across.Length() * 0.5;
  if (half > TIP_X + ACR_BaseLines.LONG_STEP) return null;
  float lateral = half - TIP_X;
  vector middle = (left + right) * 0.5;
  facing[1] = 0;
  facing.Normalize();
  ACR_Emplacement e = new ACR_Emplacement();
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(faction);
  e.m_fYaw = ACR_BaseDefences.Bearing(facing);
  e.Kernel(faction, kit);
  // How far out: what makes the side pieces their own length.
  float forward = 0;
  if (lateral >= MIN_LATERAL)
  {
   float reach = ACR_BaseLines.LONG_STEP;
   if (lateral <= SHORT_REACH - 0.6) reach = ACR_BaseLines.SHORT_STEP;
   forward = Math.Sqrt(Math.Max(reach * reach - lateral * lateral, 0));
  }
  e.m_vPos = middle + facing * (forward - TIP_Z);
  e.m_vPos[1] = ACR_BaseLiner.Ground(e.m_vPos);
  if (lateral < MIN_LATERAL)
  {
   e.m_iShape = NOTCH;
   e.Add(kit.m_sEnd, 1.534, -0.739, 93.6);
   e.Add(kit.m_sEnd, -1.711, -0.535, 93.6);
   return e;
  }
  e.m_iShape = BASTION;
  if (ACR_BaseDefences.DotXZ(ACR_BaseDefences.RightOf(facing), across * (1.0 / (half * 2))) < 0.97) e.m_iShape = CORNER;
  e.Connector(kit, Vector(TIP_X, 0, TIP_Z), e.ToLocal(right), 1);
  e.Connector(kit, Vector(-TIP_X, 0, TIP_Z), e.ToLocal(left), -1);
  return e;
 }

 // On its own: at a line's end, or a strongpoint's. Round high pieces on three sides.
 static ACR_Emplacement Ring(string faction, vector at, vector facing)
 {
  facing[1] = 0;
  facing.Normalize();
  ACR_Emplacement e = new ACR_Emplacement();
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(faction);
  e.m_iShape = RING;
  e.m_fYaw = ACR_BaseDefences.Bearing(facing);
  e.m_vPos = at;
  e.m_vPos[1] = ACR_BaseLiner.Ground(at);
  e.Kernel(faction, kit);
  e.Add(kit.m_sRound, 2.35, -1.45, 70);
  e.Add(kit.m_sRound, -2.35, -1.45, -70);
  return e;
 }

 // A world point in the emplacement's frame.
 vector ToLocal(vector world)
 {
  float rad = m_fYaw * Math.DEG2RAD;
  vector forward = Vector(Math.Sin(rad), 0, Math.Cos(rad));
  vector right = Vector(forward[2], 0, -forward[0]);
  vector at = world - m_vPos;
  at[1] = 0;
  return Vector(vector.Dot(at, right), 0, vector.Dot(at, forward));
 }

 vector GunWorld()
 {
  vector at = ACR_Fortify.ToWorld(m_vPos, m_fYaw, Vector(GUN_X, 0, GUN_Z));
  at[1] = at[1] + GUN_UP;
  return at;
 }

 // Into the world: its bags into 'walls', the gun with them. How many pieces.
 int Build(notnull array<IEntity> walls)
 {
  int made;
  ACR_FobBuild.FellTrees(m_vPos, 4);
  foreach (ACR_EmplacementPiece piece : m_aPieces)
  {
   vector at = ACR_Fortify.ToWorld(m_vPos, m_fYaw, piece.m_vLocal);
   IEntity entity;
   if (piece.m_bOnGround) entity = ACR_BaseDefences.SpawnOnGround(piece.m_sPrefab, at, m_fYaw + piece.m_fTurn);
   else entity = ACR_BaseDefences.SpawnProp(piece.m_sPrefab, at, m_fYaw + piece.m_fTurn);
   if (!entity) continue;
   walls.Insert(entity);
   ACR_Navmesh.RebuildAfterSpawn(entity);
   made++;
  }
  Resource resource = Resource.Load(m_sGun);
  if (!resource.IsValid()) return made;
  EntitySpawnParams spawnParams = new EntitySpawnParams();
  spawnParams.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(m_fYaw, 0, 0), spawnParams.Transform);
  spawnParams.Transform[3] = GunWorld();
  IEntity gun = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
  if (gun)
  {
   walls.Insert(gun);
   made++;
  }
  return made;
 }

 // The same place for another side.
 ACR_Emplacement ForOwner(string owner)
 {
  ACR_Emplacement e = new ACR_Emplacement();
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(owner);
  ACR_BaseDefenceKit was = ACR_BaseDefences.Kit("US");
  e.m_iShape = m_iShape;
  e.m_vPos = m_vPos;
  e.m_fYaw = m_fYaw;
  e.m_sGun = GUN_US;
  if (East(owner)) e.m_sGun = GUN_EAST;
  foreach (ACR_EmplacementPiece piece : m_aPieces)
  {
   ResourceName prefab = piece.m_sPrefab;
   string name = prefab;
   if (name.Contains("long")) prefab = kit.m_sLong;
   else if (name.Contains("end")) prefab = kit.m_sEnd;
   else if (name.Contains("round_high")) prefab = kit.m_sRound;
   else if (name.Contains("short_high")) prefab = kit.m_sShort;
   else if (name.Contains("E_Sandbag_01_round")) prefab = LOW_ROUND_PLASTIC;
   else if (name.Contains("E_Sandbag_01_short")) prefab = LOW_SHORT_PLASTIC;
   if (East(owner))
   {
    if (prefab == LOW_ROUND_PLASTIC) prefab = LOW_ROUND_BURLAP;
    if (prefab == LOW_SHORT_PLASTIC) prefab = LOW_SHORT_BURLAP;
   }
   e.Add(prefab, piece.m_vLocal[0], piece.m_vLocal[2], piece.m_fTurn, piece.m_bOnGround);
  }
  return e;
 }
}
