// Our structures' layout on every machine (27 Oct 2026). The ground tilt (ACR_GroundTilt.Place) moves a
// composition on the server after it spawns, and nothing sends those moves to clients: a client builds the
// composition from its prefab, level, so in TvT a remote player saw the pieces where the server's weren't
// (vanilla's Game Master needs its own RPC to move a placed object for clients,
// SCR_EditableEntityComponent.SetTransformBroadcast). So the server records the final layout on the
// composition's replicated component (SCR_CampaignBuildingCompositionComponent.ACR_SetLayout), and every
// machine, the server included, lays the pieces out from that record (Apply): the server when it is set,
// a client when the composition streams in (joining later too) or the record changes.
//
// The record holds every piece: pieces the server didn't move still don't rebuild alike everywhere (a
// barricade's logs settle on the ground as the composition is built: a level copy given only the root
// stayed 13 cm off). Ints, quantized: the root's position in mm and its angles in 0.01 degrees (5 ints),
// the piece counts (1), then 4 a piece (its index, its position in the root's frame in 2 mm steps up to
// 65 m, its angles in 0.01 degrees): 668 bytes for a 40-piece sandbag position, a third of plain floats.
class ACR_StructureLayout
{
 static const float ROOT_STEP = 0.001;  // m
 static const float PART_STEP = 0.002;  // m: 16 bits reach 65 m either way
 static const float ANGLE_STEP = 0.01;  // degrees
 static const int HEADER = 6;

 static int s_iApplied;   // pieces laid out from a record on this machine (tests)
 static int s_iMismatch;  // records that didn't fit the composition they arrived on (tests)

 // The server, once 'root' is placed (tilted, a lane opened): records its layout for every machine.
 static bool Record(IEntity root)
 {
  SCR_CampaignBuildingCompositionComponent comp = Comp(root);
  if (!comp || !Replication.IsServer()) return false;
  array<int> layout = {};
  Write(root, layout);
  comp.ACR_SetLayout(layout);
  return true;
 }

 static void Write(IEntity root, notnull array<int> layout)
 {
  vector rootT[4];
  root.GetWorldTransform(rootT);
  vector angles = Math3D.MatrixToAngles(rootT);
  vector origin = rootT[3];
  layout.Insert((int)Math.Round(origin[0] / ROOT_STEP));
  layout.Insert((int)Math.Round(origin[1] / ROOT_STEP));
  layout.Insert((int)Math.Round(origin[2] / ROOT_STEP));
  layout.Insert(Pack(Q(angles[0], ANGLE_STEP), Q(angles[1], ANGLE_STEP)));
  layout.Insert(Pack(Q(angles[2], ANGLE_STEP), 0));
  array<IEntity> parts = {};
  Parts(root, parts);
  array<int> moved = {};
  foreach (int i, IEntity part : parts)
  {
   vector local[4];
   Local(rootT, part, local);
   vector a = Math3D.MatrixToAngles(local);
   vector at = local[3];
   moved.Insert(Pack(i, Q(at[1], PART_STEP)));
   moved.Insert(Pack(Q(at[0], PART_STEP), Q(at[2], PART_STEP)));
   moved.Insert(Pack(Q(a[0], ANGLE_STEP), Q(a[1], ANGLE_STEP)));
   moved.Insert(Pack(Q(a[2], ANGLE_STEP), 0));
  }
  layout.Insert(Pack(parts.Count(), moved.Count() / 4));
  layout.InsertAll(moved);
 }

 // Lays 'root' and its pieces out from a record; false (nothing moved) when it doesn't fit. Pieces not in
 // the record keep their place in the root's frame as this machine built them (the prefab's).
 static bool Apply(IEntity root, notnull array<int> layout)
 {
  if (!root || layout.Count() < HEADER) return false;
  array<IEntity> parts = {};
  Parts(root, parts);
  int count = High(layout[5]);
  int moved = Low(layout[5]);
  if (parts.Count() != count || layout.Count() != HEADER + moved * 4)
  {
   s_iMismatch++;
   PrintFormat("[ACR] STRUCTURE LAYOUT %1: the record has %2 pieces, the composition %3; left as built", FilePath.StripPath(root.GetPrefabData().GetPrefabName()), count, parts.Count());
   return false;
  }
  // Every piece's place in the root's frame first, as it stands.
  vector oldRoot[4];
  root.GetWorldTransform(oldRoot);
  array<vector> locals = {};
  foreach (IEntity part : parts)
  {
   vector local[4];
   Local(oldRoot, part, local);
   for (int k = 0; k < 4; k++) locals.Insert(local[k]);
  }
  for (int m = 0; m < moved; m++)
  {
   int at = HEADER + m * 4;
   int index = High(layout[at]);
   if (index < 0 || index >= count) continue;
   vector angles = Vector(High(layout[at + 2]) * ANGLE_STEP, Low(layout[at + 2]) * ANGLE_STEP, High(layout[at + 3]) * ANGLE_STEP);
   vector rel[4];
   Math3D.AnglesToMatrix(angles, rel);
   rel[3] = Vector(High(layout[at + 1]) * PART_STEP, Low(layout[at]) * PART_STEP, Low(layout[at + 1]) * PART_STEP);
   for (int r = 0; r < 4; r++) locals[index * 4 + r] = rel[r];
  }
  vector rootT[4];
  Math3D.AnglesToMatrix(Vector(High(layout[3]) * ANGLE_STEP, Low(layout[3]) * ANGLE_STEP, High(layout[4]) * ANGLE_STEP), rootT);
  rootT[3] = Vector(layout[0] * ROOT_STEP, layout[1] * ROOT_STEP, layout[2] * ROOT_STEP);
  root.SetWorldTransform(rootT);
  // Parents before their children (hierarchy order), so a moved parent can't drag a set child.
  foreach (int i, IEntity piece : parts)
  {
   vector mine[4];
   for (int q = 0; q < 4; q++) mine[q] = locals[i * 4 + q];
   vector world[4];
   Math3D.MatrixMultiply4(rootT, mine, world);
   piece.SetWorldTransform(world);
   piece.Update();
  }
  root.Update();
  s_iApplied += count;
  return true;
 }

 // Every piece under 'root', depth first in hierarchy order: the same on every machine.
 static void Parts(IEntity root, notnull array<IEntity> parts)
 {
  IEntity child = root.GetChildren();
  while (child)
  {
   parts.Insert(child);
   Parts(child, parts);
   child = child.GetSibling();
  }
 }

 // The record's size on the wire: 4 bytes an int, plus the array's length.
 static int Bytes(int ints) { return ints * 4 + 4; }

 static void Local(vector rootT[4], IEntity part, out vector local[4])
 {
  vector world[4];
  part.GetWorldTransform(world);
  Math3D.MatrixInvMultiply4(rootT, world, local);
 }

 static SCR_CampaignBuildingCompositionComponent Comp(IEntity root)
 {
  if (!root) return null;
  return SCR_CampaignBuildingCompositionComponent.Cast(root.FindComponent(SCR_CampaignBuildingCompositionComponent));
 }

 protected static int Q(float value, float step)
 {
  return Math.ClampInt((int)Math.Round(value / step), -32768, 32767);
 }

 protected static int Pack(int high, int low)
 {
  return (high << 16) | (low & 0xFFFF);
 }

 protected static int High(int packed)
 {
  return packed >> 16;
 }

 protected static int Low(int packed)
 {
  int low = packed & 0xFFFF;
  if (low >= 32768) low -= 65536;
  return low;
 }
}

modded class SCR_CampaignBuildingCompositionComponent
{
 // The structure's layout as the server placed it (ACR_StructureLayout); empty: as its prefab.
 [RplProp(onRplName: "ACR_OnLayout")]
 protected ref array<int> m_aACRLayout = {};
 void ACR_SetLayout(notnull array<int> layout)
 {
  m_aACRLayout.Copy(layout);
  ACR_StructureLayout.Apply(GetOwner(), m_aACRLayout);
  Replication.BumpMe();
 }

 int ACR_LayoutSize() { return m_aACRLayout.Count(); }

 protected void ACR_OnLayout()
 {
  if (!m_aACRLayout.IsEmpty()) ACR_StructureLayout.Apply(GetOwner(), m_aACRLayout);
 }

 override void OnPostInit(IEntity owner)
 {
  super.OnPostInit(owner);
  // A client that streams the structure in may get the record before the pieces settle: once more.
  if (GetGame().InPlayMode() && !Replication.IsServer()) GetGame().GetCallqueue().CallLater(ACR_OnLayout, 1000);
 }

 override void OnDelete(IEntity owner)
 {
  GetGame().GetCallqueue().Remove(ACR_OnLayout);
  super.OnDelete(owner);
 }
}
