local P=passport
local page=0
local ids={'00','01','02','03','04','05','06','07','08','09','10','11','12','13','14','15','16','17','18','19','20','21','22','23','24','25','26','27','28','29'}
local function draw()
 P.clear() P.text('Store Grid '..(page+1)..'/5 - 30 icons\nUP / DOWN: scroll')
 for i=1,6 do P.graphics.sprite('app://icons/item-'..ids[page*6+i],20+((i-1)%3)*65,15+math.floor((i-1)/3)*75) end
end
function on_start() page=tonumber(P.get('page')) or 0 draw() end
function on_button(b) page=(page+(b==0 and 4 or 1))%5 draw() end
function on_stop() P.set('page',tostring(page)) end
