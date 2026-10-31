local P=passport
local show_sprite=false
local cell_w,cell_h,board_x,board_y=20,15,20,42
local board,score,best,piece,x,y,rotation,clock,over
local shapes={{{1,1,1,1}},{{1,1},{1,1}},{{0,1,0},{1,1,1}},{{1,0,0},{1,1,1}},{{0,0,1},{1,1,1}},{{0,1,1},{1,1,0}},{{1,1,0},{0,1,1}}}
local function cells()
 local s=shapes[piece]
 for i=1,rotation do
  local r={} for a=1,#s[1] do r[a]={} for b=1,#s do r[a][b]=s[#s-b+1][a] end end s=r
 end return s
end
local function fits(nx,ny,nr)
 local old=rotation rotation=nr local s=cells() rotation=old
 for a,row in ipairs(s) do for b,v in ipairs(row) do if v==1 then local xx,yy=nx+b-1,ny+a-1
  if xx<1 or xx>10 or yy>18 or (yy>0 and board[yy]:byte(xx)~=48) then return false end
 end end end return true
end
local function spawn() piece=math.random(7) x=4 y=1 rotation=0 if not fits(x,y,rotation) then over=true end end
local function save()
 P.set('game',table.concat(board)..';'..score..';'..best..';'..piece..';'..x..';'..y..';'..rotation..';'..(over and 1 or 0))
end
local function reset()
 board={} for a=1,18 do board[a]="0000000000" end score=0 over=false clock=0 spawn()
end
local function draw()
 P.clear()
 for a=1,18 do
  local row=board[a]
  local b=row:find("1",1,true)
  while b do
   local stop=row:find("0",b,true) or 11
   P.rect(board_x+(b-1)*cell_w,board_y+(a-1)*cell_h,(stop-b)*cell_w-1,cell_h-1,0x58b8d8)
   b=row:find("1",stop,true)
  end
 end
 if not over then for a,row in ipairs(cells()) do for b,v in ipairs(row) do if v==1 and y+a>1 then P.rect(board_x+(x+b-2)*cell_w,board_y+(y+a-2)*cell_h,cell_w-1,cell_h-1,0xf5bc61) end end end end
 if show_sprite then P.graphics.sprite('app://sprites/tile',204,4) end
 P.text('Tetris   '..score..' / '..best..(over and '\nGame over - OK restart' or '\nUP left  DOWN right  OK rotate'))
end
local function drop()
 if fits(x,y+1,rotation) then y=y+1 else
  for a,row in ipairs(cells()) do for b,v in ipairs(row) do if v==1 then local yy,xx=y+a-1,x+b-1 board[yy]=board[yy]:sub(1,xx-1).."1"..board[yy]:sub(xx+1) end end end
  local a=18 while a>=1 do local full=not board[a]:find("0",1,true)
   if full then table.remove(board,a) table.insert(board,1,"0000000000") score=score+100 else a=a-1 end
  end
  best=math.max(best,score) spawn() save()
 end
end
function on_start()
 local v=P.display.get_viewport()
 cell_w=math.floor(v.width/12) cell_h=math.floor((v.height-50)/18)
 board_x=math.floor((v.width-cell_w*10)/2) board_y=math.max(40,math.floor((v.height-cell_h*18)/2))
 best=0 reset() local data=P.get('game')
 if data and #data>=180 then
  for a=1,18 do board[a]=data:sub((a-1)*10+1,a*10) end
  local fields={} local start=182
  while start<=#data+1 do local stop=data:find(';',start,true) or #data+1 fields[#fields+1]=tonumber(data:sub(start,stop-1)) or 0 start=stop+1 end
  score=fields[1] or 0 best=fields[2] or 0 piece=fields[3] or 1 x=fields[4] or 4 y=fields[5] or 1 rotation=fields[6] or 0 over=fields[7]==1
  if piece<1 or piece>7 or rotation<0 or rotation>3 or x<1 or x>10 or y<1 or y>18 then reset() elseif not fits(x,y,rotation) then over=true end
 end draw()
end
function on_button(b)
 if over then if b==2 then reset() end else
 if b==0 and fits(x-1,y,rotation) then x=x-1 elseif b==1 and fits(x+1,y,rotation) then x=x+1 elseif b==2 and fits(x,y,(rotation+1)%4) then rotation=(rotation+1)%4 end end draw()
end
function on_tick(dt) if not over then clock=clock+dt if clock>=500 then clock=0 drop() draw() end end end
function on_stop() save() end

function on_long(b) if b==0 then show_sprite=not show_sprite draw() end end
