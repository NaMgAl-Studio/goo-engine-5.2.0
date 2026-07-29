/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 *
 * OKLab Color Ramp node (ported from Goo Engine, SH_NODE_OKLAB_COLOR_RAMP = 907).
 * Same as the stock Color Ramp, but the gradient is interpolated in the OKLab
 * perceptual color space. The OKLab interpolation is baked into the color-band
 * texture on the CPU, so the GPU side reuses the stock "valtorgb" shader and the
 * RNA side reuses "def_colorramp" (ColorBand storage).
 */

#include <cmath>

#include "MEM_guardedalloc.h"

#include "DNA_texture_types.h"

#include "BKE_colorband.hh"

#include "node_shader_util.hh"
#include "node_util.hh"

namespace blender {

namespace nodes::node_shader_oklab_color_ramp_cc {

/* --- OKLab color-space helpers (operate per RGB channel). --- */

static void linear_srgb_to_oklab(const float c[3], float out[3])
{
  float l = 0.4122214708f * c[0] + 0.5363325363f * c[1] + 0.0514459929f * c[2];
  float m = 0.2119034982f * c[0] + 0.6806995451f * c[1] + 0.1073969566f * c[2];
  float s = 0.0883024619f * c[0] + 0.2817188376f * c[1] + 0.6299787005f * c[2];
  float l_ = cbrtf(l);
  float m_ = cbrtf(m);
  float s_ = cbrtf(s);
  out[0] = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
  out[1] = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
  out[2] = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
}

static void oklab_to_linear_srgb(const float c[3], float out[3])
{
  float l_ = c[0] + 0.3963377774f * c[1] + 0.2158037573f * c[2];
  float m_ = c[0] - 0.1055613458f * c[1] - 0.0638541728f * c[2];
  float s_ = c[0] - 0.0894841775f * c[1] - 1.2914855480f * c[2];
  float l = l_ * l_ * l_;
  float m = m_ * m_ * m_;
  float s = s_ * s_ * s_;
  out[0] = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
  out[1] = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
  out[2] = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}

static float clamp01(float x)
{
  return (x < 0.0f) ? 0.0f : (x > 1.0f ? 1.0f : x);
}

/* Evaluate the color band at `in` interpolating neighbouring stops in OKLab. */
static void oklab_colorband_evaluate(const ColorBand *coba, float in, float out[4])
{
  in = clamp01(in);
  if (coba->tot == 0) {
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    return;
  }
  if (coba->tot == 1) {
    out[0] = coba->data[0].r;
    out[1] = coba->data[0].g;
    out[2] = coba->data[0].b;
    out[3] = coba->data[0].a;
    return;
  }

  int left = 0, right = 0;
  for (int i = 0; i < coba->tot; i++) {
    if (coba->data[i].pos <= in) {
      left = i;
    }
    else {
      right = i;
      break;
    }
  }

  if (left == right) {
    const CBData &cbd = (in <= coba->data[0].pos) ? coba->data[0] : coba->data[coba->tot - 1];
    out[0] = cbd.r;
    out[1] = cbd.g;
    out[2] = cbd.b;
    out[3] = cbd.a;
    return;
  }

  const CBData &l = coba->data[left];
  const CBData &r = coba->data[right];
  float f = clamp01((in - l.pos) / (r.pos - l.pos));
  /* Match Goo's GPU render path (oklab_valtorgb_opti_ease -> oklab_mix), not Goo's CPU
   * multi_function: smoothstep easing, color stops treated as already-linear, and a linear
   * (non-sRGB-encoded) result. This is what Goo actually renders with. */
  f = f * f * (3.0f - 2.0f * f);

  const float l_lin[3] = {l.r, l.g, l.b};
  const float r_lin[3] = {r.r, r.g, r.b};
  float l_ok[3], r_ok[3], mix_ok[3], mix_lin[3];
  linear_srgb_to_oklab(l_lin, l_ok);
  linear_srgb_to_oklab(r_lin, r_ok);
  for (int k = 0; k < 3; k++) {
    mix_ok[k] = l_ok[k] + (r_ok[k] - l_ok[k]) * f;
  }
  oklab_to_linear_srgb(mix_ok, mix_lin);
  /* Gamut map (scale down if any channel > 1) + clamp negatives, matching Goo's
   * oklab_to_linear_srgb GLSL. Output stays LINEAR (Blender's shader color space). */
  float max_c = mix_lin[0];
  if (mix_lin[1] > max_c) {
    max_c = mix_lin[1];
  }
  if (mix_lin[2] > max_c) {
    max_c = mix_lin[2];
  }
  if (max_c > 1.0f) {
    mix_lin[0] /= max_c;
    mix_lin[1] /= max_c;
    mix_lin[2] /= max_c;
  }
  out[0] = (mix_lin[0] < 0.0f) ? 0.0f : mix_lin[0];
  out[1] = (mix_lin[1] < 0.0f) ? 0.0f : mix_lin[1];
  out[2] = (mix_lin[2] < 0.0f) ? 0.0f : mix_lin[2];
  out[3] = clamp01(l.a + (r.a - l.a) * f);
}

static void sh_node_oklab_valtorgb_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac"_ustr)
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("The value used to map onto the OKLab color gradient");
  b.add_output<decl::Color>("Color"_ustr);
  b.add_output<decl::Float>("Alpha"_ustr);
}

static void node_shader_init_oklab_valtorgb(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_colorband_add(true);
}

static int gpu_shader_oklab_valtorgb(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  ColorBand *coba = static_cast<ColorBand *>(node->storage);

  /* Allocate the standard color-band table (MEM-owned, freed by GPU_color_band),
   * then overwrite it with OKLab-interpolated colors. */
  float *array;
  int size;
  BKE_colorband_evaluate_table_rgba(coba, &array, &size);
  for (int i = 0; i < size; i++) {
    const float pos = (size > 1) ? float(i) / float(size - 1) : 0.0f;
    oklab_colorband_evaluate(coba, pos, &array[i * 4]);
  }

  float layer;
  GPUNodeLink *tex = GPU_color_band(mat, size, array, &layer);
  return GPU_stack_link(mat, node, "valtorgb", in, out, tex, GPU_constant(&layer));
}

}  // namespace nodes::node_shader_oklab_color_ramp_cc

void register_node_type_sh_oklab_color_ramp()
{
  namespace file_ns = nodes::node_shader_oklab_color_ramp_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeOKLabColorRamp"_ustr, SH_NODE_OKLAB_COLOR_RAMP);
  ntype.ui_name = "OKLab Color Ramp";
  ntype.ui_description = "Map values to colors, interpolating in the OKLab perceptual color space";
  ntype.enum_name_legacy = "OKLAB_COLOR_RAMP";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::sh_node_oklab_valtorgb_declare;
  ntype.initfunc = file_ns::node_shader_init_oklab_valtorgb;
  ntype.default_width = bke::NodeWidth::_240;
  bke::node_type_storage(
      ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_oklab_valtorgb;

  bke::node_register_type(ntype);
}

}  // namespace blender
