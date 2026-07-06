
set ( root_src
  lodepng.cpp
  main.cpp
  md5.cpp
)
set ( calypso_src
  Calypso/BattlescapeHud.cpp
  Calypso/CalypsoContractDeliverState.cpp
  Calypso/CalypsoEconomy.cpp
  Calypso/CalypsoEconomyState.cpp
  Calypso/CalypsoMarket.cpp
  Calypso/CalypsoMarketState.cpp
  Calypso/MapGl.cpp
  Calypso/ModHd.cpp
)
set ( basescape_src
  Basescape/BaseInfoState.cpp
  Basescape/BasescapeState.cpp
  Basescape/BaseView.cpp
  Basescape/BuildFacilitiesState.cpp
  Basescape/CraftArmorState.cpp
  Basescape/CraftEquipmentLoadState.cpp
  Basescape/CraftEquipmentSaveState.cpp
  Basescape/CraftEquipmentState.cpp
  Basescape/CraftInfoState.cpp
  Basescape/CraftPilotSelectState.cpp
  Basescape/CraftPilotsState.cpp
  Basescape/CraftSoldiersState.cpp
  Basescape/CraftsState.cpp
  Basescape/CraftWeaponsState.cpp
  Basescape/DismantleFacilityState.cpp
  Basescape/GlobalAlienContainmentState.cpp
  Basescape/GlobalManufactureState.cpp
  Basescape/GlobalResearchDiaryState.cpp
  Basescape/GlobalResearchState.cpp
  Basescape/ItemLocationsState.cpp
  Basescape/ManageAlienContainmentState.cpp
  Basescape/ManufactureDependenciesTreeState.cpp
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
  Basescape/SoldierAvatarState.cpp
  Basescape/SoldierBonusState.cpp
  Basescape/SoldierDiaryLightState.cpp
  Basescape/SoldierDiaryMissionState.cpp
  Basescape/SoldierDiaryOverviewState.cpp
  Basescape/SoldierDiaryPerformanceState.cpp
  Basescape/SoldierInfoState.cpp
  Basescape/SoldierMemorialState.cpp
  Basescape/SoldierRankState.cpp
  Basescape/SoldierSortUtil.cpp
  Basescape/SoldiersState.cpp
  Basescape/SoldierTransformationState.cpp
  Basescape/SoldierTransformationListState.cpp
  Basescape/SoldierTransformState.cpp
  Basescape/StoresState.cpp
  Basescape/TechTreeSelectState.cpp
  Basescape/TechTreeViewerState.cpp
  Basescape/TransferBaseState.cpp
  Basescape/TransferConfirmState.cpp
  Basescape/TransferItemsState.cpp
  Basescape/TransfersState.cpp
)

set ( battlescape_src
  Battlescape/AbortMissionState.cpp
  Battlescape/ActionMenuItem.cpp
  Battlescape/ActionMenuState.cpp
  Battlescape/AIModule.cpp
  Battlescape/AlienInventory.cpp
  Battlescape/AlienInventoryState.cpp
  Battlescape/AliensCrashState.cpp
  Battlescape/BattlescapeGame.cpp
  Battlescape/BattlescapeGenerator.cpp
  Battlescape/BattlescapeMessage.cpp
  Battlescape/BattlescapeState.cpp
  Battlescape/BattleState.cpp
  Battlescape/BriefingLightState.cpp
  Battlescape/BriefingState.cpp
  Battlescape/Camera.cpp
  Battlescape/CannotReequipState.cpp
  Battlescape/CommendationLateState.cpp
  Battlescape/CommendationState.cpp
  Battlescape/ConfirmEndMissionState.cpp
  Battlescape/DebriefingState.cpp
  Battlescape/ExperienceOverviewState.cpp
  Battlescape/Explosion.cpp
  Battlescape/ExplosionBState.cpp
  Battlescape/ExtendedBattlescapeLinksState.cpp
  Battlescape/ExtendedInventoryLinksState.cpp
  Battlescape/InfoboxOKState.cpp
  Battlescape/InfoboxState.cpp
  Battlescape/Inventory.cpp
  Battlescape/InventoryLoadState.cpp
  Battlescape/InventoryPersonalState.cpp
  Battlescape/InventorySaveState.cpp
  Battlescape/InventoryState.cpp
  Battlescape/ItemSprite.cpp
  Battlescape/Map.cpp
  Battlescape/MedikitState.cpp
  Battlescape/MedikitView.cpp
  Battlescape/MeleeAttackBState.cpp
  Battlescape/MiniMapState.cpp
  Battlescape/MiniMapView.cpp
  Battlescape/NextTurnState.cpp
  Battlescape/NoExperienceState.cpp
  Battlescape/Particle.cpp
  Battlescape/Pathfinding.cpp
  Battlescape/PathfindingNode.cpp
  Battlescape/PathfindingOpenSet.cpp
  Battlescape/Position.cpp
  Battlescape/PrimeGrenadeState.cpp
  Battlescape/Projectile.cpp
  Battlescape/ProjectileFlyBState.cpp
  Battlescape/PromotionsState.cpp
  Battlescape/PsiAttackBState.cpp
  Battlescape/ScannerState.cpp
  Battlescape/ScannerView.cpp
  Battlescape/SkillMenuState.cpp
  Battlescape/TileEngine.cpp
  Battlescape/TurnDiaryState.cpp
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
  Engine/Game.cpp
  Engine/GMCat.cpp
  Engine/InteractiveSurface.cpp
  Engine/Language.cpp
  Engine/LanguagePlurality.cpp
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
  Engine/GpuInit.cpp
  Engine/GpuTexture.cpp
  Engine/RenderTarget.cpp
  Engine/Screen.cpp
  Engine/Script.cpp
  Engine/Shader.cpp
  Engine/Shaders/embedded.cpp
  Engine/ShadeTable.cpp
  Engine/ShadeTableCache.cpp
  Engine/ShaderManager.cpp
  Engine/Sound.cpp
  Engine/SoundSet.cpp
  Engine/State.cpp
  Engine/Surface.cpp
  Engine/SurfaceSet.cpp
  Engine/Timer.cpp
  Engine/TouchState.cpp
  Engine/TTFFont.cpp
  Engine/TTFUtil.cpp
  Engine/Unicode.cpp
  Engine/Yaml.cpp
  Engine/Zoom.cpp
)

set ( geoscape_src
  Geoscape/AlienBaseState.cpp
  Geoscape/AllocatePsiTrainingState.cpp
  Geoscape/AllocateTrainingState.cpp
  Geoscape/BaseDefenseState.cpp
  Geoscape/BaseDestroyedState.cpp
  Geoscape/BaseNameState.cpp
  Geoscape/BuildNewBaseState.cpp
  Geoscape/ConfirmCydoniaState.cpp
  Geoscape/ConfirmDestinationState.cpp
  Geoscape/ConfirmLandingState.cpp
  Geoscape/ConfirmNewBaseState.cpp
  Geoscape/CraftErrorState.cpp
  Geoscape/CraftNotEnoughPilotsState.cpp
  Geoscape/CraftPatrolState.cpp
  Geoscape/DogfightErrorState.cpp
  Geoscape/DogfightExperienceState.cpp
  Geoscape/DogfightState.cpp
  Geoscape/ExtendedGeoscapeLinksState.cpp
  Geoscape/FundingState.cpp
  Geoscape/GeoscapeCraftState.cpp
  Geoscape/GeoscapeEventState.cpp
  Geoscape/GeoscapeState.cpp
  Geoscape/Globe.cpp
  Geoscape/GraphsState.cpp
  Geoscape/InterceptState.cpp
  Geoscape/ItemsArrivingState.cpp
  Geoscape/LowFuelState.cpp
  Geoscape/MissionDetectedState.cpp
  Geoscape/MonthlyReportState.cpp
  Geoscape/MultipleTargetsState.cpp
  Geoscape/NewPossibleCraftState.cpp
  Geoscape/NewPossibleFacilityState.cpp
  Geoscape/NewPossibleManufactureState.cpp
  Geoscape/NewPossiblePurchaseState.cpp
  Geoscape/NewPossibleResearchState.cpp
  Geoscape/ProductionCompleteState.cpp
  Geoscape/PsiTrainingState.cpp
  Geoscape/ResearchCompleteState.cpp
  Geoscape/ResearchRequiredState.cpp
  Geoscape/SelectDestinationState.cpp
  Geoscape/SelectMusicTrackState.cpp
  Geoscape/TargetInfoState.cpp
  Geoscape/TrainingFinishedState.cpp
  Geoscape/TrainingState.cpp
  Geoscape/UfoDetectedState.cpp
  Geoscape/UfoLostState.cpp
  Geoscape/UfoTrackerState.cpp
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
  Interface/ProgressBar.cpp
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
  Menu/NotesState.cpp
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
  Menu/CalypsoHDUIState.cpp
  Menu/CalypsoSplashState.cpp
  Menu/SaveGameState.cpp
  Menu/SetWindowedRootState.cpp
  Menu/SlideshowState.cpp
  Menu/StartState.cpp
  Menu/StatisticsState.cpp
  Menu/TestPaletteState.cpp
  Menu/TestState.cpp
  Menu/VideoState.cpp
)

set ( mod_src
  Mod/AlienDeployment.cpp
  Mod/AlienRace.cpp
  Mod/Armor.cpp
  Mod/ArticleDefinition.cpp
  Mod/City.cpp
  Mod/CustomPalettes.cpp
  Mod/ExtraSounds.cpp
  Mod/ExtraSprites.cpp
  Mod/ExtraStrings.cpp
  Mod/MapBlock.cpp
  Mod/MapData.cpp
  Mod/MapDataSet.cpp
  Mod/MapScript.cpp
  Mod/TileAtlasBuilder.cpp
  Mod/UnitSpriteAtlasBuilder.cpp
  Mod/MCDPatch.cpp
  Mod/Mod.cpp
  Mod/Polygon.cpp
  Mod/Polyline.cpp
  Mod/RuleAlienMission.cpp
  Mod/RuleArcScript.cpp
  Mod/RuleBaseFacility.cpp
  Mod/RuleCommendations.cpp
  Mod/RuleConverter.cpp
  Mod/RuleCountry.cpp
  Mod/RuleCraft.cpp
  Mod/RuleCraftWeapon.cpp
  Mod/RuleDamageType.cpp
  Mod/RuleEnviroEffects.cpp
  Mod/RuleEvent.cpp
  Mod/RuleEventScript.cpp
  Mod/RuleGlobe.cpp
  Mod/RuleInterface.cpp
  Mod/RuleInventory.cpp
  Mod/RuleItem.cpp
  Mod/RuleItemCategory.cpp
  Mod/RuleManufacture.cpp
  Mod/RuleManufactureShortcut.cpp
  Mod/RuleMissionScript.cpp
  Mod/RuleMusic.cpp
  Mod/RuleRegion.cpp
  Mod/RuleResearch.cpp
  Mod/RuleSkill.cpp
  Mod/RuleSoldier.cpp
  Mod/RuleSoldierBonus.cpp
  Mod/RuleSoldierTransformation.cpp
  Mod/RuleStartingCondition.cpp
  Mod/RuleStatBonus.cpp
  Mod/RuleTerrain.cpp
  Mod/RuleUfo.cpp
  Mod/RuleVideo.cpp
  Mod/RuleWeaponSet.cpp
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
  Savegame/GeoscapeEvent.cpp
  Savegame/HitLog.cpp
  Savegame/ItemContainer.cpp
  Savegame/MissionSite.cpp
  Savegame/MovingTarget.cpp
  Savegame/Node.cpp
  Savegame/Production.cpp
  Savegame/RankCount.cpp
  Savegame/Region.cpp
  Savegame/ResearchProject.cpp
  Savegame/SaveConverter.cpp
  Savegame/SavedBattleGame.cpp
  Savegame/SavedGame.cpp
  Savegame/SerializationHelper.cpp
  Savegame/Soldier.cpp
  Savegame/SoldierAvatar.cpp
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
  Ufopaedia/ArticleStateSoldier.cpp
  Ufopaedia/ArticleStateText.cpp
  Ufopaedia/ArticleStateTextImage.cpp
  Ufopaedia/ArticleStateTFTD.cpp
  Ufopaedia/ArticleStateTFTDArmor.cpp
  Ufopaedia/ArticleStateTFTDCraft.cpp
  Ufopaedia/ArticleStateTFTDCraftWeapon.cpp
  Ufopaedia/ArticleStateTFTDFacility.cpp
  Ufopaedia/ArticleStateTFTDItem.cpp
  Ufopaedia/ArticleStateTFTDUso.cpp
  Ufopaedia/ArticleStateTFTDVehicle.cpp
  Ufopaedia/ArticleStateUfo.cpp
  Ufopaedia/ArticleStateUnit.cpp
  Ufopaedia/ArticleStateVehicle.cpp
  Ufopaedia/StatsForNerdsState.cpp
  Ufopaedia/Ufopaedia.cpp
  Ufopaedia/UfopaediaSelectState.cpp
  Ufopaedia/UfopaediaStartState.cpp
)

set ( rapidyaml_src
  ../libs/rapidyaml/c4/base64.cpp
  ../libs/rapidyaml/c4/char_traits.cpp
  ../libs/rapidyaml/c4/error.cpp
  ../libs/rapidyaml/c4/format.cpp
  ../libs/rapidyaml/c4/language.cpp
  ../libs/rapidyaml/c4/memory_resource.cpp
  ../libs/rapidyaml/c4/memory_util.cpp
  ../libs/rapidyaml/c4/utf.cpp
  ../libs/rapidyaml/c4/yml/common.cpp
  ../libs/rapidyaml/c4/yml/node.cpp
  ../libs/rapidyaml/c4/yml/node_type.cpp
  ../libs/rapidyaml/c4/yml/parse.cpp
  ../libs/rapidyaml/c4/yml/preprocess.cpp
  ../libs/rapidyaml/c4/yml/reference_resolver.cpp
  ../libs/rapidyaml/c4/yml/tag.cpp
  ../libs/rapidyaml/c4/yml/tree.cpp
  ../libs/rapidyaml/c4/yml/version.cpp

)

set ( cxx_src
    ${root_src}
    ${calypso_src}
    ${basescape_src}
    ${battlescape_src}
    ${engine_src}
    ${geoscape_src}
    ${interface_src}
    ${menu_src}
    ${mod_src}
    ${savegame_src}
    ${ufopedia_src}
    ${rapidyaml_src}
)

set ( c_src
  ../libs/miniz/miniz.c
)
set_property ( SOURCE ${c_src} APPEND PROPERTY COMPILE_DEFINITIONS MINIZ_NO_STDIO )
set_property ( SOURCE Engine/Scalers/xbrz.cpp APPEND_STRING PROPERTY COMPILE_FLAGS -Wno-unused\ )

set ( data_install_dir bin )
if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
  set_property ( SOURCE Engine/Options.cpp APPEND PROPERTY COMPILE_DEFINITIONS _DEBUG )
  set_property ( SOURCE OpenXcom.rc APPEND PROPERTY COMPILE_DEFINITIONS _DEBUG )
endif ()
if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
  # attempt to limit the executable size
  set_property ( SOURCE ${cxx_src} APPEND_STRING PROPERTY COMPILE_FLAGS -femit-struct-debug-reduced\ )
endif ()
if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  # C++ - only warnings
  # C and C++ warnings
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wall\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wextra\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Winit-self\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wmissing-include-dirs\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wunknown-pragmas\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wmissing-format-attribute\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wredundant-decls\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wformat-security\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wtype-limits\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wempty-body\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wuninitialized\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wignored-qualifiers\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wno-unused-parameter\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wno-inline\ )
  if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
    # apple clang 11.0 goes crazy with -Wshadow on the yaml-cpp source code
    set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wshadow\ )
    # add warning flags recognized by g++ but not by clang
    set_property ( SOURCE ${cxx_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wsuggest-override\ )
    set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wclobbered\ )
    set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Wlogical-op\ )
  elseif ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    # add warning flags recognized by clang but not by g++
  endif ()
  if ( FATAL_WARNING )
    set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Werror\ )
  endif ()
endif ()

if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -Qunused-arguments\ )
  set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS -pipe\ )
  if ( ENABLE_CLANG_ANALYSIS )
    set_property ( SOURCE ${cxx_src} ${c_src} APPEND_STRING PROPERTY COMPILE_FLAGS --analyze\ )
  endif ()
endif ()

if ( MSVC )
  # Set warning levels for Debug and Release with debug info
  set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W${MSVC_WARNING_LEVEL}" )
  set ( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /W${MSVC_WARNING_LEVEL}" )
  if ( FATAL_WARNING )
    add_compile_options ( /WX )
  endif ( FATAL_WARNING )
  set ( CMAKE_EXE_LINKER_FLAGS "/manifest:no" )
  set ( CMAKE_MODULE_LINKER_FLAGS "/manifest:no" )
  set ( CMAKE_SHARED_LINKER_FLAGS="/manifest:no" )
endif ()

if ( DUMP_CORE )
  set_property ( SOURCE main.cpp APPEND PROPERTY COMPILE_DEFINITIONS DUMP_CORE )
endif ()

if ( EMBED_ASSETS )
  set_property ( SOURCE OpenXcom.rc APPEND PROPERTY COMPILE_DEFINITIONS EMBED_ASSETS )
  set_property ( SOURCE Engine/CrossPlatform.cpp APPEND PROPERTY COMPILE_DEFINITIONS EMBED_ASSETS )
  set_property ( SOURCE Engine/Options.cpp APPEND PROPERTY COMPILE_DEFINITIONS EMBED_ASSETS )
  add_custom_command ( OUTPUT "${CMAKE_BINARY_DIR}/common.zip" COMMAND zip -r -FS "${CMAKE_BINARY_DIR}/common.zip" .
                        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/common" )
  add_custom_command ( OUTPUT "${CMAKE_BINARY_DIR}/standard.zip" COMMAND zip -r -FS "${CMAKE_BINARY_DIR}/standard.zip" .
                        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/standard" )
  add_custom_target ( zips DEPENDS "${CMAKE_BINARY_DIR}/common.zip" "${CMAKE_BINARY_DIR}/standard.zip" )
endif ()

if ( WIN32 )
  set ( application_type WIN32 )
  set ( openxcom_icon OpenXcom.rc )
endif ()

if ( UNIX AND EMBED_ASSETS )
  enable_language(ASM)
  set ( embed_src OpenXcom.s )
  set_property ( SOURCE OpenXcom.s PROPERTY COMPILE_FLAGS -I${CMAKE_BINARY_DIR} )
endif ()

set ( openxcom_src ${c_src} ${cxx_src} ${embed_src} )
if(EMSCRIPTEN)
  list(APPEND openxcom_src Engine/EmscriptenCompat.cpp)
  list(APPEND openxcom_src Engine/EmscriptenHarness.cpp)
  list(APPEND openxcom_src Engine/GpuSmokeState.cpp)
  list(APPEND openxcom_src Calypso/CalypsoTutorial.cpp)
  list(APPEND openxcom_src Calypso/CalypsoTutorialState.cpp)
  list(APPEND openxcom_src Calypso/CalypsoChecklist.cpp)
  list(APPEND openxcom_src Calypso/GeoscapeChecklist.cpp)
  list(APPEND openxcom_src Calypso/CalypsoAdvisor.cpp)
  list(APPEND openxcom_src Calypso/CalypsoTutorialAskState.cpp)

  # Phase 8b: regenerate embedded shader table before each build when any
  # *.vert or *.frag file in Engine/Shaders/ has changed.
  set(_shader_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Shaders")
  set(_embedded_cpp   "${_shader_src_dir}/embedded.cpp")
  set(_embedded_h     "${_shader_src_dir}/embedded.h")
  set(_embed_script   "${CMAKE_SOURCE_DIR}/../../scripts/embed-shaders.py")

  file(GLOB _shader_sources
       "${_shader_src_dir}/*.vert"
       "${_shader_src_dir}/*.frag")
  # Remove auto-generated files from the dependency list.
  list(REMOVE_ITEM _shader_sources "${_embedded_cpp}" "${_embedded_h}")

  add_custom_command(
    OUTPUT  "${_embedded_cpp}" "${_embedded_h}"
    COMMAND "${Python3_EXECUTABLE}" "${_embed_script}"
    DEPENDS ${_shader_sources} "${_embed_script}"
    COMMENT "Embedding GLSL shaders into embedded.cpp/h"
    VERBATIM
  )
  # Mark generated files as generated so CMake knows not to look for them.
  set_source_files_properties("${_embedded_cpp}" "${_embedded_h}"
    PROPERTIES GENERATED TRUE)
endif()
set ( install_dest RUNTIME )
set ( set_exec_path ON )
set ( install_dest_dir bin )
if ( NOT "${DATADIR}" STREQUAL "" )
  add_definitions( -DDATADIR="${DATADIR}" )
endif ()
if ( UNIX AND NOT APPLE )
  set ( data_install_dir "${CMAKE_INSTALL_FULL_DATADIR}/openxcom" )
  set_property ( SOURCE main.cpp APPEND PROPERTY COMPILE_DEFINITIONS INSTALLDIR="${data_install_dir}/" )
endif ()
if ( APPLE )
  # SDLMain.m is the SDL1 macOS entry point that calls SDL_main().
  # SDL2 does not rename main→SDL_main on macOS; it initialises Cocoa
  # internally.  Skip SDLMain.m for native SDL2 builds.
  if ( NOT EMSCRIPTEN )
    set ( sdl_src "" )
  else ()
    set ( sdl_src "${MACOS_SDLMAIN_M_PATH}" )
  endif ()
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

if (( APPLE AND CREATE_BUNDLE ) OR CPACK_GENERATOR STREQUAL "7Z" )
  set ( CMAKE_INSTALL_BINDIR "." )
endif ()

add_executable ( openxcom  ${application_type} ${openxcom_src} ${openxcom_icon} )

if(EMSCRIPTEN)
  set_target_properties(openxcom PROPERTIES
    OUTPUT_NAME "calypso"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
  target_compile_options(openxcom PRIVATE
    -fexceptions
    -sUSE_SDL_TTF=2
    # libwebp port supplies <webp/decode.h> — needed at compile time for
    # Mod.cpp (uses WebPDecodeRGBA). --use-port in link options alone only
    # makes the header available after the port builds during the first
    # link, which loses the race on clean CI runners where Mod.cpp compiles
    # before any link step has fired.
    "--use-port=${CMAKE_CURRENT_SOURCE_DIR}/ports/libwebp.py"
  )
  # Stub SDL_gfxPrimitives.h for Emscripten (no SDL2_gfx port; implementations in EmscriptenCompat.cpp).
  target_include_directories(openxcom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/compat_sdl)
  target_link_options(openxcom PRIVATE
    -sUSE_SDL=2
    -sUSE_SDL_IMAGE=2
    -sUSE_SDL_MIXER=2
    -sUSE_SDL_TTF=2
    # Phase 8c: libwebp decoder-only port (engine/oxce/src/ports/libwebp.py).
    # sdl2_image's SUPPORTED_FORMATS has no 'webp', so WebP is loaded directly
    # via WebPDecodeRGBA() in Mod.cpp — SDL_image never sees the .webp bytes.
    "--use-port=${CMAKE_CURRENT_SOURCE_DIR}/ports/libwebp.py"
    "-sSDL2_IMAGE_FORMATS=[\"png\",\"jpg\",\"bmp\"]"
    "-sSDL2_MIXER_FORMATS=[\"ogg\"]"
    -sUSE_OGG=1
    -sUSE_VORBIS=1
    -sWASM=1
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=268435456
    # Calypso: lift the heap ceiling from the 2 GB default to the wasm32 max (4 GB).
    # HD tiles + HD-scaled battlescape/menus can push a busy mission past 2 GB and
    # crash with "Cannot enlarge memory ... limit is 2147483648". Grows on demand.
    -sMAXIMUM_MEMORY=4294967296
    -sFORCE_FILESYSTEM=1
    # Phase 8b: GPU shader pipeline requires WebGL2 / GLES 3.0 (preamble emits
    # `#version 300 es`). Without these, SDL2 creates a WebGL1 context and
    # shader compile fails with "unsupported shader version".
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sFULL_ES3=1
    "-sEXPORTED_RUNTIME_METHODS=[\"FS\",\"IDBFS\",\"callMain\",\"ccall\",\"cwrap\",\"wasmMemory\"]"
    "-sEXPORTED_FUNCTIONS=[\"_main\",\"_calypso_screenshot\",\"_calypso_screenshot_gpu\",\"_calypso_gpu_smoke_activate\",\"_calypso_push_mouse_motion\",\"_calypso_set_profile_globe\",\"_calypso_set_profile_battlescape\",\"_calypso_set_profile_readback\",\"_calypso_dump_emit_once\",\"_calypso_set_ssaa_scale\",\"_calypso_gl_context_lost\",\"_calypso_gl_context_restored\",\"_calypso_log_heap\",\"_calypso_heap_used\",\"_calypso_heap_total\",\"_calypso_on_tab_hidden\",\"_calypso_apply_touch_defaults\",\"_calypso_battlescape_zoom\",\"_calypso_text_set\",\"_calypso_text_commit\",\"_calypso_text_cancel\"]"
    -sENVIRONMENT=web
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=initCalypso
    -lidbfs.js
    -Wl,--allow-multiple-definition
    -O2
    -sASSERTIONS=1
    --emit-symbol-map
    -fexceptions
    -sDISABLE_EXCEPTION_CATCHING=0
    # Only GPL OXCE engine data is bundled; TFTD and any other proprietary
    # game files MUST be loaded at runtime via AssetProvider (see
    # docs/MVP_PLAN.md Phase 4). Never add a --preload-file for game-data/.
    "--preload-file=${CMAKE_SOURCE_DIR}/bin/common@/game/common"
    "--preload-file=${CMAKE_SOURCE_DIR}/bin/standard@/game/standard"
  )
else()
  # SDL2_gfx header shim: SDL2_gfx names its header SDL2_gfxPrimitives.h but
  # the OXCE source uses the SDL1-era name SDL_gfxPrimitives.h.
  # SDL2CompatNative.cpp provides native implementations of SDL1 APIs that are
  # stubs in EmscriptenCompat.cpp (e.g. SDL_WarpMouse → SDL_WarpMouseInWindow).
  target_include_directories(openxcom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/compat_sdl_native)
  target_sources(openxcom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/compat_sdl_native/SDL2CompatNative.cpp)
endif()

if ( EMBED_ASSETS )
  add_dependencies(openxcom zips)
endif ()

install ( TARGETS openxcom ${install_dest} DESTINATION ${CMAKE_INSTALL_BINDIR} )
# Extra link flags for Windows. They need to be set before the SDL/YAML link flags, otherwise you will get strange link errors ('Undefined reference to WinMain@16')
if ( WIN32 )
  set ( basic_windows_libs advapi32.lib shell32.lib shlwapi.lib wininet.lib urlmon.lib )
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

# macOS: SDLMain.m uses Cocoa/CoreFoundation; SDL2_main renamed entry point needs SDL2main
if (APPLE AND NOT EMSCRIPTEN)
  set ( system_libs ${system_libs} "-framework Cocoa" "-framework CoreFoundation" "-framework IOKit" )
endif ()

if (WIN32)
  set(CMAKE_EXE_LINKER_FLAGS -Wl,--export-all-symbols)
  set(WIN32_LIBS imagehlp dbghelp)
endif(WIN32)

target_link_libraries ( openxcom ${system_libs} ${PKG_DEPS_LDFLAGS} ${EXTRA_XCOM_LIBS} ${WIN32_LIBS} )

# Pack libraries into bundle and link executable appropriately
if ( APPLE AND CREATE_BUNDLE )
  include ( PostprocessBundle )
  postprocess_bundle ( openxcom ${bundle_path} )
endif ()

if ( NOT EMBED_ASSETS )
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
      install ( DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${binpath} DESTINATION ${data_install_dir} )
    endif ()
  endforeach ()
endif ()

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

# ---- Unit test target (native only) ----------------------------------------
# BUILD_TESTING option and enable_testing() are declared in the parent
# CMakeLists.txt so that CTestTestfile.cmake is generated at the build root.

if(NOT EMSCRIPTEN AND BUILD_TESTING)
    # doctest vendored as a single header in engine/oxce/extern/doctest/.
    # Include path is engine/oxce/extern/ so tests use #include <doctest/doctest.h>.
    add_library(oxce_test_doctest INTERFACE)
    target_include_directories(oxce_test_doctest INTERFACE
        ${CMAKE_SOURCE_DIR}/extern)

    set(OXCE_TEST_SOURCES
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/test_main.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/GlobeMathTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/TileAtlasSpecTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/EmitShade16Test.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/wang_parse_test.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/atlas_cell_resolver_test.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/wang_mask_test.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/AIFairAliensTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/ClusteredSpawnTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/TerrorHuntCiviliansTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/BrutalAITest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/RealisticAccuracyTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/TerrainTacticsTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/HearingTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/SuppressionTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/SquadCoordinationTest.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/unit_tests/CalypsoEconomySimTest.cpp)

    add_executable(oxce_unit_tests ${OXCE_TEST_SOURCES})
    target_link_libraries(oxce_unit_tests PRIVATE oxce_test_doctest)

    add_test(NAME oxce_unit_tests COMMAND oxce_unit_tests)
endif()

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
