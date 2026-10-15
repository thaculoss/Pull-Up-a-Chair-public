"""Generate the 17 authored house menus and independent official-food copies."""
from pathlib import Path
import json,sys,subprocess,hashlib
sys.stdout.reconfigure(encoding='utf-8');sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent
sys.path.insert(0,str(ROOT/'load-order-audit'))
from inspect_records import records
# Document English names -> official source record, independent of translated/overridden names.
FOODS={
'Beef Stew':('Skyrim.esm',0xF4314,'牛肉炖汤'),
'Honningbrew Mead':('Skyrim.esm',0x508CA,'蜂蜜酒坊蜜酒'),
'Sweet Roll':('Skyrim.esm',0x64B3D,'甜卷'),
'Clam Chowder':('HearthFires.esm',0x353E,'蛤蜊浓汤'),
'Spiced Wine':('Skyrim.esm',0x85368,'香料葡萄酒'),
'Boiled Creme Treat':('Skyrim.esm',0x64B30,'奶油甜点'),
'Horker Loaf':('Skyrim.esm',0x7224E,'海象肉饼'),
'Nord Mead':('Skyrim.esm',0x34C5D,'诺德蜜酒'),
'Snowberry Crostata':('HearthFires.esm',0x353B,'雪莓果酥'),
'Salmon Steak':('Skyrim.esm',0x64B3B,'鲑鱼排'),
'Black-Briar Mead':('Skyrim.esm',0x2C35A,'黑荆棘蜜酒'),
'Honey Nut Treat':('Skyrim.esm',0x64B38,'蜂蜜坚果点心'),
'Leg of Goat Roast':('Skyrim.esm',0x7224C,'烤山羊腿'),
'Alto Wine':('Skyrim.esm',0x3133B,'阿尔托葡萄酒'),
'Juniper Berry Crostata':('HearthFires.esm',0x3539,'杜松莓果酥'),
'Venison Stew':('Skyrim.esm',0xF431D,'鹿肉炖汤'),
'Wine':('Skyrim.esm',0xC5348,'葡萄酒'),
'Apple Pie':('Skyrim.esm',0x64B43,'苹果派'),
'Steamed Mudcrab Legs':('HearthFires.esm',0x353F,'清蒸泥蟹腿'),
'Ale':('Skyrim.esm',0x34C5E,'麦酒'),
'Horker Stew':('Skyrim.esm',0xF4315,'海象肉炖汤'),
'Cabbage Potato Soup':('Skyrim.esm',0xF431B,'卷心菜土豆汤'),
'Lavender Dumpling':('HearthFires.esm',0x11801,'薰衣草酥饺'),
'Apple Cabbage Stew':('Skyrim.esm',0xEBA01,'苹果卷心菜炖汤'),
'Apple Dumpling':('HearthFires.esm',0x3533,'苹果酥饺'),
'Grilled Chicken Breast':('Skyrim.esm',0xE8947,'烤鸡胸'),
'Vegetable Soup':('Skyrim.esm',0xF431E,'蔬菜汤'),
'Chicken Dumpling':('HearthFires.esm',0x117FF,'鸡肉酥饺'),
'Jazbay Crostata':('HearthFires.esm',0x353A,'爵士葡萄果酥'),
'Rabbit Haunch':('Skyrim.esm',0x722C2,'烤兔腿'),
'Horker and Ash Yam Stew':('Dragonborn.esm',0x3CD5B,'海象肉灰烬山药炖汤'),
'Sujamma':('Dragonborn.esm',0x207E6,'苏佳玛酒')}
MENUS=[
('Skyrim.esm',0x1F86D,'The Bannered Mare','Beef Stew','Honningbrew Mead','Sweet Roll'),
('Skyrim.esm',0x200A5,'The Winking Skeever','Clam Chowder','Spiced Wine','Boiled Creme Treat'),
('Skyrim.esm',0x209EA,'Candlehearth Hall','Horker Loaf','Nord Mead','Snowberry Crostata'),
('Skyrim.esm',0x2247E,'The Bee and Barb','Salmon Steak','Black-Briar Mead','Honey Nut Treat'),
('Skyrim.esm',0x1F30E,'Silver-Blood Inn','Leg of Goat Roast','Alto Wine','Juniper Berry Crostata'),
('Skyrim.esm',0x5CA26,"Dead Man's Drink",'Venison Stew','Wine','Apple Pie'),
('Skyrim.esm',0x1EB96,'Moorside Inn','Steamed Mudcrab Legs','Ale','Honey Nut Treat'),
('Skyrim.esm',0x20065,'Windpeak Inn','Horker Stew','Nord Mead','Sweet Roll'),
('Skyrim.esm',0x1EB7C,'The Frozen Hearth','Cabbage Potato Soup','Wine','Lavender Dumpling'),
('Skyrim.esm',0x1CB8C,'Sleeping Giant Inn','Salmon Steak','Honningbrew Mead','Apple Pie'),
('Skyrim.esm',0x1F883,'Frostfruit Inn','Apple Cabbage Stew','Ale','Apple Dumpling'),
('Skyrim.esm',0x20008,'Four Shields Tavern','Grilled Chicken Breast','Ale','Sweet Roll'),
('Skyrim.esm',0x226AA,'Vilemyr Inn','Vegetable Soup','Nord Mead','Honey Nut Treat'),
('Skyrim.esm',0x20A02,'Braidwood Inn','Chicken Dumpling','Ale','Jazbay Crostata'),
('Skyrim.esm',0x1F7CB,'Old Hroldan Inn','Rabbit Haunch','Nord Mead','Juniper Berry Crostata'),
('Skyrim.esm',0x20054,'Nightgate Inn','Rabbit Haunch','Ale','Honey Nut Treat'),
('Dragonborn.esm',0x19A24,'The Retching Netch','Horker and Ash Yam Stew','Sujamma','Apple Pie')]
assert len(MENUS)==17 and len(FOODS)==32
source={};locations={}
for plugin in {v[0] for v in FOODS.values()}:
    for r in records(ROOT/'StockGame/Data'/plugin,types={'ALCH','LCTN'}):
        key=(plugin.lower(),int(r['id'].split(':')[1],16))
        if r['type']=='ALCH':source[key]=r
        else:locations[key]=r
base=[l.split('\t') for l in (HERE/'config/foods.tsv').read_text('utf-8').splitlines() if l and not l.startswith('#') and not l.startswith('Local - Inn Ordering.esp\t')]
model_profiles={}
for row in base:
    r=source.get((row[0].lower(),int(row[1],16)))
    if r:model_profiles.setdefault(r['MODL'].lower(),row)
model_audit=json.loads((ROOT/'food-coverage-audit-20261010/model-audit.json').read_text('utf-8'))
measured={}
for record in source.values():
    model=record['MODL'].lower()
    if model in model_profiles or not any(v[0].lower()+'|'+f'{v[1]:06X}'==record['id'].replace(':','|') for v in FOODS.values()):continue
    evidence=model_audit['meshes/'+model.replace('\\','/')]
    path=Path(evidence['provider'].split('::')[0])
    if not path.is_file() or path.suffix.lower()=='.bsa':path=Path(evidence['path'])
    assert hashlib.sha256(path.read_bytes()).hexdigest()==evidence['sha256']
    result=subprocess.run([str(ROOT/'food-gallery-20261010/build/Release/geometry_probe.exe'),str(path)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    g=json.loads(result.stdout);b=g['bounds']
    model_profiles[model]=['Skyrim.esm','000000','1','2','0']+list(map(str,b))+[str(max(abs(b[0]),abs(b[3]))+.5),str(max(abs(b[1]),abs(b[4]))+.5),str(g['radius']+.5),'1.0','3.1991','Measured specialty model']
    measured[model]=dict(provider=str(path),sha256=evidence['sha256'],geometry=g)
clones=[];new_profiles=[]
for index,(english,(plugin,local,chinese)) in enumerate(FOODS.items()):
    r=source[(plugin.lower(),local)];own=0x80F+index
    slots={i for menu in MENUS for i,name in enumerate(menu[3:]) if name==english};assert len(slots)==1
    from food_categories import is_soup
    course=slots.pop();row=list(model_profiles[r['MODL'].lower()])
    if is_soup(plugin,local):course=2
    row[0:5]=['Local - Inn Ordering.esp',f'{own:06X}',str(course),str(1<<course),str(int(course==0 and 'stew' not in r['MODL'].lower()))]
    row[16]=english+' (independent specialty)';new_profiles.append(row)
    clones.append(dict(local=own,source_plugin=plugin,source_local=local,source_edid=r['EDID'],name_en=english,name_zh=chinese,model=r['MODL'],course=course))
lookup={f['name_en']:f['local'] for f in clones}
(HERE/'config/specialty-foods.json').write_text(json.dumps(clones,ensure_ascii=False,indent=2),'utf-8')
lines=['# locationPlugin\tlocalLocationID\tinnName\tmainLocalID\tdrinkLocalID\tsideLocalID']
for plugin,local,name,*foods in MENUS:
    assert (plugin.lower(),local) in locations
    lines.append('\t'.join([plugin,f'{local:06X}',name]+[f'{lookup[n]:06X}' for n in foods]))
(HERE/'config/specialties.tsv').write_text('\n'.join(lines)+'\n','utf-8')
p=HERE/'config/foods.tsv';header=p.read_text('utf-8').splitlines()[0];p.write_text(header+'\n'+'\n'.join('\t'.join(row) for row in base+new_profiles)+'\n','utf-8')
for language in ['en','zh']:
    p=HERE/'config/locales'/f'{language}.tsv'
    lines=[line for line in p.read_text('utf-8').splitlines() if not line.startswith('specialty_food_')]
    lines.extend(f"specialty_food_{f['local']:06X}\t{f['name_'+language]}" for f in clones)
    p.write_text('\n'.join(lines)+'\n','utf-8')
(HERE/'reports/specialties-source-audit.json').write_text(json.dumps(dict(inns=len(MENUS),foods=clones,official_sources_only=True,measured_models=measured),ensure_ascii=False,indent=2),'utf-8')
print('17 inn menus, 32 official-source food copies, explicit calibrated profiles generated.')
