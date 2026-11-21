@echo off
cd /d "%~dp0"

start "" http://localhost:3000
node dashboard\server.js

pause