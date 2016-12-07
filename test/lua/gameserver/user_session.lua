local user_session = {}

function user_session.new(conn)
  local o = {}
  o.__index = user_session     
  setmetatable(o,o)
  o.conn = conn
  o.user = nil
  return o
end

function user_session:Close()
	self.conn:Close()
end

function user_session:Send(msg)
	return self.conn:Send(msg)
end

return {
	New = user_session.new
}