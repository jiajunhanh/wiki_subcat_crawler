cmake_minimum_required(VERSION 3.24)
project(wiki_subcat_crawler)
set(CMAKE_CXX_STANDARD 20)
add_executable(wiki_subcat_crawler main.cpp)

#target_compile_options(wiki_subcat_crawler PRIVATE
#  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
#  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
#)

if (MSVC)
    set(MYSQL_CONNECTOR_CPP_DIR "C:/Program Files/MySQL/Connector C++ 8.0")
    set(MYSQL_CONNECTOR_CPP_LIB ${MYSQL_CONNECTOR_CPP_DIR}/lib64/vs14/mysqlcppconn8.lib)
    set(MYSQL_CONNECTOR_CPP_DLL ${MYSQL_CONNECTOR_CPP_DIR}/lib64/mysqlcppconn8-2-vs14.dll)

    target_include_directories(wiki_subcat_crawler PRIVATE ${MYSQL_CONNECTOR_CPP_DIR}/include)
    add_library(libmysqlconn SHARED IMPORTED)
    set_property(TARGET libmysqlconn PROPERTY IMPORTED_LOCATION ${MYSQL_CONNECTOR_CPP_DLL})
    set_property(TARGET libmysqlconn PROPERTY IMPORTED_IMPLIB ${MYSQL_CONNECTOR_CPP_LIB})
    target_link_libraries(wiki_subcat_crawler PRIVATE libmysqlconn)

    add_custom_command(TARGET wiki_subcat_crawler POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:wiki_subcat_crawler>
            $<TARGET_FILE_DIR:wiki_subcat_crawler>)
else ()
    message(FATAL_ERROR "MSVC is required. If you use Linux or Mac, please configure CMakeLists.txt to correctly link the Connctor/C++.")
endif ()