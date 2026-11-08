"""Run the actual WKWebView transport in an iOS Simulator, without the Source engine."""
import json
import platform
import plistlib
from pathlib import Path
import subprocess
import time

root=Path(__file__).resolve().parents[2]
artifacts=root/'artifacts/ios-html'
artifacts.mkdir(parents=True,exist_ok=True)
app=root/'build-html-simulator/HTMLHarness.app'
app.mkdir(parents=True,exist_ok=True)
bundle='com.kindmops.html-transport-test'
info={
 'CFBundleIdentifier':bundle,'CFBundleExecutable':'HTMLHarness','CFBundleName':'HTMLHarness',
 'CFBundlePackageType':'APPL','CFBundleVersion':'1','CFBundleShortVersionString':'1.0',
 'CFBundleSupportedPlatforms':['iPhoneSimulator'],'MinimumOSVersion':'16.0',
 'LSRequiresIPhoneOS':True,'UIDeviceFamily':[1,2],'UILaunchScreen':{},
 'UISupportedInterfaceOrientations':['UIInterfaceOrientationPortrait','UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight'],
 'UIApplicationSceneManifest':{'UIApplicationSupportsMultipleScenes':False,'UISceneConfigurations':{
  'UIWindowSceneSessionRoleApplication':[{'UISceneConfigurationName':'Harness','UISceneClassName':'UIWindowScene','UISceneDelegateClassName':'HarnessScene'}]}}
}
(app/'Info.plist').write_bytes(plistlib.dumps(info))
sdk=subprocess.check_output(['xcrun','--sdk','iphonesimulator','--show-sdk-path'],text=True).strip()
subprocess.run(['xcrun','--sdk','iphonesimulator','clang++','-std=c++17','-fobjc-arc',
 '-target',platform.machine()+'-apple-ios16.0-simulator','-isysroot',sdk,
 str(root/'tests/ios_html/main.mm'),str(root/'platform/html/html_panel.mm'),
 '-framework','UIKit','-framework','WebKit','-framework','Foundation','-framework','ImageIO',
 '-framework','CoreGraphics','-o',str(app/'HTMLHarness')],check=True)
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
inventory=json.loads(subprocess.check_output(['xcrun','simctl','list','devices','available','-j'],text=True))
phones=[d for runtime,devices in inventory['devices'].items() if '.iOS-' in runtime for d in devices if d.get('isAvailable') and d['name'].startswith('iPhone')]
if not phones:raise SystemExit('No available iPhone Simulator runtime on this runner')
device=next((d for d in phones if d['state']=='Booted'),phones[0])
udid=device['udid']
try:
 if device['state']!='Booted':subprocess.run(['xcrun','simctl','boot',udid],check=True)
 subprocess.run(['xcrun','simctl','bootstatus',udid,'-b'],check=True)
 subprocess.run(['xcrun','simctl','install',udid,str(app)],check=True)
 container=Path(subprocess.check_output(['xcrun','simctl','get_app_container',udid,bundle,'data'],text=True).strip())
 result=container/'Documents/html-result.json'
 result.unlink(missing_ok=True)
 subprocess.run(['xcrun','simctl','launch',udid,bundle],check=True)
 deadline=time.monotonic()+90
 while not result.exists() and time.monotonic()<deadline:time.sleep(.5)
 if not result.exists():raise RuntimeError('HTML harness did not produce a result within 90 seconds')
 report=json.loads(result.read_text())
 report['simulator']=device['name']
 (artifacts/'result.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report,indent=2),flush=True)
 if not report['passed']:raise RuntimeError(report['detail'])
finally:
 subprocess.run(['xcrun','simctl','shutdown',udid],check=False)
