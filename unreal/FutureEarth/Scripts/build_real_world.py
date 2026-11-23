"""Builds the RealWorld level: Google Photorealistic 3D Tiles at a real place,
with the sea surface and climate dials driven by AFutureWorldDirector.

Needs the Cesium for Unreal plugin and the climate service running. The Google API key is NOT stored in the level;
the director applies it at runtime from the GOOGLE_MAPS_API_KEY environment variable.

Run:  UnrealEditor-Cmd <FutureEarth.uproject> -ExecutePythonScript=<this file> -unattended -nullrhi
Delete Content/FutureEarth/Maps/RealWorld.umap on disk before running it again.
"""
import json
import urllib.request

import unreal

PKG = '/Game/FutureEarth'
MAP = PKG + '/Maps/RealWorld'

# Ocean Drive, Miami Beach. Heights are metres above the WGS84 ellipsoid, which is
# what the tiles use; mean sea level here sits GEOID_M below the ellipsoid.
LAT, LON = 25.7810, -80.1300
SERVICE = 'http://localhost:8787'
with urllib.request.urlopen(f'{SERVICE}/future?lat={LAT}&lon={LON}&year=2026', timeout=240) as r:
    here = json.load(r)['location']
GEOID_M = here['geoid_height_m']                 # EGM96 mean sea level above the ellipsoid
GROUND_ABOVE_SEA_M = max(here['elevation_m'] or 0.0, 0.5)
ORIGIN_HEIGHT_M = GEOID_M + GROUND_ABOVE_SEA_M   # Unreal Z=0 is roughly street level

eal = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if eal.does_asset_exist(MAP):
    raise RuntimeError(MAP + ' already exists. Delete it on disk and run again.')
mpc = unreal.load_asset(PKG + '/MPC_Climate')
water = unreal.load_asset(PKG + '/M_Water')
if not mpc or not water:
    raise RuntimeError('Run build_test_street.py first; it creates MPC_Climate and M_Water.')
if not levels.new_level(MAP):
    raise RuntimeError('Could not create level ' + MAP)


def spawn(cls, label, loc=(0, 0, 0), rot=(0, 0, 0)):
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    a.set_actor_label(label)
    return a


georef = spawn(unreal.CesiumGeoreference, 'CesiumGeoreference')
georef.set_origin_longitude_latitude_height(unreal.Vector(LON, LAT, ORIGIN_HEIGHT_M))

tiles = spawn(unreal.Cesium3DTileset, 'GoogleTiles')
tiles.set_editor_property('tileset_source', unreal.TilesetSource.FROM_URL)
tiles.set_editor_property('tags', ['GoogleTiles'])   # the director puts the keyed URL on tagged tilesets
tiles.set_editor_property('georeference', georef)

sea = spawn(unreal.StaticMeshActor, 'SeaSurface', (0, 0, -GROUND_ABOVE_SEA_M * 100))
sea.set_mobility(unreal.ComponentMobility.MOVABLE)
sea.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
sea.static_mesh_component.set_material(0, water)
sea.set_actor_scale3d(unreal.Vector(5000, 5000, 1))   # 5 km square

sun = spawn(unreal.DirectionalLight, 'Sun', (0, 0, 3000), (0, -40, 110))
sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
spawn(unreal.SkyAtmosphere, 'SkyAtmosphere')
sky = spawn(unreal.SkyLight, 'SkyLight', (0, 0, 3000))
sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('real_time_capture', True)
spawn(unreal.ExponentialHeightFog, 'Fog')
spawn(unreal.VolumetricCloud, 'Clouds')
# Second-floor eye height on the beach side of the street, looking north along it.
spawn(unreal.PlayerStart, 'PlayerStart', (0, 0, 900), (0, 0, -90))
# A fixed camera owns the view, so mouse movement over the window cannot steer it.
camera = spawn(unreal.CameraActor, 'ViewCamera', (0, 0, 900), (0, -10, -90))
camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
camera.camera_component.set_editor_property('constrain_aspect_ratio', False)

director = spawn(unreal.FutureWorldDirector, 'FutureWorldDirector', (0, 0, 100))
director.set_editor_property('latitude', LAT)
director.set_editor_property('longitude', LON)
director.set_editor_property('year', 2126)
director.set_editor_property('scenario', unreal.ClimateScenario.CURRENT_PATH)
director.set_editor_property('sea_surface_actor', sea)
director.set_editor_property('sea_level_from_geoid', True)
director.set_editor_property('georeference_origin_height_m', ORIGIN_HEIGHT_M)
director.set_editor_property('climate_parameters', mpc)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
    raise RuntimeError('Could not save level ' + MAP)
unreal.log('FutureEarth: RealWorld built OK')
