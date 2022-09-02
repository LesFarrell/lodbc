cls
gcc -Wall -o lodbc.dll -O3 -shared -I../lua/src lodbc.c ../lua/src/liblua.a -lodbc32 -lodbccp32
pause
