/* SPDX-FileCopyrightText: 2025 Goo Engine Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Curvature node (ported from Goo Engine, SH_NODE_CURVATURE).
 * Calls the EEVEE engine function `curvature_eval` (defined in
 * eevee_nodetree_lib.bsl.hh), which samples the scene depth buffer. Outside a
 * fragment/material context the engine function returns safe defaults (0). */

[[node]] void node_screenspace_curvature(float samples,
                                         float radius,
                                         float thickness,
                                         float3 scale,
                                         float &scene_curvature,
                                         float &scene_rim)
{
  scene_curvature = 0.0f;
  scene_rim = 0.0f;
  curvature_eval(samples, radius, thickness, scale, scene_curvature, scene_rim);
}
