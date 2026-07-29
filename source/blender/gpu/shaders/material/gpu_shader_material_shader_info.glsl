/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader Info node (ported from Goo Engine, SH_NODE_SHADER_INFO).
 *
 * In the Shader-to-RGB / forward path (which this node forces via GPU_MATFLAG_SHADER_TO_RGBA),
 * `shader_info_eval` evaluates real EEVEE direct diffuse lighting + shadow ratio + light-probe
 * ambient. In other pipelines (or non-fragment stages) the engine light resources are not bound,
 * so it returns neutral defaults. */

[[node]] void node_shader_info(float3 world_position,
                               float3 normal,
                               float4 light_groups_f,
                               float4 light_group_shadows_f,
                               float4 &diffuse_shading,
                               float &cast_shadows,
                               float &self_shadows,
                               float4 &ambient,
                               float &half_lambert)
{
  shader_info_eval(world_position,
                   normal,
                   floatBitsToInt(light_groups_f),
                   floatBitsToInt(light_group_shadows_f),
                   diffuse_shading,
                   cast_shadows,
                   self_shadows,
                   ambient,
                   half_lambert);
}
