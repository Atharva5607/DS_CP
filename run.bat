@echo off
rem Builds the server with MinGW if needed and starts it.
cd /d "%~dp0"
if not exist netsim.exe mingw32-make netsim
netsim.exe %1
