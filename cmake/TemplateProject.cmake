set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

include(Platform)
include(CommonLibraries)
include(FetchContent)
include(MavenRepository)

if(BUILD_DEBUG)
    include_scripts()
endif()

include_fmt()
include_phmap()