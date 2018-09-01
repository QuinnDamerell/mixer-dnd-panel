@echo off

REM Make the build folder if it doesn't exist.
IF EXIST build_win64 (
  echo Build Folder Exists
) ELSE (
  mkdir build_win64
)
cd build_win64

REM echo Setting Up Boost
REM set BOOST_INCLUDEDIR=C:\local\boost_1_64_0\
REM set BOOST_LIBRARYDIR=C:\local\boost_1_64_0\lib64-msvc-14.1
REM set BOOST_ROOT=C:\local\boost_1_64_0\

REM IF EXIST "%BOOST_ROOT%" (
REM   echo Boost 1.64.0 Found  
REM ) ELSE (
REM   echo _
REM   echo BOOST NOT FOUND!
REM   echo Install boost from here https://sourceforge.net/projects/boost/files/boost-binaries/1.64.0/
REM   echo _
REM   exit 1
REM )

echo Running cmake -  Note that Cmake 3.8 or newer is required.
cmake -G "Visual Studio 15 2017 Win64" ..