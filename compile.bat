cls
REM gcc -shared -Wall -static-libgcc -static-libstdc++ -I../lua/src -I/TDM-GCC-32/lib/ ../lua/src/lapi.c ../lua/src/lauxlib.c ../lua/src/lbaselib.c ../lua/src/lcode.c ../lua/src/lcorolib.c ../lua/src/lctype.c ../lua/src/ldblib.c ../lua/src/ldebug.c ../lua/src/ldo.c ../lua/src/ldump.c ../lua/src/lfunc.c ../lua/src/lgc.c ../lua/src/linit.c ../lua/src/liolib.c ../lua/src/llex.c ../lua/src/lmathlib.c ../lua/src/lmem.c ../lua/src/loadlib.c ../lua/src/lobject.c ../lua/src/lopcodes.c ../lua/src/loslib.c ../lua/src/lparser.c ../lua/src/lstate.c ../lua/src/lstring.c ../lua/src/lstrlib.c ../lua/src/ltable.c ../lua/src/ltablib.c ../lua/src/ltm.c ../lua/src/lundump.c ../lua/src/lutf8lib.c ../lua/src/lvm.c ../lua/src/lzio.c -lkernel32 -luser32 -lcomdlg32  lodbc.c C:\TDM-GCC-64\x86_64-w64-mingw32\lib32\libodbc32.a C:\TDM-GCC-64\x86_64-w64-mingw32\lib32\libodbccp32.a -o lodbc.dll



gcc -o lodbc.dll -shared -static-libgcc -static-libstdc++ -I../lua/src lodbc.c ../lua/src/liblua.a   -lodbc32 -lodbccp32 -lkernel32 -luser32 -lgdi32  

pause
