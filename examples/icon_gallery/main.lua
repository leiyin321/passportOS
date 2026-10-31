local P=passport
local ids={'system.wifi','system.battery','system.back','system.settings','system.microphone','system.cloud','system.gamepad','system.chat','system.home','system.download','system.refresh','system.unknown'}
local color,size=0x68d5e8,24
local function draw()
 P.clear() P.text('Icon Gallery - 12 shared icons\nUP theme / DOWN size / OK mask')
 for i,id in ipairs(ids) do P.ui.icon(id,size,((i-1)%4)*50,math.floor((i-1)/4)*40,color) end
 P.graphics.sprite('app://masks/test',185,140)
end
function on_start() draw() end
function on_button(b) if b==0 then color=color==0x68d5e8 and 0xf5bc61 or 0x68d5e8 elseif b==1 then size=size==24 and 14 or 24 end draw() end
