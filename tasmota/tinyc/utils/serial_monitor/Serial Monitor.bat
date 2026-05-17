@echo off
rem ============================================================
rem  Serial Monitor launcher for Windows. Double-click in Explorer.
rem  Opens the browser console for ESP serial / Tasmota syslog at
rem  http://127.0.0.1:8124/ . Needs Python 3 + pyserial:
rem      py -m pip install pyserial
rem  Prefers pythonw/pyw so no console window stays open.
rem ============================================================
cd /d "%~dp0"
where pythonw >nul 2>nul && ( start "" pythonw "serial_monitor_server.py" & exit /b )
where pyw     >nul 2>nul && ( start "" pyw -3 "serial_monitor_server.py" & exit /b )
where py      >nul 2>nul && ( start "" py -3  "serial_monitor_server.py" & exit /b )
where python  >nul 2>nul && ( start "" python "serial_monitor_server.py" & exit /b )
echo Python 3 not found. Install it from https://www.python.org/ (tick
echo "Add python.exe to PATH"), then run:  py -m pip install pyserial
pause
