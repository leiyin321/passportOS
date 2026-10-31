#!/usr/bin/env python3
"""D0-D6 display matrix. Requires the authorized USB Passport and local services."""
import sys,time,json
from pathlib import Path
sys.path.insert(0,'tests/passport')
from hardware import Board

b=Board('/dev/cu.usbmodem101','local/hardware-display.log')
try:
    b.cmd('home')
    for name,mode,width,height in [
        ('weather','standard',240,272),('ws-chat','standard',240,272),
        ('tetris','fullscreen',240,320),('voicebird','fullscreen',240,320)]:
        b.install(f'dist/{name}.pap')
        b.cmd(f'launch org.passport.{name}')
        time.sleep(1)
        view=b.view()
        assert view['active'] and not view['error'],(name,view)
        assert (view['display_mode'],view['viewport_width'],view['viewport_height'])==(mode,width,height),(name,view)
        if name=='weather':
            time.sleep(3);view=b.view();assert not view['error'] and 'Temperature C:' in view['text'],view
            # NVS can be reset by an intentional merged-image refresh. Cached weather must still render;
            # live-fetch acceptance is recorded separately from this layout/runtime smoke test.
        elif name=='ws-chat':
            time.sleep(.5);view=b.view()
            assert not view['error'] and any(s in view['text'] for s in ('Sent','Channel','Not connected','No approved local service')),view
            # A real echo requires Wi-Fi; connection rejection is the expected offline path.
        elif name=='tetris':
            for key in (0,1,2,0,2,1): b.cmd(f'key {key}')
            view=b.view();assert view['images']>=0 and not view['error'],view
        else:
            # Exercise the OS microphone consent route and the real local audio worker.
            b.cmd('key 2');deadline=time.monotonic()+8
            while True:
                time.sleep(.25);view=b.view()
                assert view['active'] and not view['error'],view
                if view['microphone']:break
                assert time.monotonic()<deadline,view
        print(name,'PASS',view,flush=True)
        b.cmd('metrics');b.cmd('home');time.sleep(.1)
        closed=b.view();assert closed['asset_bytes']==0 and not closed['active'],closed
        print(name,'exit resources PASS',closed,flush=True)
    b.cmd('metrics')
    print('D5 four-app device matrix PASS',flush=True)
    start=time.monotonic()
    for i in range(200):
        name='tetris' if i%2==0 else 'counter'
        mode='fullscreen' if name=='tetris' else 'standard'
        b.cmd(f'launch org.passport.{name}')
        view=b.view()
        assert view['active'] and view['display_mode']==mode and not view['error'],(i,name,view)
        b.cmd('home')
        view=b.view()
        assert not view['active'] and view['asset_bytes']==0 and view['images']==0,(i,view)
        if i%50==49: print('display switch/exit cycles',i+1,'PASS',view,flush=True)
    print('D6 200 mixed standard/fullscreen launch, render, exit cycles PASS seconds',round(time.monotonic()-start,2),flush=True)
    b.cmd('metrics')
finally:
    try:b.cmd('home')
    finally:b.close()
