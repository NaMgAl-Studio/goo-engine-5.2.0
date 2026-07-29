/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#include "infos/eevee_geom_infos.hh"

#include "draw_model.bsl.hh"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_light_eval.bsl.hh"
#include "eevee_lightprobe.bsl.hh"
#include "eevee_lightprobe_plane.bsl.hh"
#include "eevee_nodetree_closures_lib.glsl"
#include "eevee_ray_trace_screen_lib.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_subsurface_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"

#ifdef GLSL_CPP_STUBS
#  define MAT_REFLECTION
#endif

/* Allow static compilation of forward materials. */
#ifndef CLOSURE_BIN_COUNT
#  define CLOSURE_BIN_COUNT SRT_CONSTANT_light_closure_eval_count
#endif

#ifndef GLSL_CPP_STUBS
#  if CLOSURE_BIN_COUNT != SRT_CONSTANT_light_closure_eval_count && \
      SRT_CONSTANT_light_closure_eval_count != 0
#    error Closure data count and eval count must match
#  endif
#endif

namespace eevee {

void forward_lighting_eval(const ViewMatrices view,
                           uint resource_id,
                           Thickness thickness,
                           float2 frag_co,
                           float3 &radiance,
                           float3 &transmittance)
{
  [[resource_table]] LightEvalIterator &lights = resource_table_get(eevee::LightEvalIterator);
  [[resource_table]] UtilityTexture &util_tx = resource_table_get(UtilityTexture);
  [[resource_table]] const Uniform &uni = resource_table_get(eevee::Uniform);
  /* clang-format off */ /* Multi-line macro breaks error line counting. */
  [[resource_table]] LightprobeRenderData &lightprobes = resource_table_get(eevee::LightprobeRenderData);
  [[resource_table]] LightprobePlaneRenderData &lightprobe_planes = resource_table_get(eevee::LightprobePlaneRenderData);
  /* clang-format on */
  [[resource_table]] LightEvalData &srt = lights.inner;
  [[resource_table]] draw::Infos &infos = resource_table_get(draw::Infos);

  float vPz = dot(view.forward(), g_data.P) - dot(view.forward(), view.position());
  float3 V = view.world_incident_vector(g_data.P);

  light::EvalCtx<false> ctx;
  for (uint i = 0u; i < 3; i++) [[unroll]] {
    if (srt.light_closure_eval_count_reflect > i) [[static_branch]] {
      ClosureUndetermined cl = g_closure_get(uchar(i));
      ctx.stack.cl[i] = closure_light_new(util_tx, cl, V);
    }
  }

  ctx.P = g_data.P;
  ctx.Ng = g_data.Ng;
  ctx.V = V;
  ctx.texel = frag_co;
  ctx.thickness = thickness;

  /* TODO(fclem): If transmission (no SSS) is present, we could reduce LIGHT_CLOSURE_EVAL_COUNT
   * by 1 for this evaluation and skip evaluating the transmission closure twice. */
  ObjectInfos object_infos = infos.get(resource_id);
  ctx.receiver_light_set = receiver_light_set_get(object_infos);
  ctx.terminator_normal_offset = object_infos.shadow_terminator_normal_offset;
  ctx.terminator_geometry_offset = object_infos.shadow_terminator_geometry_offset;

  lights.eval_reflection(ctx, vPz);

  if (srt.light_closure_eval_count_transmit > 0) [[static_branch]] {
    ClosureUndetermined cl_transmit = g_closure_get(0);
    if (closure_has_transmission(cl_transmit.type) || cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID)
    {
      light::EvalCtx<true> ctx_tr = light::init_from_reflect_ctx(ctx);
      ctx_tr.stack.cl[0] = closure_light_new(util_tx, cl_transmit, V, thickness);

      /* NOTE: Only evaluates `stack.cl[0]`. */
      lights.eval_transmission(ctx_tr, vPz);

      if (cl_transmit.type == CLOSURE_BSSRDF_BURLEY_ID) {
#if defined(GLSL_CPP_STUBS) || defined(MAT_SUBSURFACE)
        /* Apply transmission profile onto transmitted light and sum with reflected light. */
        float3 sss_profile = subsurface_transmission(
            util_tx, to_closure_subsurface(cl_transmit).sss_radius, thickness.value());
        ctx.stack.cl[0].light_shadowed += ctx_tr.stack.cl[0].light_shadowed * sss_profile;
        ctx.stack.cl[0].light_unshadowed += ctx_tr.stack.cl[0].light_unshadowed * sss_profile;
#endif
      }
      else {
        ctx.stack.cl[0].light_shadowed = ctx_tr.stack.cl[0].light_shadowed;
        ctx.stack.cl[0].light_unshadowed = ctx_tr.stack.cl[0].light_unshadowed;
      }
    }
  }

  LightProbeSample samp = lightprobes.load(frag_co, g_data.P, g_data.N, V);

  float clamp_indirect_sh = uni.uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics::clamp_energy(samp.volume_irradiance,
                                                             clamp_indirect_sh);

#ifdef MAT_REFLECTION /* Disable if only rough surfaces. */
  /* Planar reflection. */
  float3 planar_probe_radiance = float3(0.0f);
  float3 average_N = g_data.Ng * 0.001f;
  {
    /* Get average normal.  */
    for (uint i = 0u; i < 3; i++) [[unroll]] {
      if (srt.light_closure_eval_count_reflect > i) [[static_branch]] {
        ClosureUndetermined cl = g_closure_get(uchar(i));
        average_N += cl.N * cl.weight;
      }
    }
    average_N = safe_normalize(average_N);

    const int planar_id = lightprobe_planes.select_probe(g_data.P, average_N);

    if (planar_id == -1) {
      average_N = float3(0.0f);
    }
    else {
      float3 P_reflected = lightprobe::plane::parallax(
          lightprobe_planes.probe_planar_buf[planar_id], g_data.P, average_N, V);

      float2 ndc_P_reflected = view.point_world_to_ndc(P_reflected).xy;
      /* Planar probes are rendered upside down. */
      ndc_P_reflected.y = -ndc_P_reflected.y;
      float2 texel = view.ndc_to_screen(ndc_P_reflected);

      planar_probe_radiance =
          textureLod(lightprobe_planes.planar_radiance_tx, float3(texel, planar_id), 0.0).rgb;
      /* Discard background hits. */
      if (textureLod(lightprobe_planes.planar_depth_tx, float3(texel, planar_id), 0.0).r ==
          reverse_z::read(1.0f))
      {
        average_N = float3(0.0f);
      }
    }
  }
#endif

  /* Combine all radiance. */
  float3 radiance_direct = float3(0.0f);
  float3 radiance_indirect = float3(0.0f);

  for (uint i = 0u; i < 3; i++) [[unroll]] {
    if (srt.light_closure_eval_count_reflect > i) [[static_branch]] {
      ClosureUndetermined cl = g_closure_get_resolved(uchar(i), 1.0f);
      if (cl.weight > CLOSURE_WEIGHT_CUTOFF) {
        float3 direct_light = ctx.stack.cl[i].light_shadowed;
        float3 indirect_light = lightprobes.eval(samp, cl, g_data.P, V, thickness);

#ifdef MAT_REFLECTION
        if (cl.type == CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID) {
          const float blend = saturate(to_closure_reflection(cl).roughness * -10.0f + 1.0f) *
                              saturate(dot(average_N, cl.N) * 100.0f - 99.0f);
          indirect_light = mix(indirect_light, planar_probe_radiance, blend);
        }
#endif

        if ((cl.type == CLOSURE_BSDF_TRANSLUCENT_ID ||
             cl.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) &&
            (thickness.value() != 0.0f))
        {
          /* We model two transmission event, so the surface color need to be applied twice. */
          cl.color *= cl.color;
        }

        radiance_direct += direct_light * cl.color;
        radiance_indirect += indirect_light * cl.color;
      }
    }
  }
  /* Light clamping. */
  float clamp_direct = uni.uniform_buf.clamp.surface_direct;
  float clamp_indirect = uni.uniform_buf.clamp.surface_indirect;

  radiance_direct = colorspace::brightness_clamp_max(radiance_direct, clamp_direct);
  radiance_indirect = colorspace::brightness_clamp_max(radiance_indirect, clamp_indirect);

  radiance_direct *= uni.uniform_buf.clamp.direct_scale;
  radiance_indirect *= uni.uniform_buf.clamp.indirect_scale;

  radiance = radiance_direct + radiance_indirect + g_emission;

  transmittance = g_transmittance;
}

/* Goo Engine legacy contact shadows (`light_contact_shadows`): short-range screen-space ray
 * march toward the light, multiplied into the per-light shadow visibility of the Shader Info
 * bridge. Uses the raycast prepass textures (bound for Shader Info materials via
 * GPU_MATFLAG_RAYCAST). Returns 0 when a screen-space occluder is found, 1 otherwise. */
float goo_contact_shadow(LightData light, float3 P, float3 Ng, float3 L)
{
#if defined(MAT_RAYCAST) && defined(GPU_FRAGMENT_SHADER)
  [[resource_table]] const eevee::Uniform &uni = resource_table_get(eevee::Uniform);
  if (!uni.pipeline_buf.can_raycast) {
    return 1.0f;
  }
  /* Same origin offset as Goo (`ray.origin = vP + vNg * sh_contact_offset`). */
  float3 ws_start = P + Ng * light.contact_bias;
  float3 ws_end = ws_start + L * light.contact_dist;
  {
    FRAGMENT_SHADER_CREATE_INFO(draw_view);

    [[resource_table]] const draw::View &views = resource_table_get(draw::View);
    [[resource_table]] const eevee::Sampling &samp = resource_table_get(eevee::Sampling);
    const auto &raycast_depth_tx = sampler_get(eevee_raycast, raycast_depth_tx);

    const ViewMatrices view = views.get(0);
    /* Clip against the near plane in view space to keep the screen projection valid (the culling
     * frustum UBO is not bound in every pipeline, so no `clip_ray` here). Out-of-screen steps are
     * handled by the in-loop bounds check. */
    float3 vs_start = view.point_world_to_view(ws_start);
    float3 vs_end = view.point_world_to_view(ws_end);
    constexpr float z_near_eps = -1e-4f;
    if (vs_start.z > z_near_eps) {
      return 1.0f;
    }
    if (vs_end.z > z_near_eps) {
      float t_clip = (z_near_eps - vs_start.z) / (vs_end.z - vs_start.z);
      vs_end = mix(vs_start, vs_end, t_clip);
    }

    /* Faithful port of Goo's `raytrace()` (raytrace_lib.glsl) with contact parameters:
     * - The ray is marched in screen space, ~one pixel per base step, with a stride that grows by
     *   `trace_quality` (0.1) every step: dense near the contact point, sparse further away. This
     *   is what gives Goo contact shadows their tight roots and soft distance falloff.
     * - The per-light view-space `contact_thickness` is interpolated in screen space through the
     *   endpoint `w` components (depth of `view_z - thickness`), so occluders act as slabs. */
    float3 ss_start = view.point_view_to_screen(vs_start);
    float3 ss_end = view.point_view_to_screen(vs_end);
    float thickness = max(light.contact_thickness, 1e-4f);
    float4 ss_ray_start = float4(ss_start, view.depth_view_to_screen(vs_start.z - thickness));
    float4 ss_ray_end = float4(ss_end, view.depth_view_to_screen(vs_end.z - thickness));

    const float2 extent = float2(textureSize(raycast_depth_tx, 0).xy);
    float4 ss_delta = ss_ray_end - ss_ray_start;
    /* Normalize to advance one pixel per time unit (Goo `raytrace_screenspace_ray_finalize`). */
    float2 px_delta = abs(ss_delta.xy) * extent;
    float pixel_len = max(max(px_delta.x, px_delta.y), 1e-4f);
    float4 ss_step = ss_delta / pixel_len;
    float max_time = pixel_len;
    /* Goo: rays shorter than ~one pixel never hit (`max_time < 1.1`). */
    if (max_time < 1.1f) {
      return 1.0f;
    }

    /* Per-sample jitter; TAA accumulation turns it into a soft edge like Goo's `rand_x`. */
    float noise_offset = samp.rng_1D_get(SAMPLING_RAYTRACE_W);
    float jitter = interleaved_gradient_noise(gl_FragCoord.xy, 1.0f, noise_offset);

    constexpr float trace_quality = 0.1f; /* Goo `light_contact_shadows`. */
    /* Goo allows up to 255 steps; 128 with the growing stride covers ~950px which is enough for
     * close-ups while keeping the bridge loop cheap. */
    constexpr int max_steps = 128;
    float t = 1.001f;
    float time = 1.001f;
    for (int iter = 1; (time < max_time) && (iter < max_steps); iter++) {
      float stride = 1.0f + float(iter) * trace_quality;
      time = min(t + stride * jitter, max_time);
      t += stride;

      float4 ss_p = ss_ray_start + ss_step * time;
      if (ss_p.x < 0.0f || ss_p.x > 1.0f || ss_p.y < 0.0f || ss_p.y > 1.0f) {
        break;
      }
      float depth_sample = reverse_z::read(textureLod(raycast_depth_tx, ss_p.xy, 0.0f).r);
      if (depth_sample >= 1.0f) {
        continue; /* Background. */
      }
      float delta = depth_sample - ss_p.z;
      /* Below the surface but within the thickness slab (or step-sized tolerance). */
      if (delta < 0.0f &&
          (delta > ss_p.z - ss_p.w || abs(delta) < abs(ss_step.z * stride * 2.0f)))
      {
        return 0.0f;
      }
    }
  }
#endif
  return 1.0f;
}

/* Goo Engine `calc_shader_info`: a self-contained forward light loop (independent of the material
 * closures) that reproduces Goo's separable per-light accumulation. Runs in the forward pipeline
 * where the light resources are bound, and stores the result in the g_goo_shader_info bridge global
 * for the Shader Info node to read. Uses the standard LTC diffuse eval so units match a normal
 * render (solid-angle normalized) and shadowed/unshadowed give the cast/self shadow ratio. */
struct GooShaderInfoCtx {
  float3 P;
  float3 N;
  float3 Ng;
  float3 V;
  float2 texel;
  /* Per-object shadow terminator offsets (same as the regular forward lighting); without them
   * shadow_eval self-shadows entire curved surfaces on coarse VSM texels (large scenes). */
  float terminator_normal_offset;
  float terminator_geometry_offset;
  /* Per-light records are written straight into g_goo_shader_info; the context only tracks the
   * running count and the always-on overflow accumulator for lights beyond GOO_MAX_LIGHTS. */
  int count;
  float3 overflow_unshadowed;
  float overflow_reached_lum;
  float overflow_unshadowed_lum;
  float overflow_hl;

  void accumulate([[resource_table]] LightEvalData &srt, LightData light, const bool is_directional)
  {
    /* Goo's exact group rule: a light with all-zero group bits can never match any mask, so it
     * contributes to nothing. This also excludes EEVEE-Next's world-sun placeholder lights
     * (zero-initialized bits), which do not exist in Goo. */
    int4 lbits = light.light_group_bits;
    if ((lbits.x | lbits.y | lbits.z | lbits.w) == 0) {
      return;
    }
    [[resource_table]] ShadowRenderData &srd = srt.shadow_data;
    [[resource_table]] const Uniform &uni = srd.uniforms;
    [[resource_table]] const UtilityTexture &util = srt.utility_tx;
    const auto &util_tx = util.utility_tx;

    LightVector lv = light_vector_get(light, is_directional, P);
    /* Goo's half-lambert is a pure geometric term: every group-passing light contributes
     * `0.5 * dot(L, N) + 0.5`, including back-facing and weak lights. Compute it before any
     * attenuation gating so the dark-side wrap (0..0.5) is preserved. */
    float hl = 0.5f * dot(lv.L, N) + 0.5f;
    float attenuation = light_attenuation_surface(light, is_directional, lv);
    attenuation *= light_attenuation_facing(light, lv.L, lv.dist, N, false);

    float shadow = 1.0f;
    ClosureLight tmp;
    tmp.light_shadowed = float3(0.0f);
    tmp.light_unshadowed = float3(0.0f);
    if (attenuation >= LIGHT_ATTENUATION_THRESHOLD) {
      /* Only lit lights pay for shadow tracing + LTC eval; back-facing lights still get a
       * record (unshadowed = 0) so their half-lambert term is counted like in Goo. */
      if (light.tilemap_index != LIGHT_NO_SHADOW) {
        int ray_count = uni.uniform_buf.shadow.ray_count;
        int ray_step_count = uni.uniform_buf.shadow.step_count;
        /* Goo legacy shadow bias: legacy EEVEE subtracts `la->bias * 0.05` from the occluder
         * distance (5cm at the DNA default of 1.0, which Goo 4.4 does not expose in the UI), so
         * any occluder closer than that along the light direction never shadows. This immunity is
         * what keeps Goo's Cast Shadows clean on curved skin (VSM facet acne / self-shadowing)
         * and lets a clean sun dilute other lights' shadows in the weighted formula. Reproduce it
         * by pushing the sampling point toward the light; clamped for very close punctual lights.
         * Bridge-only: regular pipeline shadows are untouched. */
        float goo_shadow_bias = min(0.05f, lv.dist * 0.5f);
        shadow = shadow_eval(srd, light, is_directional, false, false, texel, Thickness::zero(),
                             P + lv.L * goo_shadow_bias, Ng, N, terminator_normal_offset,
                             terminator_geometry_offset, ray_count, ray_step_count);
      }
      /* Goo legacy contact shadows: screen-space short-range occlusion re-adds the true contact
       * shadows that the legacy 5cm bias (above) exempts. Only for lights with the legacy
       * LA_SHAD_CONTACT flag (contact_dist > 0), like Goo's `light_contact_shadows`. */
      if (light.contact_dist > 0.0f && shadow > 0.0f) {
        shadow *= goo_contact_shadow(light, P, Ng, lv.L);
      }
      LightVertices vertices = light_shape_corners(light, lv);
      tmp.ltc_mat = eevee::lut::ltc::identity();
      tmp.N = N;
      tmp.type = LIGHT_DIFFUSE;
      light::eval_single_closure(util_tx, light, lv, vertices, tmp, V, attenuation, shadow);
    }

    float unshadowed_lum = average(tmp.light_unshadowed);
    /* Store one record per visible light: the UNSHADOWED lighting (Goo's Diffuse Shading carries
     * no shadow factor) plus the scalar shadow visibility for the shadow outputs. Overflow lights
     * past the cap become always-on. */
    if (count < GOO_MAX_LIGHTS) {
      g_goo_shader_info.light_group_bits[count] = light.light_group_bits;
      g_goo_shader_info.light_unshadowed[count] = tmp.light_unshadowed;
      g_goo_shader_info.light_shadow[count] = shadow;
      g_goo_shader_info.light_hl[count] = hl;
      count += 1;
    }
    else {
      overflow_unshadowed += tmp.light_unshadowed;
      overflow_reached_lum += unshadowed_lum * shadow;
      overflow_unshadowed_lum += unshadowed_lum;
      overflow_hl += hl;
    }
  }
  void eval_directional([[resource_table]] LightEvalData &srt, uint /*l_idx*/, LightData light)
  {
    accumulate(srt, light, true);
  }
  void eval_local([[resource_table]] LightEvalData &srt, uint /*l_idx*/, LightData light)
  {
    accumulate(srt, light, false);
  }
};

template void light::foreach<GooShaderInfoCtx, LightEvalData>(
    const LightRenderData &, GooShaderInfoCtx &, LightEvalData &);

void goo_shader_info_compute(const ViewMatrices view, uint resource_id, float2 frag_co)
{
  [[resource_table]] LightEvalIterator &lights = resource_table_get(eevee::LightEvalIterator);
  /* clang-format off */ /* Multi-line macro breaks error line counting. */
  [[resource_table]] LightprobeRenderData &lightprobes = resource_table_get(eevee::LightprobeRenderData);
  /* clang-format on */
  [[resource_table]] LightEvalData &srt = lights.inner;
  [[resource_table]] draw::Infos &infos = resource_table_get(draw::Infos);

  float3 P = g_data.P;
  float3 N = safe_normalize(g_data.N);
  float3 V = view.world_incident_vector(P);

  GooShaderInfoCtx ctx;
  ctx.P = P;
  ctx.N = N;
  ctx.Ng = g_data.Ng;
  ctx.V = V;
  ctx.texel = frag_co;
  ObjectInfos object_infos = infos.get(resource_id);
  ctx.terminator_normal_offset = object_infos.shadow_terminator_normal_offset;
  ctx.terminator_geometry_offset = object_infos.shadow_terminator_geometry_offset;
  ctx.count = 0;
  ctx.overflow_unshadowed = float3(0.0f);
  ctx.overflow_reached_lum = 0.0f;
  ctx.overflow_unshadowed_lum = 0.0f;
  ctx.overflow_hl = 0.0f;

  /* Goo iterates every scene light regardless of power or screen tile (its half-lambert is
   * power-independent), so use the un-culled iterator like the surfel pipeline. */
  light::foreach(lights.light_data, ctx, srt);

  LightProbeSample samp = lightprobes.load(frag_co, P, N, V);

  g_goo_shader_info.light_count = ctx.count;
  g_goo_shader_info.overflow_unshadowed = ctx.overflow_unshadowed;
  g_goo_shader_info.overflow_reached_lum = ctx.overflow_reached_lum;
  g_goo_shader_info.overflow_unshadowed_lum = ctx.overflow_unshadowed_lum;
  g_goo_shader_info.overflow_hl = ctx.overflow_hl;
  g_goo_shader_info.ambient = max(samp.volume_irradiance.evaluate_lambert(N).rgb, float3(0.0f));
  g_goo_shader_info.valid = true;
}

}  // namespace eevee
