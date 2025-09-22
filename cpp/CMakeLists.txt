cmake_minimum_required(VERSION 3.21)

project(QuickGrabCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable vcpkg manifest mode if available
if(DEFINED ENV{VCPKG_MANIFEST_MODE})
    message(STATUS "Using vcpkg manifest mode")
endif()

find_package(Boost REQUIRED COMPONENTS system json)
find_package(boost_beast CONFIG REQUIRED)
find_package(boost_asio  CONFIG REQUIRED)
find_package(OpenSSL     REQUIRED)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)


add_library(quickgrab_core
    src/server/HttpServer.cpp
    src/server/Router.cpp
    src/controller/AuthController.cpp
    src/controller/ProxyController.cpp
    src/controller/GrabController.cpp
    src/controller/QueryController.cpp
    src/controller/StatisticsController.cpp
    src/controller/SubmitController.cpp
    src/controller/ToolController.cpp
    src/controller/UserController.cpp
    src/service/AuthService.cpp
    src/service/GrabService.cpp
    src/service/MailService.cpp
    src/service/ProxyService.cpp
    src/service/QueryService.cpp
    src/service/StatisticsService.cpp
    src/repository/DatabaseConfig.cpp
    src/repository/MySqlConnectionPool.cpp
    src/repository/RequestsRepository.cpp
    src/repository/ResultsRepository.cpp
    src/repository/BuyersRepository.cpp
    src/proxy/ProxyPool.cpp
    src/util/HttpClient.cpp
    src/util/JsonUtil.cpp
    src/util/Logging.cpp
    src/workflow/GrabWorkflow.cpp
)

target_include_directories(quickgrab_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(quickgrab_core
  PUBLIC
    Boost::system
    Boost::json
    OpenSSL::SSL
    OpenSSL::Crypto
    unofficial::mysql-connector-cpp::connector
)




add_executable(quickgrab_app src/main.cpp)

target_link_libraries(quickgrab_app PRIVATE quickgrab_core)

if(MSVC)
    target_compile_options(quickgrab_core PRIVATE /W4 /permissive-)
    target_compile_options(quickgrab_app PRIVATE /W4 /permissive-)
else()
    target_compile_options(quickgrab_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(quickgrab_app PRIVATE -Wall -Wextra -Wpedantic)
endif()




