/* SPDX-FileCopyrightText: 2021 Blender Authors
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * SDF Primitive node (ported from Goo Engine, SH_NODE_SDF_PRIMITIVE).
 * Generates 2D/3D signed-distance-field shapes. GPU-only, pure math.
 */

#include "DNA_node_types.h"

#include "BKE_texture.h"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_util.hh"
#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_sdf_primitive_cc {

NODE_STORAGE_FUNCS(NodeSdfPrimitive)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector"_ustr).hide_value();
  b.add_input<decl::Float>("Size"_ustr).min(-100000.0f).max(100000.0f).default_value(1.0f);
  b.add_input<decl::Float>("Radius"_ustr).min(-100000.0f).max(100000.0f).default_value(0.5f);
  b.add_input<decl::Float>("Value1"_ustr).min(-100000.0f).max(100000.0f).default_value(1.0f);
  b.add_input<decl::Float>("Value2"_ustr).min(-100000.0f).max(100000.0f).default_value(1.0f);
  b.add_input<decl::Float>("Value3"_ustr).min(-100000.0f).max(100000.0f).default_value(1.0f);
  b.add_input<decl::Float>("Value4"_ustr).min(-100000.0f).max(100000.0f).default_value(0.5f);
  b.add_input<decl::Vector>("Point"_ustr).default_value({0.5f, 0.5f, 0.5f});
  b.add_input<decl::Vector>("Point"_ustr, "Point_001"_ustr).default_value({0.5f, 0.5f, 0.5f});
  b.add_input<decl::Vector>("Point"_ustr, "Point_002"_ustr).default_value({0.5f, 0.5f, 0.5f});
  b.add_input<decl::Vector>("Point"_ustr, "Point_003"_ustr).default_value({0.5f, 0.5f, 0.5f});
  b.add_input<decl::Float>("Angle"_ustr).subtype(PROP_ANGLE);
  b.add_input<decl::Float>("Roundness"_ustr).min(0.0f).max(1.0f).default_value(0.0f).subtype(
      PROP_FACTOR);
  b.add_input<decl::Float>("Linewidth"_ustr).min(-100000.0f).max(100000.0f).default_value(0.0f);
  b.add_output<decl::Float>("Distance"_ustr);
}

static void node_shader_init_sdf_primitive(bNodeTree * /*ntree*/, bNode *node)
{
  NodeSdfPrimitive *sdf = MEM_new<NodeSdfPrimitive>(__func__);
  BKE_texture_mapping_default(&sdf->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&sdf->base.color_mapping);
  sdf->mode = SHD_SDF_3D_SPHERE;
  sdf->invert = 0;
  node->storage = sdf;
}

static void node_shader_buts_sdf_primitive(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(ptr, "invert", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static const char *sdf_primitive_get_name(int mode)
{
  switch (mode) {
    case SHD_SDF_3D_SPHERE:
      return "node_sdf_prim_3d_sphere";
    case SHD_SDF_3D_BOX:
      return "node_sdf_prim_3d_box";
    case SHD_SDF_3D_HEX_PRISM:
      return "node_sdf_prim_3d_hex_prism";
    case SHD_SDF_3D_HEX_PRISM_INCIRCLE:
      return "node_sdf_prim_3d_hex_prism_incircle";
    case SHD_SDF_3D_TORUS:
      return "node_sdf_prim_3d_torus";
    case SHD_SDF_3D_CONE:
      return "node_sdf_prim_3d_cone";
    case SHD_SDF_3D_POINT_CONE:
      return "node_sdf_prim_3d_point_cone";
    case SHD_SDF_3D_CYLINDER:
      return "node_sdf_prim_3d_cylinder";
    case SHD_SDF_3D_POINT_CYLINDER:
      return "node_sdf_prim_3d_point_cylinder";
    case SHD_SDF_3D_CAPSULE:
      return "node_sdf_prim_3d_capsule";
    case SHD_SDF_3D_OCTAHEDRON:
      return "node_sdf_prim_3d_octahedron";
    case SHD_SDF_3D_PLANE:
      return "node_sdf_prim_3d_plane";
    case SHD_SDF_3D_SOLID_ANGLE:
      return "node_sdf_prim_3d_solid_angle";
    case SHD_SDF_3D_PYRAMID:
      return "node_sdf_prim_3d_pyramid";
    case SHD_SDF_3D_DISC:
      return "node_sdf_prim_3d_disc";
    case SHD_SDF_3D_CIRCLE:
      return "node_sdf_prim_3d_circle";
    case SHD_SDF_2D_CIRCLE:
      return "node_sdf_prim_2d_circle";
    case SHD_SDF_2D_RECTANGLE:
      return "node_sdf_prim_2d_rectangle";
    case SHD_SDF_2D_LINE:
      return "node_sdf_prim_2d_line";
    case SHD_SDF_2D_RHOMBUS:
      return "node_sdf_prim_2d_rhombus";
    case SHD_SDF_2D_STAR:
      return "node_sdf_prim_2d_star";
    case SHD_SDF_2D_TRIANGLE:
      return "node_sdf_prim_2d_triangle";
    case SHD_SDF_2D_HEXAGON:
      return "node_sdf_prim_2d_hexagon";
    case SHD_SDF_2D_PIE:
      return "node_sdf_prim_2d_pie";
    case SHD_SDF_2D_ARC:
      return "node_sdf_prim_2d_arc";
    case SHD_SDF_2D_BEZIER:
      return "node_sdf_prim_2d_bezier";
    case SHD_SDF_2D_UNEVEN_CAPSULE:
      return "node_sdf_prim_2d_uneven_capsule";
    case SHD_SDF_2D_POINT_TRIANGLE:
      return "node_sdf_prim_2d_point_triangle";
    case SHD_SDF_2D_TRAPEZOID:
      return "node_sdf_prim_2d_trapezoid";
    case SHD_SDF_2D_MOON:
      return "node_sdf_prim_2d_moon";
    case SHD_SDF_2D_VESICA:
      return "node_sdf_prim_2d_vesica";
    case SHD_SDF_2D_CROSS:
      return "node_sdf_prim_2d_cross";
    case SHD_SDF_2D_ROUNDX:
      return "node_sdf_prim_2d_rounded_x";
    case SHD_SDF_2D_HORSESHOE:
      return "node_sdf_prim_2d_horseshoe";
    case SHD_SDF_2D_PARABOLA:
      return "node_sdf_prim_2d_parabola";
    case SHD_SDF_2D_PARABOLA_SEGMENT:
      return "node_sdf_prim_2d_parabola_segment";
    case SHD_SDF_2D_ELLIPSE:
      return "node_sdf_prim_2d_ellipse";
    case SHD_SDF_2D_ISOSCELES:
      return "node_sdf_prim_2d_isosceles";
    case SHD_SDF_2D_ROUND_JOINT:
      return "node_sdf_prim_2d_round_joint";
    case SHD_SDF_2D_FLAT_JOINT:
      return "node_sdf_prim_2d_flat_joint";
    case SHD_SDF_2D_PENTAGON:
      return "node_sdf_prim_2d_pentagon";
    case SHD_SDF_2D_QUAD:
      return "node_sdf_prim_2d_quad";
    case SHD_SDF_2D_HEART:
      return "node_sdf_prim_2d_heart";
    case SHD_SDF_2D_CORNER:
      return "node_sdf_prim_2d_corner";
  }
  return nullptr;
}

static int node_shader_gpu_sdf_primitive(GPUMaterial *mat,
                                         bNode *node,
                                         bNodeExecData * /*execdata*/,
                                         GPUNodeStack *in,
                                         GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  const NodeSdfPrimitive &sdf = node_storage(*node);
  const char *name = sdf_primitive_get_name(sdf.mode);
  if (name != nullptr) {
    float invert = (sdf.invert) ? 1.0f : -1.0f;
    return GPU_stack_link(mat, node, name, in, out, GPU_constant(&invert));
  }
  return 0;
}

static void node_shader_update_sdf_primitive(bNodeTree *ntree, bNode *node)
{
  const NodeSdfPrimitive &sdf = node_storage(*node);
  bNodeSocket *sock_radius = bke::node_find_socket(*node, SOCK_IN, "Radius"_ustr);
  bNodeSocket *sock_v1 = bke::node_find_socket(*node, SOCK_IN, "Value1"_ustr);
  bNodeSocket *sock_v2 = bke::node_find_socket(*node, SOCK_IN, "Value2"_ustr);
  bNodeSocket *sock_v3 = bke::node_find_socket(*node, SOCK_IN, "Value3"_ustr);
  bNodeSocket *sock_v4 = bke::node_find_socket(*node, SOCK_IN, "Value4"_ustr);
  bNodeSocket *sock_p1 = bke::node_find_socket(*node, SOCK_IN, "Point"_ustr);
  bNodeSocket *sock_p2 = bke::node_find_socket(*node, SOCK_IN, "Point_001"_ustr);
  bNodeSocket *sock_p3 = bke::node_find_socket(*node, SOCK_IN, "Point_002"_ustr);
  bNodeSocket *sock_p4 = bke::node_find_socket(*node, SOCK_IN, "Point_003"_ustr);
  bNodeSocket *sock_angle = bke::node_find_socket(*node, SOCK_IN, "Angle"_ustr);
  bNodeSocket *sock_round = bke::node_find_socket(*node, SOCK_IN, "Roundness"_ustr);

  bke::node_set_socket_availability(
      *ntree,
      *sock_round,
      ELEM(sdf.mode, SHD_SDF_3D_BOX, SHD_SDF_3D_CONE, SHD_SDF_3D_CYLINDER,
           SHD_SDF_3D_POINT_CYLINDER, SHD_SDF_3D_OCTAHEDRON, SHD_SDF_3D_PYRAMID,
           SHD_SDF_3D_HEX_PRISM, SHD_SDF_3D_HEX_PRISM_INCIRCLE, SHD_SDF_3D_DISC,
           SHD_SDF_3D_CIRCLE, SHD_SDF_3D_SOLID_ANGLE) ||
          ELEM(sdf.mode, SHD_SDF_2D_TRIANGLE, SHD_SDF_2D_RECTANGLE, SHD_SDF_2D_PENTAGON,
               SHD_SDF_2D_HEART, SHD_SDF_2D_HEXAGON, SHD_SDF_2D_ISOSCELES, SHD_SDF_2D_ARC,
               SHD_SDF_2D_CROSS, SHD_SDF_2D_PIE, SHD_SDF_2D_ROUNDX, SHD_SDF_2D_STAR,
               SHD_SDF_2D_UNEVEN_CAPSULE, SHD_SDF_2D_HORSESHOE));
  bke::node_set_socket_availability(
      *ntree,
      *sock_p1,
      ELEM(sdf.mode, SHD_SDF_3D_PLANE, SHD_SDF_3D_POINT_CONE, SHD_SDF_3D_POINT_CYLINDER,
           SHD_SDF_2D_LINE, SHD_SDF_2D_UNEVEN_CAPSULE, SHD_SDF_3D_CAPSULE, SHD_SDF_2D_BEZIER,
           SHD_SDF_2D_POINT_TRIANGLE, SHD_SDF_2D_QUAD));
  bke::node_set_socket_availability(
      *ntree,
      *sock_p2,
      ELEM(sdf.mode, SHD_SDF_3D_POINT_CONE, SHD_SDF_3D_POINT_CYLINDER, SHD_SDF_2D_LINE,
           SHD_SDF_2D_UNEVEN_CAPSULE, SHD_SDF_3D_CAPSULE, SHD_SDF_2D_BEZIER,
           SHD_SDF_2D_POINT_TRIANGLE, SHD_SDF_2D_QUAD));
  bke::node_set_socket_availability(
      *ntree, *sock_p3,
      ELEM(sdf.mode, SHD_SDF_2D_BEZIER, SHD_SDF_2D_POINT_TRIANGLE, SHD_SDF_2D_QUAD));
  bke::node_set_socket_availability(*ntree, *sock_p4, ELEM(sdf.mode, SHD_SDF_2D_QUAD));
  bke::node_set_socket_availability(
      *ntree,
      *sock_radius,
      ELEM(sdf.mode, SHD_SDF_2D_CIRCLE, SHD_SDF_2D_ARC, SHD_SDF_2D_CROSS, SHD_SDF_2D_HEXAGON,
           SHD_SDF_2D_PENTAGON, SHD_SDF_2D_HEART, SHD_SDF_2D_PIE, SHD_SDF_2D_ROUNDX,
           SHD_SDF_2D_STAR, SHD_SDF_2D_TRIANGLE, SHD_SDF_2D_UNEVEN_CAPSULE, SHD_SDF_2D_VESICA,
           SHD_SDF_2D_MOON, SHD_SDF_2D_HORSESHOE) ||
          ELEM(sdf.mode, SHD_SDF_3D_CONE, SHD_SDF_3D_DISC, SHD_SDF_3D_CIRCLE,
               SHD_SDF_3D_SOLID_ANGLE, SHD_SDF_3D_TORUS, SHD_SDF_3D_CAPSULE, SHD_SDF_3D_OCTAHEDRON,
               SHD_SDF_3D_CYLINDER, SHD_SDF_3D_POINT_CYLINDER, SHD_SDF_3D_SPHERE,
               SHD_SDF_3D_POINT_CONE));
  bke::node_set_socket_availability(
      *ntree,
      *sock_v1,
      ELEM(sdf.mode, SHD_SDF_3D_PYRAMID, SHD_SDF_3D_PLANE, SHD_SDF_3D_POINT_CONE, SHD_SDF_3D_BOX,
           SHD_SDF_3D_TORUS, SHD_SDF_3D_HEX_PRISM, SHD_SDF_3D_HEX_PRISM_INCIRCLE) ||
          ELEM(sdf.mode, SHD_SDF_2D_FLAT_JOINT, SHD_SDF_2D_ROUND_JOINT, SHD_SDF_2D_MOON,
               SHD_SDF_2D_VESICA, SHD_SDF_2D_PARABOLA, SHD_SDF_2D_PARABOLA_SEGMENT,
               SHD_SDF_2D_ISOSCELES, SHD_SDF_2D_HORSESHOE, SHD_SDF_2D_RECTANGLE, SHD_SDF_2D_RHOMBUS,
               SHD_SDF_2D_TRAPEZOID, SHD_SDF_2D_STAR, SHD_SDF_2D_UNEVEN_CAPSULE,
               SHD_SDF_2D_ELLIPSE));
  bke::node_set_socket_availability(
      *ntree,
      *sock_v2,
      ELEM(sdf.mode, SHD_SDF_2D_PARABOLA_SEGMENT, SHD_SDF_2D_ELLIPSE, SHD_SDF_2D_MOON,
           SHD_SDF_2D_STAR, SHD_SDF_2D_TRAPEZOID, SHD_SDF_2D_RECTANGLE, SHD_SDF_2D_RHOMBUS) ||
          ELEM(sdf.mode, SHD_SDF_3D_BOX));
  bke::node_set_socket_availability(
      *ntree,
      *sock_v3,
      ELEM(sdf.mode, SHD_SDF_2D_ISOSCELES, SHD_SDF_2D_TRAPEZOID, SHD_SDF_3D_CONE,
           SHD_SDF_3D_CYLINDER, SHD_SDF_2D_STAR, SHD_SDF_3D_PYRAMID, SHD_SDF_3D_HEX_PRISM,
           SHD_SDF_3D_HEX_PRISM_INCIRCLE, SHD_SDF_3D_BOX));
  bke::node_set_socket_availability(*ntree, *sock_v4, false);
  bke::node_set_socket_availability(
      *ntree,
      *sock_angle,
      ELEM(sdf.mode, SHD_SDF_3D_SOLID_ANGLE, SHD_SDF_2D_FLAT_JOINT, SHD_SDF_2D_ROUND_JOINT,
           SHD_SDF_2D_PIE, SHD_SDF_2D_ARC, SHD_SDF_2D_HORSESHOE));
}

}  // namespace nodes::node_shader_sdf_primitive_cc

void register_node_type_sh_sdf_primitive()
{
  namespace file_ns = nodes::node_shader_sdf_primitive_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeSdfPrimitive"_ustr, SH_NODE_SDF_PRIMITIVE);
  ntype.ui_name = "SDF Primitive";
  ntype.ui_description = "Generate 2D and 3D signed distance field shapes";
  ntype.enum_name_legacy = "SDF_PRIMITIVE";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_shader_init_sdf_primitive;
  ntype.updatefunc = file_ns::node_shader_update_sdf_primitive;
  ntype.draw_buttons = file_ns::node_shader_buts_sdf_primitive;
  bke::node_type_storage(
      ntype, "NodeSdfPrimitive", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_sdf_primitive;

  bke::node_register_type(ntype);
}

}  // namespace blender
