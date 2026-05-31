@echo off
rem ============================================================
rem  Tasmota Workbench launcher for Windows. Double-click in Explorer.
rem  Opens a browser SPA at http://127.0.0.1:8124/ with three panes:
rem    Monitor (serial / UDP syslog)  ·  Devices (LAN scan + OTA)  ·
rem    Shares (Tasmota multicast 239.255.255.250:1999 share-protocol).
rem  Needs Python 3 + pyserial:   py -m pip install pyserial
rem  Prefers pythonw/pyw so no console window stays open.
rem ============================================================
cd /d "%~dp0"
where pythonw >nul 2>nul && ( start "" pythonw "tasmota_workbench_server.py" & exit /b )
where pyw     >nul 2>nul && ( start "" pyw -3  "tasmota_workbench_server.py" & exit /b )
where py      >nul 2>nul && ( start "" py  -3  "tasmota_workbench_server.py" & exit /b )
where python  >nul 2>nul && ( start "" python  "tasmota_workbench_server.py" & exit /b )
echo Python 3 not found. Install it from https://www.python.org/ (tick
echo "Add python.exe to PATH"), then run:  py -m pip install pyserial
pause
