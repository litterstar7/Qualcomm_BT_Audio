@echo off
if "%1" == "" (
    call ceedling clobber
    call ceedling options:CB gcov:all utils:gcov
) else (
    call ceedling clobber
    call ceedling options:CB gcov:%1 utils:gcov
)
