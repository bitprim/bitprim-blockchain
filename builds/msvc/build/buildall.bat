@ECHO OFF
ECHO.
ECHO Downloading libitcoin_blockchain dependencies from NuGet
CALL nuget.exe install ..\vs2013\libbitcoin_blockchain\packages.config
ECHO.
CALL buildbase.bat ..\vs2013\libbitcoin_blockchain.sln 12
ECHO.
PAUSE