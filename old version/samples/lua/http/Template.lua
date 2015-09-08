local _M = {}

local function process(str)
	local records = {}
	local output  = ""
	local cur = 0
	while true do
		local b1,e1 = string.find(str,"<?lua",cur,true)
		if b1 and e1 then
			if b1 ~= cur then
				table.insert(records,{string={cur,b1-1}})
			end
			local b2,e2 = string.find(str,"/?lua>",e1+1,true)
			if b2 and e2 then
				table.insert(records,{code={e1+1,b2-1}})
				cur = e2 + 1
			else
				--got error
				return "<?lua expected /?lua>"
			end
		else
			table.insert(records,{string={cur,#str}})
			break 
		end			
	end

	for k,v in pairs(records) do
		if v.string then
			output = output .. string.sub(str,v.string[1],v.string[2])
		else
			local err,chuck,result,status
			chuck,err = load(string.sub(str,v.code[1],v.code[2]))
			if err then
				return err
			end
			status,result = pcall(chuck) 
			if not status then
				return result
			end
			output = output .. result
		end
	end
	return nil,output
end

function _M.Load(file)
	local str = ""
	local f = io.open(file,"r")
	while true do
		local content = f:read(4096)
		if content then
			str = str .. content
		else
			break
		end
	end

	local err,output = process(str)
	if err then
		return err
	else
		return output
	end
end

return _M
