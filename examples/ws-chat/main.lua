local P=passport
local count=0
function on_start()
 local v=P.display.get_viewport()
 local ok=P.ws_connect('local')
 P.text('Chat '..v.width..'x'..v.height..' '..v.mode..'\n'..(ok and 'Live channel connecting...\nOK: text / UP: binary' or 'No approved local service\nApprove Mac endpoint in Store'))
end
function on_button(b)
 if b==2 or b==0 then
  count=count+1
  local message=b==0 and 'binary\0'..count or 'Passport ping '..count
  local ok=P.ws_send(message,b==0)
  P.text(ok and 'Sent '..count..' - waiting for echo' or 'Not connected / rate limited')
 elseif b==1 then on_start() end
end
function on_message(message,opcode)
 P.text('Channel '..(opcode==2 and 'binary' or 'text')..'\n'..(opcode==2 and tostring(#message)..' bytes' or message))
end
