add_executable(protocol_tests protocol_tests.cpp)
target_link_libraries(protocol_tests PRIVATE tcp_common)
add_test(NAME protocol_and_statistics COMMAND protocol_tests)

find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
add_test(NAME socket_integration
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/integration_test.py"
            "$<TARGET_FILE:TCP_Server>" "$<TARGET_FILE:TCP_Client>")
set_tests_properties(socket_integration PROPERTIES TIMEOUT 60)
