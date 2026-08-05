/* SPDX-FileCopyrightText: 2021 Blender Authors
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * Shader Info node (ported from Goo Engine, SH_NODE_SHADER_INFO). Interface and
 * light-group storage are preserved so Goo files load. EEVEE's forward bridge computes
 * independent Cast and Self shadow visibility when material shadow-ID filtering is enabled.
 */

#include "DNA_node_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_shader_util.hh"
#include "node_util.hh"

namespace blender {

namespace nodes::node_shader_shader_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("WorldPosition"_ustr).hide_value();
  b.add_input<decl::Vector>("Normal"_ustr).hide_value();
  b.add_output<decl::Color>("Diffuse Shading"_ustr);
  b.add_output<decl::Float>("Cast Shadows"_ustr);
  b.add_output<decl::Float>("Self Shadows"_ustr);
  b.add_output<decl::Color>("Ambient Lighting"_ustr);
  b.add_output<decl::Float>("Half-lambert factor"_ustr);
}

static void node_shader_init_shader_info(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderInfo *shinfo = MEM_new<NodeShaderInfo>(__func__);
  shinfo->light_group_bits[3] = 1;
  shinfo->light_group_shadow_bits[3] = 1;
  shinfo->use_own_light_groups = 0;
  node->storage = shinfo;
}

static void node_shader_buts_shader_info(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "use_own_light_groups", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static int node_shader_gpu_shader_info(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  /* Route the material through the forward / Shader-to-RGB path where EEVEE binds the light and
   * light-probe resources that shader_info_eval reads (real diffuse/shadow/ambient).
   * GOO_SHADER_INFO gates the per-fragment bridge loop to materials that actually use it.
   * RAYCAST binds the screen-space prepass textures (depth / object id) used by the bridge's
   * legacy contact shadows. */
  GPU_material_flag_set(mat,
                        GPU_MATFLAG_DIFFUSE | GPU_MATFLAG_SHADER_TO_RGBA |
                            GPU_MATFLAG_GOO_SHADER_INFO | GPU_MATFLAG_RAYCAST);
  if (!in[0].link) {
    GPU_link(mat, "world_position_get", &in[0].link);
  }
  if (!in[1].link) {
    GPU_link(mat, "world_normals_get", &in[1].link);
  }
  /* Pass the node's per-node light-group masks as uniforms (int bits reinterpreted as floats,
   * recovered with floatBitsToInt in the shader), matching Goo's node_shader_info_light_groups.
   * When the node does not use its own light groups, pass an all-ones mask so it sums every group
   * bucket (i.e. sees all lights). */
  const NodeShaderInfo *info = static_cast<const NodeShaderInfo *>(node->storage);
  int light_groups[4];
  int light_group_shadows[4];
  for (int i = 0; i < 4; i++) {
    light_groups[i] = info->use_own_light_groups ? info->light_group_bits[i] : ~0;
    light_group_shadows[i] = info->use_own_light_groups ? info->light_group_shadow_bits[i] : ~0;
  }
  return GPU_stack_link(mat,
                        node,
                        "node_shader_info",
                        in,
                        out,
                        GPU_uniform((float *)light_groups),
                        GPU_uniform((float *)light_group_shadows));
}

}  // namespace nodes::node_shader_shader_info_cc

void register_node_type_sh_shader_info()
{
  namespace file_ns = nodes::node_shader_shader_info_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeShaderInfo"_ustr, SH_NODE_SHADER_INFO);
  ntype.ui_name = "Shader Info";
  ntype.ui_description =
      "Separate internal lighting into multiple outputs (per-node light groups)";
  ntype.enum_name_legacy = "SHADERINFO";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_shader_info;
  ntype.initfunc = file_ns::node_shader_init_shader_info;
  bke::node_type_storage(
      ntype, "NodeShaderInfo", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_shader_info;

  bke::node_register_type(ntype);
}

}  // namespace blender
