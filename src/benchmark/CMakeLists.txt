CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

SET(CMAKE_BUILD_TYPE "Release")

SET(PROJECT blackhole-benchmark)

SET(HEADERS
)

SET(SOURCES
    main
)

ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)

SET(Boost_USE_MULTITHREADED ON)

SET(BOOST_COMPONENTS
    system
    filesystem
    thread
    date_time
    log
    log_setup
)

SET(GOOGLE_LIBRARIES
)

PROJECT(${PROJECT})

INCLUDE_DIRECTORIES(../
    /Users/esafronov/sandbox/Celero/include
)

FIND_PACKAGE(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)

ADD_EXECUTABLE(${PROJECT} ${HEADERS} ${SOURCES})

TARGET_LINK_LIBRARIES(${PROJECT}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    ${GOOGLE_LIBRARIES}
    blackhole
    /Users/esafronov/sandbox/Celero/build/libcelero.dylib
)

ADD_DEFINITIONS(-std=c++0x)