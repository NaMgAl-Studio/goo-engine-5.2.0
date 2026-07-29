/* SPDX-FileCopyrightText: 2021 Blender Authors
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * Set Depth node (ported from Goo Engine, SH_NODE_SET_DEPTH). Shader passthrough
 * in EEVEE-Next; real pixel-depth-offset needs a non-early-Z engine path.
 */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_set_depth_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>("Shader"_ustr);
  b.add_input<decl::Float>("View Depth"_ustr).hide_value();
  b.add_output<decl::Shader>("Shader"_ustr);
}

static int node_shader_gpu_set_depth(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  /* Default the "View Depth" input to the fragment's own view-space depth (a no-op offset). */
  if (!in[1].link) {
    GPU_link(mat, "view_z_get", &in[1].link);
  }
  /* Route the material to the non-early-Z surface variant that writes gl_FragDepth. */
  GPU_material_flag_set(mat, GPU_MATFLAG_SET_DEPTH);
  return GPU_stack_link(mat, node, "node_set_depth", in, out);
}

}  // namespace nodes::node_shader_set_depth_cc

void register_node_type_sh_set_depth()
{
  namespace file_ns = nodes::node_shader_set_depth_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeSetDepth"_ustr, SH_NODE_SET_DEPTH);
  ntype.ui_name = "Set Depth";
  ntype.ui_description = "Pixel depth offset (shader passthrough in EEVEE-Next)";
  ntype.enum_name_legacy = "SET_DEPTH";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_set_depth;

  bke::node_register_type(ntype);
}

}  // namespace blender
