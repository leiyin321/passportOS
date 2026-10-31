import os,subprocess,tempfile,json,unittest,unittest.mock
from pathlib import Path
class Runtime(unittest.TestCase):
 def run_app(self,app,events='',source=None,perms=None):
  with tempfile.TemporaryDirectory() as d:
   m=json.loads(Path(f'examples/{app}/manifest.json').read_text())
   if perms is not None:m['permissions']=perms
   Path(d,'manifest.json').write_text(json.dumps(m));p=Path(d,m['entry']);p.write_text(source if source is not None else Path(f'examples/{app}/{m["entry"]}').read_text())
   r=subprocess.run(['build/simulator/runtime',f'{d}/manifest.json',str(p),d],input=events+'stop\n',text=True,capture_output=True,timeout=5)
   self.assertEqual(r.returncode,0,r.stderr);return [json.loads(line) for line in r.stdout.splitlines()]
 def test_json_rejects_depth_and_nul_before_conversion(self):
  for raw in ('['*100+'0'+']'*100, '{"a":1,"a":2}', '{"a":"x\\u0000y"}', '['+','.join(['0']*256)+']'):
   source='function on_start() local value,err=passport.json.decode('+json.dumps(raw)+'); passport.text(value == nil and "rejected" or "accepted") end'
   frame=self.run_app('tetris',source=source)[0]
   self.assertEqual(frame['text'],'rejected',frame)
   self.assertFalse(frame['error'])
 def test_json_valid_bounds_and_embedded_nul_key(self):
  raw='['+','.join(['0']*255)+']'
  source='function on_start() local v=passport.json.decode('+json.dumps(raw)+'); passport.text(#v) end'
  self.assertEqual(self.run_app('tetris',source=source)[0]['text'],'255')
  source='function on_start() local s,e=passport.json.encode({["a"..string.char(0).."b"]=1}); passport.text(s == nil and "rejected" or "accepted") end'
  self.assertEqual(self.run_app('tetris',source=source)[0]['text'],'rejected')
 def test_tetris(self):
  frames=self.run_app('tetris','on_tick 500\non_button 0\non_button 2\n' * 40)
  self.assertEqual(frames[0]['viewport'],{'width':240,'height':320,'mode':'fullscreen'})
  self.assertTrue(all(not x['error'] for x in frames));self.assertLessEqual(max(x['peak'] for x in frames),32768);self.assertGreater(len(frames[-1]['draw']),0)
 def test_canvas_clips_to_fullscreen_viewport(self):
  frame=self.run_app('tetris',source='function on_start() passport.rect(239,319,9,9,0x123456) end')[0]
  self.assertEqual(frame['draw'],[[239,319,1,1,0x123456]])
 def test_voice_game_audio_permission_and_fullscreen(self):
  frames=self.run_app('voicebird','permission 1\non_audio 60\non_tick 50\n')
  self.assertEqual(frames[0]['viewport'],{'width':240,'height':320,'mode':'fullscreen'})
  self.assertTrue(frames[1]['microphone'])
  self.assertFalse(any(x['error'] for x in frames))
 def test_all_button_gesture_callbacks_and_push_to_talk(self):
  source='''
function on_button_down(key)
  if key==2 then passport.microphone(true); passport.text("pressed") end
end
function on_button_up(key)
  if key==2 then passport.microphone(false); passport.text("released") end
end
function on_button_short(key) passport.text("short " .. key) end
function on_button_long(key) passport.text("long " .. key) end
function on_button_double(key) passport.text("double " .. key) end
'''
  frames=self.run_app('voicebird','permission 1\non_button_down 2\non_button_long 2\non_button_up 2\non_button_short 2\non_button_double 2\n',source=source)
  self.assertTrue(frames[2]['microphone'])
  self.assertEqual(frames[2]['text'],'pressed')
  self.assertEqual(frames[3]['text'],'long 2')
  self.assertFalse(frames[4]['microphone'])
  self.assertEqual(frames[4]['text'],'released')
  self.assertEqual(frames[5]['text'],'short 2')
  self.assertEqual(frames[6]['text'],'double 2')
  self.assertTrue(all(not frame['error'] for frame in frames))
 def test_chat_standard_buttons(self):
  frames=self.run_app('ws-chat','on_button 2\non_button_up 2\n')
  self.assertTrue(all(not x['error'] for x in frames))
 def test_xiaozhi_provisioning_websocket_and_push_to_talk(self):
  frames=self.run_app('xiaozhi','permission 1\non_button_down 2\non_tick 500\non_tick 100\non_button_up 2\n')
  self.assertEqual(frames[0]['viewport'],{'width':240,'height':320,'mode':'fullscreen'})
  self.assertTrue(all(not x['error'] for x in frames),frames)
  self.assertTrue(frames[2]['ws_connected'])
  self.assertTrue(frames[2]['ws_auth_headers'])
  self.assertTrue(frames[3]['hello_sent'])
  self.assertTrue(frames[4]['audio_stream'])
  self.assertTrue(frames[4]['microphone'])
  self.assertTrue(frames[4]['listen_start_sent'])
  self.assertFalse(frames[5]['microphone'])
  self.assertTrue(frames[5]['audio_stream']) # playback remains available for TTS
  self.assertLessEqual(max(x['peak'] for x in frames),65536)
  self.assertIn([0,0,240,320,0x070b12],frames[0]['draw'])
  self.assertIn([8,68,224,68,0x10283a],frames[0]['draw'])
  self.assertEqual(frames[0]['text_color'],0xf7fbff)
 def test_xiaozhi_audio_start_failure_is_not_permission_denial(self):
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SIM_AUDIO_START_FAIL':'1'}):
   frames=self.run_app('xiaozhi','permission 1\non_button_down 2\non_tick 500\non_tick 100\non_tick 100\non_button_up 2\n')
  self.assertTrue(all(not x['error'] for x in frames),frames)
  self.assertIn('Audio unavailable',frames[-1]['text'])
  self.assertNotIn('Microphone permission denied',frames[-1]['text'])
  self.assertFalse(frames[-1]['microphone'])
  self.assertFalse(frames[-1]['listen_start_sent'])
  self.assertEqual(frames[-1]['audio_start_count'],1)
 def test_xiaozhi_permission_does_not_preallocate_audio_worker(self):
  frames=self.run_app('xiaozhi','permission 1\n')
  self.assertFalse(frames[-1]['audio_prepared'])
  self.assertEqual(frames[-1]['audio_start_count'],0)
 def test_xiaozhi_async_audio_error_keeps_permission_feedback_distinct(self):
  frames=self.run_app('xiaozhi','permission 1\non_button_down 2\non_tick 500\non_tick 100\naudio_failed 1\non_tick 100\n')
  self.assertIn('Audio unavailable',frames[-2]['text'])
  self.assertNotIn('Microphone permission denied',frames[-2]['text'])
  self.assertFalse(frames[-1]['microphone'])
  self.assertFalse(frames[-1]['audio_stream'])
  self.assertEqual(frames[-1]['audio_start_count'],1)
 def test_xiaozhi_downlink_opus_rates_and_rejection(self):
  for rate,channels,duration,valid in [(8000,1,60,True),(12000,1,60,True),(16000,1,60,True),(24000,1,60,True),(48000,1,20,True),(44100,1,60,False),(24000,2,60,False),(24000,1,120,False)]:
   reply=json.dumps({'type':'hello','transport':'websocket','audio_params':{'format':'opus','sample_rate':rate,'channels':channels,'frame_duration':duration}})
   with unittest.mock.patch.dict(os.environ,{'PASSPORT_SIM_HELLO':reply}):
    frames=self.run_app('xiaozhi','permission 1\non_button_down 2\non_tick 500\non_tick 100\n')
   self.assertFalse(any(x['error'] for x in frames),frames)
   self.assertEqual(frames[-1]['microphone'],valid,(rate,frames[-1]))
   self.assertEqual('Unsupported audio' in frames[-1]['text'],not valid)
 def test_xiaozhi_single_press_does_not_restart_pending_handshake(self):
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SIM_WS_CONNECTING':'1'}):
   frames=self.run_app('xiaozhi','permission 1\non_button_down 2\non_button_up 2\non_button_short 2\non_tick 500\n')
  self.assertFalse(any(x['error'] for x in frames))
  self.assertEqual(frames[-1]['ws_open_count'],1)
 def test_xiaozhi_retries_busy_http_submission_without_wifi_false_alarm(self):
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SIM_HTTP_REJECT_ONCE':'1'}):
   frames=self.run_app('xiaozhi','on_tick 500\non_tick 500\n')
  self.assertIn('HTTPS queue',frames[0]['text'])
  self.assertIn('Service config saved',frames[-1]['text'])
  self.assertTrue(all(not frame['error'] for frame in frames))
 def test_xiaozhi_transport_failure_stops_after_three_attempts(self):
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SIM_HTTP_STATUS':'-1'}):
   frames=self.run_app('xiaozhi','on_tick 1000\non_tick 2000\n'+'on_tick 1000\n'*20)
  self.assertTrue(all(not x['error'] for x in frames),frames)
  self.assertIn('HTTPS -1',frames[-1]['text'])
  self.assertIn('3/3',frames[-1]['text'])
  self.assertNotIn('Checking XiaoZhi',frames[-1]['text'])
 def test_xiaozhi_accepts_optional_empty_token(self):
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SIM_HTTP_BODY':'{"websocket":{"url":"wss://api.tenclass.net/xiaozhi/v1/"}}'}):
   frames=self.run_app('xiaozhi')
  self.assertFalse(frames[0]['error'])
  self.assertIn('Service config saved',frames[0]['text'])
 def test_xiaozhi_activation_is_visible_without_websocket_config(self):
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SIM_HTTP_BODY':'{"activation":{"code":"123456","message":"Activate device"}}'}):
   frames=self.run_app('xiaozhi')
  self.assertIn('123456',frames[0]['text'])
  self.assertIn('Activation code',frames[0]['text'])
 def test_text_color_api_bounds_and_default(self):
  frame=self.run_app('ws-chat',source='function on_start() passport.text("default") end')[0]
  self.assertEqual(frame['text_color'],0xe9f0fa)
  frame=self.run_app('ws-chat',source='function on_start() passport.text("custom",0x22cc88) end')[0]
  self.assertEqual(frame['text_color'],0x22cc88)
  frame=self.run_app('ws-chat',source='function on_start() passport.text("invalid",0x1000000) end')[0]
  self.assertIn('text color bounds',frame['error'])
 def test_system_display_mode_api(self):
  source='function on_start() passport.text(passport.system.display_mode()) end'
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SYSTEM_DISPLAY_MODE':'dark'}):
   frame=self.run_app('ws-chat',source=source)[0]
  self.assertEqual(frame['text'],'dark')
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SYSTEM_DISPLAY_MODE':'light'}):
   frame=self.run_app('ws-chat',source=source)[0]
  self.assertEqual(frame['text'],'light')
  self.assertEqual(frame['text_color'],0x172334)
 def test_xiaozhi_uses_app_owned_light_palette(self):
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SYSTEM_DISPLAY_MODE':'light'}):
   frame=self.run_app('xiaozhi')[0]
  self.assertEqual(frame['text_color'],0x172334)
  self.assertIn([0,0,240,320,0xf5f7fa],frame['draw'])
  self.assertIn([8,146,224,76,0xffffff],frame['draw'])
 def test_standard_viewport_api(self):
  frame=self.run_app('ws-chat')[0]
  self.assertEqual(frame['viewport'],{'width':240,'height':272,'mode':'standard'})
  self.assertIn('240x272 standard',frame['text'])
 def test_system_language_api(self):
  source='function on_start() passport.text(passport.system.language()) end'
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SYSTEM_LANGUAGE':'zh-CN'}):
   self.assertEqual(self.run_app('tetris',source=source)[0]['text'],'zh-CN')
  with unittest.mock.patch.dict(os.environ,{'PASSPORT_SYSTEM_LANGUAGE':'en'}):
   self.assertEqual(self.run_app('tetris',source=source)[0]['text'],'en')
 def test_declarative_viewport(self):
  frames=self.run_app('weather')
  self.assertEqual(frames[0]['viewport'],{'width':240,'height':272,'mode':'standard'})
  self.assertIn('Viewport: 240',frames[0]['text'])
 def test_invalid_display(self):
  with tempfile.TemporaryDirectory() as d:
   m=json.loads(Path('examples/tetris/manifest.json').read_text());m['display']['orientation']='landscape'
   Path(d,'manifest.json').write_text(json.dumps(m));Path(d,'main.lua').write_text('function on_start() end')
   r=subprocess.run(['build/simulator/runtime',f'{d}/manifest.json',f'{d}/main.lua',d],input='stop\n',text=True,capture_output=True,timeout=5)
   self.assertEqual(r.returncode,0,r.stderr)
   self.assertIn('Invalid display mode/orientation',json.loads(r.stdout.splitlines()[0])['error'])
 def test_loop(self):self.assertIn('budget',self.run_app('tetris',source='while true do end')[0]['error'])
 def test_oom(self):self.assertTrue(self.run_app('tetris',source='local t={} for i=1,100000 do t[i]=i end')[0]['error'])
 def test_unsafe(self):self.assertFalse(self.run_app('tetris',source='assert(io==nil and os==nil and debug==nil and package==nil and load==nil and dofile==nil)')[0]['error'])
 def test_canvas(self):self.assertIn('draw bounds',self.run_app('tetris',source='passport.rect(-1,0,2,2,0)')[0]['error'])
 def test_canvas_integer_overflow(self):self.assertIn('draw bounds',self.run_app('tetris',source='passport.rect(2147483647,0,2147483647,1,0)')[0]['error'])
 def test_denied(self):self.assertIn('permission',self.run_app('tetris',source='passport.set("count","1")',perms=['ui'])[0]['error'])
 def test_voice_denied(self):self.assertFalse(self.run_app('voicebird')[0]['microphone'])
 def test_catch_removed(self):self.assertFalse(self.run_app('tetris',source='assert(pcall==nil and xpcall==nil and setmetatable==nil)')[0]['error'])
 def test_counter(self):
  f=self.run_app('counter','on_button_short 0\non_button_short 0\non_button_short 2\n');self.assertIn('2',f[-1]['text']);self.assertFalse(f[-1]['error'])
 def test_passport_lab_factory_capabilities(self):
  frames=self.run_app('passport_lab','permission 1\non_button_short 1\non_button_short 0\non_button_up 0\non_button_long 1\non_audio 48\non_tick 100\non_button_short 1\non_button_short 2\n')
  self.assertEqual(frames[0]['viewport'],{'width':240,'height':272,'mode':'standard'})
  self.assertIn('DISPLAY & BACKLIGHT',frames[0]['text'])
  self.assertIn('BUTTON / INPUT',frames[2]['text'])
  self.assertTrue(frames[5]['microphone'])
  self.assertIn('AUDIO FEATURE TEST',frames[6]['text'])
  self.assertFalse(frames[-1]['microphone'])
  self.assertIn('Saved checks: 1',frames[-1]['text'])
  self.assertLessEqual(max(frame['peak'] for frame in frames),32768)
  self.assertTrue(all(not frame['error'] for frame in frames))
if __name__=='__main__':unittest.main(verbosity=2)
