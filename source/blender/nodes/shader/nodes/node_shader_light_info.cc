/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * Light Info node (ported from Goo Engine, SH_NODE_LIGHT_INFO = 901).
 * References a light Object via node->id and bakes its color/power as GPU
 * uniforms at material compile time.
 */

#include "DNA_light_types.h"
#include "DNA_object_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_light_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Light Color"_ustr);
  b.add_output<decl::Float>("Light Power"_ustr);
  b.add_output<decl::Float>("Perceptual Power"_ustr);
}

static void node_shader_buts_light_info(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "light_object", UI_ITEM_NONE, "Light", ICON_LIGHT);
}

static int node_shader_gpu_light_info(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  /* Capture the referenced light's properties now (main thread) as uniforms. */
  Object *ob = reinterpret_cast<Object *>(node->id);
  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float power = 1.0f;
  if (ob != nullptr && ob->type == OB_LAMP && ob->data != nullptr) {
    const Light *light = reinterpret_cast<const Light *>(ob->data);
    color[0] = light->r;
    color[1] = light->g;
    color[2] = light->b;
    color[3] = 1.0f;
    power = light->energy;
  }

  GPUNodeLink *color_link = GPU_uniform(color);
  GPUNodeLink *power_link = GPU_uniform(&power);
  return GPU_stack_link(mat, node, "node_light_info_simple", in, out, color_link, power_link);
}

}  // namespace nodes::node_shader_light_info_cc

void register_node_type_sh_light_info()
{
  namespace file_ns = nodes::node_shader_light_info_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeLightInfo"_ustr, SH_NODE_LIGHT_INFO);
  ntype.ui_name = "Light Info";
  ntype.ui_description = "Read color and power from a selected light object";
  ntype.enum_name_legacy = "LIGHT_INFO";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_light_info;
  ntype.gpu_fn = file_ns::node_shader_gpu_light_info;

  bke::node_register_type(ntype);
}

}  // namespace blender
