#source_group(\\ FILES CMakeLists.txt)

set( QTLAVA_PUBLIC_HEADERS
	QVulkanWindow.h
	Engine.h
)

set( QTLAVA_SOURCES
	QVulkanWindow.cpp
	Engine.cpp
)

set( QTLAVA_HEADERS
)

set( QTLAVA_MOC_HEADERS
)

set( QTLAVA_LINK_LIBRARIES
	lava
	Qt5::Core
	Qt5::Gui
	Qt5::Widgets
)
set(QTLAVA_INCLUDE_NAME qtLava )
set(QTLAVA_NAMESPACE qtLava )
common_library( qtLava )
