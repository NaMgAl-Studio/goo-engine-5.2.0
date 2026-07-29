/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Light Info: outputs a selected light's color/power plus a perceptual power
 * (CIE L* based). Ported from Goo Engine. Color/power are baked as uniforms by
 * the node's gpu_fn; this shader only does the perceptual-power math. */

float light_srgb_to_lin(float c)
{
  if (c <= 0.04045f) {
    return c / 12.92f;
  }
  return pow((c + 0.055f) / 1.055f, 2.4f);
}

float light_y_to_lstar(float Y)
{
  if (Y <= (216.0f / 24389.0f)) {
    return Y * (24389.0f / 27.0f);
  }
  return pow(Y, 1.0f / 3.0f) * 116.0f - 16.0f;
}

[[node]] void node_light_info_simple(float4 light_color,
                                     float light_power,
                                     float4 &out_light_color,
                                     float &out_light_power,
                                     float &out_perceptual_power)
{
  out_light_color = light_color;
  out_light_power = light_power;

  float vR = light_srgb_to_lin(light_color.r);
  float vG = light_srgb_to_lin(light_color.g);
  float vB = light_srgb_to_lin(light_color.b);
  float Y = 0.2126f * vR + 0.7152f * vG + 0.0722f * vB;
  float Lstar = light_y_to_lstar(Y);
  out_perceptual_power = ((Lstar * light_power) / 255.0f) * 2.489645f;
}
