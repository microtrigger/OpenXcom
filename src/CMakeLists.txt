# Fork modification 2026-10-10: default-off public observation build options.
# See LICENSE-NOTICES.md; existing upstream license retained.
set ( root_src
  lodepng.cpp
  main.cpp
)

set ( basescape_src
  Basescape/BaseInfoState.cpp
  Basescape/BaseView.cpp
  Basescape/BasescapeState.cpp
  Basescape/BuildFacilitiesState.cpp
  Basescape/CraftArmorState.cpp
  Basescape/CraftEquipmentState.cpp
  Basescape/CraftInfoState.cpp
  Basescape/CraftSoldiersState.cpp
  Basescape/CraftWeaponsState.cpp
  Basescape/CraftsState.cpp
  Basescape/DismantleFacilityState.cpp
  Basescape/ManageAlienContainmentState.cpp
  Basescape/ManufactureInfoState.cpp
  Basescape/ManufactureStartState.cpp
  Basescape/ManufactureState.cpp
  Basescape/MiniBaseView.cpp
  Basescape/MonthlyCostsState.cpp
  Basescape/NewManufactureListState.cpp
  Basescape/NewResearchListState.cpp
  Basescape/PlaceFacilityState.cpp
  Basescape/PlaceLiftState.cpp
  Basescape/PlaceStartFacilityState.cpp
  Basescape/PurchaseState.cpp
  Basescape/ResearchInfoState.cpp
  Basescape/ResearchState.cpp
  Basescape/SackSoldierState.cpp
  Basescape/SelectStartFacilityState.cpp
  Basescape/SellState.cpp
  Basescape/SoldierArmorState.cpp
  Basescape/SoldierDiaryMissionState.cpp
  Basescape/SoldierDiaryOverviewState.cpp
  Basescape/SoldierDiaryPerformanceState.cpp
  Basescape/SoldierInfoState.cpp
  Basescape/SoldierMemorialState.cpp
  Basescape/SoldiersState.cpp
  Basescape/StoresState.cpp
  Basescape/TransferBaseState.cpp
  Basescape/TransferConfirmState.cpp
  Basescape/TransferItemsState.cpp
  Basescape/TransfersState.cpp
)

set ( battlescape_src
  Battlescape/AbortMissionState.cpp
  Battlescape/ActionMenuItem.cpp
  Battlescape/ActionMenuState.cpp
  Battlescape/AliensCrashState.cpp
  Battlescape/AIModule.cpp
  Battlescape/BattleState.cpp
  Battlescape/BattlescapeGame.cpp
  Battlescape/BattlescapeGenerator.cpp
  Battlescape/BattlescapeMessage.cpp
  Battlescape/BattlescapeState.cpp
  Battlescape/BriefingState.cpp
  Battlescape/Camera.cpp
  Battlescape/CannotReequipState.cpp
  Battlescape/CommendationLateState.cpp
  Battlescape/CommendationState.cpp
  Battlescape/DebriefingState.cpp
  Battlescape/Explosion.cpp
  Battlescape/ExplosionBState.cpp
  Battlescape/InfoboxOKState.cpp
  Battlescape/InfoboxState.cpp
  Battlescape/Inventory.cpp
  Battlescape/InventoryState.cpp
  Battlescape/Map.cpp
  Battlescape/MedikitState.cpp
  Battlescape/MedikitView.cpp
  Battlescape/MeleeAttackBState.cpp
  Battlescape/MiniMapState.cpp
  Battlescape/MiniMapView.cpp
  Battlescape/NextTurnState.cpp
  Battlescape/Particle.cpp
  Battlescape/Pathfinding.cpp
  Battlescape/PathfindingNode.cpp
  Battlescape/PathfindingOpenSet.cpp
  Battlescape/PrimeGrenadeState.cpp
  Battlescape/Projectile.cpp
  Battlescape/ProjectileFlyBState.cpp
  Battlescape/PromotionsState.cpp
  Battlescape/PsiAttackBState.cpp
  Battlescape/ScannerState.cpp
  Battlescape/ScannerView.cpp
  Battlescape/TileEngine.cpp
  Battlescape/UnitDieBState.cpp
  Battlescape/UnitFallBState.cpp
  Battlescape/UnitInfoState.cpp
  Battlescape/UnitPanicBState.cpp
  Battlescape/UnitSprite.cpp
  Battlescape/UnitTurnBState.cpp
  Battlescape/UnitWalkBState.cpp
  Battlescape/WarningMessage.cpp
)

set ( engine_src
  Engine/Action.cpp
  Engine/Adlib/adlplayer.cpp
  Engine/Adlib/fmopl.cpp
  Engine/AdlibMusic.cpp
  Engine/CatFile.cpp
  Engine/CrossPlatform.cpp
  Engine/FastLineClip.cpp
  Engine/FileMap.cpp
  Engine/FlcPlayer.cpp
  Engine/Font.cpp
  Engine/GMCat.cpp
  Engine/Game.cpp
  Engine/InteractiveSurface.cpp
  Engine/Language.cpp
  Engine/LanguagePlurality.cpp
  Engine/LoaderSession.cpp
  Engine/LoadingUiPolicy.cpp
  Engine/LocalizedText.cpp
  Engine/ModInfo.cpp
  Engine/Music.cpp
  Engine/OpenGL.cpp
  Engine/OptionInfo.cpp
  Engine/Options.cpp
  Engine/Palette.cpp
  Engine/RNG.cpp
  Engine/Scalers/hq2x.cpp
  Engine/Scalers/hq3x.cpp
  Engine/Scalers/hq4x.cpp
  Engine/Scalers/init.cpp
  Engine/Scalers/scale2x.cpp
  Engine/Scalers/scale3x.cpp
  Engine/Scalers/scalebit.cpp
  Engine/Scalers/xbrz.cpp
  Engine/Screen.cpp
  Engine/Sound.cpp
  Engine/SoundSet.cpp
  Engine/State.cpp
  Engine/Surface.cpp
  Engine/SurfaceSet.cpp
  Engine/Timer.cpp
  Engine/Unicode.cpp
  Engine/Zoom.cpp
)

set ( geoscape_src
  Geoscape/AlienBaseState.cpp
  Geoscape/AllocatePsiTrainingState.cpp
  Geoscape/BaseDefenseState.cpp
  Geoscape/BaseDestroyedState.cpp
  Geoscape/BaseNameState.cpp
  Geoscape/BuildNewBaseState.cpp
  Geoscape/ConfirmCydoniaState.cpp
  Geoscape/ConfirmDestinationState.cpp
  Geoscape/ConfirmLandingState.cpp
  Geoscape/ConfirmNewBaseState.cpp
  Geoscape/CraftErrorState.cpp
  Geoscape/CraftPatrolState.cpp
  Geoscape/DogfightErrorState.cpp
  Geoscape/DogfightState.cpp
  Geoscape/FundingState.cpp
  Geoscape/GeoscapeCraftState.cpp
  Geoscape/GeoscapeState.cpp
  Geoscape/Globe.cpp
  Geoscape/GraphsState.cpp
  Geoscape/InterceptState.cpp
  Geoscape/ItemsArrivingState.cpp
  Geoscape/LowFuelState.cpp
  Geoscape/MissionDetectedState.cpp
  Geoscape/MonthlyReportState.cpp
  Geoscape/MultipleTargetsState.cpp
  Geoscape/NewPossibleManufactureState.cpp
  Geoscape/NewPossibleResearchState.cpp
  Geoscape/ProductionCompleteState.cpp
  Geoscape/PsiTrainingState.cpp
  Geoscape/ResearchCompleteState.cpp
  Geoscape/ResearchRequiredState.cpp
  Geoscape/SelectDestinationState.cpp
  Geoscape/TargetInfoState.cpp
  Geoscape/UfoDetectedState.cpp
  Geoscape/UfoLostState.cpp
)

set ( interface_src
  Interface/ArrowButton.cpp
  Interface/Bar.cpp
  Interface/BattlescapeButton.cpp
  Interface/ComboBox.cpp
  Interface/Cursor.cpp
  Interface/FpsCounter.cpp
  Interface/Frame.cpp
  Interface/ImageButton.cpp
  Interface/NumberText.cpp
  Interface/ScrollBar.cpp
  Interface/Slider.cpp
  Interface/Text.cpp
  Interface/TextButton.cpp
  Interface/TextEdit.cpp
  Interface/TextList.cpp
  Interface/ToggleTextButton.cpp
  Interface/Window.cpp
)

set ( menu_src
  Menu/AbandonGameState.cpp
  Menu/ConfirmLoadState.cpp
  Menu/CutsceneState.cpp
  Menu/DeleteGameState.cpp
  Menu/ErrorMessageState.cpp
  Menu/ListGamesState.cpp
  Menu/ListLoadOriginalState.cpp
  Menu/ListLoadState.cpp
  Menu/ListSaveState.cpp
  Menu/LoadGameState.cpp
  Menu/MainMenuState.cpp
  Menu/NewBattleState.cpp
  Menu/NewGameState.cpp
  Menu/OptionsAdvancedState.cpp
  Menu/OptionsAudioState.cpp
  Menu/OptionsBaseState.cpp
  Menu/OptionsBattlescapeState.cpp
  Menu/OptionsConfirmState.cpp
  Menu/OptionsControlsState.cpp
  Menu/OptionsDefaultsState.cpp
  Menu/OptionsFoldersState.cpp
  Menu/OptionsGeoscapeState.cpp
  Menu/ModConfirmExtendedState.cpp
  Menu/ModListState.cpp
  Menu/OptionsNoAudioState.cpp
  Menu/OptionsVideoState.cpp
  Menu/PauseState.cpp
  Menu/SaveGameState.cpp
  Menu/SetWindowedRootState.cpp
  Menu/SlideshowState.cpp
  Menu/StartState.cpp
  Menu/StatisticsState.cpp
  Menu/TestState.cpp
  Menu/VideoState.cpp
)

set ( mod_src
  Mod/AlienDeployment.cpp
  Mod/AlienRace.cpp
  Mod/Armor.cpp
  Mod/ArticleDefinition.cpp
  Mod/City.cpp
  Mod/ExtraSounds.cpp
  Mod/ExtraSprites.cpp
  Mod/ExtraStrings.cpp
  Mod/MCDPatch.cpp
  Mod/MapBlock.cpp
  Mod/MapData.cpp
  Mod/MapDataSet.cpp
  Mod/MapScript.cpp
  Mod/Mod.cpp
  Mod/Polygon.cpp
  Mod/Polyline.cpp
  Mod/RuleAlienMission.cpp
  Mod/RuleBaseFacility.cpp
  Mod/RuleCommendations.cpp
  Mod/RuleConverter.cpp
  Mod/RuleCountry.cpp
  Mod/RuleCraft.cpp
  Mod/RuleCraftWeapon.cpp
  Mod/RuleGlobe.cpp
  Mod/RuleInterface.cpp
  Mod/RuleInventory.cpp
  Mod/RuleItem.cpp
  Mod/RuleManufacture.cpp
  Mod/RuleMissionScript.cpp
  Mod/RuleMusic.cpp
  Mod/RuleRegion.cpp
  Mod/RuleResearch.cpp
  Mod/RuleSoldier.cpp
  Mod/RuleTerrain.cpp
  Mod/RuleUfo.cpp
  Mod/RuleVideo.cpp
  Mod/SoldierNamePool.cpp
  Mod/SoundDefinition.cpp
  Mod/StatString.cpp
  Mod/StatStringCondition.cpp
  Mod/Texture.cpp
  Mod/UfoTrajectory.cpp
  Mod/Unit.cpp
)

set ( savegame_src
  Savegame/AlienBase.cpp
  Savegame/AlienMission.cpp
  Savegame/AlienStrategy.cpp
  Savegame/Base.cpp
  Savegame/BaseFacility.cpp
  Savegame/BattleItem.cpp
  Savegame/BattleUnit.cpp
  Savegame/Country.cpp
  Savegame/Craft.cpp
  Savegame/CraftWeapon.cpp
  Savegame/CraftWeaponProjectile.cpp
  Savegame/EquipmentLayoutItem.cpp
  Savegame/GameTime.cpp
  Savegame/ItemContainer.cpp
  Savegame/MissionSite.cpp
  Savegame/MovingTarget.cpp
  Savegame/Node.cpp
  Savegame/Production.cpp
  Savegame/ProductionCommands.cpp
  Savegame/PurchaseCommands.cpp
  Savegame/CraftOutfittingCommand.cpp
  Savegame/Region.cpp
  Savegame/ResearchProject.cpp
  Savegame/SaveConverter.cpp
  Savegame/SavedBattleGame.cpp
  Savegame/SavedGame.cpp
  Savegame/SerializationHelper.cpp
  Savegame/Soldier.cpp
  Savegame/SoldierDeath.cpp
  Savegame/SoldierDiary.cpp
  Savegame/Target.cpp
  Savegame/Tile.cpp
  Savegame/Transfer.cpp
  Savegame/Ufo.cpp
  Savegame/Vehicle.cpp
  Savegame/Waypoint.cpp
  Savegame/WeightedOptions.cpp
)

set ( ufopedia_src
  Ufopaedia/ArticleState.cpp
  Ufopaedia/ArticleStateArmor.cpp
  Ufopaedia/ArticleStateBaseFacility.cpp
  Ufopaedia/ArticleStateCraft.cpp
  Ufopaedia/ArticleStateCraftWeapon.cpp
  Ufopaedia/ArticleStateItem.cpp
  Ufopaedia/ArticleStateTFTD.cpp
  Ufopaedia/ArticleStateTFTDArmor.cpp
  Ufopaedia/ArticleStateTFTDCraft.cpp
  Ufopaedia/ArticleStateTFTDCraftWeapon.cpp
  Ufopaedia/ArticleStateTFTDFacility.cpp
  Ufopaedia/ArticleStateTFTDItem.cpp
  Ufopaedia/ArticleStateTFTDUso.cpp
  Ufopaedia/ArticleStateTFTDVehicle.cpp
  Ufopaedia/ArticleStateText.cpp
  Ufopaedia/ArticleStateTextImage.cpp
  Ufopaedia/ArticleStateUfo.cpp
  Ufopaedia/ArticleStateVehicle.cpp
  Ufopaedia/Ufopaedia.cpp
  Ufopaedia/UfopaediaSelectState.cpp
  Ufopaedia/UfopaediaStartState.cpp
)

# NOTE: to build with clang *and* libc++ on Linux, you need to call cmake like so:
# cmake -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++ \
#     -D CMAKE_CXX_FLAGS=-stdlib=libc++ -D CMAKE_EXE_LINKER_FLAGS=-stdlib=libc++
#     -D CMAKE_BUILD_TYPE=Release ...
# so both compiler and linker need the flag, otherwise linking breaks or uses libstdc++
# This requires yaml-cpp to also be linked to libc++ instead of libstdc++

set ( data_install_dir bin )
if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
  add_definitions ( -D_DEBUG )
endif ()
if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  if ( "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR ENABLE_WARNING )
    # Enable more GCC warnings if requested or we are doing a Debug build.
    add_compile_options ( -Wall
      -Winit-self
      -Wmissing-include-dirs
      -Wunknown-pragmas
      -Wmissing-format-attribute
      -Wredundant-decls
      -Wformat-security
      -Wtype-limits
      -Wempty-body
      -Wuninitialized
      -Wignored-qualifiers
# These warning options actually generate way too many warnings. Should be enabled later.
#      -Weffc++
#      -Winline
#      -Wshadow
      )
    if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
      # add warning flags recognized by g++ but not by clang
      add_compile_options (
	  )
    elseif ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
      # add warning flags recognized by clang but not by g++
    endif ()
    if ( FATAL_WARNING )
      add_compile_options ( -Werror )
    endif ()
  endif ()

# These warnings aren't worth polluting the code for
  add_compile_options (
    -Wno-unused-function              # cross-platform functions
    )
  if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
    # add warning flags recognized by g++ but not by clang
    add_compile_options (
      )
  elseif ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    # add warning flags recognized by clang but not by g++
    add_compile_options (
      )
  endif ()
endif ()

if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  if ( ENABLE_CLANG_ANALYSIS )
    add_compile_options ( --analyze )
  endif ()
endif ()

if ( MSVC )
  if ( ENABLE_WARNING )
    add_compile_options ( /W${MSVC_WARNING_LEVEL} )
  else ()
    # Set warning levels for Debug and Release with debug info
    set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W${MSVC_WARNING_LEVEL}" )
    set ( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /W${MSVC_WARNING_LEVEL}" )
  endif ()
  if ( FATAL_WARNING )
    add_compile_options ( /WX )
  endif ( FATAL_WARNING )
  set ( CMAKE_EXE_LINKER_FLAGS "/manifest:no" )
  set ( CMAKE_MODULE_LINKER_FLAGS "/manifest:no" ) 
  set ( CMAKE_SHARED_LINKER_FLAGS="/manifest:no" )
endif ()

if ( WIN32 )
  set ( application_type WIN32 )
  set ( openxcom_icon OpenXcom.rc )
endif ()

set ( openxcom_src ${root_src} ${basescape_src} ${battlescape_src} ${engine_src} ${geoscape_src} ${interface_src} ${menu_src} ${mod_src} ${savegame_src} ${ufopedia_src} )

set ( install_dest RUNTIME )
set ( set_exec_path ON )
set ( install_dest_dir bin )
if ( NOT "${DATADIR}" STREQUAL "" )
  add_definitions( -DDATADIR="${DATADIR}" )
endif ()
if ( UNIX AND NOT APPLE )
  set ( data_install_dir "${CMAKE_INSTALL_FULL_DATADIR}/openxcom" )
  add_definitions( -DINSTALLDIR="${data_install_dir}/" )
endif ()
if ( APPLE )
  set ( sdl_src "${MACOS_SDLMAIN_M_PATH}" )
  set ( openxcom_src ${openxcom_src} ${sdl_src} )
  set ( openxcom_icon "${PROJECT_SOURCE_DIR}/res/mac/AppIcon.icns" )
  if ( CREATE_BUNDLE )
    set ( application_type MACOSX_BUNDLE )
    set_source_files_properties( ${openxcom_icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
    set ( MACOSX_BUNDLE_ICON_FILE "AppIcon.icns" )
    set ( MACOSX_BUNDLE_INFO_STRING ${CPACK_PACKAGE_VENDOR} )
    set ( MACOSX_BUNDLE_GUI_IDENTIFIER "SupSuper.openxcom" )
    set ( MACOSX_BUNDLE_LONG_VERSION_STRING ${CPACK_PACKAGE_VERSION} )
    set ( MACOSX_BUNDLE_SHORT_VERSION_STRING "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}" )
    set ( MACOSX_BUNDLE_COPYRIGHT "Copyright © 2018 OpenXcom Developers. All rights reserved." )
    set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR} )
    set ( install_dest BUNDLE )
    set ( set_exec_path OFF )
    set ( install_dest_dir "" )
    set ( data_install_dir bin )
    if ( "${CMAKE_GENERATOR}" STREQUAL "Xcode" )
      set ( bundle_path "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_BUILD_TYPE}/openxcom.app" )
    else ()
      set ( bundle_path "${EXECUTABLE_OUTPUT_PATH}/openxcom.app" )
    endif ()
  else ()
    set ( data_install_dir "${CMAKE_INSTALL_FULL_DATADIR}/openxcom" )
    add_definitions( -DINSTALLDIR="${data_install_dir}/" )
  endif ()
endif ()
if ( set_exec_path )
  set ( EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin )
endif ()

if ( FORCE_INSTALL_DATA_TO_BIN )
  set ( data_install_dir bin )
endif ()

add_executable ( openxcom  ${application_type} ${openxcom_src} ${openxcom_icon} )

# Default OFF. The native-provider verification macro is deliberately not a
# user-settable CMake option; this stage is compile/test authoring, not release.
if ( OPENXCOM_BUILD_RESPONSIVE_LOADING )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_RESPONSIVE_LOADING )
endif()

# Independently buildable helper; responsive integration requires it explicitly.
if ( OPENXCOM_BUILD_LOADING_AUDIO_HANDOFF )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_LOADING_AUDIO_HANDOFF )
  target_sources ( openxcom PRIVATE Engine/LoadingAudioHandoff.cpp )
endif()

if ( OPENXCOM_BUILD_LOADING_PRESENTATION_HELPERS )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_LOADING_PRESENTATION_HELPERS )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; optional loader backend, 2026-10-14.
# LoaderSession.cpp stays unconditional and SDL-neutral. This helper switch
# alone enables neither Game integration nor any test hook. Isolate SDL2's headers from every SDL1 translation
# unit by replacing the bridge target's inherited include directories.
if ( OPENXCOM_BUILD_LINUX_LOADER_BACKEND )
  add_library ( openxcom_loader_sdl_tls OBJECT Engine/LoaderSdlTls.cpp )
  set_property ( TARGET openxcom_loader_sdl_tls PROPERTY INCLUDE_DIRECTORIES
    "${OPENXCOM_LOADER_SDL2_INCLUDE_DIRS}" )
  set_property ( SOURCE Engine/LoaderThreadLinux.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS _GNU_SOURCE )
  target_sources ( openxcom PRIVATE Engine/LoaderThreadLinux.cpp
    $<TARGET_OBJECTS:openxcom_loader_sdl_tls> )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; optional observer addition, 2026-10-10.
if ( EXPERIMENTAL_MIXED_AUDIO_OBSERVER OR EXPERIMENTAL_HUD_OBSERVER OR EXPERIMENTAL_CURSOR_OBSERVER OR EXPERIMENTAL_AGENT_CONTROL )
  target_sources ( openxcom PRIVATE Observer/DiagnosticProfile.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS )
endif()
if ( EXPERIMENTAL_MIXED_AUDIO_OBSERVER )
  target_sources ( openxcom PRIVATE Observer/MixedAudioObserver.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER )
endif()
if ( EXPERIMENTAL_HUD_OBSERVER )
  target_sources ( openxcom PRIVATE Observer/PublicHud.cpp Observer/HudFileObserver.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_HUD_OBSERVER )
endif()
if ( EXPERIMENTAL_CURSOR_OBSERVER )
  target_sources ( openxcom PRIVATE Observer/PresentedCursor.cpp Observer/PresentedCursorSurface.cpp Observer/CursorFileObserver.cpp Observer/PresentationObserver.cpp Observer/PresentedHud.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-10.
# Reuse the controlled rendering/transport seams, with only one fixture profile.
# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-11.
if ( EXPERIMENTAL_SMOKE_DENSITY_GATE )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SMOKE_DENSITY_GATE )
endif()
if ( EXPERIMENTAL_SMOKE_CLIPPING_GATE )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SMOKE_CLIPPING_GATE )
endif()
if ( EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_SMOKE_DENSITY_GATE OR EXPERIMENTAL_SMOKE_CLIPPING_GATE )
  target_sources ( openxcom PRIVATE ../tests/battle-observer/RenderPairGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-10.
if ( EXPERIMENTAL_TIMING_CALIBRATION )
  target_sources ( openxcom PRIVATE ../tests/battle-observer/TimingCalibrationGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE OPENXCOM_EXPERIMENTAL_TIMING_CALIBRATION )
endif()
if ( EXPERIMENTAL_OBSERVER_EFFECTS_GATE )
  target_sources ( openxcom PRIVATE ../tests/battle-observer/ObserverEffectsGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE )
endif()
install ( TARGETS openxcom ${install_dest} DESTINATION ${CMAKE_INSTALL_BINDIR} )
# Extra link flags for Windows. They need to be set before the SDL/YAML link flags, otherwise you will get strange link errors ('Undefined reference to WinMain@16')
if ( WIN32 )
  set ( basic_windows_libs advapi32.lib shell32.lib shlwapi.lib )
  if ( MINGW )
    set ( basic_windows_libs ${basic_windows_libs} mingw32 -mwindows )
    set ( static_flags  -static )
    set ( SDLMIXER_LIBRARY "${SDLMIXER_LIBRARY} -lwinmm" )
  endif ()
  set ( system_libs ${basic_windows_libs} SDLmain ${static_flags} )
endif ()

# backtrace(3) requires libexecinfo on some *BSD systems
if (${CMAKE_SYSTEM_NAME} MATCHES FreeBSD OR ${CMAKE_SYSTEM_NAME} MATCHES NetBSD OR ${CMAKE_SYSTEM_NAME} MATCHES OpenBSD)
  set ( system_libs -lexecinfo )
endif ()

set ( openxcom_sdl_libraries ${SDLIMAGE_LIBRARY} ${SDLMIXER_LIBRARY} ${SDLGFX_LIBRARY} ${SDL_LIBRARY} )
if ( OPENXCOM_BUILD_LINUX_LOADER_BACKEND )
  # Keep the legacy SDL1 compatibility provider first for overlapping exports.
  # Link SDL2 by its selected file, not an imported target whose include usage
  # requirements would leak SDL2 headers into the SDL1 application. Exact loaded
  # provider identity and same-SDL state remain separate runtime acceptance gates.
  set ( openxcom_sdl_libraries ${SDL_LIBRARY} ${SDLIMAGE_LIBRARY} ${SDLMIXER_LIBRARY} ${SDLGFX_LIBRARY} "${OPENXCOM_LOADER_SDL2_LIBRARY}" )
endif()
target_link_libraries ( openxcom ${system_libs} ${openxcom_sdl_libraries} ${OPENGL_LIBRARIES} ${EXTRA_XCOM_LIBS} debug ${YAMLCPP_LIBRARY_DEBUG} optimized ${YAMLCPP_LIBRARY} )
if ( OPENXCOM_BUILD_LINUX_LOADER_BACKEND )
  target_link_libraries ( openxcom Threads::Threads )
endif()

# Pack libraries into bundle and link executable appropriately
if ( APPLE AND CREATE_BUNDLE )
  include ( PostprocessBundle )
  postprocess_bundle ( openxcom ${bundle_path} )
endif ()

set ( bin_data_dirs TFTD UFO common standard )
foreach ( binpath ${bin_data_dirs} )
  if ( APPLE AND CREATE_BUNDLE )
    add_custom_command ( TARGET openxcom
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/bin/${binpath} ${bundle_path}/Contents/Resources/${binpath}
	  VERBATIM )
  else ()
    add_custom_command ( TARGET openxcom
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/bin/${binpath} ${EXECUTABLE_OUTPUT_PATH}/${binpath}
	  VERBATIM )
  endif ()
  install ( DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${binpath} DESTINATION ${data_install_dir} )
endforeach()

# Copy Windows DLLs to bin folder
if ( WIN32 )
  if ( CMAKE_CL_64 )
    file ( GLOB Windows_dll "${DEPS_DIR}/lib/x64/*.dll" )
  else ( )
    file ( GLOB Windows_dll "${DEPS_DIR}/lib/Win32/*.dll" )
  endif()
  foreach ( dll ${Windows_dll} )
    add_custom_command ( TARGET openxcom
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${dll} ${EXECUTABLE_OUTPUT_PATH}
	  VERBATIM )
    install ( FILES ${dll} DESTINATION bin)
  endforeach()
endif ()

#Setup source groups for IDE
if ( MSVC OR "${CMAKE_GENERATOR}" STREQUAL "Xcode" )
  source_group ( "Basescape" FILES ${basescape_src} )
  source_group ( "Battlescape" FILES ${battlescape_src} )
  source_group ( "Engine" FILES ${engine_src} )
  source_group ( "Geoscape" FILES ${geoscape_src} )
  source_group ( "Interface" FILES ${interface_src} )
  source_group ( "Menu" FILES ${menu_src} )
  source_group ( "Mod" FILES ${mod_src} )
  source_group ( "Savegame" FILES ${savegame_src} )
  source_group ( "Ufopedia" FILES ${ufopedia_src} )
  if ( APPLE )
    source_group ( "SDL" FILES ${sdl_src} )
  endif ()
endif ()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-10.
if ( EXPERIMENTAL_NATURAL_FOV_GATE )
  target_sources ( openxcom PRIVATE ../tests/battle-observer/NaturalFovGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-10.
if ( EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE )
  target_sources ( openxcom PRIVATE ../tests/battle-observer/BattlefieldAudioGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE )
  set_property ( TARGET openxcom APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--wrap=Mix_SetPostMix" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; supervised pilot, 2026-10-11.
if ( EXPERIMENTAL_SUPERVISED_STANCE )
  target_sources ( openxcom PRIVATE Observer/StanceController.cpp Observer/StanceConfirmState.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE )
endif()
if ( EXPERIMENTAL_STANCE_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/stance-control/StanceControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_STANCE_CONTROL_GATE )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; supervised selection, 2026-10-12.
if ( EXPERIMENTAL_SUPERVISED_SELECTION )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_SELECTION )
endif()
if ( EXPERIMENTAL_SUPERVISED_SELECTION AND EXPERIMENTAL_STANCE_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/stance-control/SelectionControlGate.cpp )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; supervised turn, 2026-10-12.
if ( EXPERIMENTAL_SUPERVISED_TURN )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_TURN )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; reconstructed Snapshot foundations, 2026-10-13.
# No Snapshot controller/transport is enabled by this incomplete source slice.
if ( EXPERIMENTAL_SUPERVISED_SNAPSHOT )
  target_sources ( openxcom PRIVATE Observer/SnapshotLifetime.cpp Observer/SnapshotRuntime.cpp Observer/SnapshotCatalog.cpp Observer/SnapshotContext.cpp Observer/SnapshotPresentation.cpp Observer/SnapshotImageProof.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; supervised map left-click, 2026-10-12.
if ( EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK )
endif()
if ( EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/map-left-click-control/MapLeftClickControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE )
endif()

if ( EXPERIMENTAL_TURN_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/turn-control/TurnControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_TURN_CONTROL_GATE )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; supervised inventory opening, 2026-10-12.
if ( EXPERIMENTAL_SUPERVISED_INVENTORY )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY )
endif()
if ( EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/inventory-open-control/InventoryOpenControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; supervised inventory closing, 2026-10-12.
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE )
  target_sources ( openxcom PRIVATE Observer/InventoryCloseOverlay.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE )
endif()
if ( EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/inventory-close-control/InventoryCloseControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE )
endif()
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND )
endif()
if ( EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/inventory-ground-control/InventoryGroundControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; supervised inventory click, 2026-10-12.
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK )
endif()
if ( EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/inventory-click-control/InventoryClickControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; supervised inventory right-click, 2026-10-12.
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK )
endif()
if ( EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/inventory-right-click-control/InventoryRightClickControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; supervised inventory Unload, 2026-10-12.
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD )
endif()

if ( EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE )
  target_sources ( openxcom PRIVATE ../tests/inventory-unload-control/InventoryUnloadControlGate.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE )
endif()

# Separate default-off semantic execution ABI; no supervised confirmation overlay.
if ( EXPERIMENTAL_AGENT_CONTROL )
  target_sources ( openxcom PRIVATE Observer/AgentProtocol.cpp Observer/AgentUi.cpp Observer/AgentController.cpp Observer/AgentAudioEvents.cpp Observer/AgentSoundPlayback.cpp Observer/AgentProjectileWitness.cpp Observer/AgentProjectileOutput.cpp Observer/AgentMiniMapPresentation.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_AGENT_CONTROL )
endif()

# Test-only bootstrap; no simulation/message injection in agent providers.
if ( EXPERIMENTAL_AGENT_REPORT_FIXTURE )
  target_sources ( openxcom PRIVATE ../tests/agent-report-fixture/AgentReportFixture.cpp )
  target_compile_definitions ( openxcom PRIVATE OPENXCOM_EXPERIMENTAL_AGENT_REPORT_FIXTURE )
endif()
