add_library(ro_ui STATIC
    UIWindow.h
    UIWindow.cpp
    UILoginWnd.h
    UILoginWnd.cpp
    UISelectServerWnd.h
    UISelectServerWnd.cpp
    UISelectCharWnd.h
    UISelectCharWnd.cpp
    UIMakeCharWnd.h
    UIMakeCharWnd.cpp
    UINewChatWnd.h
    UINewChatWnd.cpp
    UIBasicInfoWnd.h
    UIBasicInfoWnd.cpp
    UINotifyLevelUpWnd.h
    UINotifyLevelUpWnd.cpp
    UIItemWnd.h
    UIItemWnd.cpp
    UIChooseWnd.h
    UIChooseWnd.cpp
    UIOptionWnd.h
    UIOptionWnd.cpp
    UIFrameWnd.h
    UIFrameWnd.cpp
    UIWindowMgr.h
    UIWindowMgr.cpp
    UIWaitWnd.h
    UIWaitWnd.cpp
    UILoadingWnd.h
    UILoadingWnd.cpp
)

target_include_directories(ro_ui PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ro_ui PUBLIC
    ro_core
    ro_item
)
