function fun0()
	print("fun0")
end

function fun1(msg)
	print("fun1:" .. msg)
	return "ok"
end

function funf()
	return function ()
		print("i'm a Anonymous function")
	end
end