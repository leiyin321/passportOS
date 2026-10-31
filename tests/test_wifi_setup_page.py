"""Execute the shipped portal JavaScript with a minimal DOM; no npm dependency."""
import gzip
import re
import json
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Portal(unittest.TestCase):
    def test_shared_credentials_and_safe_scan_render(self):
        node = shutil.which('node')
        self.assertIsNotNone(node, 'Node.js required for portal regression')
        html = (ROOT/'os/services/network/wifi_setup_page.html').read_text()
        encoded = bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',
                        (ROOT/'os/services/network/wifi_setup_page.inc').read_text()))
        self.assertEqual(gzip.decompress(encoded),html.encode(), 'Rebuild portal gzip asset')
        script = html.split('<script>')[1].split('</script>')[0]
        fixture = r'''
const assert=require('node:assert/strict');
const nodes=new Map();
const document={documentElement:{},querySelectorAll:()=>[],getElementById:id=>{
 if(!nodes.has(id))nodes.set(id,{value:'',dataset:{},click(){},focus(){},appendChild(n){this.items.push(n)},items:[]});
 return nodes.get(id);
},createElement:()=>({dataset:{},items:[],appendChild(n){this.items.push(n)}})};
const navigator={language:'zh-CN'};
const setTimeout=()=>{};
let confirmed=false;const confirm=()=>confirmed;
let scanRows=[],savedRows=[],portalState="waiting",deferred=false,stored=false;const calls=[];const fetch=async (url,options)=>{calls.push({url,options});return {ok:true,json:async()=>url==='/saved'?{networks:savedRows,status:0,capacity:8}:url==='/status'?{state:portalState,deferred,stored}:scanRows};};
'''
        checks = r'''
assert.deepEqual(parseShared('WIFI:T:WPA;S:Home;P:12345678;;'),{ssid:'Home',password:'12345678'});
assert.deepEqual(parseShared('WIFI:T:WPA;S:Home\\;Desk;P:1234\\:5678;;'),{ssid:'Home;Desk',password:'1234:5678'});
assert.deepEqual(parseShared('WIFI:T:nopass;S:Guest;;'),{ssid:'Guest',password:''});
for(const s of ['WIFI:T:WEP;S:a;P:12345678;;','WIFI:T:WPA;S:a;P:short;;','WIFI:T:WPA;S:a;S:b;P:12345678;;','WIFI:T:WPA;S:a;P:12345678','x','WIFI:T:WPA;S:;P:12345678;;'])assert.equal(parseShared(s),null);
assert(valid('中文网络','12345678'));assert(!valid('中'.repeat(11),'12345678'));
assert(!valid('x','中'.repeat(22)));
(async()=>{scanRows=[{ssid:'<img src=x onerror=alert(1)>',rssi:-42,secure:true}];await el('scan').onclick();const n=el('networks').items[0];assert(n.textContent.includes('<img'));assert.equal(n.innerHTML,undefined);assert.equal(el('scan').disabled,false);savedRows=['<b>A</b>','B'];await loadSaved();const row=el('saved').items.at(-2);assert.equal(row.items[0].textContent,'<b>A</b>');const button=row.items[1];let count=calls.length;await button.onclick();assert.equal(calls.length,count);confirmed=true;await button.onclick();let sent=calls.at(-1);assert.equal(sent.url,'/forget');assert.equal(sent.options.headers['X-Passport-Setup'],'1');assert.equal(sent.options.body.get('ssid'),'<b>A</b>');el('ssid').value='New';el('password').value='12345678';await el('form').onsubmit({preventDefault(){}});sent=calls.at(-1);assert.equal(sent.url,'/wifi');assert.equal(sent.options.headers['X-Passport-Setup'],'1');assert.equal(el('password').value,'');await row.items[2].onclick();assert.equal(calls.at(-1).url,'/connect');for(const state of ['connecting','failed','connected']){portalState=state;await pollConnection();assert.equal(el('connection').textContent,t(state));}el('phoneHotspot').checked=true;el('phoneHotspot').onchange();assert.equal(el('scan').disabled,true);el('password').value='12345678';await el('form').onsubmit({preventDefault(){}});assert.equal(calls.at(-1).url,'/hotspot');await row.items[2].onclick();assert.equal(calls.at(-1).url,'/connect-hotspot');deferred=stored=true;portalState='connecting';await pollConnection();assert.equal(el('connection').textContent,t('hotspotSaved'));console.log('Wi-Fi portal JavaScript PASS scan/import/saved connect/forget, status, deferred phone hotspot and same-origin mutations');})().catch(e=>{console.error(e);process.exitCode=1});
'''
        subprocess.run([node,'-e',fixture+script+checks],check=True)

if __name__ == '__main__':
    unittest.main()
