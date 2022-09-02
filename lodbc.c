/*
MIT License

Copyright (c) 2022 Les Farrell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/*
 * This is the data structure every database instance will hold.
 * it contains a pointer to the odbc data of the initialised database and
 * a pointer to the connection string of the database.
 */
struct lodbc_data
{
  SQLHENV env;            /* Environment handle. */
  SQLHDBC dbc;            /* Connection handle. */
  SQLHSTMT stmt;          /* Statement handle. */
  char *connectionstring; /* ConnectionString used to connect to the database. */
  char *SQL;              /* Last SQL query used. */
};
typedef struct lodbc_data lodbc_data;

void odbc_geterror(lua_State *L, char *fn, SQLHANDLE handle, SQLSMALLINT type)
{
  SQLINTEGER i = 0;
  SQLINTEGER native;
  SQLCHAR state[7];
  SQLCHAR text[256];
  SQLSMALLINT len;
  SQLRETURN ret;

  do
  {
    ret = SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len);
    if (SQL_SUCCEEDED(ret))
    {
      lua_pushfstring(L, "(%s):%s:%d:%d:%s\n", fn, state, i, native, text);
    }
  } while (ret == SQL_SUCCESS);
}

/*
 * Register our modules data structure with Lua.
 *
 * Passed:
 * L               The lua state
 * index           Index to the argument to check
 *
 * Returns:
 * lodbc_data *    A new instance of our data structure
 */
lodbc_data *check_lODBC(lua_State *L, int index)
{
  /* Checks whether the function argument (index) is userdata of the type ODBC.DB. */
  return (lodbc_data *)luaL_checkudata(L, index, "ODBC.DB");
}

/*
 * db:close() closes a database handle and invalidates it.
 *
 * Passed:
 * L               The lua state.
 *
 * Returns:
 * int(0); 		   nothing
 */
static int lodbc_close(lua_State *L)
{
  // Grab the user data
  lodbc_data *self = check_lODBC(L, 1);

  // Disconnect from the database
  SQLDisconnect(self->dbc);

  free(self->connectionstring);
  self->connectionstring = NULL;

  // Free the connection handles.
  SQLFreeHandle(SQL_HANDLE_DBC, self->dbc);
  SQLFreeHandle(SQL_HANDLE_ENV, self->env);
  SQLFreeHandle(SQL_HANDLE_STMT, self->stmt);

  return 0;
}

/*
 * db = connect(ConnectionString) opens a ODBC database connection.
 *
 * Passed:
 * L			The Lua state
 *
 * Returns:
 * int(1);      A handle to this modules data structure or nil
 */
static int lodbc_connect(lua_State *L)
{
  SQLCHAR outstr[1024];
  SQLSMALLINT outstrlen;
  SQLRETURN ret;
  const char *fn;

  SQLHENV henv = SQL_NULL_HENV; /* Environment handle */
  SQLHDBC hdbc = SQL_NULL_HDBC; /* Connection handle */

  if (lua_gettop(L) != 1)
  {
    luaL_error(L, "usage: connect( ConnectionString )");
  }

  fn = luaL_checkstring(L, 1);

  /* Initialize the ODBC environment handle. */
  ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
  if (!SQL_SUCCEEDED(ret))
  {
    lua_pushnumber(L, 0);
    odbc_geterror(L, "connect", henv, SQL_NULL_HANDLE);
    return 2;
  }

  /* Set the ODBC version to version 3 */
  ret = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
  if (!SQL_SUCCEEDED(ret))
  {
    lua_pushnumber(L, 0);
    odbc_geterror(L, "connect", henv, SQL_ATTR_ODBC_VERSION);
    return 2;
  }

  /* Allocate the connection handle. */
  ret = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
  if (!SQL_SUCCEEDED(ret))
  {
    lua_pushnumber(L, 0);
    odbc_geterror(L, "connect", hdbc, SQL_HANDLE_DBC);
    return 2;
  }

  /* Try to connect to the database. */
  ret = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) fn, strlen(fn), outstr, sizeof(outstr), &outstrlen, SQL_DRIVER_NOPROMPT);
  if (SQL_SUCCEEDED(ret))
  {
    lodbc_data *self = (lodbc_data *) lua_newuserdata(L, sizeof(lodbc_data));
    self->connectionstring = strdup(fn);
    self->env = henv;
    self->dbc = hdbc;

    luaL_getmetatable(L, "ODBC.DB");
    lua_setmetatable(L, -2);
  }
  else
  {
    lua_settop(L, 0);
    lua_pushnil(L);
    odbc_geterror(L, "connect", hdbc, SQL_HANDLE_DBC);
    lodbc_close(L);
  }

  return 1;
}

/*
 * results, error = db:exec(statements) executes statements and returns the results, if any and a possible error.
 *
 * Passed:
 * L           The Lua state
 *
 * Returns:
 * int(2);     Rows, Error
 */
static int lodbc_exec(lua_State *L)
{
  SQLRETURN ret;
  const char *fn;
  SQLSMALLINT columns;
  int index = 0;
  char str[1024] = {'\0'};

  if (lua_gettop(L) != 2)
  {
    luaL_error(L, "usage: lodbc.exec( statements )");
  }

  /* Grab the user data. */
  lodbc_data *self = check_lODBC(L, 1);

  fn = luaL_checkstring(L, 2);
  self->SQL = strdup(fn);

  /* Clear the stack */
  lua_settop(L, 0);

  /* Allocate a statement handle */
  ret = SQLAllocHandle(SQL_HANDLE_STMT, self->dbc, &self->stmt);
  if (!SQL_SUCCEEDED(ret))
  {
    lua_pushnumber(L, 0);
    odbc_geterror(L, "exec", self->stmt, SQL_HANDLE_STMT);
    return 2;
  }

  /* Try and execute query */
  ret = SQLExecDirect(self->stmt, (SQLCHAR *)self->SQL, SQL_NTS);
  if (!SQL_SUCCEEDED(ret))
  {
    lua_pushnumber(L, 0);
    odbc_geterror(L, "exec", self->stmt, SQL_HANDLE_STMT);
    return 2;
  }

  /* How many columns did we get back? */
  ret = SQLNumResultCols(self->stmt, &columns);
  if (!SQL_SUCCEEDED(ret))
  {
    lua_pushnumber(L, 0);
    odbc_geterror(L, "exec", self->stmt, SQL_HANDLE_STMT);
    return 2;
  }

  lua_newtable(L);

  /* Loop through each row in the result set */
  while (SQL_SUCCEEDED(ret = SQLFetch(self->stmt)))
  {
    SQLUSMALLINT i;
    lua_pushnumber(L, ++index);
    lua_newtable(L);

    /* Loop through the columns */
    for (i = 1; i <= columns; i++)
    {
      SQLLEN indicator;
      char buf[1024] = {'\0'};

      SQLColAttribute(self->stmt, i, SQL_DESC_BASE_COLUMN_NAME, &str, 1024, 0, 0);

      lua_pushstring(L, str);

      /* Retrieve column data as a string */
      ret = SQLGetData(self->stmt, i, SQL_C_CHAR, (SQLPOINTER)buf, sizeof(buf), &indicator);

      if (SQL_SUCCEEDED(ret))
      {
        /* Handle null columns */
        if (indicator == SQL_NULL_DATA)
        {
          strcpy(buf, "NULL");
        }

        lua_pushstring(L, buf);
        lua_settable(L, -3);
      }
    }
    lua_settable(L, -3);
  }

  lua_pushnil(L);

  return 2;
}

/*
 * When you leave lua without closing the database,
 * the garbage collector will clean up for us.
 *
 * Passed:
 * L       - The lua state
 *
 * Returns
 * int(0)  - Nothing
 */
static int lodbc__gc(lua_State *L)
{
  lodbc_close(L);
  return 0;
}

/*
 * The methods our database object exposes to Lua.
 */
static const luaL_Reg lodbc_method_map[] = {
    {"close", lodbc_close},
    {"exec", lodbc_exec},
    {"__gc", lodbc__gc},
    {NULL, NULL}};

/*
 * The methods this module exposes to Lua.
 */
static const luaL_Reg lodbc_module[] = {
    {"connect", lodbc_connect},
    {NULL, NULL}};

/*
 * The loader called when our shared library is loaded.
 *
 * Passed:
 * L		The Lua state.
 *
 * Returns:
 * int(1);	our module in Lua's format
 */
int luaopen_lodbc(lua_State *L)
{
#if LUA_VERSION_NUM == 501
  luaL_register(L, "lodbc", lodbc_module);
#else
  luaL_newlib(L, lodbc_module);
#endif

  luaL_newmetatable(L, "ODBC.DB");

  lua_pushvalue(L, -1);

  lua_setfield(L, -2, "__index");

#if LUA_VERSION_NUM == 501
  luaL_register(L, NULL, lodbc_method_map);
#else
  luaL_setfuncs(L, lodbc_method_map, 0);
#endif

  lua_pop(L, 1);

  return 1;
}
