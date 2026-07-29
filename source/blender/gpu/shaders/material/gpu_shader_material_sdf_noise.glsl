/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* SDF Noise: Fractal Brownian Motion signed-distance noise, ported from Goo
 * Engine. Self-contained (based on Inigo Quilez fbmsdf). Written in Blender
 * 5.2's shading language. */

/* http://iquilezles.org/www/articles/smin/smin.htm */
float sdf_noise_smin(float a, float b, float k)
{
  float h = max(k - abs(a - b), 0.0f);
  return min(a, b) - h * h * 0.25f / k;
}

float sdf_noise_smax(float a, float b, float k)
{
  float h = max(k - abs(a - b), 0.0f);
  return max(a, b) + h * h * 0.25f / k;
}

/* https://iquilezles.org/www/articles/fbmsdf/fbmsdf.htm */
float sdf_noise_sph(float3 i, float3 f, float3 c)
{
  float3 p = 17.0f * fract((i + c) * 0.3183099f + float3(0.11f, 0.17f, 0.13f));
  float w = fract(p.x * p.y * p.z * (p.x + p.y + p.z));
  float r = 0.7f * w * w;
  return length(f - c) - r;
}

float sdf_noise_base(float3 p)
{
  float3 i = floor(p);
  float3 f = fract(p);
  return min(min(min(sdf_noise_sph(i, f, float3(0.0f, 0.0f, 0.0f)),
                     sdf_noise_sph(i, f, float3(0.0f, 0.0f, 1.0f))),
                 min(sdf_noise_sph(i, f, float3(0.0f, 1.0f, 0.0f)),
                     sdf_noise_sph(i, f, float3(0.0f, 1.0f, 1.0f)))),
             min(min(sdf_noise_sph(i, f, float3(1.0f, 0.0f, 0.0f)),
                     sdf_noise_sph(i, f, float3(1.0f, 0.0f, 1.0f))),
                 min(sdf_noise_sph(i, f, float3(1.0f, 1.0f, 0.0f)),
                     sdf_noise_sph(i, f, float3(1.0f, 1.0f, 1.0f)))));
}

float sdf_noise_fbm(
    float3 p, float detail, float rough, float inflate, float smooth_fac, float d)
{
  float s = 1.0f;
  for (int i = 0; i < min(int(detail), 12); i++) {
    float n = s * sdf_noise_base(p);
    n = sdf_noise_smax(n, d - inflate * s, smooth_fac * s);
    d = sdf_noise_smin(n, d, smooth_fac * s);
    p = float3x3(0.00f, 1.60f, 1.20f, -1.60f, 0.72f, -0.96f, -1.20f, -0.96f, 1.28f) * p;
    s = rough * s;
  }
  return d;
}

[[node]] void node_sdf_noise(float3 pos,
                             float dist_in,
                             float detail,
                             float rough,
                             float inflate_fac,
                             float smooth_fac,
                             float &dist_out)
{
  dist_out = sdf_noise_fbm(pos, detail, rough, inflate_fac, smooth_fac, dist_in);
}
