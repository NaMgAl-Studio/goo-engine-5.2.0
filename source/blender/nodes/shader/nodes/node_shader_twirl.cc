/* SPDX-FileCopyrightText: 2025 Blender Authors
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * Twirl vector node (ported from Goo Engine, SH_NODE_TWIRL = 903).
 * Rotates the input vector around a center point by an amount proportional to
 * the distance from the center. Pure math, engine-independent.
 */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_twirl_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector"_ustr).default_value({0.0f, 0.0f, 0.0f});
  b.add_input<decl::Vector>("Center"_ustr)
      .default_value({0.5f, 0.5f, 0.0f})
      .min(0.0f)
      .max(1.0f);
  b.add_input<decl::Float>("Amount"_ustr).default_value(0.0f).min(-100.0f).max(100.0f);
  b.add_output<decl::Vector>("Vector"_ustr);
}

static int gpu_shader_twirl(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData * /*execdata*/,
                            GPUNodeStack *in,
                            GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_twirl", in, out);
}

}  // namespace nodes::node_shader_twirl_cc

void register_node_type_sh_twirl()
{
  namespace file_ns = nodes::node_shader_twirl_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTwirl"_ustr, SH_NODE_TWIRL);
  ntype.ui_name = "Twirl";
  ntype.ui_description = "Twirl the input vector around a center point by a specified amount";
  ntype.enum_name_legacy = "TWIRL";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_twirl;

  bke::node_register_type(ntype);
}

}  // namespace blender
