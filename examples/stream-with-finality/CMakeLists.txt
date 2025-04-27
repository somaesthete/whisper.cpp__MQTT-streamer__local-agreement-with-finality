cmake_minimum_required(VERSION 3.14)

# Include FetchContent
include(FetchContent)

# Fetch and build Paho MQTT C client first
FetchContent_Declare(
    paho-mqtt-c
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
    GIT_TAG v1.3.12
)
FetchContent_MakeAvailable(paho-mqtt-c)

# Fetch and build Paho MQTT C++ client
FetchContent_Declare(
    paho-mqtt-cpp
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
    GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(paho-mqtt-cpp)

if (WHISPER_SDL2)
    # stream
    set(TARGET stream-with-finality)
    add_executable(${TARGET} stream.cpp driver.cpp overlapIndex.cpp localConsensusByN.cpp simpleTcpDebug.cpp)

    include(DefaultTargetOptions)

    # Add include directories for MQTT
    target_include_directories(${TARGET} PRIVATE
        ${paho-mqtt-cpp_SOURCE_DIR}/src
        ${paho-mqtt-cpp_BINARY_DIR}/src
        ${paho-mqtt-c_SOURCE_DIR}/src
        ${paho-mqtt-c_BINARY_DIR}/src
    )

    # Link against MQTT libraries
    target_link_libraries(${TARGET} PRIVATE 
        common 
        common-sdl 
        whisper 
        ${CMAKE_THREAD_LIBS_INIT} 
        paho-mqttpp3
        paho-mqtt3a
    )
endif ()

set(CMAKE_CXX_STANDARD 17)

add_executable(test__localConsensusByN test/test__localConsensusByN.cpp localConsensusByN)
add_executable(test__overlapIndex test/test__overlapIndex.cpp overlapIndex.cpp)
add_executable(test__driver test/test__driver.cpp driver.cpp overlapIndex.cpp localConsensusByN.cpp)
add_executable(test__simpleTcpDebug test/test__simpleTcpDebug.cpp simpleTcpDebug.cpp)
