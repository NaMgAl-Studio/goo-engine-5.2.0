/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * Hex Grid Texture node (ported from Goo Engine, SH_NODE_TEX_HEXAGON = 902).
 * GPU-only (no CPU multi_function). Pure math, engine-independent.
 */

#include "DNA_node_types.h"

#include "BKE_texture.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_util.hh"
#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_tex_hexagon_cc {

NODE_STORAGE_FUNCS(NodeTexHexagon)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector"_ustr).hide_value();
  b.add_input<decl::Float>("Scale"_ustr).min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>("Size"_ustr).min(0.0f).max(16.0f).default_value(1.0f);
  b.add_input<decl::Float>("Radius"_ustr).min(0.0f).max(1000.0f).default_value(0.0f);
  b.add_input<decl::Float>("Roundness"_ustr).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Float>("Value"_ustr);
  b.add_output<decl::Color>("Color"_ustr);
  b.add_output<decl::Vector>("Hex Coords"_ustr);
  b.add_output<decl::Vector>("Position"_ustr);
  b.add_output<decl::Vector>("Cell UV"_ustr);
  b.add_output<decl::Vector>("Cell ID"_ustr);
}

static void node_shader_buts_tex_hexagon(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "value_mode", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(ptr, "direction", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(ptr, "coord_mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_shader_init_tex_hexagon(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexHexagon *tex = MEM_new<NodeTexHexagon>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  node->storage = tex;
}

static int node_shader_gpu_tex_hexagon(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  const NodeTexHexagon &tex = node_storage(*node);
  float coord_mode = float(tex.coord_mode);
  float value_mode = float(tex.value_mode);
  float direction = float(tex.direction);
  return GPU_stack_link(mat,
                        node,
                        "node_tex_hexagon",
                        in,
                        out,
                        GPU_constant(&coord_mode),
                        GPU_constant(&value_mode),
                        GPU_constant(&direction));
}

static void node_shader_update_tex_hexagon(bNodeTree *ntree, bNode *node)
{
  const NodeTexHexagon &tex = node_storage(*node);
  bNodeSocket *sock_radius = bke::node_find_socket(*node, SOCK_IN, "Radius"_ustr);
  bNodeSocket *sock_round = bke::node_find_socket(*node, SOCK_IN, "Roundness"_ustr);
  bke::node_set_socket_availability(
      *ntree, *sock_radius, tex.value_mode != SHD_HEXAGON_VALUE_DOT);
  bke::node_set_socket_availability(
      *ntree, *sock_round, tex.value_mode == SHD_HEXAGON_VALUE_SDF);
}

}  // namespace nodes::node_shader_tex_hexagon_cc

void register_node_type_sh_tex_hexagon()
{
  namespace file_ns = nodes::node_shader_tex_hexagon_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexHexagon"_ustr, SH_NODE_TEX_HEXAGON);
  ntype.ui_name = "Hex Grid Texture";
  ntype.ui_description = "Generate a hexagonal grid pattern";
  ntype.enum_name_legacy = "TEX_HEXAGON";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_hexagon;
  ntype.initfunc = file_ns::node_shader_init_tex_hexagon;
  ntype.updatefunc = file_ns::node_shader_update_tex_hexagon;
  bke::node_type_storage(
      ntype, "NodeTexHexagon", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_hexagon;

  bke::node_register_type(ntype);
}

}  // namespace blender
