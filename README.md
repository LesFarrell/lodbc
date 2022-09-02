# lodbc
Small Lua module for connecting to and querying ODBC databases.

I'm still learning Lua, so this is very much work in progress!


Simple Example:

```lua
odbc = require("lodbc")

db = odbc.connect('Driver={MariaDB ODBC 3.1 Driver};Server=127.0.0.1;Port=3306;Database=<Your Database>;User=<Your Username>;Password=<Your Password>;Option=3;')

results, error = db:exec('CREATE TABLE IF NOT EXISTS Names (FirstName TEXT,  Surname TEXT, TimeStamp DATETIME);')
if (error ~= nill) then print(error) end

results, error = db:exec('DELETE FROM Names;')
if (error ~= nill) then print(error) end

results, error = db:exec('INSERT INTO Names (FirstName, Surname, TimeStamp) VALUES ("Les", "Farrell", "1970-01-01")')
if (error ~= nill) then print(error) end

results, error = db:exec('INSERT INTO Names (FirstName, Surname, TimeStamp) VALUES ("Ema", "Nymton", "1970-01-02")')
if (error ~= nill) then print(error) end

results, error = db:exec("SELECT FirstName, Surname FROM NAMES; ")
if (error ~= nill) then print(error) end

print("Number of Rows returned: " .. #results)
for i = 1, #results do
    print("Row:" .. i)
    for k, v in pairs(results[i]) do
        print(k .. ":", v)
    end
    print()
end

db:close()
```
