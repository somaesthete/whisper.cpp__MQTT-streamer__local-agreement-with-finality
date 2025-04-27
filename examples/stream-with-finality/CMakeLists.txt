if (WHISPER_SDL2)
    # stream
    set(TARGET stream-with-finality)
    add_executable(${TARGET} stream.cpp driver.cpp overlapIndex.cpp localConsensusByN.cpp simpleTcpDebug.cpp)

    include(DefaultTargetOptions)

    target_link_libraries(${TARGET} PRIVATE common common-sdl whisper ${CMAKE_THREAD_LIBS_INIT})
endif ()

set(CMAKE_CXX_STANDARD 17)

add_executable(test__localConsensusByN test/test__localConsensusByN.cpp localConsensusByN)
add_executable(test__overlapIndex test/test__overlapIndex.cpp overlapIndex.cpp)
add_executable(test__driver test/test__driver.cpp driver.cpp overlapIndex.cpp localConsensusByN.cpp)
add_executable(test__simpleTcpDebug test/test__simpleTcpDebug.cpp simpleTcpDebug.cpp)
