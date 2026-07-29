/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * SDF Noise node (ported from Goo Engine, SH_NODE_SDF_NOISE = 805).
 * Pure math, engine-independent. No custom storage.
 */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_sdf_noise_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Position"_ustr).hide_value();
  b.add_input<decl::Float>("Distance"_ustr);
  b.add_input<decl::Float>("Detail"_ustr).default_value(4.0f).min(0.0f).max(12.0f);
  b.add_input<decl::Float>("Roughness"_ustr)
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Detail Inflation"_ustr)
      .default_value(0.1f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Detail Blend"_ustr)
      .default_value(0.3f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_output<decl::Float>("Distance"_ustr);
}

static int node_shader_gpu_sdf_noise(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  return GPU_stack_link(mat, node, "node_sdf_noise", in, out);
}

}  // namespace nodes::node_shader_sdf_noise_cc

void register_node_type_sh_sdf_noise()
{
  namespace file_ns = nodes::node_shader_sdf_noise_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeSdfNoise"_ustr, SH_NODE_SDF_NOISE);
  ntype.ui_name = "SDF Noise";
  ntype.ui_description = "Fractal Brownian Motion noise applied to a signed distance field";
  ntype.enum_name_legacy = "SDF_NOISE";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_sdf_noise;

  bke::node_register_type(ntype);
}

}  // namespace blender
