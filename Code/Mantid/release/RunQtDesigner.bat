@echo off
REM Sets a path the Mantid libraries and runs the designer

SET QT_PLUGIN_PATH=%CD%
SET PATH=%PATH%;%CD%

designer.exe