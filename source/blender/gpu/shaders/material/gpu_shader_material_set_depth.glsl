/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Set Depth node (ported from Goo Engine, SH_NODE_SET_DEPTH).
 *
 * The closure passes through unchanged; the requested view-space depth is recorded in the
 * `g_set_depth` bridge global. A dedicated non-early-Z surface variant (selected via
 * GPU_MATFLAG_SET_DEPTH) converts it to a screen depth and writes gl_FragDepth. This reproduces
 * Goo's real per-pixel depth offset on EEVEE-Next, whose standard surface passes use
 * `early_fragment_tests` and therefore cannot write gl_FragDepth. */

#include "gpu_shader_material_transform_utils.glsl"

/* Default View Depth = the fragment's own view-space depth (a no-op offset when unlinked). */
[[node]] void view_z_get(out float z)
{
  float3 vP;
  point_transform_world_to_view(g_data.P, vP);
  z = abs(vP.z);
}

[[node]] void node_set_depth(Closure in_shader, float view_depth, Closure &out_shader)
{
  out_shader = in_shader;
#if defined(GPU_FRAGMENT_SHADER)
  g_set_depth = view_depth;
  g_set_depth_written = true;
#endif
}
