
local netcmd = require("netcmd_define")
local chuck = require("chuck")
local buffer = chuck.buffer
local packet = chuck.packet
local log = chuck.log

local rooms = {} --所有的房间

local room = {}

local room_id = 0

function room.new(id)
  local o = {}
  o.__index = room     
  setmetatable(o,o)
  o.users = {}
  o.user_size = 0
  o.id = id
  return o
end

function room:RandPos()
	return {x=0,y=0,z=0}
end

function room:BroadCastMsg(msg,except)
	for k,v in pairs(self.users) do
		if v ~= except then
			v.session:Send(msg)
		end
	end
end

function room:Enter(user)
	local avatar = {
		user = user,
		pos  = self:RandPos(),         --坐标
		size = 100,                    --大小
		dir  = {x=0,y=0,z=0},
		see_able = false,
		dead = false
	}
	user.avatar = avatar
	user.room = self
	self.users[user.userid] = user
	self.user_size = self.user_size + 1
end


local function PackAvatar(user,wpacket)
	wpacket:WriteStr(user.userid)
	wpacket:WriteNum(user.avatar.pos.x)
	wpacket:WriteNum(user.avatar.pos.y)
	wpacket:WriteNum(user.avatar.pos.z)
	wpacket:WriteNum(user.avatar.dir.x)
	wpacket:WriteNum(user.avatar.dir.y)
	wpacket:WriteNum(user.avatar.dir.z)
	wpacket:WriteNum(user.avatar.size)		
end

function room:Move(user,msg)
	
	user.avatar.pos.x = msg:ReadNum()
	user.avatar.pos.y = msg:ReadNum()
	user.avatar.pos.z = msg:ReadNum()
	user.avatar.dir.x = msg:ReadNum()
	user.avatar.dir.y = msg:ReadNum()
	user.avatar.dir.z = msg:ReadNum()

	local buff = chuck.buffer.New()
	local w = packet.Writer(buff)
	w:WriteI16(netcmd.CMD_SC_MOVE)
	PackAvatar(user,w)
	self:BroadCastMsg(buff)

end

function room:Kill(user,msg)
	local target = msg:ReadStr()
	target = self.users[target]
	
	if target then
		target.avatar.dead = true
		target.avatar.see_able = false
		user.avatar.size = user.avatar.size + target.avatar.size
		
		--通告target消失
		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteI16(netcmd.CMD_SC_END_SEE)
		w:WriteStr(target.userid)
		self:BroadCastMsg(buff)

		--通告user变大
		buff = chuck.buffer.New()
		w = packet.Writer(buff)
		w:WriteI16(netcmd.CMD_SC_MOVE)
		PackAvatar(user,w)
		self:BroadCastMsg(buff)

	end

end

function room:Relive(user,msg)

	if user.avatar.dead then
		user.avatar.dead = false
		user.avatar.dir = {x=0,y=0,z=0}
		user.avatar.size = 100
		user.avatar.pos = self:RandPos()

		--通告出现
		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteI16(netcmd.CMD_SC_BEGIN_SEE)
		w:WriteStr(user.userid)
		PackAvatar(user,w)	
		self:BroadCastMsg(buff)		
	end

end

function room:Leave(user)
		
	GameLog:Log(log.info,string.format("user %s leave Room",user.userid))	

	self.users[user.userid] = nil
	self.user_size = self.user_size - 1

	if self.user_size <= 0 then
		rooms[self.id] = nil
	else
		--通告消失
		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteI16(netcmd.CMD_SC_END_SEE)
		w:WriteStr(user.userid)
		self:BroadCastMsg(buff)
	end

end

function room:ClientEnter(user)
	user = self.users[user.userid]
	if user then

		GameLog:Log(log.info,string.format("user %s ClientEnter",user.userid))	

		user.avatar.see_able = true

		--通告user出现
		local buff = chuck.buffer.New()
		local w = packet.Writer(buff)
		w:WriteI16(netcmd.CMD_SC_BEGIN_SEE)
		w:WriteStr(user.userid)
		PackAvatar(user,w)	
		self:BroadCastMsg(buff,user)

		--将房间中可视对象通告给user
		for k,v in pairs(self.users) do
			if v.avatar.see_able then
				local buff = chuck.buffer.New()
				local w = packet.Writer(buff)
				w:WriteI16(netcmd.CMD_SC_BEGIN_SEE)
				w:WriteStr(v.userid)
				PackAvatar(v,w)						
				user.session:Send(buff)
			end
		end
	end
end

local function EnterRoom(user)
	for k,v in pairs(rooms) do
		if v.user_size < 20 then
			return
		end
	end
	room_id = room_id + 1
	--创建新房间
	local new_room = room.new(room_id)
	rooms[new_room.id] = room
	new_room:Enter(user)
end

return {
	EnterRoom = EnterRoom
}