cls
gcc -o lodbc.dll -Wall -O3 -shared -I../lua/src lodbc.c ../lua/src/liblua.a -lodbc32 -lodbccp32

strip lodbc.dll

pause
