/* SPDX-FileCopyrightText: 2021 Blender Authors
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * SDF Operator node (ported from Goo Engine, SH_NODE_SDF_OP). Combines/modifies
 * scalar signed-distance-field values. GPU-only, pure math.
 */

#include "DNA_node_types.h"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_util.hh"
#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_sdf_op_cc {

NODE_STORAGE_FUNCS(NodeSdfOp)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Distance"_ustr).min(-100000.0f).max(100000.0f).default_value(0.5f);
  b.add_input<decl::Float>("Distance"_ustr, "Distance_001"_ustr)
      .min(-100000.0f)
      .max(100000.0f)
      .default_value(0.5f);
  b.add_input<decl::Float>("Value"_ustr).min(-100000.0f).max(100000.0f).default_value(0.5f);
  b.add_input<decl::Float>("Value"_ustr, "Value_001"_ustr)
      .min(-100000.0f)
      .max(100000.0f)
      .default_value(0.5f);
  b.add_input<decl::Int>("Count"_ustr).min(-100000).max(100000).default_value(1);
  b.add_output<decl::Float>("Distance"_ustr);
}

static const char *sdf_op_get_name(int mode)
{
  switch (mode) {
    case SHD_SDF_OP_DILATE:
      return "node_sdf_op_dilate";
    case SHD_SDF_OP_ONION:
      return "node_sdf_op_onion";
    case SHD_SDF_OP_ANNULAR:
      return "node_sdf_op_annular";
    case SHD_SDF_OP_BLEND:
      return "node_sdf_op_blend";
    case SHD_SDF_OP_INVERT:
      return "node_sdf_op_invert";
    case SHD_SDF_OP_FLATTEN:
      return "node_sdf_op_flatten";
    case SHD_SDF_OP_MASK:
      return "node_sdf_op_mask";
    case SHD_SDF_OP_PULSE:
      return "node_sdf_op_pulse";
    case SHD_SDF_OP_PIPE:
      return "node_sdf_op_pipe";
    case SHD_SDF_OP_ENGRAVE:
      return "node_sdf_op_engrave";
    case SHD_SDF_OP_GROOVE:
      return "node_sdf_op_groove";
    case SHD_SDF_OP_TONGUE:
      return "node_sdf_op_tongue";
    case SHD_SDF_OP_UNION:
      return "node_sdf_op_union";
    case SHD_SDF_OP_INTERSECT:
      return "node_sdf_op_intersect";
    case SHD_SDF_OP_DIFF:
      return "node_sdf_op_diff";
    case SHD_SDF_OP_UNION_SMOOTH:
      return "node_sdf_op_union_smooth";
    case SHD_SDF_OP_INTERSECT_SMOOTH:
      return "node_sdf_op_intersect_smooth";
    case SHD_SDF_OP_DIFF_SMOOTH:
      return "node_sdf_op_diff_smooth";
    case SHD_SDF_OP_UNION_STAIRS:
      return "node_sdf_op_union_stairs";
    case SHD_SDF_OP_INTERSECT_STAIRS:
      return "node_sdf_op_intersect_stairs";
    case SHD_SDF_OP_DIFF_STAIRS:
      return "node_sdf_op_diff_stairs";
    case SHD_SDF_OP_UNION_CHAMFER:
      return "node_sdf_op_union_chamfer";
    case SHD_SDF_OP_INTERSECT_CHAMFER:
      return "node_sdf_op_intersect_chamfer";
    case SHD_SDF_OP_DIFF_CHAMFER:
      return "node_sdf_op_diff_chamfer";
    case SHD_SDF_OP_UNION_COLUMNS:
      return "node_sdf_op_union_columns";
    case SHD_SDF_OP_INTERSECT_COLUMNS:
      return "node_sdf_op_intersect_columns";
    case SHD_SDF_OP_DIFF_COLUMNS:
      return "node_sdf_op_diff_columns";
    case SHD_SDF_OP_UNION_ROUND:
      return "node_sdf_op_union_round";
    case SHD_SDF_OP_INTERSECT_ROUND:
      return "node_sdf_op_intersect_round";
    case SHD_SDF_OP_DIFF_ROUND:
      return "node_sdf_op_diff_round";
    case SHD_SDF_OP_DIVIDE:
      return "node_sdf_op_divide";
    case SHD_SDF_OP_EXCLUSION:
      return "node_sdf_op_exclusion";
  }
  return nullptr;
}

static int node_shader_gpu_sdf_op(GPUMaterial *mat,
                                  bNode *node,
                                  bNodeExecData * /*execdata*/,
                                  GPUNodeStack *in,
                                  GPUNodeStack *out)
{
  const NodeSdfOp &sdf = node_storage(*node);
  const char *name = sdf_op_get_name(sdf.operation);
  if (name != nullptr) {
    float invert = (sdf.invert) ? 1.0f : -1.0f;
    return GPU_stack_link(mat, node, name, in, out, GPU_constant(&invert));
  }
  return 0;
}

static void node_shader_init_sdf_op(bNodeTree * /*ntree*/, bNode *node)
{
  NodeSdfOp *sdf = MEM_new<NodeSdfOp>(__func__);
  sdf->operation = SHD_SDF_OP_UNION;
  sdf->invert = 0;
  node->storage = sdf;
}

static void node_shader_buts_sdf_op(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
  int type = RNA_enum_get(ptr, "operation");
  if (ELEM(type, SHD_SDF_OP_MASK)) {
    layout.prop(ptr, "invert", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void node_shader_update_sdf_op(bNodeTree *ntree, bNode *node)
{
  const NodeSdfOp &sdf = node_storage(*node);
  bNodeSocket *sock_a = bke::node_find_socket(*node, SOCK_IN, "Distance"_ustr);
  bNodeSocket *sock_b = bke::node_find_socket(*node, SOCK_IN, "Distance_001"_ustr);
  bNodeSocket *sock_v = bke::node_find_socket(*node, SOCK_IN, "Value"_ustr);
  bNodeSocket *sock_v2 = bke::node_find_socket(*node, SOCK_IN, "Value_001"_ustr);
  bNodeSocket *sock_count = bke::node_find_socket(*node, SOCK_IN, "Count"_ustr);

  bke::node_set_socket_availability(*ntree, *sock_a, true);
  bke::node_set_socket_availability(*ntree,
                                    *sock_b,
                                    !ELEM(sdf.operation,
                                          SHD_SDF_OP_MASK,
                                          SHD_SDF_OP_INVERT,
                                          SHD_SDF_OP_DILATE,
                                          SHD_SDF_OP_ONION,
                                          SHD_SDF_OP_ANNULAR,
                                          SHD_SDF_OP_PULSE,
                                          SHD_SDF_OP_FLATTEN));
  bke::node_set_socket_availability(
      *ntree,
      *sock_v,
      !ELEM(sdf.operation, SHD_SDF_OP_UNION, SHD_SDF_OP_INTERSECT, SHD_SDF_OP_DIFF, SHD_SDF_OP_INVERT));
  bke::node_set_socket_availability(*ntree,
                                    *sock_v2,
                                    ELEM(sdf.operation,
                                         SHD_SDF_OP_DIVIDE,
                                         SHD_SDF_OP_FLATTEN,
                                         SHD_SDF_OP_PULSE,
                                         SHD_SDF_OP_EXCLUSION,
                                         SHD_SDF_OP_TONGUE,
                                         SHD_SDF_OP_GROOVE,
                                         SHD_SDF_OP_UNION_COLUMNS,
                                         SHD_SDF_OP_INTERSECT_COLUMNS,
                                         SHD_SDF_OP_DIFF_COLUMNS,
                                         SHD_SDF_OP_UNION_STAIRS,
                                         SHD_SDF_OP_INTERSECT_STAIRS,
                                         SHD_SDF_OP_DIFF_STAIRS));
  bke::node_set_socket_availability(*ntree, *sock_count, ELEM(sdf.operation, SHD_SDF_OP_ONION));
}

}  // namespace nodes::node_shader_sdf_op_cc

void register_node_type_sh_sdf_op()
{
  namespace file_ns = nodes::node_shader_sdf_op_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeSdfOp"_ustr, SH_NODE_SDF_OP);
  ntype.ui_name = "SDF Operator";
  ntype.ui_description = "Combine or modify signed distance field values";
  ntype.enum_name_legacy = "SDF_OP";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_shader_init_sdf_op;
  ntype.updatefunc = file_ns::node_shader_update_sdf_op;
  ntype.draw_buttons = file_ns::node_shader_buts_sdf_op;
  bke::node_type_storage(
      ntype, "NodeSdfOp", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_sdf_op;

  bke::node_register_type(ntype);
}

}  // namespace blender
