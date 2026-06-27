import bpy

src = bpy.context.active_object
assert src and src.type == 'MESH', "select the broken mesh first"
sm = src.data

# --- explode to face-soup: every face gets its own verts, so no shared/non-manifold edges ---
verts, faces, src_vert_of = [], [], []
for p in sm.polygons:
    f = []
    for li in range(p.loop_start, p.loop_start + p.loop_total):
        sv = sm.loops[li].vertex_index
        verts.append(sm.vertices[sv].co[:])
        src_vert_of.append(sv)          # new vert -> source vert, for weight transfer
        f.append(len(verts) - 1)
    faces.append(tuple(f))

nm = bpy.data.meshes.new(sm.name + "_soup")
nm.from_pydata(verts, [], faces)
nm.update()
assert len(nm.loops) == len(sm.loops), (len(nm.loops), len(sm.loops))
nloops = len(nm.loops)

# --- per-loop UVs (all layers) ---
for uv in sm.uv_layers:
    dst = nm.uv_layers.get(uv.name) or nm.uv_layers.new(name=uv.name)
    buf = [0.0] * (2 * nloops)
    uv.data.foreach_get("uv", buf)
    dst.data.foreach_set("uv", buf)
if sm.uv_layers.active:
    nm.uv_layers.active = nm.uv_layers.get(sm.uv_layers.active.name)

# --- per-face smooth flag + material index, and the material slots ---
tmp = [False] * len(sm.polygons); sm.polygons.foreach_get("use_smooth", tmp)
nm.polygons.foreach_set("use_smooth", tmp)
tmp = [0] * len(sm.polygons); sm.polygons.foreach_get("material_index", tmp)
nm.polygons.foreach_set("material_index", tmp)
for m in sm.materials:
    nm.materials.append(m)

# --- custom split normals (per-loop) -> restores the smooth shading the old soup lost ---
norms = None
try:
    norms = [tuple(cn.vector) for cn in sm.corner_normals]   # Blender 4.1+ read cache
except Exception:
    try:
        norms = [tuple(l.normal) for l in sm.loops]
    except Exception as e:
        print("custom normals not readable:", e)
if norms:
    try:
        nm.normals_split_custom_set(norms)
    except Exception as e:
        print("custom normals not applied:", e)
nm.update()

# --- new object, placed where the original is ---
no = bpy.data.objects.new(src.name + "_soup", nm)
src.users_collection[0].objects.link(no)
no.matrix_world = src.matrix_world.copy()

# --- vertex groups (same names/order -> indices match) + weights remapped to exploded verts ---
for g in src.vertex_groups:
    no.vertex_groups.new(name=g.name)
src_w = [[(g.group, g.weight) for g in v.groups] for v in sm.vertices]
for ni, sv in enumerate(src_vert_of):
    for gi, w in src_w[sv]:
        no.vertex_groups[gi].add((ni,), w, 'REPLACE')

# --- armature deform (the only modifier on these meshes) ---
added_arm = False
for md in src.modifiers:
    if md.type == 'ARMATURE':
        am = no.modifiers.new(name=md.name, type='ARMATURE')
        am.object = md.object
        am.use_vertex_groups = md.use_vertex_groups
        am.use_bone_envelopes = md.use_bone_envelopes
        added_arm = True
no.parent = src.parent
no.parent_type = 'OBJECT' if added_arm else src.parent_type
no.matrix_parent_inverse = src.matrix_parent_inverse.copy()

nm.update()
print(f"created '{no.name}': {len(verts)} verts, {len(faces)} faces, "
      f"{len(nm.uv_layers)} uv layer(s), {len(no.vertex_groups)} groups, "
      f"normals={'yes' if norms else 'no'}, armature={'yes' if added_arm else 'no'}")