/* SPDX-FileCopyrightText: 2018-2021 Inigo Quilez (MIT)
 * SPDX-FileCopyrightText: 2011-2021 Mercury Demogroup (MIT)
 * SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Vector SDF operator node entries (ported from Goo Engine). Reuses the shared
 * sdf_util helpers. Converted to the 5.2 dialect: out->reference, vec->float,
 * swizzle writes expanded to component writes, safe_divide->sdf_safe_divide. */

#include "gpu_shader_common_hash.glsl"
#include "gpu_shader_material_sdf_util.glsl"

#define SDF_AXIS_XYZ 0
#define SDF_AXIS_XZY 1
#define SDF_AXIS_YXZ 2
#define SDF_AXIS_YZX 3
#define SDF_AXIS_ZXY 4
#define SDF_AXIS_ZYX 5

float3 axis_swizzle(float3 p, float xyz)
{
  int axis = int(xyz);
  if (axis == SDF_AXIS_XYZ) {
    return p;
  }
  else if (axis == SDF_AXIS_XZY) {
    return p.xzy;
  }
  else if (axis == SDF_AXIS_YXZ) {
    return p.yxz;
  }
  else if (axis == SDF_AXIS_YZX) {
    return p.yzx;
  }
  else if (axis == SDF_AXIS_ZXY) {
    return p.zxy;
  }
  else if (axis == SDF_AXIS_ZYX) {
    return p.zyx;
  }
  return p;
}

float3 axis_unswizzle(float3 p, float xyz)
{
  int axis = int(xyz);
  if (axis == SDF_AXIS_XYZ) {
    return p;
  }
  else if (axis == SDF_AXIS_XZY) {
    return p.xzy;
  }
  else if (axis == SDF_AXIS_YXZ) {
    return p.yxz;
  }
  else if (axis == SDF_AXIS_YZX) {
    return p.zxy;
  }
  else if (axis == SDF_AXIS_ZXY) {
    return p.yzx;
  }
  else if (axis == SDF_AXIS_ZYX) {
    return p.zyx;
  }
  return p;
}

float2 uv_tileset(float3 puv, float pos, float padding, float scale, float cx, float cy)
{
  float2 uv = float2(0.0f, 0.0f);
  float stepx = 1.00001f / cx;
  float stepy = 1.00001f / cy;
  float posx = fract(pos);
  float posy = fract(pos * cx);
  float fx = floor(sdf_safe_divide(posx, stepx)) * stepx;
  float fy = floor(sdf_safe_divide(posy, stepy)) * stepy;
  uv.x = fx + puv.x * stepx;
  uv.y = fy + puv.y * stepy;
  float hx = stepx * 0.5f;
  float hy = stepy * 0.5f;
  float cfx = hx + fx;
  float cfy = hy + fy;
  uv.x -= cfx;
  uv.x = sdf_safe_divide(uv.x, scale);
  uv.x += cfx;
  uv.y -= cfy;
  uv.y = sdf_safe_divide(uv.y, scale);
  uv.y += cfy;
  uv.x = clamp(uv.x, fx, fx + stepx);
  uv.y = clamp(uv.y, fy, fy + stepy);
  if (padding > 0.0f) {
    uv = map_value(uv, 0.0f, 1.0f, 0.0f + padding, 1.0f - padding, 0.0f);
  }
  return uv;
}

float2 uv_rotate_90(float2 p, int direction)
{
  float2 v = float2(0.5f, 0.5f);
  float2 uv = p - v;
  uv = (direction > 0) ? float2(uv.y, -uv.x) : float2(-uv.y, uv.x);
  return uv + v;
}

float2 uv_coords_rotate(float2 puv, float rnd)
{
  float2 uv = float2(0.0f, 0.0f);
  if (rnd > 0.75f) {
    uv = puv;
  }
  else if (rnd > 0.5f) {
    uv = uv_rotate_90(puv, 1);
  }
  else if (rnd > 0.25f) {
    uv = float2(1.0f - puv.x, 1.0f - puv.y);
  }
  else {
    uv = uv_rotate_90(puv, 0);
  }
  return uv;
}

float2 uv_coords_flip(float2 puv, float rnd)
{
  float2 uv = float2(0.0f, 0.0f);
  if (rnd > 0.75f) {
    uv = puv;
  }
  else if (rnd > 0.5f) {
    uv = float2(1.0f - puv.x, puv.y);
  }
  else if (rnd > 0.25f) {
    uv = float2(1.0f - puv.x, 1.0f - puv.y);
  }
  else {
    uv = float2(puv.x, 1.0f - puv.y);
  }
  return uv;
}

float2 uv_coords_flip_rotate(float2 puv, float rnd)
{
  float2 uv = float2(0.0f, 0.0f);
  if (rnd > 0.83333f) {
    uv = puv;
  }
  else if (rnd > 0.66667f) {
    uv = uv_rotate_90(puv, 1);
  }
  else if (rnd > 0.50000f) {
    uv = uv_rotate_90(puv, 0);
  }
  else if (rnd > 0.33333f) {
    uv = float2(1.0f - puv.x, puv.y);
  }
  else if (rnd > 0.16667f) {
    uv = float2(1.0f - puv.x, 1.0f - puv.y);
  }
  else {
    uv = float2(puv.x, 1.0f - puv.y);
  }
  return uv;
}

float2 uv_random_coords_flip_rotate(float3 pos, float2 puv)
{
  float rnd = hash_vec3_to_float(pos);
  return uv_coords_flip_rotate(puv, rnd);
}

float2 uv_random_coords_flip(float3 pos, float2 puv)
{
  float rnd = hash_vec3_to_float(pos);
  return uv_coords_flip(puv, rnd);
}

float2 uv_random_coords_rotate(float3 pos, float2 puv)
{
  float rnd = hash_vec3_to_float(pos);
  return uv_coords_rotate(puv, rnd);
}

float2 sdf_op_radial_shear(float2 uv, float2 center, float strength, float2 offset)
{
  float2 pos = uv - center;
  float pos2 = dot(pos, pos);
  float pos_offset = pos2 * strength;
  return float2(pos.y, -pos.x) * pos_offset + offset;
}

float2 sdf_op_swirl(float2 uv, float2 center, float strength, float2 offset)
{
  float2 pos = uv - center;
  float angle = strength * length(pos);
  float x = cos(angle) * pos.x - sin(angle) * pos.y;
  float y = sin(angle) * pos.x + cos(angle) * pos.y;
  return float2(x + center.x + offset.x, y + center.y + offset.y);
}

float3 sdf_op_pinch_inflate(float3 uv, float3 center, float strength, float radius)
{
  uv -= center;
  float dist_len = length(uv);
  if (dist_len < radius) {
    float percent = dist_len / radius;
    if (strength > 0.0f) {
      uv *= mix(1.0f, smoothstep(0.0f, radius / dist_len, percent), strength * 0.75f);
    }
    else {
      uv *= mix(1.0f, pow(percent, 1.0f + strength * 0.75f) * radius / dist_len, 1.0f - percent);
    }
  }
  uv += center;
  return uv;
}

/* Node entries. */

[[node]] void node_sdf_vector_op_extrude(float3 p, float3 p2, float3 p3, float scale, float v,
                                         float v2, float angle, float n, float n2, float axis,
                                         float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  d = sdf_op_extrude(p, p2);
  p = axis_unswizzle(p, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_spin(float3 p, float3 p2, float3 p3, float scale, float v,
                                      float v2, float angle, float n, float n2, float axis,
                                      float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  p = sdf_op_spin(p, v);
  p = axis_unswizzle(p, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_twist(float3 p, float3 p2, float3 p3, float scale, float v,
                                       float v2, float angle, float n, float n2, float axis,
                                       float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  p = sdf_op_twist(p, v, angle);
  p = axis_unswizzle(p, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_bend(float3 p, float3 p2, float3 p3, float scale, float v,
                                      float v2, float angle, float n, float n2, float axis,
                                      float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  p = sdf_op_bend(p, angle);
  p = axis_unswizzle(p, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_repeat_inf_mirror(float3 p, float3 p2, float3 p3, float scale,
                                                   float v, float v2, float angle, float n,
                                                   float n2, float axis, float3 &vout,
                                                   float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p_mod_mirror3(p, p2);
  vout = p;
}

[[node]] void node_sdf_vector_op_repeat_inf(float3 p, float3 p2, float3 p3, float scale, float v,
                                            float v2, float angle, float n, float n2, float axis,
                                            float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  vout = sdf_safe_mod(p + 0.5f * p2, p2) - 0.5f * p2;
}

[[node]] void node_sdf_vector_op_repeat(float3 p, float3 p2, float3 p3, float scale, float v,
                                        float v2, float angle, float n, float n2, float axis,
                                        float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  vout = p - clamp(floor(p / p2 + float3(0.5f, 0.5f, 0.5f)), -p3, p3) * p2;
}

[[node]] void node_sdf_vector_op_rotate(float3 p, float3 p2, float3 p3, float scale, float v,
                                        float v2, float angle, float n, float n2, float axis,
                                        float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  float2 rxy = cos(angle) * float2(p.x, p.y) + sin(angle) * float2(p.y, -p.x);
  p.x = rxy.x;
  p.y = rxy.y;
  p = axis_unswizzle(p, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_reflect(float3 p, float3 p2, float3 p3, float scale, float v,
                                         float v2, float angle, float n, float n2, float axis,
                                         float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = sdf_op_reflect(p, normalize(p2), v);
  vout = p;
}

[[node]] void node_sdf_vector_op_mirror(float3 p, float3 p2, float3 p3, float scale, float v,
                                        float v2, float angle, float n, float n2, float axis,
                                        float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  float2 pxy = float2(p.x, p.y);
  float2 gridc = p_mod_grid2(pxy, float2(p2.x, p2.y));
  p.x = pxy.x;
  p.y = pxy.y;
  pos = float3(gridc.x, gridc.y, 0.0f);
  p = axis_unswizzle(p, axis);
  pos = axis_unswizzle(pos, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_polar(float3 p, float3 p2, float3 p3, float scale, float v,
                                       float v2, float angle, float n, float n2, float axis,
                                       float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  float2 pxy = float2(p.x, p.y);
  d = sdf_op_polar(pxy, v);
  p.x = pxy.x;
  p.y = pxy.y;
  p = axis_unswizzle(p, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_swizzle(float3 p, float3 p2, float3 p3, float scale, float v,
                                         float v2, float angle, float n, float n2, float axis,
                                         float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  vout = axis_swizzle(p, axis);
}

[[node]] void node_sdf_vector_op_grid(float3 p, float3 p2, float3 p3, float scale, float v,
                                      float v2, float angle, float n, float n2, float axis,
                                      float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  p = (p + float3(0.000001f, 0.000001f, 0.000001f)) * 0.999999f;
  p = p * p2;
  float3 pf = floor(p);
  p = p - pf;
  vout = axis_unswizzle(p, axis);
  pos = sdf_safe_divide(pf, p2);
  pos = axis_unswizzle(pos, axis);
}

[[node]] void node_sdf_vector_op_random_uv_rotate(float3 p, float3 p2, float3 p3, float scale,
                                                  float v, float v2, float angle, float n,
                                                  float n2, float axis, float3 &vout, float3 &pos,
                                                  float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  float2 uvr = uv_random_coords_rotate(p2, float2(p.x, p.y));
  vout = float3(uvr.x, uvr.y, 0.0f);
}

[[node]] void node_sdf_vector_op_random_uv_flip(float3 p, float3 p2, float3 p3, float scale,
                                                float v, float v2, float angle, float n, float n2,
                                                float axis, float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  float2 uvr = uv_random_coords_flip_rotate(p2, float2(p.x, p.y));
  vout = float3(uvr.x, uvr.y, 0.0f);
}

[[node]] void node_sdf_vector_op_tileset(float3 p, float3 p2, float3 p3, float scale, float v,
                                         float v2, float angle, float n, float n2, float axis,
                                         float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  float2 uvt = uv_tileset(p, v, v2, scale, n, n2);
  vout = float3(uvt.x, uvt.y, 0.0f);
}

[[node]] void node_sdf_vector_op_octant(float3 p, float3 p2, float3 p3, float scale, float v,
                                        float v2, float angle, float n, float n2, float axis,
                                        float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  float size = v;
  float px = p.x;
  p_mirror(px, size);
  p.x = px;
  float py = p.y;
  p_mirror(py, size);
  p.y = py;
  if (p.y > p.x) {
    p = float3(p.y, p.x, p.z);
  }
  vout = axis_unswizzle(p, axis);
}

[[node]] void node_sdf_vector_op_map_uv(float3 p, float3 p2, float3 p3, float scale, float v,
                                        float v2, float angle, float n, float n2, float axis,
                                        float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  vout = map_value(p, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
}

[[node]] void node_sdf_vector_op_map_11(float3 p, float3 p2, float3 p3, float scale, float v,
                                        float v2, float angle, float n, float n2, float axis,
                                        float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  vout = map_value(p, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f);
}

[[node]] void node_sdf_vector_op_map_05(float3 p, float3 p2, float3 p3, float scale, float v,
                                        float v2, float angle, float n, float n2, float axis,
                                        float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  vout = map_value(p, 0.0f, 1.0f, -0.5f, 0.5f, 0.0f);
}

[[node]] void node_sdf_vector_op_uv_rotate(float3 p, float3 p2, float3 p3, float scale, float v,
                                           float v2, float angle, float n, float n2, float axis,
                                           float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  float2 center = float2(p2.x, p2.y);
  p.x -= center.x;
  p.y -= center.y;
  float2 rot = cos(angle) * float2(p.x, p.y) + sin(angle) * float2(p.y, -p.x);
  p.x = rot.x + center.x;
  p.y = rot.y + center.y;
  vout = float3(p.x, p.y, 0.0f);
}

[[node]] void node_sdf_vector_op_uv_scale(float3 p, float3 p2, float3 p3, float scale, float v,
                                          float v2, float angle, float n, float n2, float axis,
                                          float3 &vout, float3 &pos, float &d)
{
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  float2 center = float2(0.5f, 0.5f);
  p.x -= center.x;
  p.y -= center.y;
  p.x *= scale;
  p.y *= scale;
  p.x += center.x;
  p.y += center.y;
  vout = p;
}

[[node]] void node_sdf_vector_op_swirl(float3 p, float3 p2, float3 p3, float scale, float v,
                                       float v2, float angle, float n, float n2, float axis,
                                       float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  float2 sw = sdf_op_swirl(float2(p.x, p.y), float2(p2.x, p2.y), v, float2(p3.x, p3.y));
  p.x = sw.x;
  p.y = sw.y;
  p = axis_unswizzle(p, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_radial_shear(float3 p, float3 p2, float3 p3, float scale,
                                              float v, float v2, float angle, float n, float n2,
                                              float axis, float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  float2 rs = sdf_op_radial_shear(float2(p.x, p.y), float2(p2.x, p2.y), v, float2(p3.x, p3.y));
  p.x = rs.x;
  p.y = rs.y;
  p = axis_unswizzle(p, axis);
  vout = p;
}

[[node]] void node_sdf_vector_op_pinch_inflate(float3 p, float3 p2, float3 p3, float scale,
                                               float v, float v2, float angle, float n, float n2,
                                               float axis, float3 &vout, float3 &pos, float &d)
{
  vout = p;
  pos = float3(0.0f, 0.0f, 0.0f);
  d = 0.0f;
  p = axis_swizzle(p, axis);
  p = sdf_op_pinch_inflate(p, p2, v, v2);
  p = axis_unswizzle(p, axis);
  vout = p;
}
