"""Kill Wire Edges -- registered operator.

Deletes edges that belong to the mesh but border no face (BMEdge.is_wire),
optionally removing verts left loose afterwards.

Run in the Text Editor to (re-)register. Re-running is safe: the previous
registration (operator + menu entry) is torn down first via driver_namespace.

After registering, reach it via:
  - F3 search "Kill Wire Edges"
  - Edit Mode > Mesh > Clean Up > Kill Wire Edges
  - bpy.ops.mesh.kill_wire_edges()
"""
import bpy
import bmesh

NS = bpy.app.driver_namespace
_MENU = bpy.types.VIEW3D_MT_edit_mesh_clean
_KEY = "_kill_wire_edges_menu_fn"


class MESH_OT_kill_wire_edges(bpy.types.Operator):
    bl_idname = "mesh.kill_wire_edges"
    bl_label = "Kill Wire Edges"
    bl_description = "Delete edges that are connected to the mesh but used by no face"
    bl_options = {'REGISTER', 'UNDO'}

    remove_orphaned_verts: bpy.props.BoolProperty(
        name="Remove Orphaned Verts",
        description="Also delete verts left loose once the wire edges are gone",
        default=True,
    )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.type == 'MESH'

    def execute(self, context):
        obj = context.active_object
        me = obj.data
        edit = obj.mode == 'EDIT'
        bm = bmesh.from_edit_mesh(me) if edit else bmesh.new()
        if not edit:
            bm.from_mesh(me)

        # is_wire -> edge connected to the mesh but used by no face
        wire = [e for e in bm.edges if e.is_wire]
        ends = {v for e in wire for v in e.verts}
        for e in wire:
            bm.edges.remove(e)

        verts_killed = 0
        if self.remove_orphaned_verts:
            orphans = [v for v in ends if v.is_valid and not v.link_edges]
            verts_killed = len(orphans)
            for v in orphans:
                bm.verts.remove(v)

        if edit:
            bmesh.update_edit_mesh(me)
        else:
            bm.to_mesh(me)
            bm.free()

        self.report({'INFO'},
                    f"Killed {len(wire)} wire edge(s), {verts_killed} orphaned vert(s)")
        return {'FINISHED'}


def _menu_draw(self, context):
    self.layout.operator(MESH_OT_kill_wire_edges.bl_idname)


def register():
    bpy.utils.register_class(MESH_OT_kill_wire_edges)
    _MENU.append(_menu_draw)
    NS[_KEY] = _menu_draw  # remember the exact func so a re-run can remove it


def unregister():
    fn = NS.pop(_KEY, None)
    if fn is not None:
        try:
            _MENU.remove(fn)
        except Exception:
            pass
    cls = getattr(bpy.types, "MESH_OT_kill_wire_edges", None)
    if cls is not None:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    unregister()  # tear down a previous run if present
    register()
