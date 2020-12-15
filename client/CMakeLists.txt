cmake_minimum_required(VERSION 3.17)
project(remote_backup_m1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS -pthread)
#[[set(Boost_USE_STATIC_LIBS ON)]]
#[[set(OPENSSL_USE_STATIC_LIBS TRUE)]]

set(BOOST_ROOT boost)
find_package(Boost 1.73.0 REQUIRED COMPONENTS filesystem regex thread serialization program_options)
find_package(OpenSSL REQUIRED)

include_directories(${Boost_INCLUDE_DIR})
add_executable(client main.cpp core/file_watcher.cpp core/file_watcher.h core/connection.cpp core/connection.h directory/c_resource.cpp directory/c_resource.h core/scheduler.cpp core/scheduler.h communication/f_message.cpp communication/f_message.h core/auth_data.cpp core/auth_data.h ../shared/utilities/tools.cpp ../shared/utilities/tools.h ../shared/directory/dir.h ../shared/communication/message.cpp ../shared/communication/message.h ../shared/communication/types.h ../shared/communication/tlv_view.cpp ../shared/communication/tlv_view.h)

target_link_libraries(client ${Boost_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)