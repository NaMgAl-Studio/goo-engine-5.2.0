/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_shadow_tilemap_lib.bsl.hh"
#include "eevee_uniform.bsl.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee {

enum class ShadowIdMode : uint {
  Disabled = 0u,
  IgnoreSelf = 1u,
  OnlySelf = 2u,
};

struct ShadowIdFilter {
  ShadowIdMode mode;
  uint receiver_id;
};

ShadowIdFilter shadow_id_filter_disabled()
{
  return ShadowIdFilter{ShadowIdMode::Disabled, 0u};
}

ShadowIdFilter shadow_id_filter_ignore_self(uint receiver_id)
{
  return ShadowIdFilter{ShadowIdMode::IgnoreSelf, receiver_id};
}

ShadowIdFilter shadow_id_filter_only_self(uint receiver_id)
{
  return ShadowIdFilter{ShadowIdMode::OnlySelf, receiver_id};
}

struct ShadowAtlasAddress {
  int3 texel;
  bool is_valid;
};

/* Any entry point function using this should also use `[[texture_atomic]]`. */
struct ShadowRenderData {
  [[sampler(SHADOW_ATLAS_TEX_SLOT)]] usampler2DArrayAtomic shadow_atlas_tx;
  [[sampler(SHADOW_ATLAS_ID_TEX_SLOT)]] usampler2DArrayAtomic shadow_atlas_id_tx;
  [[sampler(SHADOW_TILEMAPS_TEX_SLOT)]] usampler2D shadow_tilemaps_tx;
  [[storage(SHADOW_ID_DIAGNOSTIC_BUF_SLOT, read_write)]]
  ShadowIdDiagnosticData &shadow_id_diagnostic;

  [[compilation_constant]] bool shadow_random;

  [[resource_table]] srt_t<Uniform> uniforms;
  [[resource_table]] srt_t<draw::View> views;
  [[resource_table, condition(shadow_random)]] srt_t<Sampling> sampling;
  [[resource_table, condition(shadow_random)]] srt_t<UtilityTexture> util_tx;

  /** Resolve one virtual shadow coordinate to a physical atlas texel exactly once. */
  ShadowAtlasAddress atlas_address(ShadowCoordinates coord) const
  {
    ShadowSamplingTile tile = shadow_tile_load(
        shadow_tilemaps_tx, coord.tilemap_tile, coord.tilemap_index);
    if (!tile.is_valid) {
      return ShadowAtlasAddress{int3(0), false};
    }

    /* Using bitwise ops is way faster than integer ops. */
    constexpr uint page_shift = uint(SHADOW_PAGE_LOD);
    constexpr uint page_mask = ~(0xFFFFFFFFu << uint(SHADOW_PAGE_LOD));

    uint2 texel = coord.tilemap_texel;
    /* Shift LOD0 pixels so that they get wrapped at the right position for the given LOD. */
    texel += uint2(tile.lod_offset << SHADOW_PAGE_LOD);
    /* Scale to LOD pixels (merge LOD0 pixels together) then mask to get pixel in page. */
    uint2 texel_page = (texel >> tile.lod) & page_mask;
    texel = (uint2(tile.page.xy) << page_shift) | texel_page;

    return ShadowAtlasAddress{int3(int2(texel), int(tile.page.z)), true};
  }

  float read_depth(ShadowAtlasAddress address) const
  {
    if (!address.is_valid) {
      return -1.0f;
    }
    return uintBitsToFloat(texelFetch(shadow_atlas_tx, address.texel, 0).r);
  }

  float read_depth(ShadowCoordinates coord) const
  {
    return read_depth(atlas_address(coord));
  }

  uint read_caster_id(ShadowAtlasAddress address) const
  {
    if (!address.is_valid) {
      return 0xFFFFFFFFu;
    }
    return texelFetch(shadow_atlas_id_tx, address.texel, 0).r;
  }

  /** Return true when the depth winner must behave as an empty tracing sample. */
  bool shadow_id_sample_filtered(ShadowAtlasAddress address, ShadowIdFilter id_filter)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
    if (id_filter.mode == ShadowIdMode::Disabled || !uni.uniform_buf.shadow.use_shadow_id) {
      return false;
    }

    uint caster_id = read_caster_id(address);
    if (uni.uniform_buf.shadow.use_shadow_id_diagnostics) {
      atomicCompSwap(shadow_id_diagnostic.first_receiver_id, 0xFFFFFFFFu, id_filter.receiver_id);
      if (caster_id == 0xFFFFFFFFu) {
        atomicAdd(shadow_id_diagnostic.sentinel_reads, 1u);
      }
      else {
        atomicAdd(shadow_id_diagnostic.valid_id_reads, 1u);
      }
    }

    bool filtered;
    if (caster_id == 0xFFFFFFFFu) {
      /* Missing identity must never cause IgnoreSelf light leaks. OnlySelf treats it as empty. */
      filtered = id_filter.mode == ShadowIdMode::OnlySelf;
    }
    else if (id_filter.mode == ShadowIdMode::IgnoreSelf) {
      filtered = caster_id == id_filter.receiver_id;
    }
    else {
      filtered = caster_id != id_filter.receiver_id;
    }

    if (filtered && uni.uniform_buf.shadow.use_shadow_id_diagnostics) {
      if (id_filter.mode == ShadowIdMode::IgnoreSelf) {
        atomicAdd(shadow_id_diagnostic.ignore_self_hits, 1u);
      }
      else {
        atomicAdd(shadow_id_diagnostic.only_self_hits, 1u);
      }
    }
    return filtered;
  }

  float punctual_sample_get(LightData light, float3 P) const
  {
    float3 shadow_position = light.local().local.shadow_position;
    float3 lP = transform_point_inversed(light.object_to_world, P);
    lP -= shadow_position;
    int face_id = shadow_punctual_face_index_get(lP);
    lP = shadow_punctual_local_position_to_face_local(face_id, lP);
    ShadowCoordinates coord = shadow_punctual_coordinates(light, lP, face_id);

    float radial_dist = read_depth(coord);
    if (radial_dist == -1.0f) {
      return 1e10f;
    }
    float receiver_dist = length(lP);
    float occluder_dist = radial_dist;
    return receiver_dist - occluder_dist;
  }

  float directional_sample_get(LightData light, float3 P) const
  {
    float3 lP = transform_direction_transposed(light.object_to_world, P);
    ShadowCoordinates coord = shadow_directional_coordinates(light, lP);

    float depth = read_depth(coord);
    if (depth == -1.0f) {
      return 1e10f;
    }
    /* Use increasing distance from the light. */
    float receiver_dist = -lP.z - orderedIntBitsToFloat(light.clip_near);
    float occluder_dist = depth;
    return receiver_dist - occluder_dist;
  }

  float shadow_sample(const bool is_directional, LightData light, float3 P) const
  {
    if (is_directional) {
      return directional_sample_get(light, P);
    }
    return punctual_sample_get(light, P);
  }
};

}  // namespace eevee
