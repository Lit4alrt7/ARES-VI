"""
MarsSurvival project bootstrap (run inside Unreal Editor).

Creates:
  /Game/MarsSurvival/Materials/M_PS1_PostProcess   - pixelation, Bayer dither, palette quantisation,
                                                     CRT scanlines/aperture mask, chromatic aberration,
                                                     hallucination warp, Cherenkov flash, hypoxia grey-out
  /Game/MarsSurvival/Materials/M_PS1_Surface       - vertex snapping ("jitter") + affine texture warping
  /Game/MarsSurvival/Materials/MI_HullPanel, MI_MarsRegolith, MI_MarsRock
  /Game/MarsSurvival/Materials/M_Phantom           - translucent hallucination silhouette
  /Game/MarsSurvival/Maps/AresStation              - six-module base on procedural terrain + lander crash site

Run:  Tools > Execute Python Script... > this file
  or  UnrealEditor-Cmd.exe MarsSurvival.uproject -run=pythonscript -script="<path>/setup_mars_project.py"
Re-running is safe: existing assets are rebuilt in place, the map is recreated.
"""

import math
import random
import unreal

MAT_DIR = "/Game/MarsSurvival/Materials"
MAP_PATH = "/Game/MarsSurvival/Maps/AresStation"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[MarsSetup] " + msg)


def enum_value(enum_type, *names):
    """Return the first enum member that exists (enum names shift between engine versions)."""
    for name in names:
        if hasattr(enum_type, name):
            return getattr(enum_type, name)
    raise AttributeError("None of %s found on %s" % (names, enum_type))


# ---------------------------------------------------------------------------
# Material helpers
# ---------------------------------------------------------------------------

def fresh_material(name):
    path = "%s/%s" % (MAT_DIR, name)
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
        return mat
    return asset_tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())


def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def scalar(mat, name, default, x, y):
    e = expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", default)
    return e


def vector(mat, name, rgba, x, y):
    e = expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*rgba))
    return e


def custom(mat, desc, code, out_type, input_names, x, y):
    e = expr(mat, unreal.MaterialExpressionCustom, x, y)
    e.set_editor_property("description", desc)
    e.set_editor_property("code", code)
    e.set_editor_property("output_type", out_type)
    inputs = []
    for n in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    e.set_editor_property("inputs", inputs)
    return e


def link(src, src_out, dst, dst_in):
    if not mel.connect_material_expressions(src, src_out, dst, dst_in):
        unreal.log_warning("[MarsSetup] failed link %s.%s -> %s.%s" % (src.get_name(), src_out, dst.get_name(), dst_in))


def finish(mat):
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)


CMOT = unreal.CustomMaterialOutputType
F1 = enum_value(CMOT, "CMOT_FLOAT1")
F2 = enum_value(CMOT, "CMOT_FLOAT2")
F3 = enum_value(CMOT, "CMOT_FLOAT3")


# ---------------------------------------------------------------------------
# 1. PS1 post-process
# ---------------------------------------------------------------------------

HLSL_QUANT_UV = r"""
float2 uv = UV;
if (Enabled < 0.5) return uv;
// Mild CRT barrel curvature
float2 c = uv - 0.5;
uv = 0.5 + c * (1.0 + 0.05 * dot(c, c));
// Hallucination: breathing warp + slow lateral drift
float h = Hallucination;
uv += h * 0.006 * float2(sin(uv.y * 23.0 + Time * 1.3), cos(uv.x * 19.0 + Time * 1.1));
uv.x += h * h * 0.008 * sin(Time * 0.7 + uv.y * 4.0);
// Stage 3 nausea: slow, large rolling waves
uv += Nausea * 0.012 * float2(sin(Time * 0.9 + uv.y * 3.0), sin(Time * 0.7 + uv.x * 2.5));
// Stage 4 violent distortion: horizontal tearing bands that jitter every frame
float band = floor(uv.y * 24.0 + floor(Time * 12.0));
float tear = frac(sin(band * 91.3) * 437.1);
uv.x += Distortion * (tear > 0.75 ? (tear - 0.75) * 0.3 : 0.0) * sin(Time * 40.0);
// Snap to the low-resolution PS1 pixel grid (320x240-ish at PixelSize 3-4)
float2 lowRes = max(floor(Res / max(PixelSize, 1.0)), 1.0);
uv = (floor(uv * lowRes) + 0.5) / lowRes;
return saturate(uv);
"""

HLSL_OFFSET_UV = r"""
float2 d = QUV - 0.5;
float amt = 0.0012 + Hallucination * 0.007 + Panic * 0.004;
return saturate(QUV + d * amt * Dir);
"""

HLSL_FINALIZE = r"""
float3 col = float3(SR.r, SG.g, SB.b);
if (Enabled < 0.5) return SG.rgb;

float2 lowRes = max(floor(Res / max(PixelSize, 1.0)), 1.0);
float2 cell = floor(UV * lowRes);

float2 vc = UV - 0.5;
float edge = saturate(dot(vc, vc) * 3.0);

// Hypoxia: desaturate + grey-out
float lum = dot(col, float3(0.299, 0.587, 0.114));
col = lerp(col, float3(lum, lum, lum), Hypoxia * 0.8);
col *= 1.0 - Hypoxia * 0.35;

// Stage 2+: desaturated, cold colour grade
col = lerp(col, float3(lum * 0.82, lum * 0.95, lum * 1.18), ColdGrade * 0.6);

// Stage 1+: peripheral cyanosis - blue-violet creeping in from the edges
col = lerp(col, col * float3(0.55, 0.68, 1.2), Cyanosis * edge * 0.85);

// Dust storm haze
col = lerp(col, float3(0.55, 0.30, 0.15) * (0.4 + lum), Dust * 0.45);

// Vavilov-Cherenkov phosphene: blue-white starburst + horizontal streak
float2 dv = (UV - FlashPos.xy) * float2(Res.x / max(Res.y, 1.0), 1.0);
float r = length(dv);
float burst = exp(-r * 45.0) + 0.35 * exp(-abs(dv.y) * 220.0) * exp(-abs(dv.x) * 12.0);
col += Flash * burst * float3(0.55, 0.75, 1.0) * 3.0;

// Vignette / tunnel vision (Stage 3+ narrows hard)
float vig = 1.0 - dot(vc, vc) * (0.6 + Panic * 1.6 + Hallucination * 0.9 + Hypoxia * 1.0 + Tunnel * 3.5);
vig += Hallucination * 0.06 * sin(Time * 2.3);
col *= saturate(vig);

// Film grain per low-res cell
float n = frac(sin(dot(cell + frac(Time) * 91.7, float2(12.9898, 78.233))) * 43758.5453);
col += (n - 0.5) * (0.03 + Dust * 0.06 + Hallucination * 0.05 + Distortion * 0.12);

// Stage 4: constant Cherenkov static - blue-white sparkles everywhere
float n2 = frac(sin(dot(cell * 1.37 + frac(Time * 7.0) * 53.1, float2(39.346, 11.135))) * 24634.6345);
col += Distortion * step(0.985, n2) * float3(0.6, 0.8, 1.0);

// 4x4 ordered Bayer dither, then quantise to the PS1 15-bit palette (32 levels / channel)
int2 p = int2(fmod(cell, 4.0));
const float bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
float threshold = (bayer[p.y * 4 + p.x] + 0.5) / 16.0 - 0.5;
float levels = max(ColorLevels, 2.0);
col = saturate(col + threshold * Dither / levels);
col = floor(col * (levels - 1.0) + 0.5) / (levels - 1.0);

// CRT scanlines + RGB aperture mask at native resolution
float scan = 0.5 + 0.5 * cos(UV.y * Res.y * 3.14159);
col *= 1.0 - Scanline * 0.35 * (1.0 - scan);
float m = fmod(floor(UV.x * Res.x), 3.0);
float3 ap = m < 1.0 ? float3(1.0, 0.85, 0.85) : (m < 2.0 ? float3(0.85, 1.0, 0.85) : float3(0.85, 0.85, 1.0));
col *= lerp(float3(1.0, 1.0, 1.0), ap, Scanline * 0.5);
return col;
"""


def build_postprocess():
    mat = fresh_material("M_PS1_PostProcess")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    try:
        mat.set_editor_property("blendable_location",
                                enum_value(unreal.BlendableLocation, "BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"))
    except Exception as exc:  # noqa
        unreal.log_warning("[MarsSetup] blendable location: %s" % exc)

    screen = expr(mat, unreal.MaterialExpressionScreenPosition, -1600, 0)
    view = expr(mat, unreal.MaterialExpressionViewSize, -1600, 150)
    time = expr(mat, unreal.MaterialExpressionTime, -1600, 250)

    p = {}
    defaults = [("RetroEnabled", 1.0), ("PixelSize", 3.0), ("ColorLevels", 32.0), ("DitherStrength", 0.6),
                ("ScanlineIntensity", 0.25), ("Hallucination", 0.0), ("Panic", 0.0), ("Hypoxia", 0.0),
                ("DustStorm", 0.0), ("CherenkovFlash", 0.0), ("Cyanosis", 0.0), ("ColdGrade", 0.0),
                ("Nausea", 0.0), ("TunnelVision", 0.0), ("Distortion", 0.0)]
    for i, (name, value) in enumerate(defaults):
        p[name] = scalar(mat, name, value, -1600, 400 + i * 70)
    flash_pos = vector(mat, "FlashPosition", (0.15, 0.5, 0.0, 0.0), -1600, 1600)

    quv = custom(mat, "PS1_QuantizeUV", HLSL_QUANT_UV, F2, ["UV", "Res", "PixelSize", "Hallucination", "Time", "Enabled", "Nausea", "Distortion"], -1100, 0)
    link(screen, "ViewportUV", quv, "UV")
    link(view, "", quv, "Res")
    link(p["PixelSize"], "", quv, "PixelSize")
    link(p["Hallucination"], "", quv, "Hallucination")
    link(time, "", quv, "Time")
    link(p["RetroEnabled"], "", quv, "Enabled")
    link(p["Nausea"], "", quv, "Nausea")
    link(p["Distortion"], "", quv, "Distortion")

    plus = expr(mat, unreal.MaterialExpressionConstant, -1100, 300)
    plus.set_editor_property("r", 1.0)
    minus = expr(mat, unreal.MaterialExpressionConstant, -1100, 400)
    minus.set_editor_property("r", -1.0)

    offsets = []
    for idx, sign in enumerate((plus, minus)):
        off = custom(mat, "PS1_ChromaOffset", HLSL_OFFSET_UV, F2, ["QUV", "Dir", "Hallucination", "Panic"], -800, 200 + idx * 200)
        link(quv, "", off, "QUV")
        link(sign, "", off, "Dir")
        link(p["Hallucination"], "", off, "Hallucination")
        link(p["Panic"], "", off, "Panic")
        offsets.append(off)

    pp_input = enum_value(unreal.SceneTextureId, "PPI_POST_PROCESS_INPUT0")
    samples = []
    for idx, uv_src in enumerate((offsets[0], quv, offsets[1])):
        st = expr(mat, unreal.MaterialExpressionSceneTexture, -500, idx * 200)
        st.set_editor_property("scene_texture_id", pp_input)
        link(uv_src, "", st, "UVs")
        samples.append(st)

    fin = custom(mat, "PS1_Finalize", HLSL_FINALIZE, F3,
                 ["SR", "SG", "SB", "UV", "Res", "PixelSize", "ColorLevels", "Dither", "Scanline", "Hallucination",
                  "Panic", "Hypoxia", "Dust", "Flash", "FlashPos", "Time", "Enabled",
                  "Cyanosis", "ColdGrade", "Tunnel", "Distortion"], -150, 0)
    link(samples[0], "Color", fin, "SR")
    link(samples[1], "Color", fin, "SG")
    link(samples[2], "Color", fin, "SB")
    link(screen, "ViewportUV", fin, "UV")
    link(view, "", fin, "Res")
    link(p["PixelSize"], "", fin, "PixelSize")
    link(p["ColorLevels"], "", fin, "ColorLevels")
    link(p["DitherStrength"], "", fin, "Dither")
    link(p["ScanlineIntensity"], "", fin, "Scanline")
    link(p["Hallucination"], "", fin, "Hallucination")
    link(p["Panic"], "", fin, "Panic")
    link(p["Hypoxia"], "", fin, "Hypoxia")
    link(p["DustStorm"], "", fin, "Dust")
    link(p["CherenkovFlash"], "", fin, "Flash")
    link(flash_pos, "", fin, "FlashPos")
    link(time, "", fin, "Time")
    link(p["RetroEnabled"], "", fin, "Enabled")
    link(p["Cyanosis"], "", fin, "Cyanosis")
    link(p["ColdGrade"], "", fin, "ColdGrade")
    link(p["TunnelVision"], "", fin, "Tunnel")
    link(p["Distortion"], "", fin, "Distortion")

    mel.connect_material_property(fin, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)
    log("M_PS1_PostProcess built")


# ---------------------------------------------------------------------------
# 2. PS1 surface master: vertex snapping + affine texture mapping
# ---------------------------------------------------------------------------

HLSL_VERTEX_SNAP = r"""
// PS1 GTE had no sub-pixel precision: vertices snapped to an integer screen grid.
// Emulated by snapping the camera-relative position to a grid that grows with distance,
// so the wobble stays roughly constant in screen space.
float d = max(length(Rel), 1.0);
float g = max(Jitter * d, 0.01);
float3 snapped = floor(Rel / g + 0.5) * g;
return snapped - Rel;
"""

HLSL_AFFINE_PACK = r"""
// Vertex stage: pre-multiply UV by depth. Perspective-correct interpolation of (uv*w, w)
// followed by a per-pixel divide yields screen-linear (affine) UVs, like the PS1 rasteriser.
float w = max(length(Rel), 1.0);
return float3(UV * w, w);
"""

HLSL_AFFINE_UNPACK = r"""
float2 affineUV = Packed.xy / max(Packed.z, 0.001);
return lerp(UV, affineUV, Strength);
"""

HLSL_PATTERN = r"""
float3 c = Tint;
if (Pattern > 0.5 && Pattern < 1.5)
{
    // Hull panels: world-aligned 1 m x 0.5 m plates (tri-planar pick by face normal), seams, rivets, tone.
    float3 an = abs(N);
    float2 wuv = an.z > an.x && an.z > an.y ? WP.xy : (an.x > an.y ? WP.yz : WP.xz);
    float2 puv = wuv / float2(100.0, 50.0);
    float2 g = frac(puv);
    float seam = saturate(step(g.x, 0.02) + step(g.y, 0.04));
    float2 rv = frac(puv * float2(4.0, 2.0)) - 0.5;
    float rivet = step(length(rv * float2(1.0, 2.0)), 0.06) * step(g.y, 0.12);
    float tone = frac(sin(dot(floor(puv), float2(12.9898, 78.233))) * 43758.5453);
    c *= (1.0 - seam * 0.4 - rivet * 0.18) * (0.88 + 0.12 * tone);
}
else if (Pattern > 1.5)
{
    // Regolith speckle
    float n = frac(sin(dot(floor(UV * 16.0), float2(12.9898, 78.233))) * 43758.5453);
    c *= 0.85 + 0.3 * n;
}
return c;
"""


def camera_relative_position(mat, x, y):
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, x, y)
    try:
        wp.set_editor_property("world_position_shader_offset",
                               enum_value(unreal.WorldPositionIncludedOffsets, "WPT_CAMERA_RELATIVE_NO_OFFSETS",
                                          "WPT_CAMERA_RELATIVE"))
        return wp
    except Exception as exc:  # noqa
        unreal.log_warning("[MarsSetup] camera-relative WorldPosition unavailable (%s); using subtraction" % exc)
        wp.set_editor_property("world_position_shader_offset",
                               enum_value(unreal.WorldPositionIncludedOffsets, "WPT_EXCLUDE_ALL_SHADER_OFFSETS"))
        cam = expr(mat, unreal.MaterialExpressionCameraPositionWS, x, y + 120)
        sub = expr(mat, unreal.MaterialExpressionSubtract, x + 200, y)
        link(wp, "", sub, "A")
        link(cam, "", sub, "B")
        return sub


def build_surface_master():
    mat = fresh_material("M_PS1_Surface")
    mat.set_editor_property("two_sided", False)

    jitter = scalar(mat, "JitterAmount", 0.004, -1400, -300)
    affine = scalar(mat, "AffineStrength", 1.0, -1400, 300)
    tiling = scalar(mat, "TextureTiling", 1.0, -1400, 380)
    pattern = scalar(mat, "PatternType", 1.0, -1400, 460)
    use_vc = scalar(mat, "UseVertexColor", 0.0, -1400, 540)
    tint = vector(mat, "BaseColor", (0.55, 0.56, 0.58, 1.0), -1400, 620)

    # Vertex snapping -> World Position Offset
    rel_vs = camera_relative_position(mat, -1400, -450)
    snap = custom(mat, "PS1_VertexSnap", HLSL_VERTEX_SNAP, F3, ["Rel", "Jitter"], -1000, -400)
    link(rel_vs, "", snap, "Rel")
    link(jitter, "", snap, "Jitter")
    # MP_WorldPositionOffset is hidden from Python in UE 5.3 -> small C++ helper in the game module.
    # (ConnectWorldPositionOffset also inserts a ShadowPassSwitch so shadow casters skip the PS1 jitter.)
    wpo_src = snap
    if not unreal.MarsEditorScriptLibrary.connect_world_position_offset(mat, wpo_src):
        unreal.log_warning("[MarsSetup] could not connect World Position Offset")

    # Affine UVs via vertex interpolator
    tex = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 150)
    uv_mul = expr(mat, unreal.MaterialExpressionMultiply, -1200, 150)
    link(tex, "", uv_mul, "A")
    link(tiling, "", uv_mul, "B")

    rel_ps = camera_relative_position(mat, -1400, -150)
    pack = custom(mat, "PS1_AffinePack", HLSL_AFFINE_PACK, F3, ["UV", "Rel"], -1000, 0)
    link(uv_mul, "", pack, "UV")
    link(rel_ps, "", pack, "Rel")
    interp = expr(mat, unreal.MaterialExpressionVertexInterpolator, -750, 0)
    link(pack, "", interp, "VS")
    unpack = custom(mat, "PS1_AffineUnpack", HLSL_AFFINE_UNPACK, F2, ["Packed", "UV", "Strength"], -500, 100)
    link(interp, "PS", unpack, "Packed")
    link(uv_mul, "", unpack, "UV")
    link(affine, "", unpack, "Strength")

    pat = custom(mat, "PS1_Pattern", HLSL_PATTERN, F3, ["UV", "Pattern", "Tint", "WP", "N"], -250, 300)
    link(unpack, "", pat, "UV")
    link(pattern, "", pat, "Pattern")
    link(tint, "", pat, "Tint")
    world_pos = expr(mat, unreal.MaterialExpressionWorldPosition, -600, 450)
    link(world_pos, "", pat, "WP")
    normal_ws = expr(mat, unreal.MaterialExpressionVertexNormalWS, -600, 550)
    link(normal_ws, "", pat, "N")

    # Optional texture (white by default) sampled with the affine UVs
    color_out = pat
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    if white:
        ts = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -250, 0)
        ts.set_editor_property("parameter_name", "BaseTexture")
        ts.set_editor_property("texture", white)
        link(unpack, "", ts, "UVs")
        mul_tex = expr(mat, unreal.MaterialExpressionMultiply, 0, 150)
        link(pat, "", mul_tex, "A")
        link(ts, "RGB", mul_tex, "B")
        color_out = mul_tex

    vcol = expr(mat, unreal.MaterialExpressionVertexColor, 0, 400)
    mul_vc = expr(mat, unreal.MaterialExpressionMultiply, 200, 350)
    link(color_out, "", mul_vc, "A")
    link(vcol, "", mul_vc, "B")
    lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, 400, 200)
    link(color_out, "", lerp, "A")
    link(mul_vc, "", lerp, "B")
    link(use_vc, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Emissive channel for glowing prop parts (screens, bioreactor tubes, reflex sight, LEDs).
    emissive = vector(mat, "EmissiveColor", (0.0, 0.0, 0.0, 1.0), 400, 700)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Per-part material response (rubber seals vs. machined metal vs. fabric) via MID parameters.
    rough = scalar(mat, "Roughness", 0.9, 400, 500)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metal = scalar(mat, "Metallic", 0.0, 400, 560)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    spec = scalar(mat, "Specular", 0.2, 400, 620)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    finish(mat)
    log("M_PS1_Surface built")
    return mat


def make_instance(name, parent, scalars, vectors):
    path = "%s/%s" % (MAT_DIR, name)
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in vectors.items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    eal.save_loaded_asset(mi)
    log("%s built" % name)


# ---------------------------------------------------------------------------
# 3. Phantom material
# ---------------------------------------------------------------------------

def build_glass():
    """Translucent glass for viewports, partitions, terrariums and spore pods."""
    mat = fresh_material("M_Glass")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    color = vector(mat, "Color", (0.75, 0.88, 0.95, 1.0), -700, 0)
    opacity = scalar(mat, "Opacity", 0.3, -700, 200)
    emissive = vector(mat, "EmissiveColor", (0.0, 0.0, 0.0, 1.0), -700, 300)
    # Fresnel rim so the pane edges read even when nearly clear.
    fres = expr(mat, unreal.MaterialExpressionFresnel, -700, 450)
    mul = expr(mat, unreal.MaterialExpressionMultiply, -450, 0)
    link(color, "", mul, "A")
    dim = expr(mat, unreal.MaterialExpressionConstant, -700, 120)
    dim.set_editor_property("r", 0.12)
    link(dim, "", mul, "B")
    add = expr(mat, unreal.MaterialExpressionAdd, -250, 0)
    link(mul, "", add, "A")
    link(emissive, "", add, "B")
    mel.connect_material_property(add, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    op_add = expr(mat, unreal.MaterialExpressionAdd, -250, 250)
    link(opacity, "", op_add, "A")
    fres_mul = expr(mat, unreal.MaterialExpressionMultiply, -450, 450)
    link(fres, "", fres_mul, "A")
    half = expr(mat, unreal.MaterialExpressionConstant, -700, 560)
    half.set_editor_property("r", 0.35)
    link(half, "", fres_mul, "B")
    link(fres_mul, "", op_add, "B")
    mel.connect_material_property(op_add, "", unreal.MaterialProperty.MP_OPACITY)
    finish(mat)
    log("M_Glass built")


def import_team_art():
    """Imports the team PARADOX logo and poster from Content/UI/Source as UI textures."""
    import os
    src_dir = os.path.join(unreal.Paths.project_content_dir(), "UI", "Source")
    tasks = []
    for name in ("ParadoxLogo", "ParadoxTeamPoster"):
        for ext in (".png", ".jpg", ".jpeg"):
            path = os.path.join(src_dir, name + ext)
            if os.path.exists(path):
                task = unreal.AssetImportTask()
                task.set_editor_property("filename", path)
                task.set_editor_property("destination_path", "/Game/UI")
                task.set_editor_property("destination_name", name)
                task.set_editor_property("automated", True)
                task.set_editor_property("replace_existing", True)
                task.set_editor_property("save", True)
                tasks.append(task)
                break
    if tasks:
        asset_tools.import_asset_tasks(tasks)
        for name in ("ParadoxLogo", "ParadoxTeamPoster"):
            tex = unreal.load_asset("/Game/UI/" + name)
            if tex:
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
                tex.set_editor_property("never_stream", True)
                eal.save_loaded_asset(tex)
                log("Imported texture " + name)


def build_phantom():
    mat = fresh_material("M_Phantom")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    opacity = scalar(mat, "Opacity", 0.8, -600, 200)
    col = vector(mat, "Color", (0.004, 0.003, 0.006, 1.0), -600, 0)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    finish(mat)
    log("M_Phantom built")


# ---------------------------------------------------------------------------
# 4. Map: Ares Station
# ---------------------------------------------------------------------------

def spawn(cls, loc, rot=(0.0, 0.0, 0.0), label=None):
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actor_sub.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    if actor and label:
        actor.set_actor_label(label)
    return actor


def set_props(actor, **props):
    for k, v in props.items():
        try:
            actor.set_editor_property(k, v)
        except Exception as exc:  # noqa
            unreal.log_warning("[MarsSetup] %s.%s: %s" % (actor.get_name(), k, exc))


def build_map():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if eal.does_asset_exist(MAP_PATH):
        eal.delete_asset(MAP_PATH)
    if not les.new_level(MAP_PATH):
        unreal.log_error("[MarsSetup] could not create level " + MAP_PATH)
        return

    # --- Sky & lighting (Mars: butterscotch day sky from suspended dust) ---
    sun = spawn(unreal.DirectionalLight, (0, 0, 2000), (-35.0, 110.0, 0.0), "Sun")
    sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_comp.set_editor_property("intensity", 4.3)
    sun_comp.set_editor_property("light_color", unreal.Color(255, 228, 200, 255))
    # Clean shadows: no contact-shadow speckle, enough bias to kill acne on low-poly surfaces.
    for prop, value in (("contact_shadow_length", 0.0), ("shadow_bias", 0.9), ("shadow_slope_bias", 0.8),
                        ("dynamic_shadow_distance_movable_light", 30000.0), ("cascade_distribution_exponent", 2.5)):
        try:
            sun_comp.set_editor_property(prop, value)
        except Exception as exc:  # noqa
            unreal.log_warning("[MarsSetup] sun.%s: %s" % (prop, exc))
    try:
        sun_comp.set_editor_property("atmosphere_sun_light", True)
    except Exception:  # noqa
        pass

    sky = spawn(unreal.SkyAtmosphere, (0, 0, 0), label="MarsSkyAtmosphere")
    sky_comp = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    for prop, value in (("rayleigh_scattering_scale", 0.004),
                        ("mie_scattering_scale", 0.02),
                        ("mie_scattering", unreal.LinearColor(0.85, 0.55, 0.32, 1.0)),
                        ("mie_absorption", unreal.LinearColor(0.3, 0.45, 0.6, 1.0)),
                        ("ground_albedo", unreal.Color(150, 85, 45, 255)),
                        ("atmosphere_height", 40.0)):
        try:
            sky_comp.set_editor_property(prop, value)
        except Exception as exc:  # noqa
            unreal.log_warning("[MarsSetup] sky %s: %s" % (prop, exc))

    skylight = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
    sl = skylight.get_component_by_class(unreal.SkyLightComponent)
    skylight.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    set_props(sl, real_time_capture=True, intensity=0.8)

    fog = spawn(unreal.ExponentialHeightFog, (0, 0, -200), label="DustFog")
    fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    set_props(fog_comp, fog_density=0.0025, fog_inscattering_luminance=unreal.LinearColor(0.55, 0.38, 0.25, 1.0))

    # --- Terrain ---
    terrain = spawn(unreal.MarsTerrainGenerator, (0, 0, 0), label="MarsTerrain")

    # --- Ares-IV ring-and-hub station (procedural, see AMarsStationGenerator) ---
    IT = unreal.MarsItemType
    generator = spawn(unreal.MarsStationGenerator, (0, 0, 5.0), label="AresIV_StationGenerator")
    generator.generate_station()

    # --- Open-world dressing: story POIs (relay R-3, drill rig, Ares-III), beacons, basalt fields ---
    surface = spawn(unreal.MarsSurfaceGenerator, (0, 0, 0), label="SurfaceGenerator")
    surface.generate_surface(terrain)

    def pickup(item, count, loc, label):
        pk = spawn(unreal.MarsPickup, loc, label=label)
        set_props(pk, item_type=item, count=count)
        return pk

    # --- Surface: beacon outside the airlock, sample sites, crashed supply lander ---
    ext = generator.get_airlock_exterior_point()
    home = spawn(unreal.SurfaceBeacon, (ext.x + 250, ext.y, 0), label="Beacon_HOME")
    set_props(home, beacon_label="HOME")

    rng = random.Random(2087)

    def ground(xx, yy):
        return terrain.get_height_at_world(xx, yy)

    for i in range(6):
        ang = rng.uniform(-1.2, 1.2)
        dist = rng.uniform(2500, 12000)
        px, py = dist * math.cos(ang), dist * math.sin(ang)
        item = IT.REGOLITH_SAMPLE if i % 2 == 0 else IT.ROCK_SAMPLE
        pickup(item, 1, (px, py, ground(px, py) + 20), "Pickup_Sample_%d" % i)

    # Crashed cargo lander ~650 m out: two quantum transistors, parts, O2 - and a leaking RTG.
    lander = (52000.0, 31000.0)
    lz = ground(*lander)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    for i, (dx, dy, sx, sy, sz, yaw) in enumerate([(0, 0, 6, 3.5, 2.5, 20), (450, 120, 2.5, 2.5, 1.2, 65),
                                                     (-380, -200, 1.5, 4.0, 0.4, -30), (120, -420, 3.0, 0.4, 2.0, 10)]):
        wreck = spawn(unreal.StaticMeshActor, (lander[0] + dx, lander[1] + dy, lz + sz * 40), (rng.uniform(-15, 15), yaw, rng.uniform(-20, 20)), "LanderWreck_%d" % i)
        smc = wreck.static_mesh_component
        smc.set_static_mesh(cube)
        wreck.set_actor_scale3d(unreal.Vector(sx, sy, sz))
    # (Quantum transistors now come from Storage, relay R-3 and the drill rig - see AMarsSurfaceGenerator.)
    pickup(IT.SPARE_PARTS, 2, (lander[0] - 150, lander[1] + 250, lz + 30), "Pickup_SpareParts_Lander")
    pickup(IT.O2_CANISTER, 1, (lander[0] + 300, lander[1] + 300, lz + 30), "Pickup_O2_Lander")
    lb = spawn(unreal.SurfaceBeacon, (lander[0] - 300, lander[1] - 300, lz), label="Beacon_LANDER")
    set_props(lb, beacon_label="LANDER")
    hz = spawn(unreal.RadiationHazardZone, (lander[0] + 100, lander[1], lz), label="Hazard_LanderRTG")
    set_props(hz, peak_dose_rate_msv_per_hour=0.6, storm_multiplier=4.0)

    # Natural hotspot inside the largest crater.
    craters = terrain.get_craters()
    if craters:
        big = max(craters, key=lambda c: c.radius)
        cxy = big.center
        hz2 = spawn(unreal.RadiationHazardZone, (cxy.x, cxy.y, ground(cxy.x, cxy.y)), label="Hazard_CraterFloor")
        set_props(hz2, peak_dose_rate_msv_per_hour=0.35, storm_multiplier=5.0)

    les.save_current_level()
    log("Map %s built" % MAP_PATH)


def main():
    build_postprocess()
    surface = build_surface_master()
    make_instance("MI_HullPanel", surface, {"PatternType": 1.0, "UseVertexColor": 0.0, "TextureTiling": 0.5, "Roughness": 0.7, "Specular": 0.35}, {"BaseColor": (0.21, 0.22, 0.24, 1.0)})
    make_instance("MI_MarsRegolith", surface, {"PatternType": 2.0, "UseVertexColor": 1.0, "TextureTiling": 0.2, "JitterAmount": 0.006}, {"BaseColor": (1.0, 1.0, 1.0, 1.0)})
    make_instance("MI_MarsRock", surface, {"PatternType": 2.0, "UseVertexColor": 0.0, "TextureTiling": 1.0}, {"BaseColor": (0.33, 0.19, 0.13, 1.0)})
    build_phantom()
    build_glass()
    import_team_art()
    build_map()
    log("DONE")


main()
