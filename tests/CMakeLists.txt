include_directories(${CMAKE_SOURCE_DIR}/include/neuron)

aux_source_directory(${CMAKE_SOURCE_DIR}/src/parser SRC_PARSER)
add_executable(json_test json_test.cc)
target_include_directories(json_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(json_test neuron-base gtest_main gtest pthread jansson)

add_executable(http_test http_test.cc 
	${CMAKE_SOURCE_DIR}/src/utils/http.c)
	
target_include_directories(http_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/plugins/restful)
target_link_libraries(http_test neuron-base gtest_main gtest jansson nng)

file(COPY ${CMAKE_SOURCE_DIR}/neuron.key DESTINATION ${CMAKE_BINARY_DIR}/tests/config)
file(COPY ${CMAKE_SOURCE_DIR}/neuron.pem DESTINATION ${CMAKE_BINARY_DIR}/tests/config)
add_executable(jwt_test jwt_test.cc)
target_include_directories(jwt_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/src 
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/src/utils)
target_link_libraries(jwt_test neuron-base gtest_main gtest jansson ssl crypto jwt libzlog.so)

add_executable(base64_test base64_test.cc ${CMAKE_SOURCE_DIR}/src/utils/base64.c)
target_include_directories(base64_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include       
)
target_link_libraries(base64_test neuron-base gtest_main gtest crypto)

aux_source_directory(${CMAKE_SOURCE_DIR}/src/base/tag_sort.c SRC_SORT)
add_executable(tag_sort_test tag_sort_test.cc ${SRC_SORT})
target_include_directories(tag_sort_test PRIVATE 
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(tag_sort_test neuron-base gtest_main gtest)
#target_link_directories(modbus_point_test PRIVATE /usr/local/lib)

include(GoogleTest)
gtest_discover_tests(json_test)
gtest_discover_tests(http_test)
gtest_discover_tests(jwt_test)
gtest_discover_tests(base64_test)
gtest_discover_tests(tag_sort_test)