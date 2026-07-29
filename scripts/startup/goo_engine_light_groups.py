# SPDX-License-Identifier: GPL-2.0-or-later

"""
Light group key management for Goo Engine NPR nodes, ported to Blender 5.2 (EEVEE-Next).

Provides the user-facing UI (Light Data properties + Shader Info node sidebar) to create and
assign named light groups. Named groups are synced into the per-data-block ``light_group_bits``
bitfields (on Lights and Shader Info nodes) that the shader actually reads.

Unlike original Goo, materials do not carry light groups in this port: grouping lives on the
Shader Info node (via ``use_own_light_groups``) and on lights, which is what the ported EEVEE-Next
shader path consumes.
"""

import bpy
from bpy.types import (
    Panel,
    Light,
    PropertyGroup,
    UIList,
    Operator,
    ShaderNodeShaderInfo,
    ShaderNodeTree,
)
from bpy.props import (
    StringProperty,
    CollectionProperty,
    IntProperty,
    PointerProperty,
    EnumProperty,
    BoolProperty,
)
from bpy.utils import register_classes_factory
from bpy.app.handlers import persistent
from itertools import chain
from ctypes import c_int32

SIZEOF_INT = 32
MAX_LIGHT_GROUP_BIT = 127


def set_bit(vec, bit):
    index = bit // SIZEOF_INT
    mask = 1 << ((bit + 1) % SIZEOF_INT)
    if index > 3:
        return
    vec[index] = int(c_int32(vec[index] | mask).value)


def map_bits(data, mapping):
    data.light_group_bits = (0, 0, 0, 0)
    # Only Shader Info nodes carry a separate per-group shadow mask.
    has_shadow = isinstance(data, ShaderNodeShaderInfo)
    if has_shadow:
        data.light_group_shadow_bits = (0, 0, 0, 0)

    for grp in data.light_groups.groups:
        index = mapping.get(grp.name)
        if index is not None:
            set_bit(data.light_group_bits, index)
            if has_shadow and not grp.ignore_shadow:
                set_bit(data.light_group_shadow_bits, index)

    if data.light_groups.use_default:
        set_bit(data.light_group_bits, MAX_LIGHT_GROUP_BIT)
        if has_shadow and not data.light_groups.ignore_default_shadow:
            set_bit(data.light_group_shadow_bits, MAX_LIGHT_GROUP_BIT)


def iter_light_group_owners():
    # Shader Info nodes (in node groups and material trees) that opt into their own groups.
    for node_tree in chain(bpy.data.node_groups, (m.node_tree for m in bpy.data.materials)):
        if not node_tree:
            continue
        for node in node_tree.nodes:
            if isinstance(node, ShaderNodeShaderInfo) and node.use_own_light_groups:
                yield node

    for light in bpy.data.lights:
        yield light


def sync_light_groups():
    # Group names are authored on lights; assign each a stable bit index.
    light_names = set()
    for light in bpy.data.lights:
        for grp in light.light_groups.groups:
            light_names.add(grp.name)

    if len(light_names) >= MAX_LIGHT_GROUP_BIT:
        print("WARNING: Max number of light groups (127) reached. "
              "Some Light Groups will not be included.")

    # The default group is reserved at bit 127.
    bit_mapping = {name: index for index, name in enumerate(light_names)}

    for data in iter_light_group_owners():
        map_bits(data, bit_mapping)


def update_handler(_s, _c):
    sync_light_groups()


@persistent
def sync_handler(*_):
    sync_light_groups()


@persistent
def sync_dg_handler(scn, dg):
    if dg.mode == 'RENDER':
        return

    for update in dg.updates:
        uid = update.id
        if not uid:
            continue
        if isinstance(uid, (bpy.types.Material, bpy.types.Light)):
            sync_light_groups()
            return


def rename_group(data, src, tgt):
    if getattr(data, 'library', False):
        return
    for grp in data.light_groups.groups:
        if grp.name == src:
            grp.name = tgt


def get_name(self):
    return self.name


# Update all matching names across the file.
def set_name(self, value):
    orig_name = self.name
    for data in iter_light_group_owners():
        rename_group(data, orig_name, value)


class LightGroup(PropertyGroup):
    name: StringProperty()
    viz_name: StringProperty(name="Name", get=get_name, set=set_name)
    ignore_shadow: BoolProperty(
        name="Ignore Shadows",
        description="Ignore shadows cast from this light group",
        default=False,
        options=set(),
        update=update_handler,
    )


class LightGroups(PropertyGroup):
    groups: CollectionProperty(type=LightGroup)
    group_index: IntProperty(name="Active Light Group", update=update_handler)
    use_default: BoolProperty(
        name="Use default Light Group",
        default=True,
        description="Use builtin default light group",
        update=update_handler,
    )
    ignore_default_shadow: BoolProperty(
        name="Ignore default Light Group shadows",
        default=False,
        description="Ignore default light group shadows",
        update=update_handler,
    )


def get_name_set():
    names = set()
    for data in iter_light_group_owners():
        for grp in data.light_groups.groups:
            names.add(grp.name)
    return names


def unique_group_name():
    names = get_name_set()
    i = 1
    name = "LightGroup"
    while name in names:
        name = f"LightGroup.{i:03}"
        i += 1
    return name


def get_groups(obj):
    if obj and obj.type == 'LIGHT':
        return obj.data.light_groups
    raise ValueError("Light groups are only available on lights or Shader Info nodes")


def get_groups_ctx(ctx):
    node = getattr(ctx, 'active_node', None)
    if node is not None and isinstance(node, ShaderNodeShaderInfo):
        return node.light_groups
    return get_groups(ctx.object)


class MAT_UL_LightGroupList(UIList):
    def draw_item(self, context, layout, data, item, icon,
                  active_data, active_property, index=0, flt_flag=0):
        row = layout.row(align=True)
        row.prop(item, "viz_name", emboss=False, text="")
        # Per-group shadow toggle only makes sense on Shader Info nodes.
        if isinstance(data.id_data, ShaderNodeTree):
            row.prop(item, "ignore_shadow", text="",
                     icon="REC" if item.ignore_shadow else "OVERLAY", emboss=False)

    def filter_items(self, context, data, property):
        keys = getattr(data, property)
        flt_flags = []
        flt_order = []
        helper_funcs = bpy.types.UI_UL_list
        if self.filter_name:
            flt_flags = helper_funcs.filter_items_by_name(
                self.filter_name, self.bitflag_filter_item, keys, "name")
        if self.use_filter_sort_alpha:
            flt_order = helper_funcs.sort_items_by_name(keys, "name")
        return flt_flags, flt_order


class ALightGroupPanel(Panel):
    bl_label = "Light Groups"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'

    def get_groups(self, ctx):
        return get_groups_ctx(ctx)

    def draw(self, ctx):
        layout = self.layout
        groups = self.get_groups(ctx)
        if groups is None:
            return
        row = layout.row()
        row.template_list("MAT_UL_LightGroupList", "",
                          groups, "groups", groups, "group_index",
                          rows=5, type='DEFAULT')
        # Right-hand operator column.
        col = row.column(align=True)
        col.operator('light_groups.link', icon='LINKED', text="")
        col.operator('light_groups.unlink', icon='UNLINKED', text="")
        col.separator()
        col.operator('light_groups.new', icon='ADD', text="")
        col.operator('light_groups.remove', icon='X', text="")
        col.separator()
        col.operator('light_groups.resync', icon='FILE_REFRESH', text="")

        row = layout.row()
        row.prop(groups, 'use_default', text="Use Default Group")
        # Shadow-related options only apply to Shader Info nodes (not to lights).
        if getattr(self, 'bl_context', '') != "data" or self.bl_space_type == 'NODE_EDITOR':
            row = row.row()
            row.enabled = groups.use_default
            row.prop(groups, 'ignore_default_shadow', text="Ignore Default Shadows")


class OBJ_PT_LLightGroupPanel(ALightGroupPanel):
    bl_context = 'data'

    def get_groups(self, ctx):
        ob = ctx.object
        light = ctx.light
        space = ctx.space_data
        if ob:
            data = ob.data
        elif light:
            data = space.pin_id
        else:
            return None
        return data.light_groups

    @classmethod
    def poll(cls, ctx):
        return ctx.light


class NOD_PT_LightGroupPanel(ALightGroupPanel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = 'Node'

    @classmethod
    def poll(cls, ctx):
        return isinstance(getattr(ctx, 'active_node', None), ShaderNodeShaderInfo)

    def get_groups(self, ctx):
        node = ctx.active_node
        if isinstance(node, ShaderNodeShaderInfo):
            return node.light_groups
        return None


class LightGroupOp(Operator):
    bl_options = {'UNDO'}

    @staticmethod
    def get_groups(ctx):
        return get_groups_ctx(ctx)

    @classmethod
    def poll(cls, ctx):
        try:
            return cls.get_groups(ctx) is not None
        except Exception:
            return False


class LightGroupSelectionOp(Operator):
    bl_options = {'UNDO'}

    @staticmethod
    def get_groups(ctx):
        return get_groups_ctx(ctx)

    @classmethod
    def poll(cls, ctx):
        try:
            grp = cls.get_groups(ctx)
            return grp is not None and 0 <= grp.group_index < len(grp.groups)
        except Exception:
            return False


class MAT_OT_NewLightGroup(LightGroupOp):
    """Create a new unique light group"""
    bl_label = "New Light Group"
    bl_idname = 'light_groups.new'

    def execute(self, ctx):
        lgs = self.get_groups(ctx)
        name = unique_group_name()
        new = lgs.groups.add()
        new.name = name
        lgs.group_index = lgs.groups.find(new.name)
        sync_light_groups()
        return {'FINISHED'}


class MAT_OT_LinkLightGroup(LightGroupOp):
    """Link an existing light group to this data-block"""
    bl_label = "Link Existing Light Group"
    bl_idname = 'light_groups.link'
    bl_property = "name"

    name: EnumProperty(
        items=lambda scn, ctx: sorted(
            [(x, x, x) for x in get_name_set() if x not in get_groups_ctx(ctx).groups]))

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'FINISHED'}

    def execute(self, ctx):
        lgs = self.get_groups(ctx)
        new = lgs.groups.add()
        new.name = self.name
        lgs.group_index = lgs.groups.find(new.name)
        sync_light_groups()
        return {'FINISHED'}


class MAT_OT_DeleteLightGroup(LightGroupSelectionOp):
    """Remove this light group from all data-blocks"""
    bl_label = "Delete Light Group"
    bl_idname = 'light_groups.remove'

    def invoke(self, ctx, evt):
        return ctx.window_manager.invoke_confirm(self, evt)

    def execute(self, ctx):
        lgs = self.get_groups(ctx)
        grp_name = lgs.groups[lgs.group_index].name
        for data in iter_light_group_owners():
            if grp_name not in data.light_groups.groups:
                continue
            data.light_groups.groups.remove(data.light_groups.groups.find(grp_name))
        lgs.group_index -= 1
        sync_light_groups()
        return {'FINISHED'}


class MAT_OT_UnlinkLightGroup(LightGroupSelectionOp):
    """Remove this light group from this data-block"""
    bl_label = "Unlink Light Group"
    bl_idname = 'light_groups.unlink'

    def execute(self, ctx):
        lgs = self.get_groups(ctx)
        lgs.groups.remove(lgs.group_index)
        lgs.group_index -= 1
        sync_light_groups()
        return {'FINISHED'}


class MAT_OT_ResyncLightGroups(LightGroupOp):
    """Ensure light group layers are up to date"""
    bl_label = "Resync Light Groups"
    bl_idname = 'light_groups.resync'

    def execute(self, ctx):
        sync_light_groups()
        return {'FINISHED'}


_classes = (
    LightGroup,
    LightGroups,
    MAT_UL_LightGroupList,
    OBJ_PT_LLightGroupPanel,
    NOD_PT_LightGroupPanel,
    MAT_OT_NewLightGroup,
    MAT_OT_LinkLightGroup,
    MAT_OT_DeleteLightGroup,
    MAT_OT_UnlinkLightGroup,
    MAT_OT_ResyncLightGroups,
)

_register, _unregister = register_classes_factory(classes=_classes)


def _safe_append(handler_list, fn):
    if fn not in handler_list:
        handler_list.append(fn)


def _safe_remove(handler_list, fn):
    if fn in handler_list:
        handler_list.remove(fn)


def register():
    _register()
    Light.light_groups = PointerProperty(type=LightGroups)
    ShaderNodeShaderInfo.light_groups = PointerProperty(type=LightGroups)

    _safe_append(bpy.app.handlers.render_init, sync_handler)
    _safe_append(bpy.app.handlers.load_post, sync_handler)
    _safe_append(bpy.app.handlers.depsgraph_update_post, sync_dg_handler)


def unregister():
    _safe_remove(bpy.app.handlers.depsgraph_update_post, sync_dg_handler)
    _safe_remove(bpy.app.handlers.load_post, sync_handler)
    _safe_remove(bpy.app.handlers.render_init, sync_handler)

    del Light.light_groups
    del ShaderNodeShaderInfo.light_groups
    _unregister()
