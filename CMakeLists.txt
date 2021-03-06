cmake_minimum_required(VERSION 3.2)
set(KDE_MIN_VERSION "4.4.0")

find_package(ECM 0.0.11 REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(FeatureSummary)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(KF5 REQUIRED COMPONENTS
      Config		#needed for kconfig_add_kcfg_files.
      Completion	#KComboBox
      ConfigWidgets	#KConfigDialog
      DocTools	 	#needed for doc building
      IconThemes	#
      I18n
      KIO             	#KIO
      WidgetsAddons)

#add_definitions(-DKDE_DEFAULT_DEBUG_AREA=7199)


add_subdirectory(doc)
add_subdirectory(po)

set(kio_locate_PART_SRCS
   pattern.cpp
   kio_locate.cpp
   locater.cpp
   UrlUtils.cpp)

qt5_wrap_ui(kio_locate_PART_SRCS
   klocateconfigfilterwidget.ui
   klocateconfiglocatewidget.ui
   klocateconfigwidget.ui)

kconfig_add_kcfg_files(kio_locate_PART_SRCS klocateconfig.kcfgc)

add_library(kio_locate MODULE ${kio_locate_PART_SRCS} ${kio_locate_PART_RCCS})


target_link_libraries(kio_locate KF5::KIOCore
    KF5::Completion KF5::WidgetsAddons KF5::IconThemes KF5::ConfigWidgets
    KF5::I18n)

install(TARGETS kio_locate DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio )

########### install files ###############

install( FILES locate.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/searchproviders )
install( FILES locate.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES locater.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES rlocate.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )

