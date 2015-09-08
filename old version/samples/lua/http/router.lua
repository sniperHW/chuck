local router = {
	handler = {}
}

local function parse(req)
	local method = req:Method()
	local urlstr = req:Url()
	local url
	local idx1 = string.find(urlstr,"/",1)
	if not idx1 then
		url = "/"
	else
		local idx2 = string.find(urlstr,"?",idx1)
		if idx2 then idx2 = idx2 - 1 end
		url = string.sub(urlstr,idx1,idx2)
	end	
	if method == "POST" then
		return url,req:Content()
	elseif method == "GET" then
		local param = {}
		for k, v in string.gmatch(urlstr, "(%w+)=(%w+)") do
  			param[k] = v
		end		
		return url,param
	else
		return
	end
end


function router.Dispatch(req,res)
	local url,param = parse(req)
	if url then
		local handler = router.handler[url]
		if handler then
			return handler(req,res,param)
		else
			return "error"
		end
	else
		return "error"
	end
end

function router.RegHandler(url,handler)
	router.handler[url] = handler
end

return router
