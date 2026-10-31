local P = passport
local lang, page, display_mode = "en", 1, "dark"
local phase, notice = "setup", ""
local activation_code, activation_message = "", ""
local ws_url, ws_token, device_id, client_id = "", "", "", ""
local socket_pending, hello_sent, session_id = false, false, ""
local config_pending, config_attempts = false, 0
local ptt_held, listening, speaking = false, false, false
local heard, answer = "", ""
local elapsed_ms, last_poll, wait_ms = 0, 0, 0

local words = {
 en = {
  title="XIAOZHI AI", voice="VOICE COMPANION", conversation="CONVERSATION", settings="CONNECTION",
  hold="Hold OK to talk", waiting="Waiting for network…", ready="Ready · hold OK", speaking="XiaoZhi is speaking",
  setup="Checking XiaoZhi service…", activate="Activate device in XiaoZhi account", code="Activation code",
  setup_error="Service setup failed", audio_error="Audio unavailable; release OK and retry", no_config="No XiaoZhi service config",
  transcript="YOU SAID", reply="XIAOZHI", empty="Your conversation appears here", check="OK: check service",
  server="Service", connected="Connected", disconnected="Not connected", mic="Microphone streams only while OK is held",
  press="Hold OK: speak   UP/DOWN: pages", back="Double OK: leave", error="Network or protocol error",
  denied="Microphone permission denied", unsupported="Unsupported audio settings from server",
  disconnected_msg="Connection closed", connecting="Opening secure connection…", setup_ok="Service config saved",
  done="Stopped listening", notice="Say something after the tone", action="OK: setup / reconnect",
 },
 ["zh-CN"] = {
  title="小智 AI", voice="语音伙伴", conversation="对话记录", settings="连接设置",
  hold="按住 OK 说话", waiting="等待网络…", ready="已就绪 · 按住 OK 说话", speaking="小智正在回复",
  setup="正在检查小智服务…", activate="请在小智账号中激活设备", code="激活码",
  setup_error="服务配置失败", audio_error="麦克风暂不可用，请重试", no_config="尚无小智服务配置",
  transcript="你说", reply="小智回复", empty="对话内容会显示在这里", check="OK：检查服务",
  server="服务地址", connected="已连接", disconnected="未连接", mic="仅在按住 OK 时传输麦克风音频",
  press="按住 OK：说话   上下键：切页", back="双击 OK：退出",
  error="网络或协议错误", denied="麦克风权限被拒绝", unsupported="服务器要求了不支持的音频格式",
  disconnected_msg="连接已关闭", connecting="正在建立安全连接…", setup_ok="服务配置已保存",
  done="已停止聆听", notice="请在提示音后开始说话", action="OK：配置服务 / 重连",
 }
}
local function t(k) return words[k] or k end
local function clip_utf8(s, limit)
  if type(s) ~= "string" or #s <= limit then return s or "" end
  local n = limit
  while n > 0 do
    local b = string.byte(s, n + 1)
    if not b or b < 128 or b >= 192 then break end
    n = n - 1
  end
  return string.sub(s, 1, n)
end
local function json_encode(v)
  local body, err = P.json.encode(v)
  if not body then notice = err or t("error"); return nil end
  return body
end
local function draw_text(value)
  P.text(value, display_mode == "light" and 0x172334 or 0xf7fbff)
end
local function draw()
  P.clear()
  local light = display_mode == "light"
  -- Match the system appearance while keeping MiniApp colors explicitly owned here.
  P.rect(0, 0, 240, 320, light and 0xf5f7fa or 0x070b12)
  P.rect(0, 0, 240, 58, light and 0xe6edf4 or 0x101c2b)
  P.rect(0, 56, 240, 2, light and 0x007f6b or 0x39c9ad)
  P.icon("system.app-default", 36, 194, 10, light and 0x007f6b or 0x6ee7ca)
  if page == 1 then
    local status_color = light and (speaking and 0xfff1d6 or listening and 0xf1e9ff or 0xe4f6f0)
      or (speaking and 0x382714 or listening and 0x21172d or 0x10283a)
    local accent = light and (speaking and 0xa65c00 or listening and 0x7041ad or 0x007f6b)
      or (speaking and 0xffc56e or listening and 0xc59bff or 0x61e1c0)
    local status = phase == "ready" and (speaking and t("speaking") or ptt_held and t("notice") or t("ready")) or t(phase)
    P.rect(8, 68, 224, 68, status_color)
    P.rect(8, 68, 4, 68, accent)
    P.rect(8, 146, 224, 76, light and 0xffffff or 0x101827)
    P.rect(8, 232, 224, 76, light and 0xe9eff5 or 0x0d1724)
    P.icon("system.microphone", 36, 184, 82, accent)
    draw_text(t("title") .. "\n" .. t("voice") .. "\n\n" .. status .. "\n" .. notice .. "\n\n" .. t("conversation") .. "\n" .. t("transcript") .. ": " .. (heard ~= "" and clip_utf8(heard, 48) or t("empty")) .. "\n\n" .. t("reply") .. ": " .. clip_utf8(answer, 48) .. "\n\n" .. t("press") .. "\n" .. t("back"))
  elseif page == 2 then
    P.rect(8, 68, 224, 104, light and 0xe4f1f8 or 0x10283a)
    P.rect(8, 182, 224, 104, light and 0xffffff or 0x142033)
    draw_text(t("title") .. "\n" .. t("conversation") .. "\n\n" .. t("transcript") .. "\n" .. (heard ~= "" and clip_utf8(heard, 72) or t("empty")) .. "\n\n" .. t("reply") .. "\n" .. clip_utf8(answer, 72) .. "\n\n" .. t("press") .. "\n" .. t("back"))
  else
    local svc = ws_url ~= "" and ws_url or "api.tenclass.net/xiaozhi/ota/"
    P.rect(8, 68, 224, 108, light and 0xe4f1f8 or 0x10283a)
    P.rect(8, 186, 224, 122, light and 0xffffff or 0x101827)
    P.icon("system.settings", 32, 188, 78, light and 0x007f6b or 0x61e1c0)
    draw_text(t("title") .. "\n" .. t("settings") .. "\n\n" .. t("server") .. ": " .. clip_utf8(svc, 42) .. "\n" .. (P.ws_connected() and t("connected") or t("disconnected")) .. "\n\n" .. t("mic") .. "\n\n" .. (activation_code ~= "" and (t("code") .. ": " .. activation_code .. "\n" .. clip_utf8(activation_message, 48) .. "\n\n") or "") .. t("action") .. "\n" .. t("back"))
  end
end
local function send_json(obj)
  local s = json_encode(obj)
  return s and P.ws_send(s, false) or false
end
local function close_session()
  if listening then send_json({type="listen", state="stop", session_id=session_id}) end
  listening, speaking = false, false
  P.audio.stream_stop()
  P.microphone(false)
end
local function begin_listening()
  if not ptt_held or not P.ws_connected() or phase ~= "ready" or listening then return end
  if not P.audio.stream_start() then
    ptt_held = false
    notice = t("audio_error"); draw(); return
  end
  if not send_json({type="listen", state="start", mode="manual", session_id=session_id}) then
    P.audio.stream_stop(); notice = t("error"); draw(); return
  end
  listening, speaking = true, false
  P.microphone(true)
  notice = ""
  draw()
end
local function connect_ws()
  if ws_url == "" then phase = "setup"; notice = t("no_config"); draw(); return end
  if P.ws_connected() then phase = "ready"; begin_listening(); return end
  if socket_pending then return end
  local auth = ws_token ~= "" and (string.find(ws_token, " ", 1, true) and ws_token or ("Bearer " .. ws_token)) or nil
  socket_pending = P.ws_connect(ws_url, {
    ["Authorization"] = auth,
    ["Protocol-Version"] = "1",
    ["Device-Id"] = device_id,
    ["Client-Id"] = client_id,
  }) and true or false
  wait_ms = 0
  hello_sent = false
  phase = "waiting"
  notice = t("connecting")
  if not socket_pending then phase="error"; notice = "WSS: " .. t("error") end
  draw()
end
local function config_failure(status)
  phase = "error"
  notice = t("setup_error") .. " (HTTPS " .. status .. ") " .. config_attempts .. "/3"
  wait_ms = config_attempts < 3 and -1000 * config_attempts or 0
  draw()
end
local function configure()
  if config_pending or wait_ms < 0 then return end
  if not device_id or device_id == "" then
    local id = P.system.public_device_identity()
    if not id then notice = t("error"); draw(); return end
    device_id, client_id = id.device_id, id.client_id
  end
  local body = json_encode({
    application={name="Passport OS XiaoZhi MiniApp", version="0.2.8"},
    board={type="ai-passport", name="FoloToy AI Passport", runtime="Passport OS"},
  })
  if not body then draw(); return end
  config_attempts = config_attempts + 1
  phase = "setup"; notice = t("setup"); draw()
  local ok = P.http.request("POST", "https://api.tenclass.net/xiaozhi/ota/", {
    ["Activation-Version"]="1",
    ["Device-Id"]=device_id,
    ["Client-Id"]=client_id,
    ["User-Agent"]="PassportOS-MiniApp/0.2.8",
    ["Accept-Language"]=lang,
    ["Content-Type"]="application/json",
  }, body)
  config_pending = ok and true or false
  if ok then
    wait_ms = 0
  else
    config_failure("queue")
  end
end
local function start_ptt()
  ptt_held = true
  if not P.ws_connected() then connect_ws() else begin_listening() end
end
local function handle_json(raw)
  local msg, err = P.json.decode(raw)
  if type(msg) ~= "table" then notice = err or t("error"); return end
  local typ = msg.type
  if typ == "hello" then
    local ap = msg.audio_params or {}
    local rate = ap.sample_rate or 16000
    local valid_rate = rate==8000 or rate==12000 or rate==16000 or rate==24000 or rate==48000
    if msg.transport ~= "websocket" or (ap.format and ap.format ~= "opus") or
       not valid_rate or (ap.channels and ap.channels ~= 1) or
       (ap.frame_duration and (type(ap.frame_duration) ~= "number" or ap.frame_duration <= 0 or ap.frame_duration > 60)) then
      phase = "setup"; notice = t("unsupported") .. " (" .. tostring(rate) .. " Hz)"; P.ws_close(); return
    end
    session_id = type(msg.session_id) == "string" and msg.session_id or ""
    phase = "ready"; notice = "Opus " .. rate .. " Hz"
  elseif typ == "stt" then
    heard = clip_utf8(msg.text, 110)
  elseif typ == "llm" then
    if type(msg.text) == "string" then answer = clip_utf8(msg.text, 110) end
  elseif typ == "tts" then
    if msg.state == "start" then speaking = true
    elseif msg.state == "stop" then speaking = false
    elseif msg.state == "sentence_start" and type(msg.text) == "string" then answer = clip_utf8(msg.text, 110) end
  elseif typ == "alert" then
    notice = clip_utf8(msg.message or msg.status or t("error"), 72)
  elseif typ == "goodbye" then
    close_session(); phase = "ready"
  elseif typ == "system" or typ == "mcp" or typ == "iot" then
    -- This MiniApp deliberately exposes no remote device-control or OS commands.
  end
end

function on_start()
  lang = P.system.language()
  words = words[lang] or words.en
  display_mode = P.system.display_mode and P.system.display_mode() or "dark"
  ws_url = P.get("xiaozhi_ws_url") or ""
  ws_token = P.get("xiaozhi_ws_token") or ""
  local id = P.system.public_device_identity()
  if id then device_id, client_id = id.device_id, id.client_id end
  draw()
  if ws_url == "" then configure() else phase="setup"; notice=t("action"); draw() end
end
function on_permission(allowed)
  if allowed ~= 1 then phase="setup"; notice=t("denied"); draw(); return end
  if ws_url == "" then configure() end
end
function on_http(status, body)
  if socket_pending then
    if status == 101 then return end
    socket_pending = false
    phase="error"; notice="WSS: " .. t("error"); draw(); return
  end
  config_pending = false
  socket_pending = false
  if status == -1 or status == 408 or status == 429 or status >= 500 then
    config_failure(status); return
  end
  wait_ms = 0
  if status ~= 200 then phase="setup"; notice=t("setup_error") .. " (HTTP " .. status .. ")"; draw(); return end
  local data, err = P.json.decode(body)
  if type(data) ~= "table" then phase="setup"; notice=err or t("setup_error"); draw(); return end
  config_attempts = 0
  local activation = type(data.activation) == "table" and data.activation or {}
  activation_code = type(activation.code) == "string" and activation.code or ""
  activation_message = type(activation.message) == "string" and clip_utf8(activation.message, 84) or ""
  local config = type(data.websocket) == "table" and data.websocket or {}
  local url = config.url or ""
  local token = config.token or ""
  if type(url) == "string" and string.sub(url, 1, 6) == "wss://" and type(token) == "string" then
    ws_url, ws_token = url, token
    P.set("xiaozhi_ws_url", ws_url)
    P.set("xiaozhi_ws_token", ws_token)
    phase="setup"; notice=t("setup_ok")
    if activation_code ~= "" then page=3; notice=t("activate") end
  elseif activation_code ~= "" then
    page=3; phase="setup"; notice=t("activate")
  else
    phase="setup"; notice=t("setup_error")
  end
  draw()
end
function on_message(data, opcode)
  if opcode == 2 then
    if speaking then P.audio.play_opus(data) end
  elseif opcode == 1 then handle_json(data) end
  draw()
end
function on_audio_error(_code)
  ptt_held = false
  close_session()
  phase = P.ws_connected() and "ready" or "setup"
  notice = t("audio_error")
  draw()
end
function on_audio_frame(frame)
  if listening and ptt_held and P.ws_connected() then
    if not P.ws_send(frame, true) then notice=t("error") end
  end
end
function on_button_down(key)
  if key == 2 and page == 1 then start_ptt() end
end
function on_button_up(key)
  if key == 2 then
    ptt_held = false
    if listening then close_session(); notice=t("done") end
    draw()
  end
end
function on_button_short(key)
  if key == 0 then page = (page + 1) % 3 + 1
  elseif key == 1 then page = page % 3 + 1
  elseif page == 3 then
    if not config_pending then config_attempts=0; wait_ms=0; configure() end
  elseif page == 1 and not P.ws_connected() then connect_ws()
  elseif page == 2 then heard, answer = "", "" end
  draw()
end
function on_button_double(key)
  if key == 2 then close_session(); P.ws_close() end
end
function on_tick(now)
  elapsed_ms = elapsed_ms + now
  if wait_ms < 0 then
    wait_ms = wait_ms + now
    if wait_ms >= 0 then wait_ms = 0; configure() end
  end
  if phase == "waiting" and P.ws_connected() and not hello_sent then
    socket_pending = false
    local hello = json_encode({type="hello", version=1, features={mcp=false}, transport="websocket",
      audio_params={format="opus", sample_rate=16000, channels=1, frame_duration=60}})
    if hello and P.ws_send(hello, false) then hello_sent=true; notice=t("waiting") end
  elseif phase == "waiting" and hello_sent and not P.ws_connected() then
    phase="setup"; socket_pending=false; notice=t("disconnected_msg")
  end
  if phase == "waiting" then
    wait_ms = wait_ms + now
    if wait_ms >= 10000 then
      P.ws_close(); close_session(); phase="setup"; socket_pending=false
      hello_sent=false; notice="WSS/hello: " .. t("error")
    end
  end
  if phase == "ready" then begin_listening() end
  if elapsed_ms - last_poll >= 500 then last_poll=elapsed_ms; draw() end
end
function on_stop()
  ptt_held=false; close_session(); P.ws_close()
end
