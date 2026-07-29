/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Water Ripples: procedural water surface distortion, ported from Goo Engine.
 * Four modes (Drops/Ripples/Flow/Caustic). Self-contained. Written in Blender
 * 5.2's shading language ([[node]], float types, reference out-params). */

float water_bias_function(float x, float b)
{
  float min_b = 1.0f / 100.0f;
  float max_b = 99.0f / 100.0f;
  float safe_b = clamp(b, min_b, max_b);
  return x / ((1.0f / safe_b - 2.0f) * (1.0f - x) + 1.0f);
}

float water_hash12(float2 p, float hash_scale)
{
  float3 p3 = fract(float3(p.x, p.y, p.x) * hash_scale);
  float3 add_val = float3(p3.y, p3.z, p3.x) + 19.19f;
  p3 += dot(p3, add_val);
  return fract((p3.x + p3.y) * p3.z);
}

float2 water_hash22(float2 p, float3 hash_scale)
{
  float3 p3 = fract(float3(p.x, p.y, p.x) * hash_scale);
  float3 add_val = float3(p3.y, p3.z, p3.x) + 19.19f;
  p3 += dot(p3, add_val);
  return fract((float2(p3.x, p3.x) + float2(p3.y, p3.z)) * float2(p3.z, p3.y));
}

[[node]] void node_water_ripples(float3 vector,
                                 float time,
                                 float mode,
                                 float scale,
                                 float intensity,
                                 float speed,
                                 float detail,
                                 float bias_amount,
                                 float3 &distorted_vector,
                                 float &mask)
{
  float2 uv = float2(vector.x, vector.y) * max(scale, 1.0f / 1000.0f);
  float h = 0.0f;
  int imode = int(mode + 0.5f);

  if (imode == 0) {
    /* DROPS MODE */
    int divisions = int(detail * 8.0f + 2.0f);
    divisions = clamp(divisions, 2, 8);
    for (int iy = 0; iy < 8; iy++) {
      if (iy >= divisions) {
        break;
      }
      for (int ix = 0; ix < 16; ix++) {
        if (ix >= divisions * 2) {
          break;
        }
        float2 noise_coord = float2(float(ix), float(iy)) / float(divisions);
        float hash_scale = 1031.0f / 10000.0f;
        float4 t = float4(water_hash12(noise_coord, hash_scale),
                          water_hash12(noise_coord + float2(1.0f / 10.0f, 0.0f), hash_scale),
                          water_hash12(noise_coord + float2(0.0f, 1.0f / 10.0f), hash_scale),
                          water_hash12(noise_coord + float2(1.0f / 10.0f, 1.0f / 10.0f), hash_scale));
        float div_minus_one = max(float(divisions - 1), 1.0f);
        float2 p = float2(float(ix), float(iy)) * (1.0f / div_minus_one);
        float pos_offset = 75.0f / 100.0f;
        p += (pos_offset / div_minus_one) * (float2(t.x, t.y) * 2.0f - 1.0f);
        float2 v = uv - p;
        float d = pow(max(dot(v, v), 1.0f / 1000.0f), 7.0f / 10.0f);
        float n = time * speed * 5.0f * (t.w + 2.0f / 10.0f) - t.z * 6.0f;
        n *= 1.0f / 10.0f + t.w;
        n = mod(n, 10.0f + t.z * 3.0f + 10.0f);
        n = max(n, 1.0f / 1000.0f);
        float x = d * 99.0f;
        float two_pi_n = 2.0f * M_PI * n;
        float T = (x < two_pi_n) ? 1.0f : 0.0f;
        float e = max(1.0f - (n / 10.0f), 0.0f);
        float F = e * x / max(two_pi_n, 1.0f / 1000.0f);
        float s = sin(x - two_pi_n - M_PI * 5.0f / 10.0f);
        s = s * 5.0f / 10.0f + 5.0f / 10.0f;
        s = water_bias_function(s, bias_amount);
        s = (F * s) / (x + 11.0f / 10.0f) * T;
        h += s * 100.0f * (5.0f / 10.0f + t.w) * intensity;
      }
    }
    float eps = 1.0f / 1000.0f;
    float h_x = 0.0f;
    float2 uv_x = (float2(vector.x, vector.y) + float2(eps, 0.0f)) * max(scale, 1.0f / 1000.0f);
    for (int iy = 0; iy < 8; iy++) {
      if (iy >= divisions) {
        break;
      }
      for (int ix = 0; ix < 16; ix++) {
        if (ix >= divisions * 2) {
          break;
        }
        float2 noise_coord = float2(float(ix), float(iy)) / float(divisions);
        float hash_scale = 1031.0f / 10000.0f;
        float4 t = float4(water_hash12(noise_coord, hash_scale),
                          water_hash12(noise_coord + float2(1.0f / 10.0f, 0.0f), hash_scale),
                          water_hash12(noise_coord + float2(0.0f, 1.0f / 10.0f), hash_scale),
                          water_hash12(noise_coord + float2(1.0f / 10.0f, 1.0f / 10.0f), hash_scale));
        float div_minus_one = max(float(divisions - 1), 1.0f);
        float2 p = float2(float(ix), float(iy)) * (1.0f / div_minus_one);
        p += (75.0f / 100.0f / div_minus_one) * (float2(t.x, t.y) * 2.0f - 1.0f);
        float2 v_x = uv_x - p;
        float d_x = pow(max(dot(v_x, v_x), 1.0f / 1000.0f), 7.0f / 10.0f);
        float n = time * speed * 5.0f * (t.w + 2.0f / 10.0f) - t.z * 6.0f;
        n *= 1.0f / 10.0f + t.w;
        n = mod(n, 10.0f + t.z * 3.0f + 10.0f);
        n = max(n, 1.0f / 1000.0f);
        float x = d_x * 99.0f;
        float two_pi_n = 2.0f * M_PI * n;
        float T = (x < two_pi_n) ? 1.0f : 0.0f;
        float e = max(1.0f - (n / 10.0f), 0.0f);
        float F = e * x / max(two_pi_n, 1.0f / 1000.0f);
        float s = sin(x - two_pi_n - M_PI * 5.0f / 10.0f);
        s = s * 5.0f / 10.0f + 5.0f / 10.0f;
        s = water_bias_function(s, bias_amount);
        s = (F * s) / (x + 11.0f / 10.0f) * T;
        h_x += s * 100.0f * (5.0f / 10.0f + t.w) * intensity;
      }
    }
    float2 gradient = float2((h_x - h) / eps, 0.0f);
    distorted_vector = vector + float3(gradient * intensity * 0.01f, 0.0f);
    mask = clamp(h * (1.0f / 100.0f), 0.0f, 1.0f);
  }
  else if (imode == 1) {
    /* RIPPLES MODE */
    int max_radius = clamp(int(detail * 3.0f + 1.0f), 1, 3);
    float2 p0 = floor(uv);
    float2 circles = float2(0.0f, 0.0f);
    for (int j = -3; j <= 3; j++) {
      if (abs(j) > max_radius) {
        continue;
      }
      for (int i = -3; i <= 3; i++) {
        if (abs(i) > max_radius) {
          continue;
        }
        float2 pcell = p0 + float2(float(i), float(j));
        float2 p = pcell + water_hash22(pcell, float3(1031.0f / 10000.0f, 1030.0f / 10000.0f, 973.0f / 10000.0f));
        float t = fract(speed * time + water_hash12(pcell, 1031.0f / 10000.0f));
        float2 v = p - uv;
        float d = length(v) - (float(max_radius) + 1.0f) * t;
        float h_val = 1.0f / 1000.0f;
        float d1 = d - h_val;
        float d2 = d + h_val;
        float smooth1 = -6.0f / 10.0f;
        float smooth2 = -3.0f / 10.0f;
        float p1 = sin(31.0f * d1) * smoothstep(smooth1, smooth2, d1) * smoothstep(0.0f, smooth2, d1);
        float p2 = sin(31.0f * d2) * smoothstep(smooth1, smooth2, d2) * smoothstep(0.0f, smooth2, d2);
        float ripple_fade = (1.0f - t) * (1.0f - t);
        float2 norm_v = float2(0.0f, 1.0f);
        float v_length = length(v);
        if (v_length > 1.0f / 1000.0f) {
          norm_v = v / v_length;
        }
        circles += norm_v * ((p2 - p1) / (2.0f * h_val) * ripple_fade);
      }
    }
    circles *= intensity;
    h = length(circles);
    distorted_vector = vector + float3(circles * intensity * 0.1f, 0.0f);
    mask = h;
  }
  else if (imode == 2) {
    /* FLOW MODE */
    float2 flow_uv = uv;
    float time_scaled = time * speed;
    float wave1 = sin(flow_uv.x * detail * 10.0f + time_scaled * 2.0f) * 0.1f;
    float wave2 = cos(flow_uv.y * detail * 8.0f + time_scaled * 1.5f) * 0.15f;
    float2 flow_offset = float2(wave1 + sin(flow_uv.y * detail * 5.0f + time_scaled) * 0.05f,
                                wave2 + cos(flow_uv.x * detail * 6.0f + time_scaled * 0.8f) * 0.08f);
    flow_offset *= intensity * 0.1f;
    distorted_vector = vector + float3(flow_offset, 0.0f);
    mask = clamp(length(flow_offset) * 10.0f, 0.0f, 1.0f);
  }
  else if (imode == 3) {
    /* CAUSTIC MODE */
    float2 p = uv * scale;
    float a = 1.0f;
    float scale_safe = max(bias_amount, 1e-3f);
    float3 k = float3(p.x / scale_safe * detail, p.y / scale_safe * detail, sin(time * 0.2f));
    float3x3 m = float3x3(-2.0f, -1.0f, 2.0f, 3.0f, -2.0f, 1.0f, 1.0f, 2.0f, 2.0f) * 0.3f;
    k = m * k;
    a = min(a, length(float3(0.5f, 0.5f, 0.5f) - fract(k)));
    k = m * k;
    a = min(a, length(float3(0.5f, 0.5f, 0.5f) - fract(k)));
    k = m * k;
    a = min(a, length(float3(0.5f, 0.5f, 0.5f) - fract(k)));
    float caustic = pow(a, detail) * 25.0f * intensity;
    distorted_vector = vector;
    mask = clamp(caustic, 0.0f, 1.0f);
  }
  else {
    /* Fallback: passthrough */
    distorted_vector = vector;
    mask = 0.0f;
  }
}
