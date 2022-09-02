#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdint.h>
#include "../lua/src/lauxlib.h"
#include "../lua/src/lua.h"
#include "../lua/src/lualib.h"
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

char debugbuffer[512] = { '\0' };
#define DebugPrint(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define DebugMsg(M, ...)  sprintf(debugbuffer, "%s :(%d)\n\n" M "\n", __FILE__, __LINE__, ##__VA_ARGS__); MessageBoxA(0, debugbuffer, "Debug Message", 0);


/*
 * this is the data structure every database instance will hold.
 * it contains a pointer to the odbc data of the initialised database and
 * a pointer to the connection string of the database, as given by the user.
 */
typedef struct lodbc_data lodbc_data;
struct lodbc_data
{
    SQLHENV env;   				// Environment handle
    SQLHDBC dbc;   				// Connection handle
    SQLHSTMT stmt; 				// Statement handle
    char *connectionstring;		// ConnectionString used to connect to the database
    char *SQL;					// Last SQL query used.
};


/*
 * register our modules data structure to lua.
 *
 * @param  L     	The lua state
 * @param  index 	Index to the argument to check
 * @return       	A new instance of our data structure
 */
lodbc_data *check_lODBC(lua_State *L, int index)
{
	// Checks whether the function argument (index) is a userdata of the type tname.
    return (lodbc_data *)luaL_checkudata(L, index, "ODBC.DB");
}


void odbc_geterror(lua_State *L, char *fn, SQLHANDLE handle, SQLSMALLINT type, char *buffer)
{
    SQLINTEGER i = 0;
    SQLINTEGER native;
    SQLCHAR state[7];
    SQLCHAR text[256];
    SQLSMALLINT len;
    SQLRETURN ret;
	char errormsg[1024]={'\0'};
	
	//lodbc_data *self = check_lODBC(L, 1);
	buffer[0] = '\0';
    do
    {
        ret = SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len);
        if (SQL_SUCCEEDED(ret)) {					
            lua_pushfstring(L, "%s:%d:%d:%s\n", state, i, native, text);
		}
    } while (ret == SQL_SUCCESS);
}


/*
 * db:close() closes a database handle and invalidates it.
 *
 * @param  L the lua state
 * @return   int(0); nothing
 */
static int lodbc_close(lua_State *L)
{
    // Grab the user data
    lodbc_data *self = check_lODBC(L, 1);

    /* Disconnect from the database */
    SQLDisconnect(self->dbc);

    free(self->connectionstring);
    self->connectionstring = NULL;

    /* Free the connection handles. */
    SQLFreeHandle(SQL_HANDLE_DBC, self->dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, self->env);
    SQLFreeHandle(SQL_HANDLE_STMT, self->stmt);

    return 0;
}


/*
 * db = lodbcConnect(ConnectionString) opens a ODBC database file.
 *
 * @param  L the lua state
 * @return   int(1); a handle to this modules data structure or nil
 */
static int lodbc_connect(lua_State *L)
{
    SQLCHAR outstr[1024];
    SQLSMALLINT outstrlen;
    SQLRETURN ret;
    const char *fn;
	char errormsg[1024] = {'\0'};
	
	// Grab the user data.
    //lodbc_data *self = check_lODBC(L, 1);
    
    SQLHENV henv = SQL_NULL_HENV; // Environment handle
    SQLHDBC hdbc = SQL_NULL_HDBC; // Connection handle

	// Returns the index of the top element in the stack. Because indices start at 1, this result is equal to the number of elements in the stack (and so 0 means an empty stack). 
    if (lua_gettop(L) != 1)
    {
        /* wrong count of arguments */
        luaL_error(L, "usage: ODBC_Connect( ConnectionString )");
    }

	// Checks whether the function argument [narg] is a string and returns this string. 
    fn = luaL_checkstring(L, 1);
	

    /* Initialize the ODBC enviroment handle. */
    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
	if (!SQL_SUCCEEDED(ret))
    {
		lua_pushnumber(L, 0);
        odbc_geterror(L, "SQLAllocHandle", henv, SQL_NULL_HANDLE,errormsg);
		return 2;
    }
	
	
    /* Set the ODBC version to version 3 */
    ret = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
	if (!SQL_SUCCEEDED(ret))
    {
		lua_pushnumber(L, 0);
        odbc_geterror(L, "SQLSetEnvAttr", henv, SQL_ATTR_ODBC_VERSION,errormsg);
		return 2;
    }
	
    /* Allocate the connection handle. */
    ret=SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
	if (!SQL_SUCCEEDED(ret))
    {
		lua_pushnumber(L, 0);
        odbc_geterror(L, "SQLAllocHandle", hdbc, SQL_HANDLE_DBC,errormsg);
		return 2;
    }

    /* Try to connect to the database. */    
	ret = SQLDriverConnect(hdbc, NULL, (SQLCHAR *)fn, strlen(fn), outstr, sizeof(outstr), &outstrlen, SQL_DRIVER_NOPROMPT);
	if (SQL_SUCCEEDED(ret))
    {	

		//  This function allocates a new block of memory with the given size, pushes onto the stack a new full userdata with the block address, and returns this address.
		//	Userdata represent C values in Lua. A full userdata represents a block of memory. It is an object (like a table): you must create it, it can have its own metatable, and you can detect when it is being collected. A full userdata is only equal to itself (under raw equality).
		//	When Lua collects a full userdata with a gc metamethod, Lua calls the metamethod and marks the userdata as finalized. When this userdata is collected again then Lua frees its corresponding memory. 
        lodbc_data *self = (lodbc_data *)lua_newuserdata(L, sizeof(lodbc_data));
        self->connectionstring = strdup(fn);
        self->env = henv;
        self->dbc = hdbc;

		//	Pushes onto the stack the metatable associated with name [tname] in the registry (see luaL_newmetatable)
        luaL_getmetatable(L, "ODBC.DB");
		
		// Pops a table from the stack and sets it as the new metatable for the value at the given acceptable index. 
        lua_setmetatable(L, -2);
    }
    else
    {				
        odbc_geterror(L, "connect", henv, SQL_HANDLE_ENV,errormsg);		
        lodbc_close(L);
		
		// Pushes a nil value onto the stack. 
        lua_pushnil(L);
    }

    return 1;
}


/*
 * results, error = db:exec(statements) executes statements and returns the
 * results, if any and a possible error.
 *
 * @param  L the lua state
 * @return   int(2); rows, error
 */
static int lodbc_exec(lua_State *L)
{
    SQLRETURN ret;
    const char *fn;
    SQLSMALLINT columns;
    int index = 0;
    char str[1024] = {'\0'};
	char *err = NULL;
	char errormsg[1024] = {'\0'};

	if( lua_gettop(L) != 2 ){ luaL_error(L, "usage: lsqlite.exec( statements )"); }

    // Grab the user data.
    lodbc_data *self = check_lODBC(L, 1);

	
	// Checks whether the function argument [narg] is a string and returns this string. 
    fn = luaL_checkstring(L, 2);
    self->SQL = strdup(fn);

	// Clear the stack!
	lua_settop (L, 0);

    /* Allocate a statement handle */
    ret = SQLAllocHandle(SQL_HANDLE_STMT, self->dbc, &self->stmt);
    if (!SQL_SUCCEEDED(ret))
    {
		lua_pushnumber(L, 0);
        odbc_geterror(L, "SQLAllocHandle", self->stmt, SQL_HANDLE_STMT,errormsg);
		return 2;
    }

    /* Execute query */
    ret = SQLExecDirect(self->stmt, (SQLCHAR *)self->SQL, SQL_NTS);
    if (!SQL_SUCCEEDED(ret))
    {
		lua_pushnumber(L, 0);
        odbc_geterror(L, "SQLExecDirect", self->stmt, SQL_HANDLE_STMT,errormsg);	
		return 2;
    }


    /* How many columns are there */
    ret = SQLNumResultCols(self->stmt, &columns);
    if (!SQL_SUCCEEDED(ret))
    {
        lua_pushnumber(L, 0);
		odbc_geterror(L, "SQLNumResultCols", self->stmt, SQL_HANDLE_STMT,errormsg);
		return 2;
    }

	// Pushes a number with value n onto the stack. 
    //lua_pushnumber(L, 1);
    
	// Creates a new empty table and pushes it onto the stack. It is equivalent to lua_createtable(L, 0, 0). 
	lua_newtable(L);

    /* Loop through each row in the result-set */
    while (SQL_SUCCEEDED(ret = SQLFetch(self->stmt)))
    {
        SQLUSMALLINT i;

        /* This creates an index and a row of data, the metatable was already created */
        // Pushes a number with value n onto the stack. 
		lua_pushnumber(L, ++index);
		
		// Creates a new empty table and pushes it onto the stack. It is equivalent to lua_createtable(L, 0, 0). 
        lua_newtable(L);

        /* Loop through the columns */
        for (i = 1; i <= columns; i++)
        {
            SQLLEN indicator;
            char buf[1024] = {'\0'};

            SQLColAttribute(self->stmt, i, SQL_DESC_BASE_COLUMN_NAME, &str, 1024, 0, 0);
			
			// Pushes the zero-terminated string pointed to by s onto the stack. Lua makes (or reuses) an internal copy of the given string, so the memory at s can be freed or reused immediately after the function returns. The string cannot contain embedded zeros; it is assumed to end at the first zero. 
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
				
				// Pushes the zero-terminated string pointed to by s onto the stack. Lua makes (or reuses) an internal copy of the given string, so the memory at s can be freed or reused immediately after the function returns. The string cannot contain embedded zeros; it is assumed to end at the first zero. 
                lua_pushstring(L, buf);
				
				// Does the equivalent to t[k] = v, where t is the value at the given valid index, v is the value at the top of the stack, and k is the value just below the top.
				// This function pops both the key and the value from the stack. As in Lua, this function may trigger a metamethod for the "newindex" event (see §2.8). 
                lua_settable(L, -3);
            }

			}

		// Does the equivalent to t[k] = v, where t is the value at the given valid index, v is the value at the top of the stack, and k is the value just below the top.
		// This function pops both the key and the value from the stack. As in Lua, this function may trigger a metamethod for the "newindex" event (see §2.8). 
        lua_settable(L, -3);
		
		
        
    }
	    
    lua_pushstring(L, errormsg);
		
    /* return the results and the error, if any */
    return 2;
}


/*
 * when you leave lua without closing the database, the garbage collector
 * will clean up for us.
 *
 * @param  L the lua state, again
 * @return   int(0); nothing
 */
static int lodbc__gc(lua_State *L)
{
    lodbc_data *self = check_lODBC(L, 1);

    lodbc_close(L);
    return 0;
}


/*
 * a string representation of a db instance.
 *
 * @param  L the lua state, over and over again
 * @return   int(1); some string like "SQLite Database (<name of it>)" or "SQLite Database (closed)"
 */
static int lodbc__tostring(lua_State *L)
{
    lodbc_data *self = check_lODBC(L, 1);

    if (self->connectionstring)
        lua_pushfstring(L, "Connection String (%s)", self->connectionstring);
    else
        lua_pushstring(L, "Connection String Not Found.");

    return 1;
}


/*
 * the map of methods a database object exposes to lua.
 */
static const luaL_Reg lodbc_method_map[] =
{
	{"close", lodbc_close},
	{"exec", lodbc_exec},
	{"__gc", lodbc__gc},
	{"__tostring", lodbc__tostring},
	{NULL, NULL}
};


/*
 * the methods this module exposes to lua.
 */
static const luaL_Reg lodbc_module[] =
{
	{"connect", lodbc_connect},
	{NULL, NULL}
};


/*
 * The loader called when our shared library is loaded.
 *
 * @param  	L the lua state
 * @return  int(1); our module in lua's format
 */
int luaopen_lodbc(lua_State *L)
{
    /* The module system has changed. This switches the behaviour. */
	#if LUA_VERSION_NUM == 501
		luaL_register(L, "lodbc", lodbc_module);
	#else
		luaL_newlib(L, lodbc_module);
	#endif

	// If the registry already has the key tname, returns 0. Otherwise, creates a new table to be used as a metatable for userdata, adds it to the registry with key tname, and returns 1.
	// In both cases pushes onto the stack the final value associated with tname in the registry. 
    luaL_newmetatable(L, "ODBC.DB");
    
	// ushes a copy of the element at the given valid index onto the stack. 
	lua_pushvalue(L, -1);
	
	// Does the equivalent to t[k] = v, where t is the value at the given valid index and v is the value at the top of the stack.
	// This function pops the value from the stack. As in Lua, this function may trigger a metamethod for the "newindex" event (see §2.8). 
    lua_setfield(L, -2, "__index");

    /* The module system has changed. This switches the behaviour. */
	#if LUA_VERSION_NUM == 501
		luaL_register(L, NULL, lodbc_method_map);
	#else
		luaL_setfuncs(L, lodbc_method_map, 0);
	#endif

	// Pops n elements from the stack
    lua_pop(L, 1);

    return 1;
}
