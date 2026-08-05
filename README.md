<!--
Keep this document short & concise,
linking to external resources instead of including content in-line.
See 'release/text/readme.html' for the end user read-me.
-->

Blender
=======

Blender is the free and open source 3D creation suite.
It supports the entirety of the 3D pipeline—modeling, rigging, animation, simulation, rendering, compositing,
motion tracking and video editing.

![Blender screenshot](https://code.blender.org/wp-content/uploads/2018/12/springrg.jpg "Blender screenshot")

Project Pages
-------------

- [Main Website](https://www.blender.org)
- [Reference Manual](https://docs.blender.org/manual/en/latest/index.html)
- [User Community](https://www.blender.org/community/)

Development
-----------

- [Build Instructions](https://developer.blender.org/docs/handbook/building_blender/)
- [Code Review & Bug Tracker](https://projects.blender.org)
- [Developer Forum](https://devtalk.blender.org)
- [Developer Documentation](https://developer.blender.org/docs/)


License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different but compatible license.

See [blender.org/about/license](https://www.blender.org/about/license) for details.


Goo Engine 5.2 Port
===================

This repository is an **unofficial port of Goo Engine to Blender 5.2 and EEVEE-Next**.

Goo Engine is an NPR (non-photorealistic / toon rendering) focused fork of Blender
created and maintained by **DillonGoo Studios**. The original project is based on
Blender 4.4.x and the legacy EEVEE render engine:

- Original repository: https://github.com/dillongoostudios/goo-engine
- Original authors: DillonGoo Studios and the Goo Engine contributors
- Studio website: https://www.dillongoo.com

This port takes the Goo Engine v4.4-release feature set and re-implements it on top of
official Blender 5.2.0 (EEVEE-Next render engine, BSL shader pipeline). This repository
is **not affiliated with the Blender Foundation or DillonGoo Studios**. All code remains
under the GNU General Public License.

What Was Ported and How
-----------------------

- **13 Goo shader nodes**, with their GLSL re-implemented for the EEVEE-Next BSL pipeline:
  Shader Info, Screenspace Info, SDF Primitive, SDF Op, SDF Vector Op, SDF Noise,
  Set Depth, Curvature, Light Info, Hexagon Texture, Twirl, Water Ripples,
  and OKLab Color Ramp.
- **Set Depth** rewritten for the reverse-Z depth convention of EEVEE-Next.
- **Screenspace Info**: Scene Depth samples the EEVEE-Next depth buffer (hiz_tx); Scene
  Color samples the previous-layer radiance texture for transparent Shader-to-RGB
  materials (the same source screen-space refraction reads) — matching Goo, where Scene
  Color likewise only has values behind transparent layers.
- **Curvature** ported with the original 8-direction sampling algorithm, and screen-space
  depth sampling aligned with Goo's output.
- **OKLab Color Ramp** aligned with Goo's render path (easing behavior and linear output).
- **Light Groups** management UI (`scripts/startup/goo_engine_light_groups.py`) together
  with the Light DNA/RNA extensions; shader Add menu regrouped into Goo categories.
- **Legacy material semantics** restored through file versioning:
  - `MA_LEGACY_OPAQUE` (legacy `blend_method == Opaque`) is tagged at load time, gated by
    "material node tree uses a Goo node OR file version < 4.2" (file subversion 502.47,
    with clear-and-recompute). This keeps Goo scenes force-opaque where intended while
    not breaking transparent materials in files saved by official Blender (whose legacy
    DNA fields are zero-filled and would otherwise be misread).
  - Legacy **Shadow Mode = None** (`blend_shadow`) is honored at render time so shadow
    proxy workflows from Goo scenes keep working.
  - Files older than Blender 2.80 (whose SDNA lacks `Material::blend_shadow`) are
    backfilled to the solid default via an SDNA member-existence check, preventing
    the zero-filled value from silently disabling all shadow casting.
- **Highlight (HL) fix**: purely geometric highlights were being culled by the light
  attenuation gate and light culling; fixed with a bridged gate.
- **Forward pipeline light cap** raised to 512 visible lights per fragment.
- **Contact shadows** bridged to EEVEE-Next equivalents; legacy **Bloom migrated to Glare**
  through versioning; a file browser crash fixed.
- **Build configuration**: `WITH_INTERNATIONAL`, `WITH_INPUT_IME` and `WITH_BULLET`
  enabled (the lite profile shipped with these off, which broke language switching,
  IME input and rigid body physics).

Known Boundaries and Their Handling
-----------------------------------

- **Visible light count per fragment**: 128/256/512 verified working; 1024 fails on the
  tested hardware. The cap is set to 512.
- **Light resource access in material nodes**: EEVEE-Next does not expose light data to
  material shaders in every pass; the affected Goo nodes degrade gracefully per pass.
- **BSL limitation**: conditional attributes/parameters (`#if`) are not supported inside
  fragment signatures by the shader tooling; code is organized around unconditional
  signatures.
- **"Check Self Shadowing" (`check_shadow_id`) not implemented yet**: the flag is stored
  and round-trips, but EEVEE-Next does not consume it. Self-shadow suppression relies on
  the legacy 5cm shadow bias approximation in the Shader Info bridge, which covers most
  stylized self-shadow cases. An identity-based filter (shadow-ID side-channel in the
  shadow atlas) was prototyped but did not reach the runtime material shaders as
  expected and was reverted; it remains under investigation.
- **`blend_shadow` zero-value ambiguity (residual)**: a full fix via a dedicated
  `MA_LEGACY_NO_SHADOW` flag (file-level Goo fingerprint at load, flag read at render)
  has been designed but not implemented. Known corner case: appending a shadow-None
  material that itself contains no Goo nodes into a scene without any Goo nodes will
  make it cast shadows.
- **RNA re-entrancy deadlock**: `property_pointer_get` (rna_access.cc) can self-deadlock
  on a non-recursive mutex when an ID-type IDProperty group is overwritten with a flat
  value and later resolved. The engine defect is documented; callers must write geometry
  nodes modifier inputs as `inputs[identifier]["value"] = value` (never overwrite the
  `{value, type}` group), which avoids the code path entirely.
- **File subversions 502.45-48** are used by this fork. If a future official 5.2.x LTS
  release uses the same subversion numbers for its own versioning, files saved by this
  fork could skip those official versioning blocks.

Disclaimer
----------

This is an experimental, unofficial build provided under the GPL without any warranty.
For production NPR work on Blender 4.4, use the original Goo Engine release from
DillonGoo Studios.
