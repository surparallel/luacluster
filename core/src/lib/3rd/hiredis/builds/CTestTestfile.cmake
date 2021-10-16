# CMake generated Testfile for 
# Source directory: D:/workround/grypania/core/src/lib/3rd/hiredis
# Build directory: D:/workround/grypania/core/src/lib/3rd/hiredis/builds
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(hiredis-test "D:/workround/grypania/core/src/lib/3rd/hiredis/test.sh")
  set_tests_properties(hiredis-test PROPERTIES  _BACKTRACE_TRIPLES "D:/workround/grypania/core/src/lib/3rd/hiredis/CMakeLists.txt;227;ADD_TEST;D:/workround/grypania/core/src/lib/3rd/hiredis/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(hiredis-test "D:/workround/grypania/core/src/lib/3rd/hiredis/test.sh")
  set_tests_properties(hiredis-test PROPERTIES  _BACKTRACE_TRIPLES "D:/workround/grypania/core/src/lib/3rd/hiredis/CMakeLists.txt;227;ADD_TEST;D:/workround/grypania/core/src/lib/3rd/hiredis/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(hiredis-test "D:/workround/grypania/core/src/lib/3rd/hiredis/test.sh")
  set_tests_properties(hiredis-test PROPERTIES  _BACKTRACE_TRIPLES "D:/workround/grypania/core/src/lib/3rd/hiredis/CMakeLists.txt;227;ADD_TEST;D:/workround/grypania/core/src/lib/3rd/hiredis/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(hiredis-test "D:/workround/grypania/core/src/lib/3rd/hiredis/test.sh")
  set_tests_properties(hiredis-test PROPERTIES  _BACKTRACE_TRIPLES "D:/workround/grypania/core/src/lib/3rd/hiredis/CMakeLists.txt;227;ADD_TEST;D:/workround/grypania/core/src/lib/3rd/hiredis/CMakeLists.txt;0;")
else()
  add_test(hiredis-test NOT_AVAILABLE)
endif()
