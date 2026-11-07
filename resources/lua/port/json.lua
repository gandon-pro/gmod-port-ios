-- Strict JSON used by the menu/WebKit boundary; never evaluates received text.
local J = {}
J.null = {}
local escape = { ['"']='\\"', ['\\']='\\\\', ['\b']='\\b', ['\f']='\\f', ['\n']='\\n', ['\r']='\\r', ['\t']='\\t' }
function J.quote(s)
    return '"' .. s:gsub('[%z\1-\31\\"]', function(c) return escape[c] or string.format('\\u%04x', c:byte()) end) .. '"'
end
function J.encode(value)
    local seen = {}
    local function encode(v, depth)
        if depth > 64 then error('JSON nesting exceeds 64') end
        if v == nil or v == J.null then return 'null' end
        local t = type(v)
        if t == 'string' then return J.quote(v) end
        if t == 'boolean' then return tostring(v) end
        if t == 'number' then
            if v ~= v or v == math.huge or v == -math.huge then error('Non-finite JSON number') end
            return string.format('%.17g', v)
        end
        if t ~= 'table' then error('Unsupported JSON type: ' .. t) end
        if seen[v] then error('JSON cycle') end
        seen[v] = true
        local count, max, array = 0, 0, true
        for k in pairs(v) do
            count = count + 1
            if type(k) ~= 'number' or k < 1 or k % 1 ~= 0 then array = false else max = math.max(max, k) end
        end
        array = array and count == max
        local parts = {}
        if array then
            for i=1,max do parts[i] = encode(v[i], depth+1) end
        else
            for k,item in pairs(v) do
                if type(k) ~= 'string' and type(k) ~= 'number' then error('Invalid JSON object key') end
                parts[#parts+1] = J.quote(tostring(k)) .. ':' .. encode(item, depth+1)
            end
            table.sort(parts)
        end
        seen[v] = nil
        return (array and '[' or '{') .. table.concat(parts, ',') .. (array and ']' or '}')
    end
    return encode(value, 0)
end
function J.decode(s)
    local at, len = 1, #s
    local function fail() error('Invalid JSON at byte ' .. at, 0) end
    local function ws() local _,last=s:find('^[ \t\r\n]*', at); at=(last or at-1)+1 end
    local function utf8(n)
        if n<128 then return string.char(n) end
        if n<2048 then return string.char(192+math.floor(n/64),128+n%64) end
        if n<65536 then return string.char(224+math.floor(n/4096),128+math.floor(n/64)%64,128+n%64) end
        return string.char(240+math.floor(n/262144),128+math.floor(n/4096)%64,128+math.floor(n/64)%64,128+n%64)
    end
    local function hex()
        local h=s:sub(at,at+3);if not h:match('^%x%x%x%x$') then fail() end
        at=at+4;return tonumber(h,16)
    end
    local function str()
        at=at+1;local out={}
        while at<=len do
            local c=s:sub(at,at);at=at+1
            if c=='"' then return table.concat(out) end
            if c=='\\' then
                c=s:sub(at,at);at=at+1
                local e={['"']='"',['\\']='\\',['/']='/',b='\b',f='\f',n='\n',r='\r',t='\t'}
                if c=='u' then
                    local n=hex()
                    if n>=0xD800 and n<=0xDBFF then
                        if s:sub(at,at+1)~='\\u' then fail() end
                        at=at+2;local low=hex();if low<0xDC00 or low>0xDFFF then fail() end
                        n=65536+(n-0xD800)*1024+low-0xDC00
                    elseif n>=0xDC00 and n<=0xDFFF then fail() end
                    out[#out+1]=utf8(n)
                elseif e[c] then out[#out+1]=e[c] else fail() end
            else if c:byte()<32 then fail() end;out[#out+1]=c end
        end
        fail()
    end
    local parse
    parse=function(depth)
        if depth>64 then fail() end
        ws();local c=s:sub(at,at)
        if c=='"' then return str() end
        if c=='[' or c=='{' then
            local object=c=='{';local close=object and '}' or ']';local out={};at=at+1;ws()
            if s:sub(at,at)==close then at=at+1;return out end
            repeat
                local key=#out+1
                if object then if s:sub(at,at)~='"' then fail() end;key=str();ws();if s:sub(at,at)~=':' then fail() end;at=at+1 end
                out[key]=parse(depth+1);ws();c=s:sub(at,at);at=at+1
                if c==close then return out end
                if c~=',' then fail() end
                ws()
            until false
        end
        for text,v in pairs({['true']=true,['false']=false,['null']=J.null}) do
            if s:sub(at,at+#text-1)==text then at=at+#text;return v end
        end
        local token=s:sub(at):match('^-?%d+%.?%d*[eE]?[+-]?%d*')
        if not token then fail() end
        local mantissa,exponent=token:match('^(.-)[eE](.*)$')
        mantissa=mantissa or token
        if not (mantissa:match('^-?%d+$') or mantissa:match('^-?%d+%.%d+$')) or mantissa:match('^-?0%d') then fail() end
        if exponent and not exponent:match('^[+-]?%d+$') then fail() end
        local number=tonumber(token);if not number or number~=number or math.abs(number)==math.huge then fail() end
        at=at+#token;return number
    end
    local value=parse(0);ws();if at<=len then fail() end;return value
end
return J
