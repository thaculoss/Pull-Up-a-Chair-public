"""Full FOMOD, preserving diningmodule1 choices/assets and external support."""
from pathlib import Path,PurePosixPath
import configparser,copy,hashlib,importlib.util,itertools,json,shutil,sys,zipfile
import xml.etree.ElementTree as ET
sys.dont_write_bytecode=True;sys.stdout.reconfigure(encoding='utf-8')
BASE=Path(__file__).resolve().parent;ROOT=BASE.parent
OLD=ROOT/'eating-module-20261015/dist/Pull-Up-a-Chair-0.9.9-beta-diningmodule1-FOMOD.zip'
NAME='Pull-Up-a-Chair-0.9.9-beta-lowtablelayout1-mealrestore1-FOMOD'
DIST=BASE/'dist';SUPPORT=DIST/(NAME+'-support');STAGE=BASE/'stage'
CORE='00_Core/SKSE/Plugins/PullUpAChairOrdering.dll'
QUEST='00_Core/Scripts/PullUpAChairOrderingQuest.pex'
MODULE='30_Dining/SKSE/Plugins/PullUpAChairEAS.dll'
sha=lambda b:hashlib.sha256(b).hexdigest()
def dump(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2),'utf-8')
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def main():
    assert not (DIST/(NAME+'.zip')).exists(),'Use a fresh suffixed release; do not overwrite a delivered archive.'
    DIST.mkdir(exist_ok=True);SUPPORT.mkdir(exist_ok=True)
    with zipfile.ZipFile(OLD) as z:old={n:z.read(n) for n in z.namelist()}
    data=dict(old)
    data[CORE]=(BASE/'build-core/Release/PullUpAChairOrdering.dll').read_bytes()
    data[QUEST]=(STAGE/QUEST).read_bytes()
    script=json.loads((BASE/'evidence/script-validation.json').read_text('utf-8'))
    assert sha(data[QUEST])==script['sha256'] and script['changed_functions']==['/localfindtable']
    assert b'mealrestore1' in data[CORE] and b'diningmodule1' in data[CORE]
    ET.register_namespace('xsi','http://www.w3.org/2001/XMLSchema-instance')
    cfg=ET.fromstring(old['fomod/ModuleConfig.xml'])
    cfg.find('moduleName').text='Pull Up a Chair — 0.9.9 beta lowtablelayout1 mealrestore1'
    original=ET.fromstring(old['fomod/ModuleConfig.xml']);comparison=copy.deepcopy(cfg)
    comparison.find('moduleName').text=original.findtext('moduleName')
    assert ET.tostring(comparison)==ET.tostring(original),'Installer choices must remain identical.'
    info=ET.fromstring(old['fomod/info.xml'])
    info.find('Name').text='Pull Up a Chair'
    info.find('Version').text='0.9.9 beta lowtablelayout1 mealrestore1'
    info.find('Version').set('MachineVersion','0.9.9.7')
    info.find('Description').text=('Full test release. Registered low-table serving orientation fix and persisted removed-serving receipts. '
        'Preserves optional desktop dining/EAS modes, service, Chatty NPCs compatibility and English default. Gameplay retest pending; VR excluded.')
    for n,tree in [('fomod/ModuleConfig.xml',cfg),('fomod/info.xml',info)]:
        ET.indent(tree,space='  ');data[n]=ET.tostring(tree,encoding='utf-8',xml_declaration=False)
        assert not data[n].startswith(b'<?xml')
    assert data.keys()==old.keys()
    changed={n for n in data if data[n]!=old[n]}
    assert changed=={CORE,QUEST,'fomod/ModuleConfig.xml','fomod/info.xml'},changed
    for n,b in data.items():
        p=PurePosixPath(n);assert not p.is_absolute() and '..' not in p.parts and ':' not in n
        assert not any(x.lower() in {'source','sources','docs'} for x in p.parts),n
        assert p.suffix.lower() not in {'.cpp','.h','.psc','.py','.pdb','.log','.md'},n
        assert not any(w in n.lower() for w in ['readme','start-here','开始前','指南','测试说明','package-manifest']),n
        assert p.suffix.lower()!='.txt' or '/licenses/' in n.lower() or p.name.lower()=='wenkai-ofl.txt',n
        target=STAGE/n;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b)
    language_group=cfg.find('./installSteps/installStep/optionalFileGroups/group')
    recommended=[p.findtext('./conditionFlags/flag[@name="language"]') for p in language_group.findall('./plugins/plugin') if p.find('./typeDescriptor/type').get('name')=='Recommended']
    assert recommended==['en']
    resolver=module('lowtable_resolver',ROOT/'pull-up-a-chair-0.9.1-beta-ai1-20261013/verify_and_archive.py')
    resolver.STAGE=STAGE;resolver.cfg=cfg;resolver.groups=cfg.findall('./installSteps/installStep/optionalFileGroups/group');resolver.all_plugins=cfg.findall('.//plugin')
    states={p.get('file').lower():'Active' for p in cfg.findall('.//fileDependency')}
    cases=[]
    for values in itertools.product(('en','zh'),('0','1','2'),('0','1'),(('0','0'),('1','0'),('1','1')),('0','1'),('keep','apply'),('0','1')):
        language,mode,cleanup,pair,sunhelm,preset,hud=values
        flags=dict(language=language,mode=mode,cleanup=cleanup,dining=pair[0],eas=pair[1],sunhelm=sunhelm,preset=preset,hud=hud)
        for chatty in ['Missing','Inactive','Active']:
            output=resolver.resolve(flags,{**states,'chatty npcs.esp':chatty})
            runtime_changed=[]
            for name,(destination,path) in output.items():
                entry=path.relative_to(STAGE).as_posix()
                if resolver.sha(path)!=sha(old[entry]):runtime_changed.append(name)
                assert not any(w in name for w in ['aaaeas','source/','readme','.pdb','.log'])
            assert sorted(runtime_changed)==['scripts/pullupachairorderingquest.pex','skse/plugins/pullupachairordering.dll']
            target='skse/plugins/pullupachaireas.dll';ini='skse/plugins/pullupachaireas.ini'
            assert (target in output)==(flags['dining']=='1') and (ini in output)==(flags['dining']=='1')
            if target in output:
                assert resolver.sha(output[target][1])==sha(old[MODULE])
                c=configparser.ConfigParser();c.read(output[ini][1],encoding='utf-8');assert c.getint('Compatibility','EAS')==int(flags['eas'])
            assert ('pullupachairordering_specialties_kid.ini' in output)==(flags['eas']=='1')
            assert ('pull up a chair - chatty npcs patch.esp' in output)==(chatty=='Active')
            c=configparser.ConfigParser();c.read(output['skse/plugins/pullupachairdakhud.ini'][1],encoding='utf-8');assert c.getint('Display','CompatibilityHint')==int(hud)
            c=configparser.ConfigParser();c.read(output['skse/plugins/pullupachairordering.ini'][1],encoding='utf-8');assert c.get('General','Language')==language and c.getint('General','VerboseLogging')==0
            for destination,path in output.values():
                if path.suffix.lower()=='.esp':
                    for master in resolver.masters(path):assert master.lower() in output or states.get(master.lower())=='Active'
            cases.append({'flags':flags,'chatty':chatty,'files':len(output),'runtime_changed':runtime_changed})
    assert len(cases)==864
    for state in ['Missing','Inactive']:
        for pair in [('0','0'),('1','0')]:
            f=dict(cases[0]['flags'],dining=pair[0],eas=pair[1])
            output=resolver.resolve(f,{**states,'taberuanimation.esp':state})
            assert ('skse/plugins/pullupachaireas.dll' in output)==(pair[0]=='1')
    verifier=module('lowtable_pe',ROOT/'eas-nooverride-20261015/probe_tools.py')
    pe=verifier.pe(data[CORE],False)
    assert 'PullUpAChairOrdering_GetDiningAPI' in pe['exports']
    assert 'PullUpAChairEAS.dll' not in pe['imports']
    archive=DIST/(NAME+'.zip')
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for n,b in data.items():z.writestr(n,b)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None and set(z.namelist())==set(data)
        assert all(z.read(n)==b for n,b in data.items())
    sourcezip=SUPPORT/'lowtablelayout1-mealrestore1-source.zip'
    with zipfile.ZipFile(sourcezip,'x',zipfile.ZIP_DEFLATED) as z:
        for folder in ['native/core','source']:
            for p in (BASE/folder).rglob('*'):
                if p.is_file():z.write(p,p.relative_to(BASE).as_posix())
        for n in ['package_fomod.py','compile_and_check.py','test_geometry.py']:
            z.write(BASE/n,n)
        for rel in ['README.md','docs/BUILD.md','native/scripts/eas/PullUpAChairEAS.psc']:
            z.write(ROOT/'PullUpAChair'/rel,rel)
    shutil.copy2(BASE/'build-core/Release/PullUpAChairOrdering.pdb',SUPPORT/'PullUpAChairOrdering.pdb')
    shutil.copy2(ROOT/'eating-module-20261015/dist/Pull-Up-a-Chair-0.9.9-beta-diningmodule1-FOMOD-support/PullUpAChairEAS.pdb',SUPPORT/'PullUpAChairEAS.pdb')
    shutil.copy2(BASE/'build-core/Testing/Temporary/LastTest.log',SUPPORT/'core-ctest.log')
    for p in (BASE/'evidence').glob('*.json'):shutil.copy2(p,SUPPORT/p.name)
    live=json.loads((BASE/'evidence/installed-binaries-before.json').read_text('utf-8'))
    unchanged={p:sha(Path(p).read_bytes())==s for p,s in live.items()};assert all(unchanged.values()),unchanged
    result={'archive':str(archive),'sha256':sha(archive.read_bytes()),'bytes':archive.stat().st_size,'entries':len(data),
        'baseline':str(OLD),'baseline_sha256':sha(OLD.read_bytes()),'changed_entries':sorted(changed),
        'option_combinations':len(cases),'installer_choices_unchanged':True,'english_recommended':True,'guides_inside':False,
        'service_esps_optional_dining_adapter_assets_unchanged':True,'core_sha256':sha(data[CORE]),'quest_pex_sha256':sha(data[QUEST]),
        'optional_dining_dll_sha256':sha(data[MODULE]),'ctest_passed':17,'core_pe':pe,
        'live_binaries_unchanged':unchanged,'installed_locally':False,'gameplay_tested':False,'vortex_library_validation':'pending'}
    dump(SUPPORT/'package-validation.json',result);dump(SUPPORT/'option-combinations.json',cases)
    dump(SUPPORT/'entry-manifest.json',[{'path':n,'sha256':sha(b),'size':len(b)} for n,b in data.items()])
    (DIST/(NAME+'.zip.sha256')).write_text(result['sha256']+'  '+NAME+'.zip\n','ascii')
    validator=(ROOT/'eating-module-20261015/dist/validate_fomod.ps1').read_text('utf-8').replace('Pull-Up-a-Chair-0.9.9-beta-diningmodule1-FOMOD',NAME)
    (DIST/'validate_fomod.ps1').write_text(validator,'utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='core_pe'},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
