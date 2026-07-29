/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Hex Grid Texture, ported from Goo Engine. Helpers are prefixed to avoid
 * collisions in the shared material shader library, and safe_divide is
 * re-supplied (removed from 5.2 common utils). */

#include "gpu_shader_common_hash.glsl"

#define HRATIO 1.1547005f
#define HSQRT3 1.7320508f
#define HSQRT2 1.4142136f

#define HEX_HORIZONTAL 0
#define HEX_VERTICAL 1
#define HEX_HORIZONTAL_TILED 2
#define HEX_VERTICAL_TILED 3

float3 hex_safe_divide(float3 a, float3 b)
{
  return float3((b.x != 0.0f) ? a.x / b.x : 0.0f,
                (b.y != 0.0f) ? a.y / b.y : 0.0f,
                (b.z != 0.0f) ? a.z / b.z : 0.0f);
}

float hex_sdf_dimension(float w, float &round)
{
  float sw = sign(w);
  w = abs(w);
  round = mix(0.0f, w, clamp(round, 0.0f, 1.0f));
  float dim = max(w - round, 0.0f);
  round *= 0.5f;
  return dim * sw;
}

float hex_value_sdf(float3 pos, float r, float rd)
{
  float2 p = float2(pos.x, pos.y);
  r = hex_sdf_dimension(r, rd);
  const float3 k = float3(HSQRT3 * -0.5f, 0.5f, HRATIO * 0.5f);
  p = abs(p);
  p -= 2.0f * min(dot(float2(k.x, k.y), p), 0.0f) * float2(k.x, k.y);
  p -= float2(clamp(p.x, -k.z * r, k.z * r), r);
  return length(p) * sign(p.y) - rd * 2.0f;
}

float hex_value(float3 hp, float radius)
{
  float3 fac = float3(abs(hp.x - hp.y), abs(hp.y - hp.z), abs(hp.z - hp.x));
  float f = max(fac.x, max(fac.y, fac.z));
  return (radius == 0.0f) ? f : mix(f, length(fac) / HSQRT2, radius);
}

float3 hex_xy_to_hex(float3 xy, float ratio)
{
  float3 p = xy;
  p.x *= ratio;
  p.z = -0.5f * p.x - p.y;
  p.y = -0.5f * p.x + p.y;
  return p;
}

float hex_compatible_mod(float a, float b)
{
  return (b != 0.0f && a != b) ? a - b * floor(a / b) : 0.0f;
}

float hexagon(float3 p,
              float scale,
              float size,
              float radius,
              float roundness,
              int coord_mode,
              int value_mode,
              int direction,
              float4 &cell_color,
              float3 &hex_coords,
              float3 &grid_position,
              float3 &cell_coords,
              float3 &cell_id)
{
  float ratio = (direction == HEX_HORIZONTAL_TILED || direction == HEX_VERTICAL_TILED) ? 1.0f :
                                                                                        HRATIO;
  if (direction == HEX_VERTICAL || direction == HEX_VERTICAL_TILED) {
    p = float3(p.y, p.x, p.z);
  }
  p = hex_xy_to_hex(p * scale, ratio);
  hex_coords = p;
  float3 ip = floor(p + 0.5f);
  float s = ip.x + ip.y + ip.z;
  float3 abs_d = float3(0.0f, 0.0f, 0.0f);
  if (s != 0.0f) {
    abs_d = abs(ip - p);
    if (abs_d.x >= abs_d.y && abs_d.x >= abs_d.z) {
      ip.x -= s;
    }
    else if (abs_d.y >= abs_d.x && abs_d.y >= abs_d.z) {
      ip.y -= s;
    }
    else {
      ip.z -= s;
    }
  }

  float3 hp = p - ip;
  hp *= (size != 0.0f) ? 1.0f / size : 0.0f;
  float3 xy_coords = float3(hp.x * HSQRT3, hp.y - hp.z, 0.0f);
  if (coord_mode == 1) {
    cell_coords = hp;
    cell_id = ip;
  }
  else {
    cell_coords = xy_coords;
    cell_id = float3(
        ip.x / ratio, (ip.y - ip.z + (1.0f - hex_compatible_mod(ip.x, 2.0f))) / 2.0f, 0.0f);
  }
  if (direction == HEX_VERTICAL || direction == HEX_VERTICAL_TILED) {
    hp = float3(hp.y, hp.x, hp.z);
    cell_coords = float3(cell_coords.y, cell_coords.x, cell_coords.z);
    cell_id = float3(cell_id.y, cell_id.x, cell_id.z);
  }
  grid_position = hex_safe_divide(cell_id, float3(scale, scale, scale));
  cell_color = float4(hash_vec3_to_vec3(cell_id), 1.0f);
  if (value_mode == 2) {
    return length(hp);
  }
  else if (value_mode == 1) {
    return hex_value_sdf(xy_coords, radius, roundness);
  }
  return hex_value(hp, radius);
}

[[node]] void node_tex_hexagon(float3 co,
                               float scale,
                               float size,
                               float radius,
                               float roundness,
                               float coord_mode,
                               float value_mode,
                               float direction,
                               float &value,
                               float4 &cell_color,
                               float3 &coords,
                               float3 &position,
                               float3 &cell_coords,
                               float3 &cell)
{
  value = hexagon(co,
                  scale,
                  size,
                  radius,
                  roundness,
                  int(coord_mode),
                  int(value_mode),
                  int(direction),
                  cell_color,
                  coords,
                  position,
                  cell_coords,
                  cell);
}
