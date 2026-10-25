cmake_minimum_required(VERSION 3.16)

project(CapsuX VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenCV Yapılandırması (C:/opencv)
set(OpenCV_DIR "C:/opencv/build")
if(MINGW AND NOT EXISTS "C:/opencv/build/x64/mingw/lib/OpenCVConfig.cmake" AND EXISTS "C:/opencv/build/x64/vc16/lib/OpenCVConfig.cmake")
    set(OpenCV_ARCH "x64")
    set(OpenCV_RUNTIME "vc16")
endif()
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort LinguistTools)

set(TS_FILES CapsuX_tr_TR.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(CapsuX
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(CapsuX SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(CapsuX
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# MinGW derleyicisi ile C:/opencv (vc16) DLL sembol köprüsü
set(OPENCV_MINGW_BRIDGE_LIB "")
if(MINGW AND EXISTS "C:/opencv/build/x64/vc16/bin/opencv_world4140.dll")
    set(OPENCV_DEF_FILE "${CMAKE_CURRENT_BINARY_DIR}/opencv_mingw_bridge.def")
    set(OPENCV_BRIDGE_A "${CMAKE_CURRENT_BINARY_DIR}/libopencv_mingw_bridge.a")
    file(WRITE "${OPENCV_DEF_FILE}"
"LIBRARY opencv_world4140.dll
EXPORTS
    _ZN2cv12VideoCaptureC1Eii == ??0VideoCapture@cv@@QEAA@HH@Z
    _ZN2cv12VideoCaptureC1Ev == ??0VideoCapture@cv@@QEAA@XZ
    _ZN2cv12VideoCaptureD1Ev == ??1VideoCapture@cv@@UEAA@XZ
    _ZN2cv12VideoCapture4openEii == ?open@VideoCapture@cv@@UEAA_NHH@Z
    _ZNK2cv12VideoCapture8isOpenedEv == ?isOpened@VideoCapture@cv@@UEBA_NXZ
    _ZN2cv12VideoCapture7releaseEv == ?release@VideoCapture@cv@@UEAAXXZ
    _ZN2cv12VideoCapturersERNS_3MatE == ??5VideoCapture@cv@@UEAAAEAV01@AEAVMat@1@@Z
    _ZN2cv12VideoCapture4readERKNS_12_OutputArrayE == ?read@VideoCapture@cv@@UEAA_NAEBV_OutputArray@2@@Z
    _ZN2cv12VideoCapture3setEid == ?set@VideoCapture@cv@@UEAA_NHN@Z
    _ZNK2cv12VideoCapture3getEi == ?get@VideoCapture@cv@@UEBANH@Z
    _ZN2cv3MatC1Ev == ??0Mat@cv@@QEAA@XZ
    _ZN2cv3MatD1Ev == ??1Mat@cv@@QEAA@XZ
    _ZNK2cv3Mat5emptyEv == ?empty@Mat@cv@@QEBA_NXZ
    _ZN2cv8cvtColorERKNS_11_InputArrayERKNS_12_OutputArrayEiiNS_13AlgorithmHintE == ?cvtColor@cv@@YAXAEBV_InputArray@1@AEBV_OutputArray@1@HHW4AlgorithmHint@1@@Z
")
    find_program(MINGW_DLLTOOL dlltool HINTS "C:/Qt/Tools/mingw1310_64/bin")
    if(MINGW_DLLTOOL)
        execute_process(
            COMMAND "${MINGW_DLLTOOL}" -d "${OPENCV_DEF_FILE}" -l "${OPENCV_BRIDGE_A}"
        )
        set(OPENCV_MINGW_BRIDGE_LIB "${OPENCV_BRIDGE_A}")
    endif()

    add_custom_command(TARGET CapsuX POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/opencv/build/x64/vc16/bin/opencv_world4140.dll"
            "$<TARGET_FILE_DIR:CapsuX>/opencv_world4140.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/opencv/build/bin/opencv_videoio_ffmpeg4140_64.dll"
            "$<TARGET_FILE_DIR:CapsuX>/opencv_videoio_ffmpeg4140_64.dll"
    )
endif()

target_link_libraries(CapsuX PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    ${OpenCV_LIBS}
    ${OPENCV_MINGW_BRIDGE_LIB}
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.CapsuX)
endif()
set_target_properties(CapsuX PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS CapsuX
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(CapsuX)
endif()
