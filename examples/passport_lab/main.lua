local P=passport
local page=1
local page_count=5
local colors={0xf25353,0x42c978,0x4c82f0,0xf4f0e8,0x111923}
local color_names={'RED','GREEN','BLUE','WHITE','BLACK'}
local color_index=1
local key_count,release_count,long_count=0,0,0
local last_key,last_event='-','Choose Buttons to test input'
local mic_granted,mic_enabled=false,false
local rms,peak=0,0
local checks=tonumber(P.get('checks') or '0') or 0
local storage_status='Saved checks: '..checks

local function microphone(enabled)
 if enabled and mic_granted then
  mic_enabled=P.microphone(true) and true or false
 elseif mic_enabled then
  P.microphone(false)
  mic_enabled=false
 end
end

local function draw()
 P.clear()
 local details=''
 P.rect(6,5,204,28,0x1d2c40)
 P.icon('system.settings',18,182,10,0x72d9c0)
 P.rect(8,39,200,2,0x31455d)
 if page==1 then
  P.rect(8,49,200,92,colors[color_index])
  P.rect(8,150,200,25,0x203246)
  details='DISPLAY & BACKLIGHT\n'..color_names[color_index]..' color field\n'
   ..'OK: next color\nUP/DOWN: open next test\nChange brightness in Settings'
 elseif page==2 then
  P.rect(8,49,200,92,0x203246)
  P.rect(16,57,184,20,0x28415a)
  details='BUTTON / INPUT\n'
   ..'Last: '..last_key..' '..last_event..'\n'
   ..'Presses: '..key_count..'  Releases: '..release_count..'\n'
   ..'Hold UP/DOWN to leave this test'
 elseif page==3 then
  P.rect(8,49,200,92,0x203246)
  P.rect(16,105,184,18,0x101a29)
  P.rect(16,105,math.max(2,math.floor(rms*1.84)),18,0x62d4b4)
  details='AUDIO FEATURE TEST\n'
   ..(mic_enabled and 'MIC ON - local RMS only' or mic_granted and 'Mic ready' or 'Mic permission denied')
   ..'\nLevel: '..rms..'%   Peak: '..peak..'%\n'
   ..'No raw audio is stored or sent.'
 elseif page==4 then
  P.rect(8,49,200,92,0x203246)
  P.icon('system.battery',24,20,57,0x6cdb99)
  details='BATTERY & APP STORAGE\n'
   ..'Battery percent stays in the OS bar above.\n'
   ..storage_status..'\nOK: save a test record\nData is private to this app.'
 else
  P.rect(8,49,200,92,0x203246)
  details='SYSTEM-ONLY FACTORY TESTS\n'
   ..'Wi-Fi scanning, BLE advertising and sleep\n'
   ..'remain owned by Passport OS. MiniApps cannot\n'
   ..'control radios or enter system sleep.\n'
   ..'Use Settings for Wi-Fi and screen timeout.'
 end
 P.text('PASSPORT LAB  '..page..'/'..page_count..'\n'..details
  ..'\nUP/DOWN: test  OK: action  Hold OK: Home')
end

local function change_page(next_page)
 if page==3 and next_page~=3 then microphone(false) end
 page=next_page
 if page==3 then microphone(true) end
 draw()
end

function on_start()
 draw()
end

function on_permission(allowed)
 mic_granted=allowed==1
 if page==3 then microphone(mic_granted) end
 draw()
end

function on_button(key)
 if page==2 then
  key_count=key_count+1
  last_key=key==0 and 'UP' or key==1 and 'DOWN' or 'OK'
  last_event='PRESS'
 elseif key==0 then
  change_page((page+page_count-2)%page_count+1)
  return
 elseif key==1 then
  change_page(page%page_count+1)
  return
 elseif page==1 then
  color_index=color_index%#colors+1
 elseif page==3 then
  microphone(not mic_enabled)
 elseif page==4 then
  checks=checks+1
  local ok=P.set('checks',tostring(checks))
  storage_status=ok and ('Saved checks: '..checks) or 'Storage write failed'
 end
 draw()
end

function on_button_up(key)
 if page==2 then
  release_count=release_count+1
  last_key=key==0 and 'UP' or key==1 and 'DOWN' or 'OK'
  last_event='RELEASE'
  draw()
 end
end

function on_long(key)
 if page==2 then
  long_count=long_count+1
  last_key=key==0 and 'UP' or 'DOWN'
  last_event='LONG ('..long_count..')'
  if key==0 then change_page(1) elseif key==1 then change_page(3) else draw() end
 end
end

function on_audio(level)
 rms=math.max(0,math.min(100,level))
 peak=math.max(peak,rms)
end

function on_tick(dt)
 if page==3 then draw() end
end

function on_stop()
 microphone(false)
end
