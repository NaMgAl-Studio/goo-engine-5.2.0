/* SPDX-FileCopyrightText: 2021 Blender Authors
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * SDF Vector Operator node (ported from Goo Engine, SH_NODE_SDF_VECTOR_OP).
 * Transforms/repeats coordinates for signed-distance-field work. GPU-only.
 */

#include "DNA_node_types.h"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_util.hh"
#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_sdf_vector_op_cc {

NODE_STORAGE_FUNCS(NodeSdfVectorOp)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector1"_ustr).hide_value();
  b.add_input<decl::Vector>("Vector2"_ustr);
  b.add_input<decl::Vector>("Vector3"_ustr);
  b.add_input<decl::Float>("Scale"_ustr).min(-100000.0f).max(100000.0f).default_value(1.0f);
  b.add_input<decl::Float>("Value1"_ustr).min(-100000.0f).max(100000.0f).default_value(0.0f);
  b.add_input<decl::Float>("Value2"_ustr).min(-100000.0f).max(100000.0f).default_value(0.0f);
  b.add_input<decl::Float>("Angle"_ustr).subtype(PROP_ANGLE);
  b.add_input<decl::Int>("Count1"_ustr).min(0).max(127).default_value(4);
  b.add_input<decl::Int>("Count2"_ustr).min(0).max(127).default_value(4);
  b.add_output<decl::Vector>("Vector"_ustr);
  b.add_output<decl::Vector>("Position"_ustr);
  b.add_output<decl::Float>("Value"_ustr);
}

static const char *sdf_vector_op_get_name(int mode)
{
  switch (mode) {
    case SHD_SDF_VEC_OP_SPIN:
      return "node_sdf_vector_op_spin";
    case SHD_SDF_VEC_OP_SWIZZLE:
      return "node_sdf_vector_op_swizzle";
    case SHD_SDF_VEC_OP_EXTRUDE:
      return "node_sdf_vector_op_extrude";
    case SHD_SDF_VEC_OP_TWIST:
      return "node_sdf_vector_op_twist";
    case SHD_SDF_VEC_OP_SWIRL:
      return "node_sdf_vector_op_swirl";
    case SHD_SDF_VEC_OP_RADIAL_SHEAR:
      return "node_sdf_vector_op_radial_shear";
    case SHD_SDF_VEC_OP_PINCH_INFLATE:
      return "node_sdf_vector_op_pinch_inflate";
    case SHD_SDF_VEC_OP_BEND:
      return "node_sdf_vector_op_bend";
    case SHD_SDF_VEC_OP_REPEAT_FINITE:
      return "node_sdf_vector_op_repeat";
    case SHD_SDF_VEC_OP_REPEAT_INF:
      return "node_sdf_vector_op_repeat_inf";
    case SHD_SDF_VEC_OP_REPEAT_INF_MIRROR:
      return "node_sdf_vector_op_repeat_inf_mirror";
    case SHD_SDF_VEC_OP_ROTATE:
      return "node_sdf_vector_op_rotate";
    case SHD_SDF_VEC_OP_REFLECT:
      return "node_sdf_vector_op_reflect";
    case SHD_SDF_VEC_OP_MIRROR:
      return "node_sdf_vector_op_mirror";
    case SHD_SDF_VEC_OP_POLAR:
      return "node_sdf_vector_op_polar";
    case SHD_SDF_VEC_OP_MAP_UV:
      return "node_sdf_vector_op_map_uv";
    case SHD_SDF_VEC_OP_MAP_11:
      return "node_sdf_vector_op_map_11";
    case SHD_SDF_VEC_OP_MAP_05:
      return "node_sdf_vector_op_map_05";
    case SHD_SDF_VEC_OP_ROTATE_UV:
      return "node_sdf_vector_op_uv_rotate";
    case SHD_SDF_VEC_OP_SCALE_UV:
      return "node_sdf_vector_op_uv_scale";
    case SHD_SDF_VEC_OP_RND_UV:
      return "node_sdf_vector_op_random_uv_rotate";
    case SHD_SDF_VEC_OP_RND_UV_FLIP:
      return "node_sdf_vector_op_random_uv_flip";
    case SHD_SDF_VEC_OP_OCTANT:
      return "node_sdf_vector_op_octant";
    case SHD_SDF_VEC_OP_TILESET:
      return "node_sdf_vector_op_tileset";
    case SHD_SDF_VEC_OP_GRID:
      return "node_sdf_vector_op_grid";
  }
  return nullptr;
}

static int node_shader_gpu_sdf_vector_op(GPUMaterial *mat,
                                         bNode *node,
                                         bNodeExecData * /*execdata*/,
                                         GPUNodeStack *in,
                                         GPUNodeStack *out)
{
  const NodeSdfVectorOp &sdf = node_storage(*node);
  const char *name = sdf_vector_op_get_name(sdf.operation);
  if (name != nullptr) {
    float axis = float(sdf.axis);
    return GPU_stack_link(mat, node, name, in, out, GPU_constant(&axis));
  }
  return 0;
}

static void node_shader_init_sdf_vector_op(bNodeTree * /*ntree*/, bNode *node)
{
  NodeSdfVectorOp *sdf = MEM_new<NodeSdfVectorOp>(__func__);
  sdf->operation = SHD_SDF_VEC_OP_GRID;
  sdf->axis = SHD_SDF_AXIS_XYZ;
  node->storage = sdf;
}

static void node_shader_buts_sdf_vector_op(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
  int type = RNA_enum_get(ptr, "operation");
  if (ELEM(type,
           SHD_SDF_VEC_OP_ROTATE_UV,
           SHD_SDF_VEC_OP_OCTANT,
           SHD_SDF_VEC_OP_GRID,
           SHD_SDF_VEC_OP_TWIST,
           SHD_SDF_VEC_OP_SWIRL,
           SHD_SDF_VEC_OP_RADIAL_SHEAR,
           SHD_SDF_VEC_OP_MIRROR,
           SHD_SDF_VEC_OP_SWIZZLE,
           SHD_SDF_VEC_OP_ROTATE,
           SHD_SDF_VEC_OP_POLAR,
           SHD_SDF_VEC_OP_BEND,
           SHD_SDF_VEC_OP_SPIN,
           SHD_SDF_VEC_OP_EXTRUDE))
  {
    layout.prop(ptr, "axis", UI_ITEM_NONE, "", ICON_NONE);
  }
}

static void node_shader_update_sdf_vector_op(bNodeTree *ntree, bNode *node)
{
  const NodeSdfVectorOp &sdf = node_storage(*node);
  bNodeSocket *sock_v1 = bke::node_find_socket(*node, SOCK_IN, "Vector1"_ustr);
  bNodeSocket *sock_v2 = bke::node_find_socket(*node, SOCK_IN, "Vector2"_ustr);
  bNodeSocket *sock_v3 = bke::node_find_socket(*node, SOCK_IN, "Vector3"_ustr);
  bNodeSocket *sock_scale = bke::node_find_socket(*node, SOCK_IN, "Scale"_ustr);
  bNodeSocket *sock_val = bke::node_find_socket(*node, SOCK_IN, "Value1"_ustr);
  bNodeSocket *sock_val2 = bke::node_find_socket(*node, SOCK_IN, "Value2"_ustr);
  bNodeSocket *sock_angle = bke::node_find_socket(*node, SOCK_IN, "Angle"_ustr);
  bNodeSocket *sock_count = bke::node_find_socket(*node, SOCK_IN, "Count1"_ustr);
  bNodeSocket *sock_count2 = bke::node_find_socket(*node, SOCK_IN, "Count2"_ustr);
  bNodeSocket *sock_pos_out = bke::node_find_socket(*node, SOCK_OUT, "Position"_ustr);
  bNodeSocket *sock_val_out = bke::node_find_socket(*node, SOCK_OUT, "Value"_ustr);

  UNUSED_VARS(sock_v1);

  bke::node_set_socket_availability(
      *ntree,
      *sock_val_out,
      ELEM(sdf.operation,
           SHD_SDF_VEC_OP_MIRROR,
           SHD_SDF_VEC_OP_REFLECT,
           SHD_SDF_VEC_OP_POLAR,
           SHD_SDF_VEC_OP_EXTRUDE));
  bke::node_set_socket_availability(
      *ntree, *sock_pos_out, ELEM(sdf.operation, SHD_SDF_VEC_OP_MIRROR, SHD_SDF_VEC_OP_GRID));
  bke::node_set_socket_availability(*ntree,
                                    *sock_v2,
                                    ELEM(sdf.operation,
                                         SHD_SDF_VEC_OP_SWIRL,
                                         SHD_SDF_VEC_OP_RADIAL_SHEAR,
                                         SHD_SDF_VEC_OP_PINCH_INFLATE,
                                         SHD_SDF_VEC_OP_ROTATE_UV,
                                         SHD_SDF_VEC_OP_GRID,
                                         SHD_SDF_VEC_OP_RND_UV,
                                         SHD_SDF_VEC_OP_RND_UV_FLIP,
                                         SHD_SDF_VEC_OP_MIRROR,
                                         SHD_SDF_VEC_OP_EXTRUDE,
                                         SHD_SDF_VEC_OP_REFLECT,
                                         SHD_SDF_VEC_OP_REPEAT_FINITE,
                                         SHD_SDF_VEC_OP_REPEAT_INF,
                                         SHD_SDF_VEC_OP_REPEAT_INF_MIRROR));
  bke::node_set_socket_availability(*ntree,
                                    *sock_v3,
                                    ELEM(sdf.operation,
                                         SHD_SDF_VEC_OP_SWIRL,
                                         SHD_SDF_VEC_OP_RADIAL_SHEAR,
                                         SHD_SDF_VEC_OP_REPEAT_FINITE));
  bke::node_set_socket_availability(
      *ntree, *sock_scale, ELEM(sdf.operation, SHD_SDF_VEC_OP_SCALE_UV, SHD_SDF_VEC_OP_TILESET));
  bke::node_set_socket_availability(*ntree,
                                    *sock_val,
                                    !ELEM(sdf.operation,
                                          SHD_SDF_VEC_OP_MAP_11,
                                          SHD_SDF_VEC_OP_MAP_05,
                                          SHD_SDF_VEC_OP_MAP_UV,
                                          SHD_SDF_VEC_OP_RND_UV_FLIP,
                                          SHD_SDF_VEC_OP_ROTATE_UV,
                                          SHD_SDF_VEC_OP_RND_UV,
                                          SHD_SDF_VEC_OP_SCALE_UV,
                                          SHD_SDF_VEC_OP_EXTRUDE,
                                          SHD_SDF_VEC_OP_GRID,
                                          SHD_SDF_VEC_OP_MIRROR,
                                          SHD_SDF_VEC_OP_ROTATE,
                                          SHD_SDF_VEC_OP_SWIZZLE,
                                          SHD_SDF_VEC_OP_BEND,
                                          SHD_SDF_VEC_OP_REPEAT_FINITE,
                                          SHD_SDF_VEC_OP_REPEAT_INF,
                                          SHD_SDF_VEC_OP_REPEAT_INF_MIRROR));
  bke::node_set_socket_availability(
      *ntree, *sock_val2, ELEM(sdf.operation, SHD_SDF_VEC_OP_TILESET, SHD_SDF_VEC_OP_PINCH_INFLATE));
  bke::node_set_socket_availability(*ntree,
                                    *sock_angle,
                                    ELEM(sdf.operation,
                                         SHD_SDF_VEC_OP_BEND,
                                         SHD_SDF_VEC_OP_TWIST,
                                         SHD_SDF_VEC_OP_ROTATE,
                                         SHD_SDF_VEC_OP_ROTATE_UV));
  bke::node_set_socket_availability(*ntree, *sock_count, ELEM(sdf.operation, SHD_SDF_VEC_OP_TILESET));
  bke::node_set_socket_availability(
      *ntree, *sock_count2, ELEM(sdf.operation, SHD_SDF_VEC_OP_TILESET));
}

}  // namespace nodes::node_shader_sdf_vector_op_cc

void register_node_type_sh_sdf_vector_op()
{
  namespace file_ns = nodes::node_shader_sdf_vector_op_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeSdfVectorOp"_ustr, SH_NODE_SDF_VECTOR_OP);
  ntype.ui_name = "SDF Vector Operator";
  ntype.ui_description = "Transform or repeat coordinates for signed distance field work";
  ntype.enum_name_legacy = "SDF_VECTOR_OP";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_shader_init_sdf_vector_op;
  ntype.updatefunc = file_ns::node_shader_update_sdf_vector_op;
  ntype.draw_buttons = file_ns::node_shader_buts_sdf_vector_op;
  bke::node_type_storage(
      ntype, "NodeSdfVectorOp", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_sdf_vector_op;

  bke::node_register_type(ntype);
}

}  // namespace blender
