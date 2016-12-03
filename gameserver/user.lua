local user = {}

function user.new(session,userid)
  local o = {}
  o.__index = user     
  setmetatable(o,o)
  o.session = session
  session.user = o
  o.userid = userid
  o.room   = nil
  o.room_avatar = nil
  return o
end

return {
	New = user.new
}