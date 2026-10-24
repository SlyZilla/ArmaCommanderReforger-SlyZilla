"""Fortify command posts (Jacob, 24 Oct): one layout per building model, designed in the
building's own frame (x right, z forward, metres from the model origin) from a few primitives:
the CP (overhead net, table with radio, map board, chairs, crates, lantern), door blast walls,
corner Ls and sandbag lines. Faces: +x -x +z -z. Tag 'always' = built whatever the threat; a
face tag = built only when that side of the building looks towards the enemy.

  python tools/fortify_posts.py            regenerate Scripts/Game/ACR/ACR_FortifyPosts.c
  python tools/fortify_posts.py <dir>      also write <dir>/layouts.json (for drawing sketches)

Building geometry comes from srv:fortifymodels / srv:fortifymodelskolguyev (FORTMODEL lines).
Edit the layouts here, never the generated .c file."""
import json, math, os, sys

FACES = {'+x': (1, 0), '-x': (-1, 0), '+z': (0, 1), '-z': (0, -1)}
LONG, END = 3.43, 2.13


def yaw_of(n):
    return math.degrees(math.atan2(n[0], n[1]))


def right(n):
    return (n[1], -n[0])


def add(v, w, k=1.0):
    return (v[0] + w[0] * k, v[1] + w[1] * k)


def piece(kind, p, yaw, tag):
    return {'k': kind, 'x': round(p[0], 2), 'z': round(p[1], 2), 'yaw': round(yaw, 1), 'tag': tag}


def baffle(d, face, side=1):
    """Blast wall in front of a door, offset to one side so the door stays usable."""
    n = FACES[face]; r = right(n)
    c = add(add(d, n, 2.8), r, 0.9 * side)
    out = [piece('long', c, yaw_of(n), 'always')]
    # end piece bent back at the far end
    a = math.radians(40)
    axis = (r[0] * side * math.cos(a) - n[0] * math.sin(a), r[1] * side * math.cos(a) - n[1] * math.sin(a))
    e = add(add(c, r, side * (LONG / 2)), axis, END / 2)
    g = (-axis[1], axis[0])
    if g[0] * n[0] + g[1] * n[1] < 0: g = (-g[0], -g[1])
    out.append(piece('end', e, yaw_of(g), 'always'))
    return out


def corner(c, face, side_face):
    """L off a building corner: long piece facing `face`, end piece covering `side_face`."""
    n = FACES[face]; s = FACES[side_face]
    at = add(add(c, n, 1.4), s, 0.4)
    flank = add(add(at, s, LONG / 2 + 0.3), n, -END / 2)
    return [piece('long', at, yaw_of(n), face), piece('end', flank, yaw_of(s), face),
            piece('lantern', add(add(at, n, -1.0), s, -0.8), 0, face)]


def line(a, b, face, gap=None):
    """Sandbags from a to b facing `face`; gap = (centre distance along, width) left open."""
    n = FACES[face]
    dx, dz = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dz); u = (dx / length, dz / length)
    out = []
    spans = [(0.0, length)]
    if gap: spans = [(0.0, gap[0] - gap[1] / 2), (gap[0] + gap[1] / 2, length)]
    for s0, s1 in spans:
        span = s1 - s0
        if span < LONG * 0.8: continue
        count = max(1, round(span / LONG))
        step = span / count
        for i in range(count):
            out.append(piece('long', add(a, u, s0 + step * (i + 0.5)), yaw_of(n), face))
    return out


def cp(p, face):
    """The command post itself: overhead camo net; a table (long side across the facing) with a
    paper map in the middle, the radio at one end and a lantern at the other; two chairs pulled
    up behind it; a briefing flipchart in front; crates to one side. Items marked on the table
    stand on its top."""
    n = FACES[face]; r = right(n)
    y = yaw_of(n)
    return [piece('nettop', p, y, 'always'),
            piece('table', p, y, 'always'),
            piece('map', p, y + 180, 'always'),
            piece('radio', add(p, r, 0.42), y + 180, 'always'),
            piece('lamp', add(p, r, -0.5), 0, 'always'),
            piece('chair', add(add(p, n, -0.85), r, -0.35), y, 'always'),
            piece('chair', add(add(p, n, -0.85), r, 0.4), y + 12, 'always'),
            piece('chart', add(p, n, 1.4), y + 180, 'always'),
            piece('crates', add(add(p, r, 2.0), n, -0.4), y, 'always')]


L = {}
L['House_Village_E_1L03t'] = (
    # Jacob's sketch: bags along the terrace railing and across the courtyard's open side.
    line((-3.2, 11.9), (4.2, 11.9), '+z')
    + line((-3.0, 18.8), (13.0, 18.8), '+z', gap=(8.0, 3.2))
    + cp((7.5, 15.0), '+z')
    + corner((13.2, -5.3), '-z', '+x') + corner((-12.7, -5.3), '-z', '-x')
    + corner((-12.7, 20.3), '-x', '+z') + corner((13.2, 9.3), '+x', '-z'))
L['House_Village_E_1I02_Furniture'] = (
    baffle((-4.9, -1.0), '-x', -1) + cp((-7.2, 4.0), '-x')
    + corner((-4.9, 8.7), '+z', '-x') + corner((5.8, 8.7), '+z', '+x')
    + corner((-4.9, -10.9), '-z', '-x') + corner((5.8, -10.9), '-z', '+x'))
L['Barracks_USSR_01_military'] = (
    baffle((0, 13.9), '+z', 1) + baffle((0, -16.3), '-z', -1) + cp((-4.5, 17.0), '+z')
    + line((-9.4, 6.5), (-9.4, -6.9), '-x', gap=(6.7, 3.4)) + line((9.4, -6.9), (9.4, 6.5), '+x', gap=(6.7, 3.4))
    + corner((7.9, 13.9), '+x', '+z') + corner((-7.9, -16.3), '-x', '-z'))
L['Warehouse_01'] = (
    # Under the roof: the CP inside, the open long sides closed with bags.
    cp((-15.5, 0.0), '+x')
    + line((-18.5, 9.0), (14.5, 9.0), '+z', gap=(16.5, 6.0)) + line((14.5, -9.3), (-18.5, -9.3), '-z', gap=(16.5, 6.0))
    + baffle((16.8, 0.0), '+x', 1)
    + corner((-20.5, 8.0), '-x', '+z') + corner((-20.5, -8.3), '-x', '-z'))
L['House_Wooden_E_1I01'] = (
    baffle((1.5, 3.6), '+z', 1) + cp((10.5, 1.0), '+x')
    + corner((-5.9, 3.6), '-x', '+z') + corner((-5.9, -3.6), '-x', '-z')
    + corner((7.6, -3.6), '-z', '+x'))
L['Workshop_02_grey'] = (
    baffle((0.0, 6.3), '+z', -1) + baffle((-3.6, -7.7), '-x', 1) + cp((6.6, -7.6), '+x')
    + corner((-4.5, 6.2), '-x', '+z') + corner((4.5, 6.2), '+x', '+z') + corner((-3.6, -9.4), '-z', '-x'))
L['Barn_E_01'] = (
    # The barn floor is the CP; the big doors get blast walls.
    cp((3.5, -2.0), '-z') + baffle((0.3, 5.4), '+z', 1) + baffle((0.3, -5.5), '-z', -1)
    + corner((-7.7, 5.4), '-x', '+z') + corner((-7.7, -5.5), '-x', '-z'))
L['Barn_E_02'] = (
    cp((1.2, 1.0), '-z') + baffle((-0.6, -4.5), '-z', 1) + baffle((-1.4, 4.3), '+z', -1)
    + corner((-4.9, 4.3), '-x', '+z') + corner((4.7, 4.3), '+x', '+z')
    + corner((-4.9, -4.5), '-x', '-z') + corner((4.7, -4.5), '+x', '-z'))
L['FarmHouse_E_1L01'] = (
    # The inner corner (north-west) becomes a walled yard with the CP in it.
    cp((-6.5, 3.0), '+z')
    + line((-11.2, -1.0), (-11.2, 8.0), '-x') + line((-10.6, 9.0), (-6.8, 9.0), '+z')
    + baffle((-2.6, 7.0), '-x', 1)
    + corner((5.9, 11.6), '+x', '+z') + corner((5.9, -9.6), '+x', '-z') + corner((-10.1, -9.6), '-x', '-z'))
L['House_Village_E_1I08t'] = (
    # A terraced house: neighbours on both sides, so only front and back.
    baffle((6.5, 5.2), '+z', 1) + cp((0.8, 8.6), '+z')
    + line((-8.5, 7.6), (-2.5, 7.6), '+z')
    + baffle((3.8, -5.2), '-z', 1) + line((-8.5, -7.4), (-1.5, -7.4), '-z'))

HEADER = """// GENERATED by tools/fortify_posts.py from the command-post layouts there: do not edit by hand.
// Command posts per building model (ACR_Fortify POST kind): pieces in the building's own frame
// (x right, z forward, metres from the model origin; yaw in degrees from +z). Face "always" is
// built whatever the threat, "+x"/"-x"/"+z"/"-z" only when that side faces the enemy.

class ACR_FortifyPostPiece : Managed
{
	string m_sKind;     // long, end, table, map, radio, lamp (these three on the table), chair, chart, crates, lantern, nettop
	float m_fX;
	float m_fZ;
	float m_fYaw;
	string m_sFace;
}

class ACR_FortifyPosts
{
	protected static ref map<string, ref array<ref ACR_FortifyPostPiece>> s_mLayouts;

	// The layout for a building model name (prefab or mesh, no extension), or null.
	static array<ref ACR_FortifyPostPiece> Layout(string model)
	{
		if (!s_mLayouts) Build();
		return s_mLayouts.Get(model);
	}

	protected static void P(array<ref ACR_FortifyPostPiece> layout, string kind, float x, float z, float yaw, string face)
	{
		ACR_FortifyPostPiece piece = new ACR_FortifyPostPiece();
		piece.m_sKind = kind;
		piece.m_fX = x;
		piece.m_fZ = z;
		piece.m_fYaw = yaw;
		piece.m_sFace = face;
		layout.Insert(piece);
	}

	protected static void Build()
	{
		s_mLayouts = new map<string, ref array<ref ACR_FortifyPostPiece>>();
		array<ref ACR_FortifyPostPiece> layout;
"""


def enforce():
    lines = [HEADER]
    for model, pieces in L.items():
        lines.append(TAB2 + 'layout = {};' + NL + TAB2 + 's_mLayouts.Insert("%s", layout);' % model + NL)
        for p in pieces:
            lines.append(TAB2 + 'P(layout, "%s", %s, %s, %s, "%s");' % (p['k'], p['x'], p['z'], p['yaw'], p['tag']) + NL)
    lines.append(TAB + '}' + NL + '}' + NL)
    return ''.join(lines)


TAB, NL = chr(9), chr(10)
TAB2 = TAB + TAB
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
target = os.path.join(root, 'Scripts', 'Game', 'ACR', 'ACR_FortifyPosts.c')
open(target, 'w', encoding='utf-8', newline=NL).write(enforce())
print('wrote', target)
if len(sys.argv) > 1:
    json.dump(L, open(os.path.join(sys.argv[1], 'layouts.json'), 'w'), indent=1)
for k, v in L.items():
    walls = sum(1 for p in v if p['k'] in ('long', 'end', 'round'))
    print(f'{k:34} {len(v):3} pieces, {walls:2} sandbags, {sum(1 for p in v if p["tag"]=="always"):2} always')
