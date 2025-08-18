cmake_minimum_required(VERSION 3.12)
project(live_spread_binance_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

add_executable(live_spread
  src/main.cpp
)

target_include_directories(live_spread PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(live_spread
  PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(UNIX AND NOT APPLE)
  target_link_libraries(live_spread PRIVATE pthread)
endif()
