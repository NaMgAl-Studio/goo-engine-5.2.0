/* SPDX-FileCopyrightText: 2018-2021 Inigo Quilez (MIT)
 * SPDX-FileCopyrightText: 2011-2021 Mercury Demogroup (MIT)
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Scalar SDF operator node entries (ported from Goo Engine). Each entry is a
 * thin [[node]] wrapper over the shared sdf_util helpers. */

#include "gpu_shader_material_sdf_util.glsl"

[[node]] void node_sdf_op_dilate(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = a - v;
}

[[node]] void node_sdf_op_onion(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_onion(a, v, int(n));
}

[[node]] void node_sdf_op_annular(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = abs(a) - v * 0.5f;
}

[[node]] void node_sdf_op_blend(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = mix(a, b, v);
}

[[node]] void node_sdf_op_flatten(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_flatten(v, v2, a);
}

[[node]] void node_sdf_op_pulse(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = cubic_pulse(v, v2, a);
}

[[node]] void node_sdf_op_mask(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  a -= v2;
  if (v == 0.0f) {
    dist = (a > 0.0f) ? 0.0f : 1.0f;
  }
  else {
    dist = map_value(a, -v, 0.0f, 0.0f, 1.0f, 0.0f);
    if (v > 0.0f) {
      dist = 1.0f - dist;
    }
    dist = clamp(dist, 0.0f, 1.0f);
  }
  if (invert > 0.0f) {
    dist = 1.0f - dist;
  }
}

[[node]] void node_sdf_op_invert(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = -a;
}

[[node]] void node_sdf_op_pipe(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_pipe(a, b, v);
}

[[node]] void node_sdf_op_engrave(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_engrave(a, b, v);
}

[[node]] void node_sdf_op_groove(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_groove(a, b, v, v2);
}

[[node]] void node_sdf_op_tongue(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_tongue(a, b, v, v2);
}

[[node]] void node_sdf_op_union(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_union(a, b);
}

[[node]] void node_sdf_op_intersect(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_intersect(a, b);
}

[[node]] void node_sdf_op_diff(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_diff(a, b);
}

[[node]] void node_sdf_op_union_smooth(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_union_smooth(a, b, v);
}

[[node]] void node_sdf_op_intersect_smooth(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_intersect_smooth(a, b, v);
}

[[node]] void node_sdf_op_diff_smooth(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_diff_smooth(a, b, v);
}

[[node]] void node_sdf_op_union_stairs(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_union_stairs(a, b, v, v2);
}

[[node]] void node_sdf_op_intersect_stairs(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_intersect_stairs(a, b, v, v2);
}

[[node]] void node_sdf_op_diff_stairs(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_diff_stairs(a, b, v, v2);
}

[[node]] void node_sdf_op_union_chamfer(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_union_chamfer(a, b, v);
}

[[node]] void node_sdf_op_intersect_chamfer(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_intersect_chamfer(a, b, v);
}

[[node]] void node_sdf_op_diff_chamfer(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_diff_chamfer(a, b, v);
}

[[node]] void node_sdf_op_union_columns(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_union_columns(a, b, v, v2);
}

[[node]] void node_sdf_op_intersect_columns(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_intersect_columns(a, b, v, v2);
}

[[node]] void node_sdf_op_diff_columns(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_diff_columns(a, b, v, v2);
}

[[node]] void node_sdf_op_union_round(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_union_round(a, b, v);
}

[[node]] void node_sdf_op_intersect_round(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_intersect_round(a, b, v);
}

[[node]] void node_sdf_op_diff_round(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_diff_round(a, b, v);
}

[[node]] void node_sdf_op_divide(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_divide(a, b, v, v2);
}

[[node]] void node_sdf_op_exclusion(
    float a, float b, float v, float v2, float n, float invert, float &dist)
{
  dist = sdf_op_exclusion(a, b, v, v2);
}
