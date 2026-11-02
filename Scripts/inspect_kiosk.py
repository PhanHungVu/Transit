"""Read-only asset inspection, run with UnrealEditor-Cmd -run=pythonscript."""
import unreal
import json
import os

report = {}
paths = ['/Game/Kiosk/UI/KioskUI', '/Game/Kiosk/UI/InteractPrompt',
         '/Game/Kiosk/Blueprint/BP_TransitKiosk',
         '/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter',
         '/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController',
         '/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode', '/Game/Input/IMC_Default']
for path in paths:
    obj = unreal.load_asset(path)
    info = {'class': obj.get_class().get_name(), 'properties': {}, 'api': [x for x in dir(obj) if any(k in x.lower() for k in ['widget', 'graph', 'parent', 'component'])]}
    for prop in ['parent_class', 'widget_tree', 'ubergraph_pages', 'simple_construction_script', 'mappings']:
        try:
            value = obj.get_editor_property(prop)
            info['properties'][prop] = str(value)
            if prop == 'widget_tree':
                info['tree_api'] = dir(value)
        except Exception as error:
            info['properties'][prop] = str(error)
    report[path] = info
report['widget_apis'] = [x for x in dir(unreal) if 'Widget' in x and any(k in x for k in ['Editor', 'Tree', 'Blueprint'])]
out = os.path.join(unreal.Paths.project_saved_dir(), 'KioskInspectionPython.json')
with open(out, 'w', encoding='utf-8') as f:
    json.dump(report, f, indent=2)
unreal.log('KIOSK_INSPECTION_COMPLETE ' + out)
