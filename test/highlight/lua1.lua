-- lua1.lua
--[[
  Lua Syntax Highlighting Test File
  This file exercises a wide variety of lexical elements.
  It also intentionally includes some malformed constructs.

  This test was created by first asking ChatGPT to write a test, then
  augmenting it with examples from spec at
  https://www.lua.org/manual/5.4/manual.html#3.1 .
]]

------------------------------------------------------------
-- Keywords
------------------------------------------------------------
and break do else elseif end
false for function goto if in
local nil not or repeat return
then true until while

-- "Other tokens" (operators and punctuators)
+     -     *     /     %     ^     #
&     ~     |     <<    >>    //
==    ~=    <=    >=    <     >     =
(     )     {     }     [     ]     ::
;     :     ,     .     ..    ...

------------------------------------------------------------
-- Variables and literals
------------------------------------------------------------
local a = 42
local b = 3.14159
local c = 0xff        -- hex
local d = 0x1.8p1     -- hex float (Lua 5.3+)
local e = 1e10
local f = .5
local g = 5.
local h = true
local i = false
local j = nil

-- Examples of valid integer constants are

     3   345   0xff   0xBEBADA

-- Examples of valid float constants are

     3.0     3.1416     314.16e-2     0.31416E1     34e1
     0x0.1E  0xA23p-4   0X1.921FB54442D18P+1

------------------------------------------------------------
-- Strings
------------------------------------------------------------
local s1 = "double quoted string"
local s2 = 'single quoted string'
local s3 = "escape sequences: \a\b\f\n\r\t\v\\\"\'"
local s4 = 'more escapes: \n\t\\'
local s5 = "hex escape: \x41"       -- 'A'
local s6 = "unicode: \u{1F600}"     -- 😀 (Lua 5.3+)

-- Long strings
local s7 = [[
multi-line
string with [=[ nested delimiters ]=]
]]

local s8 = [=[
another long string
with different delimiter level
]=]

-- As an example, in a system using ASCII (in which 'a' is coded as 97,
-- newline is coded as 10, and '1' is coded as 49), the five literal
-- strings below denote the same string:

a = 'alo\n123"'
a = "alo\n123\""
a = '\97lo\10\04923"'
a = [[alo
123"]]
a = [==[
alo
123"]==]

------------------------------------------------------------
-- Comments
------------------------------------------------------------
-- single line comment

--[[ multi-line comment
     with nested-looking stuff [==[ not really nested ]==]
]]

--[=[
  another long comment
]=]

------------------------------------------------------------
-- Operators
------------------------------------------------------------
local op = (a + b) - c * d / e % f ^ g
local concat = "foo" .. "bar"
local len = #s1

local comparisons = (a == b) or (a ~= b) and (a < b) or (a > b) or (a <= b) or (a >= b)

------------------------------------------------------------
-- Tables
------------------------------------------------------------
local t = {
  1, 2, 3;
  key = "value",
  ["computed" .. "key"] = 123,
  nested = {
    x = 10,
    y = 20,
  },
}

------------------------------------------------------------
-- Functions
------------------------------------------------------------
local function foo(x, y, ...)
  local sum = x + y
  for i = 1, 10 do
    sum = sum + i
  end

  if sum > 100 then
    return sum, ...
  elseif sum == 100 then
    return 0
  else
    return -1
  end
end

------------------------------------------------------------
-- Control flow
------------------------------------------------------------
for k, v in pairs(t) do
  print(k, v)
end

local n = 0
while n < 5 do
  n = n + 1
end

repeat
  n = n - 1
until n == 0

if a and not b or c then
  goto label1
end

::label1::

------------------------------------------------------------
-- do/end block
------------------------------------------------------------
do
  local scoped = "inside do-end"
end

------------------------------------------------------------
-- break
------------------------------------------------------------
for i = 1, 10 do
  if i == 5 then
    break
  end
end

------------------------------------------------------------
-- Lexical edge cases / tricky tokens
------------------------------------------------------------
local tricky1 = "string with -- not a comment"
local tricky2 = 'string with [[ not a long string'
local tricky3 = [[ string with "quotes" and 'quotes' ]]

------------------------------------------------------------
-- Intentional lexical errors
------------------------------------------------------------

-- Unterminated string
local bad1 = "this string never ends

-- Invalid escape
local bad2 = "bad escape: \q"

-- Malformed number
local bad3 = 0xGHI

-- Invalid token
local bad4 = @illegal_token

-- Unfinished long string
local bad5 = [[ unterminated long string...

-- EOF
