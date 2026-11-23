"""Builds the TestStreet level: a generic street block whose water level and
materials are driven by AFutureWorldDirector. Generates everything under
Content/FutureEarth; delete that folder on disk before running it again.

Run:  UnrealEditor-Cmd <FutureEarth.uproject> -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import random
import unreal

PKG = '/Game/FutureEarth'


def make(cls, **props):
    """Create an Unreal struct/object and set its properties (constructors take no arguments here)."""
    obj = cls()
    for k, v in props.items():
        obj.set_editor_property(k, v)
    return obj

MAP = PKG + '/Maps/TestStreet'

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def fresh(name, cls, factory, folder=PKG):
    path = f'{folder}/{name}'
    if eal.does_asset_exist(path):
        raise RuntimeError(path + ' already exists. Delete Content/FutureEarth on disk and run again.')
    asset = tools.create_asset(name, folder, cls, factory)
    if asset is None:
        raise RuntimeError('Could not create ' + path)
    return asset


def color(r, g, b):
    return unreal.LinearColor(r, g, b, 1.0)


# --- Material Parameter Collection: the dials the director writes ------------
DIALS = ['HeatStress', 'Aridity', 'VegetationDieback', 'FireRisk', 'WaterLevelM', 'LocalWarmingC', 'WaterSurfaceZ']
mpc = fresh('MPC_Climate', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
params = []
for dial in DIALS:
    p = unreal.CollectionScalarParameter()
    p.set_editor_property('parameter_name', dial)
    p.set_editor_property('default_value', 0.0)
    params.append(p)
mpc.set_editor_property('scalar_parameters', params)


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def dial_node(mat, name, x, y):
    n = mel.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter, x, y)
    n.set_editor_property('collection', mpc)
    n.set_editor_property('parameter_name', name)
    return n


def climate_material(name, healthy, ruined, dial, roughness):
    """Base colour = lerp(Tint, Ruined, dial). Tint is overridable per instance."""
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew())
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -700, -100, parameter_name='Tint', default_value=healthy)
    dead = node(mat, unreal.MaterialExpressionConstant3Vector, -700, 100, constant=ruined)
    alpha = dial_node(mat, dial, -700, 300)
    lerp = node(mat, unreal.MaterialExpressionLinearInterpolate, -350, 0)
    mel.connect_material_expressions(tint, '', lerp, 'A')
    mel.connect_material_expressions(dead, '', lerp, 'B')
    mel.connect_material_expressions(alpha, '', lerp, 'Alpha')
    mel.connect_material_property(lerp, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(mat, unreal.MaterialExpressionConstant, -350, 250, r=roughness)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    return mat


def instance(name, parent, tint):
    mi = fresh(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_vector_parameter_value(mi, 'Tint', tint)
    return mi


m_facade = climate_material('M_Facade', color(0.55, 0.52, 0.48), color(0.42, 0.33, 0.22), 'Aridity', 0.85)
m_ground = climate_material('M_Ground', color(0.07, 0.07, 0.075), color(0.30, 0.24, 0.16), 'Aridity', 0.9)
m_canopy = climate_material('M_Canopy', color(0.05, 0.22, 0.03), color(0.20, 0.13, 0.06), 'VegetationDieback', 0.7)
m_trunk = climate_material('M_Trunk', color(0.10, 0.07, 0.05), color(0.16, 0.14, 0.12), 'VegetationDieback', 0.9)
facades = [
    instance('MI_Facade_Stone', m_facade, color(0.55, 0.52, 0.48)),
    instance('MI_Facade_Brick', m_facade, color(0.38, 0.16, 0.11)),
    instance('MI_Facade_Plaster', m_facade, color(0.70, 0.66, 0.55)),
    instance('MI_Facade_Dark', m_facade, color(0.16, 0.17, 0.19)),
]

# Water: Unreal's single-layer-water shading (depth-coloured, refractive) with two
# layers of slowly drifting wave normals projected in world space.
P = unreal.MaterialProperty
m_water = fresh('M_Water', unreal.Material, unreal.MaterialFactoryNew())
m_water.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
waves = unreal.load_asset('/Water/Textures/Normals/T_Water_TilingNormal_Waves_02')
if not waves:
    raise RuntimeError('Water plugin wave normal texture not found')
wpos = node(m_water, unreal.MaterialExpressionWorldPosition, -1500, 0)
xy = node(m_water, unreal.MaterialExpressionComponentMask, -1300, 0, r=True, g=True, b=False, a=False)
mel.connect_material_expressions(wpos, '', xy, '')
layers = []
for i, (tile_cm, sx, sy) in enumerate([(900.0, 0.010, 0.004), (2300.0, -0.004, 0.006)]):
    scale = node(m_water, unreal.MaterialExpressionMultiply, -1100, i * 250, const_b=1.0 / tile_cm)
    mel.connect_material_expressions(xy, '', scale, 'A')
    pan = node(m_water, unreal.MaterialExpressionPanner, -900, i * 250, speed_x=sx, speed_y=sy)
    mel.connect_material_expressions(scale, '', pan, 'Coordinate')
    tex = node(m_water, unreal.MaterialExpressionTextureSample, -700, i * 250,
               texture=waves, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(pan, '', tex, 'UVs')
    layers.append(tex)
both = node(m_water, unreal.MaterialExpressionAdd, -450, 100)
mel.connect_material_expressions(layers[0], 'RGB', both, 'A')
mel.connect_material_expressions(layers[1], 'RGB', both, 'B')
up = node(m_water, unreal.MaterialExpressionConstant3Vector, -450, 300, constant=color(0, 0, 1))
calm = node(m_water, unreal.MaterialExpressionLinearInterpolate, -250, 150, const_alpha=0.22)
mel.connect_material_expressions(up, '', calm, 'A')
mel.connect_material_expressions(both, '', calm, 'B')
norm = node(m_water, unreal.MaterialExpressionNormalize, -100, 150)
mel.connect_material_expressions(calm, '', norm, 'VectorInput')
mel.connect_material_property(norm, '', P.MP_NORMAL)
mel.connect_material_property(node(m_water, unreal.MaterialExpressionConstant3Vector, -400, -300, constant=color(0, 0, 0)), '', P.MP_BASE_COLOR)
mel.connect_material_property(node(m_water, unreal.MaterialExpressionConstant, -400, -200, r=0.06), '', P.MP_ROUGHNESS)
mel.connect_material_property(node(m_water, unreal.MaterialExpressionConstant, -400, -120, r=1.0), '', P.MP_SPECULAR)
slw = node(m_water, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 100, 400)
# Per-cm coefficients: silty flood water that turns opaque green-brown within a few metres.
mel.connect_material_expressions(node(m_water, unreal.MaterialExpressionConstant3Vector, -300, 400, constant=color(0.0030, 0.0034, 0.0022)), '', slw, 'ScatteringCoefficients')
mel.connect_material_expressions(node(m_water, unreal.MaterialExpressionConstant3Vector, -300, 520, constant=color(0.0075, 0.0040, 0.0048)), '', slw, 'AbsorptionCoefficients')
mel.connect_material_expressions(node(m_water, unreal.MaterialExpressionConstant, -300, 640, r=0.3), '', slw, 'PhaseG')
mel.recompile_material(m_water)

# Post-process pass that makes ANY geometry (grey-box, map tiles, hero sites) respond to
# the climate dials: vegetation browns with VegetationDieback, surfaces dust over with
# Aridity, and a dark wet tide line appears just above the risen water.
m_post = fresh('M_ClimatePost', unreal.Material, unreal.MaterialFactoryNew())
m_post.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
scene = node(m_post, unreal.MaterialExpressionSceneTexture, -900, -200, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
albedo = node(m_post, unreal.MaterialExpressionSceneTexture, -900, 0, scene_texture_id=unreal.SceneTextureId.PPI_BASE_COLOR)
depth = node(m_post, unreal.MaterialExpressionSceneTexture, -900, 200, scene_texture_id=unreal.SceneTextureId.PPI_SCENE_DEPTH)
pp_pos = node(m_post, unreal.MaterialExpressionWorldPosition, -900, 400)
grade = node(m_post, unreal.MaterialExpressionCustom, -400, 0)
grade.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
names = ['Color', 'Albedo', 'Depth', 'WorldPos', 'Dieback', 'Aridity', 'WaterZ', 'WaterLevelM']
grade.set_editor_property('inputs', [make(unreal.CustomInput, input_name=n) for n in names])
grade.set_editor_property('code', """
float3 c = Color.rgb;
float solid = step(Depth.r, 3000000.0);                 // 0 on the sky
float lum = dot(c, float3(0.30, 0.59, 0.11));
float green = saturate((Albedo.g - max(Albedo.r, Albedo.b)) * 7.0);
c = lerp(c, lum * float3(1.28, 1.00, 0.58), green * Dieback * solid);
c = lerp(c, lum * float3(1.12, 1.00, 0.80), Aridity * 0.45 * solid);
float h = WorldPos.z - WaterZ;                          // cm above the water surface
float flooded = saturate(WaterLevelM / 0.3);
float stain = saturate(1.0 - h / 160.0) * step(3.0, h) * flooded * solid;
c *= lerp(float3(1, 1, 1), float3(0.42, 0.45, 0.40), stain);
return c;
""")
mel.connect_material_expressions(scene, 'Color', grade, 'Color')
mel.connect_material_expressions(albedo, 'Color', grade, 'Albedo')
mel.connect_material_expressions(depth, 'Color', grade, 'Depth')
mel.connect_material_expressions(pp_pos, '', grade, 'WorldPos')
for i, (pin, dial) in enumerate([('Dieback', 'VegetationDieback'), ('Aridity', 'Aridity'), ('WaterZ', 'WaterSurfaceZ'), ('WaterLevelM', 'WaterLevelM')]):
    mel.connect_material_expressions(dial_node(m_post, dial, -900, 600 + i * 120), '', grade, pin)
mel.connect_material_property(grade, '', P.MP_EMISSIVE_COLOR)
mel.recompile_material(m_post)


def add_climate_post():
    volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    volume.set_actor_label('ClimatePost')
    volume.set_editor_property('unbound', True)
    settings = volume.get_editor_property('settings')
    settings.set_editor_property('weighted_blendables', make(unreal.WeightedBlendables, array=[make(unreal.WeightedBlendable, weight=1.0, object=m_post)]))
    volume.set_editor_property('settings', settings)

# --- Level -------------------------------------------------------------------
if not levels.new_level(MAP):
    raise RuntimeError('Could not create level ' + MAP)

cube = unreal.load_asset('/Engine/BasicShapes/Cube')
plane = unreal.load_asset('/Engine/BasicShapes/Plane')
cylinder = unreal.load_asset('/Engine/BasicShapes/Cylinder')
sphere = unreal.load_asset('/Engine/BasicShapes/Sphere')


def mesh(label, asset, material, loc, scale, movable=False):
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(0, 0, 0))
    a.set_actor_label(label)
    if movable:
        a.set_mobility(unreal.ComponentMobility.MOVABLE)
    a.static_mesh_component.set_static_mesh(asset)
    a.static_mesh_component.set_material(0, material)
    a.set_actor_scale3d(unreal.Vector(*scale))
    return a


def spawn(cls, label, loc=(0, 0, 0), rot=(0, 0, 0)):
    # Rotator args are (roll, pitch, yaw).
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    a.set_actor_label(label)
    return a


# Units are cm. The street runs along X; ground top is Z=0, one metre above today's sea.
mesh('Ground', cube, m_ground, (0, 0, -50), (120, 60, 1))
random.seed(7)
for side in (-1, 1):
    for i in range(8):
        height = random.choice([900, 1200, 1500, 2100, 2700])
        mesh(f'Building_{"L" if side < 0 else "R"}{i}', cube, random.choice(facades),
             (-4200 + i * 1200, side * 1500, height / 2), (11, 10, height / 100))
        x = -3600 + i * 1200
        mesh(f'Trunk_{side}_{i}', cylinder, m_trunk, (x, side * 850, 200), (0.3, 0.3, 4))
        mesh(f'Canopy_{side}_{i}', sphere, m_canopy, (x, side * 850, 520), (3.6, 3.6, 3.0))

sea = mesh('SeaSurface', plane, m_water, (0, 0, -100), (600, 600, 1), movable=True)

sun = spawn(unreal.DirectionalLight, 'Sun', (0, 0, 3000), (0, -38, 25))
sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
spawn(unreal.SkyAtmosphere, 'SkyAtmosphere')
sky = spawn(unreal.SkyLight, 'SkyLight', (0, 0, 3000))
sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('real_time_capture', True)
spawn(unreal.ExponentialHeightFog, 'Fog')
spawn(unreal.VolumetricCloud, 'Clouds')
# Eye height is a third-floor balcony so the view stays above water at every year.
spawn(unreal.PlayerStart, 'PlayerStart', (-4600, -250, 1000), (0, 0, 4))

add_climate_post()

director = spawn(unreal.FutureWorldDirector, 'FutureWorldDirector', (0, 0, 100))
director.set_editor_property('latitude', 25.79)
director.set_editor_property('longitude', -80.13)
director.set_editor_property('year', 2526)
director.set_editor_property('scenario', unreal.ClimateScenario.WORST_CASE)
director.set_editor_property('sea_surface_actor', sea)
director.set_editor_property('present_sea_level_z', -100.0)
director.set_editor_property('climate_parameters', mpc)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
    raise RuntimeError('Could not save level ' + MAP)
eal.save_directory(PKG, only_if_is_dirty=False, recursive=True)
unreal.log('FutureEarth: TestStreet built OK')
