# build service
set(SOURCE_FILES
    SimpleLRU.cpp
	StripedLockLRU.cpp
)

add_library(Storage ${SOURCE_FILES})
target_link_libraries(Storage ${CMAKE_THREAD_LIBS_INIT})
