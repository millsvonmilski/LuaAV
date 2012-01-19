

local control = require "control"
local Widget = control.Widget

-- this will be optional soon, once bonjour etc. is working:
control.init{
	name = "LuaAV",
	orientation = "portrait",
	
	host = "10.0.2.4",
	port = 8081,
	
	remote_host = "10.0.2.1",
	remote_port = 8080
}

-- add widgets
Widget{} -- default type Slider
Widget{ type="Button", name="Bang", min=1, max=2, }
Widget{ type="Knob", name="Twiddler", }
Widget{ type="Accelerometer" }

wait(1)

-- send a value:
control.set("Bang", 2)

control.remove("Twiddler")



-- install a generic handler:
control.handler = function(m)
	print(m.addr, m.types, unpack(m))
end


--[=[


go(function()
	while true do
		--send:send("/Slider1", math.random())
		--send:send("/Knobbersmith", math.random())
		wait(0.5)
	end
end)

-- receive controls:
while true do
	for m in recv:recv() do
		print(m.addr, m.types, unpack(m))
		local slidername = m.addr
		local value = m[1]
	end
	wait(0.1)
end


--]=]