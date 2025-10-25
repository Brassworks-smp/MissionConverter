cmake_minimum_required(VERSION 3.10)
project(MissionConverter)

# Set C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add our executable. Assumes main.cpp, json.hpp, httplib.h, and csv.h
# are all in the same directory.
add_executable(mission_converter main.cpp)

# Tell the compiler to look for headers in the current directory
target_include_directories(mission_converter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- Find httplib.h dependencies ---

# Find threads, required by httplib
find_package(Threads REQUIRED)
target_link_libraries(mission_converter PRIVATE Threads::Threads)

# Platform-specific dependencies for HTTPS (OpenSSL) and networking
if (WIN32)
    # Windows: link against system libraries for networking and crypto
    target_link_libraries(mission_converter PRIVATE ws2_32.lib winmm.lib crypt32.lib)
else()
    # Linux, macOS, and other UNIX-likes
    # Find OpenSSL and Zlib
    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)

    # Link against OpenSSL (SSL, Crypto) and Zlib
    target_link_libraries(mission_converter PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)

    if (APPLE)
        # macOS also needs system frameworks
        target_link_libraries(mission_converter PRIVATE "-framework Security -framework SystemConfiguration")
    endif()
endif()

# Set an output directory for the executable (optional, but clean)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
