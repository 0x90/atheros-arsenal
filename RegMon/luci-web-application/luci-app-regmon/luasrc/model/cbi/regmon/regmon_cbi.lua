
m = Map("regmon", translate("Regmon Config"), translate("Set options for Regmon"))

c = m:section(TypedSection, "regmon", translate("regmon entries"))
c.anonymous = true

regmon_path = c:option(DynamicList, "regmonpath", translate("Regmon paths")) 
regmon_path.optional = false
regmon_path.rmempty = false

sampling_rate = c:option(Value, "samplingrate", translate("Sampling rate")) 
sampling_rate.optional = false
sampling_rate.rmempty = false

function sampling_rate.validate ( self, value )
    if ( tonumber ( value ) > 1000000000 ) then
        return nil, "Maximum 'Sampling rate' limit exceeded."
    elseif ( tonumber ( value ) < 100000000 ) then
        return nil, "Minimum 'Sampling rate' limit underrun."
    else
        return value
    end
end

time_from = c:option (Value, "timefrom", translate ("Time from field"))
time_from.optional = false
time_from.rmempty = false

use_sys_time = c:option(Flag, "usesystime", translate("Use System Time"))
use_sys_time.optional = false
use_sys_time.rmempty = false

absolute_counter = c:option ( Value, "absolutecounter", translate ("Absolute counter from field"))
absolute_counter.optional = false
absolute_counter.rmempty = false

startindex = c:option(Value, "startindex", translate("Start index of relative counter fields"))
startindex.optional = false
startindex.rmempty = false

metrics = c:option(DynamicList, "metrics", translate("Register Log Fields"))
metrics.optional = false
metrics.rmempty = false

metrics_values = {}
function metrics.validate ( self, value )
    for _, metric in ipairs ( value ) do
        if ( metrics_values [ metric ] == nil ) then
            metrics_values [ metric ] = true
        else
            return nil, "Duplicate field '" .. metric .. "'."
        end
    end
    return value
end

busy_counter = c:option ( Value, "busycounter", translate ("Busy counter field"))
busy_counter.optional = false
busy_counter.rmempty = false

function busy_counter.validate ( self, value )
    if ( metrics_values [ value ] == nil ) then
        return nil, "'Busy counter field' not contained in 'Register log fields'."
    else
        return value
    end
end

d = m:section(TypedSection, "collectd", translate("collectd entries"))
d.anonymous = true

interval = d:option(Value, "interval", translate("Data collection interval"))
interval.optional = false
interval.rmempty = false

enable_log = d:option(Flag, "enablelog", translate("Enable collectd logging")) 
enable_log.optional = false
enable_log.rmempty = false

e = m:section(TypedSection, "rrdtool", translate("rrdtool entries"))
e.anonymous = true

imgpath = e:option(Value, "imagepath", translate("RRDTool image path"))
imgpath.optional = false
imgpath.rmempty = false

imgwidth = e:option(Value, "width", translate("RRDTool image width"))
imgwidth.optional = false
imgwidth.rmempty = false

imgheight = e:option(Value, "height", translate("RRDTool image height"))
imgheight.optional = false
imgheight.rmempty = false

stack = e:option(Flag, "stacked", translate("RRDTool stacked graph"))
stack.optional = false
stack.rmempty = false

shapes = { "LINE2", "AREA" }
shape = e:option(ListValue, "shape", translate("RRDTool graph shape"))
shape.optional = false
shape.rmempty = false

highlight = e:option(Flag, "highlight", translate("RRDTool highlight lines"))
highlight.optional = false
highlight.rmempty = false

for _,s in ipairs(shapes) do
    shape:value(s)
end

return m
