"""Builds the TestStreet level: a generic street block whose water level and
materials are driven by AFutureWorldDirector. Generates everything under
Content/FutureEarth; delete that folder on disk before running it again.

Run:  UnrealEditor-Cmd <FutureEarth.uproject> -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import random
import unreal

PKG = '/Game/FutureEarth'
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
DIALS = ['HeatStress', 'Aridity', 'VegetationDieback', 'FireRisk', 'WaterLevelM', 'LocalWarmingC']
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

# Still, dark, mirror-like water.
m_water = fresh('M_Water', unreal.Material, unreal.MaterialFactoryNew())
mel.connect_material_property(node(m_water, unreal.MaterialExpressionConstant3Vector, -400, 0, constant=color(0.010, 0.030, 0.035)), '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(node(m_water, unreal.MaterialExpressionConstant, -400, 150, r=0.04), '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(node(m_water, unreal.MaterialExpressionConstant, -400, 250, r=0.6), '', unreal.MaterialProperty.MP_METALLIC)
mel.recompile_material(m_water)

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
