/* SPDX-FileCopyrightText: 2021 Blender Authors
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * Curvature node (ported from Goo Engine, SH_NODE_CURVATURE). Screen-space
 * curvature + rim estimated from the EEVEE scene depth buffer (hiz_tx).
 */

#include "node_util.hh"
#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_curvature_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Samples"_ustr).min(1.0f).max(64.0f).default_value(8.0f);
  b.add_input<decl::Float>("Sample Radius"_ustr).min(0.0f).max(64.0f).default_value(1.0f);
  b.add_input<decl::Float>("Thickness"_ustr).min(0.0f).max(100.0f).default_value(1.0f);
  b.add_input<decl::Vector>("Scale"_ustr).default_value({1.0f, 1.0f, 0.0f});
  b.add_output<decl::Float>("Scene Curvature"_ustr);
  b.add_output<decl::Float>("Scene Rim"_ustr);
}

static int node_shader_gpu_curvature(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  /* Ensure the material is treated as needing shading data (matches Goo). */
  GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE);
  return GPU_stack_link(mat, node, "node_screenspace_curvature", in, out);
}

}  // namespace nodes::node_shader_curvature_cc

void register_node_type_sh_curvature()
{
  namespace file_ns = nodes::node_shader_curvature_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeCurvature"_ustr, SH_NODE_CURVATURE);
  ntype.ui_name = "Curvature";
  ntype.ui_description = "Screen-space curvature and rim estimated from scene depth";
  ntype.enum_name_legacy = "CURVATURE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_curvature;

  bke::node_register_type(ntype);
}

}  // namespace blender
