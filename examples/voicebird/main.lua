local P=passport
local calibrating,baseline=0,0
local y,velocity,wall,score,threshold,level,dead=145,0,218,0,15,0,false
local function draw()
 P.clear() P.graphics.sprite("app://sprites/bird",25,math.floor(math.max(0,math.min(286,y))))
 P.rect(math.floor(wall),0,12,130,0x58b8d8) P.rect(math.floor(wall),230,12,90,0x58b8d8)
 P.text('Voice Bird '..score..'  threshold '..threshold..'\n'..(calibrating>0 and 'Calibrating - stay quiet' or dead and 'OK: restart' or 'Speak to fly / OK flap'))
end
function on_start() draw() end
function on_permission(allowed) if allowed==1 then P.microphone(true) calibrating=1200 baseline=0 end draw() end
function on_button(b) if b==0 then threshold=math.min(90,threshold+2) elseif b==1 then threshold=math.max(2,threshold-2) elseif dead then y=145 velocity=0 wall=218 dead=false score=0 else velocity=-3 end draw() end
function on_audio(rms) level=rms if calibrating>0 and calibrating<1000 then baseline=baseline*.85+rms*.15 end end
function on_tick(dt)
 if calibrating>0 then calibrating=math.max(0,calibrating-dt) if calibrating==0 then threshold=math.min(90,math.max(5,math.floor(baseline+6))) end draw() return end
 if dead then return end if level>threshold then velocity=-2 end velocity=velocity+0.18 y=y+velocity wall=wall-2
 if wall<2 then wall=218 score=score+1 end
 if y<0 or y>286 or (wall<52 and wall>13 and (y<130 or y>214)) then dead=true end draw()
end
function on_stop() P.microphone(false) end
