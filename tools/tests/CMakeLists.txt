cmake_minimum_required(VERSION 3.20)
project(SkillParityChecks LANGUAGES CXX)
enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
function(extract_body variable source start_marker end_marker)
    string(FIND "${source}" "${start_marker}" body_start)
    if(body_start LESS 0)
        message(FATAL_ERROR "Cannot locate production body: ${start_marker}")
    endif()
    string(SUBSTRING "${source}" ${body_start} -1 body_remainder)
    string(FIND "${body_remainder}" "${end_marker}" relative_body_end)
    if(relative_body_end LESS 0)
        message(FATAL_ERROR "Cannot locate end marker '${end_marker}' after production body '${start_marker}'")
    endif()
    math(EXPR body_length "${relative_body_end}")
    string(SUBSTRING "${source}" ${body_start} ${body_length} body)
    set(body_scan "\n${body}")
    string(REGEX MATCHALL "\n[A-Za-z_][A-Za-z0-9_:<>*& ,\t]*[ \t]+[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z_][A-Za-z0-9_]*)*[ \t]*\\([^;{}]*\\)[ \t\r\n]*\\{" function_definitions "${body_scan}")
    list(LENGTH function_definitions function_count)
    if(function_count GREATER 1)
        message(FATAL_ERROR "Extracted body for '${start_marker}' contains ${function_count} function definitions; end marker '${end_marker}' is not the next function boundary")
    endif()
    set(${variable} "${body}" PARENT_SCOPE)
endfunction()
function(extract_test_region variable source region_name)
    set(start_marker "// TEST_BODY_BEGIN ${region_name}")
    set(end_marker "// TEST_BODY_END ${region_name}")
    string(FIND "${source}" "${start_marker}" region_start)
    string(FIND "${source}" "${end_marker}" region_end)
    if(region_start LESS 0 OR region_end LESS region_start)
        message(FATAL_ERROR "Missing or reversed explicit test region markers for '${region_name}'")
    endif()
    string(LENGTH "${start_marker}" marker_length)
    math(EXPR region_start "${region_start} + ${marker_length}")
    math(EXPR region_length "${region_end} - ${region_start}")
    string(SUBSTRING "${source}" ${region_start} ${region_length} region)
    set(${variable} "${region}" PARENT_SCOPE)
endfunction()
add_executable(wave_math_checks WaveMathTests.cpp)
target_compile_features(wave_math_checks PRIVATE cxx_std_17)
target_include_directories(wave_math_checks PRIVATE ../../src)
add_test(NAME wave_math_checks COMMAND wave_math_checks)

# Compile the production packet/update bodies against silent test doubles.
# This exercises their actual control flow without creating a game or device.
file(READ "../../src/gamemode/GameModePacket.cpp" packet_source)
extract_body(skill_packet_actor_lookup "${packet_source}" "CGameActor* ResolveSkillPacketActor(CGameMode& mode, u32 aid)" "CSkill* ResolveSkillPacketUnit(CGameMode& mode, u32 aid)")
extract_body(spirit_has_effect_state_flag "${packet_source}" "bool HasEffectStateFlag(int effectState, int mask)" "bool IsEffectStatePushCart(")
extract_body(spirit_effect_exists "${packet_source}" "bool EffectExists(CAbleToMakeEffect* actor, int effectId)\n{" "CRagEffect* LaunchOn(")
extract_body(spirit_handle_one "${packet_source}" "void HandleSpirits(CGameMode& mode, const PacketView& packet)" "void HandleSpirits2(")
extract_body(spirit_handle_two "${packet_source}" "void HandleSpirits2(CGameMode& mode, const PacketView& packet)" "void HandleDevotionList(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SpiritPacketBodies.inc"
    "${skill_packet_actor_lookup}\n${spirit_has_effect_state_flag}\n${spirit_effect_exists}\n${spirit_handle_one}\n${spirit_handle_two}")
extract_body(devotion_packet_handler "${packet_source}" "void HandleDevotionList(CGameMode& mode, const PacketView& packet)" "void InvalidateWindowTree(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/DevotionPacketBody.inc" "${skill_packet_actor_lookup}\n${devotion_packet_handler}")
extract_body(recovery_resolve_effect "${packet_source}" "int ResolveRecoveryEffectId(" "u32 ResolveRecoveryNumberColor(")
extract_body(recovery_resolve_color "${packet_source}" "u32 ResolveRecoveryNumberColor(" "void EmitRecoveryNumber(")
extract_body(recovery_emit_number "${packet_source}" "void EmitRecoveryNumber(" "std::string ToLowerAsciiStatus(")
extract_body(resurrection_local_actor "${packet_source}" "bool IsLocalPlayerActor(const CGameMode& mode, u32 gid)" "CGameActor* FindRuntimeActorForVanish(")
extract_body(resurrection_find_actor "${packet_source}" "CGameActor* FindRuntimeActorForVanish(CGameMode& mode, u32 gid)" "void BeginActorDeath(")
extract_body(recovery_handler "${packet_source}" "void HandleRecovery(CGameMode& mode, const PacketView& packet)" "void HandleAttackRange(")
extract_body(resurrection_handler "${packet_source}" "void HandleActorResurrection(CGameMode& mode, const PacketView& packet)" "void LogActorPacketSample(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/RecoveryResurrectionPacketBodies.inc"
    "${recovery_resolve_effect}\n${recovery_resolve_color}\n${recovery_emit_number}\n${resurrection_local_actor}\n${resurrection_find_actor}\n${recovery_handler}\n${resurrection_handler}")
extract_body(skill_packet_unit_lookup "${packet_source}" "CSkill* ResolveSkillPacketUnit(CGameMode& mode, u32 aid)" "void HandleNotifyEffect(")
extract_body(replay_try_read_packet "${packet_source}" "bool TryReadPacket(const u8* stream, int availableBytes, PacketView& outPacket, int& consumedBytes)" "void DecodeSrcDst(")
extract_body(replay_decode_src_dst "${packet_source}" "void DecodeSrcDst(const u8* src, int& srcX, int& srcY, int& dstX, int& dstY, int& srcDir, int& dstDir)" "void CGameModePacketRouter::Register(")
extract_body(replay_router_register "${packet_source}" "void CGameModePacketRouter::Register(u16 packetId, Handler handler)" "bool CGameModePacketRouter::Dispatch(")
extract_body(replay_router_dispatch "${packet_source}" "bool CGameModePacketRouter::Dispatch(CGameMode& gameMode, const PacketView& packet) const" "void CGameModePacketRouter::Clear(")
extract_body(replay_router_clear "${packet_source}" "void CGameModePacketRouter::Clear()" "void RegisterDefaultGameModePacketHandlers(")
extract_body(replay_actor_action_handler "${packet_source}" "void HandleActorActionNotify(CGameMode& mode, const PacketView& packet)" "void ApplyMapChangeSessionState(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ReplayPacketCore.inc"
    "${replay_try_read_packet}\n${replay_decode_src_dst}\n${replay_router_register}\n${replay_router_dispatch}\n${replay_router_clear}\n${replay_actor_action_handler}\n")
add_executable(skill_packet_replay_checks replay/SkillPacketReplayTests.cpp)
target_compile_features(skill_packet_replay_checks PRIVATE cxx_std_17)
target_include_directories(skill_packet_replay_checks PRIVATE "../../src" "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME skill_packet_replay_checks COMMAND skill_packet_replay_checks
    "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/skill/replay/actor_action.tsv")
extract_test_region(sound_body "${packet_source}" server_sound_packet_family)
file(READ "../../src/gamemode/GameMode.cpp" mode_source)
extract_body(ambient_sound_body "${mode_source}" "struct AmbientSoundTrigger {" "float g_savedOutdoorCameraLatitude")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AmbientSoundGeometry.inc" "${ambient_sound_body}")
add_executable(ambient_sound_checks AmbientSoundTests.cpp)
target_compile_features(ambient_sound_checks PRIVATE cxx_std_17)
target_include_directories(ambient_sound_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME ambient_sound_checks COMMAND ambient_sound_checks)
add_executable(actor_tint_checks ActorTintTests.cpp)
target_compile_features(actor_tint_checks PRIVATE cxx_std_17)
target_include_directories(actor_tint_checks PRIVATE "../../src")
add_test(NAME actor_tint_checks COMMAND actor_tint_checks)
extract_body(update_body "${mode_source}" "void CGameMode::ProcessPlayWave()" "void CGameMode::PlaceEffect(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SoundBodies.inc" "${skill_packet_actor_lookup}\n${sound_body}\n${update_body}")
file(READ "../../src/session/Session.cpp" session_source)
extract_body(combo_body "${session_source}" "u32 CSession::GetForceAnimEndTick(" "// Ref CSession::GetWeaponType")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ComboBodies.inc" "${combo_body}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/gamemode/GameModePacket.cpp ../../src/gamemode/GameMode.cpp ../../src/session/Session.cpp)
add_executable(server_sound_checks ServerSoundTests.cpp)
target_compile_features(server_sound_checks PRIVATE cxx_std_17)
target_include_directories(server_sound_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME server_sound_checks COMMAND server_sound_checks)
file(READ "../../src/world/GameActorMsgHandler.cpp" combo_actor_source)
file(READ "../../src/world/GameActor.cpp" combo_actor_impl_source)
extract_body(combo_delay_packet_handler "${packet_source}"
    "void HandleComboDelay(CGameMode& mode, const PacketView& packet)" "void HandleBladeStop(")
extract_body(combo_force_animation "${combo_actor_impl_source}"
    "void CRenderObject::SetForceAnimation(" "void CRenderObject::SetForceAnimation2(")
extract_body(combo_force_end_tick "${combo_actor_impl_source}"
    "void CRenderObject::SetForceAnimEndTick(" "void CGameActor::SetNpcState(")
string(FIND "${combo_actor_source}" "void CGameActor::Am_Skill(const int* arg1)" combo_am_skill_start)
if(combo_am_skill_start LESS 0)
    message(FATAL_ERROR "Could not locate production CGameActor::Am_Skill for combo-delay fixture")
endif()
string(SUBSTRING "${combo_actor_source}" ${combo_am_skill_start} -1 combo_am_skill_source)
string(FIND "${combo_am_skill_source}" "    case 272:" combo_skill272_start)
string(FIND "${combo_am_skill_source}" "    case 273:" combo_skill272_end)
if(combo_skill272_start LESS 0 OR combo_skill272_end LESS combo_skill272_start)
    message(FATAL_ERROR "Could not isolate production skill272 handler fragment")
endif()
math(EXPR combo_skill272_length "${combo_skill272_end} - ${combo_skill272_start}")
string(SUBSTRING "${combo_am_skill_source}" ${combo_skill272_start} ${combo_skill272_length} combo_skill272_case)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ComboDelayIntegrationBodies.inc"
    "${skill_packet_actor_lookup}\n${combo_body}\n${combo_delay_packet_handler}\n"
    "${combo_force_animation}\n${combo_force_end_tick}\n"
    "void CGameActor::ApplyComboSkill272(CGameActor* target) { switch (272) {\n${combo_skill272_case}\ndefault: break; } }\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/gamemode/GameModePacket.cpp ../../src/session/Session.cpp
    ../../src/world/GameActor.cpp ../../src/world/GameActorMsgHandler.cpp)
add_executable(combo_delay_skill272_integration_checks ComboDelayIntegrationTests.cpp)
target_compile_features(combo_delay_skill272_integration_checks PRIVATE cxx_std_17)
target_include_directories(combo_delay_skill272_integration_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME combo_delay_skill272_integration_checks COMMAND combo_delay_skill272_integration_checks)
add_test(NAME combo_delay_skill272_integration_source_check
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_combo_delay_skill272_integration.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/gamemode/GameModePacket.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/GameModePacket.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/world/GameActorMsgHandler.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/GameActorMsgHandler.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/world/GameActor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/GameActor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../HighPriest.exe.c")
file(READ "../../src/audio/Wave.cpp" wave_source)
extract_body(wave_body "${wave_source}" "bool CAudio::PlayWaveRelative(" "\n}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/WaveBody.inc" "${wave_body}\n}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/audio/Wave.cpp)
add_executable(wave_dispatch_checks WaveDispatchTests.cpp)
target_compile_features(wave_dispatch_checks PRIVATE cxx_std_17)
target_include_directories(wave_dispatch_checks PRIVATE ../../src "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME wave_dispatch_checks COMMAND wave_dispatch_checks)
file(READ "../../src/world/GameActor.cpp" actor_source)
extract_body(motion_body "${actor_source}" "void CRenderObject::ProcessForcedMotion()" "void CRenderObject::SetRenderInfo(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ForcedMotionBody.inc" "${motion_body}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/world/GameActor.cpp)
add_executable(forced_motion_checks ForcedMotionTests.cpp)
target_compile_features(forced_motion_checks PRIVATE cxx_std_17)
target_include_directories(forced_motion_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME forced_motion_checks COMMAND forced_motion_checks)
extract_body(blade_stop_packet_handler "${packet_source}"
    "void HandleBladeStop(CGameMode& mode, const PacketView& packet)" "void HandleIgnorePacket(")
extract_body(blade_stop_force_animation "${actor_source}"
    "void CRenderObject::SetForceAnimation(" "void CRenderObject::SetForceAnimation2(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/BladeStopIntegrationBodies.inc"
    "${skill_packet_actor_lookup}\n${blade_stop_packet_handler}\n${blade_stop_force_animation}\n${motion_body}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/gamemode/GameModePacket.cpp ../../src/world/GameActor.cpp)
add_executable(blade_stop_integration_checks BladeStopIntegrationTests.cpp)
target_compile_features(blade_stop_integration_checks PRIVATE cxx_std_17)
target_include_directories(blade_stop_integration_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME blade_stop_integration_checks COMMAND blade_stop_integration_checks)
add_test(NAME blade_stop_integration_source_check
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_blade_stop_integration.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/gamemode/GameModePacket.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/GameModePacket.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/world/GameActor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/GameActor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../HighPriest.exe.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/BladeStopIntegrationTests.cpp")

file(READ "../../src/gamemode/GameModePacket.cpp" skill_message_source)
extract_body(skill_message_handler "${skill_message_source}" "void HandleSkillMessage(CGameMode&, const PacketView& packet)" "bool SessionIsAttack(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SkillMessageBody.inc" "${skill_message_handler}")
add_executable(skill_message_checks SkillMessageTests.cpp)
target_compile_features(skill_message_checks PRIVATE cxx_std_17)
target_include_directories(skill_message_checks PRIVATE ../../src "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME skill_message_checks COMMAND skill_message_checks)
file(READ "../../src/gamemode/GameModePacket.cpp" notify_effect3_source)
extract_body(effect_status_rng "${notify_effect3_source}" "void AdvanceEffectStatusRandomState()" "void HandleNotifyEffect3(")
extract_body(notify_effect3_handler "${notify_effect3_source}" "void HandleNotifyEffect3(CGameMode& mode, const PacketView& packet)" "void HandleSkillCastCancel(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/NotifyEffect3Body.inc" "${skill_packet_actor_lookup}\n${effect_status_rng}\n${notify_effect3_handler}")
add_executable(notify_effect3_checks NotifyEffect3Tests.cpp)
target_compile_features(notify_effect3_checks PRIVATE cxx_std_17)
target_include_directories(notify_effect3_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME notify_effect3_checks COMMAND notify_effect3_checks)
file(READ "../../src/gamemode/GameModePacket.cpp" talkbox_source)
extract_body(talkbox_chat_contents "${talkbox_source}" "void HandleTalkboxChatContents(CGameMode& mode, const PacketView& packet)" "void HandleChangeGuild(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/TalkboxChatContentsBody.inc" "${skill_packet_unit_lookup}\n${talkbox_chat_contents}")
add_executable(talkbox_chat_contents_checks TalkboxChatContentsTests.cpp)
target_compile_features(talkbox_chat_contents_checks PRIVATE cxx_std_17)
target_include_directories(talkbox_chat_contents_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME talkbox_chat_contents_checks COMMAND talkbox_chat_contents_checks)
file(READ "../../src/world/SkillUnit.cpp" skill_unit_source)
extract_body(skill_talkbox_text "${skill_unit_source}" "void SetSkillTalkboxText(CSkill& skill, const char* text)" "void CSkill::HideModel()")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SkillTalkboxTextBody.inc" "${skill_talkbox_text}")
add_executable(skill_talkbox_text_checks SkillTalkboxTextTests.cpp)
target_compile_features(skill_talkbox_text_checks PRIVATE cxx_std_17)
target_include_directories(skill_talkbox_text_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME skill_talkbox_text_checks COMMAND skill_talkbox_text_checks)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/world/SkillUnit.cpp)
file(READ "../../src/world/GameActor.cpp" guild_actor_source)
extract_body(set_guild_info "${guild_actor_source}" "void CGameActor::SetGuildInfo(int guildId, int emblemVersion)" "void CGameActor::ClearChatBubbleText()")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SetGuildInfoBody.inc" "${set_guild_info}")
file(READ "../../src/gamemode/GameModePacket.cpp" guild_packet_source)
extract_body(change_guild_packet "${guild_packet_source}" "void HandleChangeGuild(CGameMode& mode, const PacketView& packet)" "namespace {\n\nconstexpr const char* kGuildSkillIdNames[16]")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ChangeGuildPacketBody.inc" "${skill_packet_actor_lookup}\n${change_guild_packet}")
add_executable(change_guild_checks ChangeGuildTests.cpp)
target_compile_features(change_guild_checks PRIVATE cxx_std_17)
target_include_directories(change_guild_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME change_guild_checks COMMAND change_guild_checks)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/world/GameActor.cpp ../../src/gamemode/GameModePacket.cpp)
file(READ "../../src/gamemode/GameMode.cpp" show_image_mode_source)
extract_body(show_image_state "${show_image_mode_source}" "void UpdateShowImageInfo(CGameMode& mode, const char* imageName, int type)" "msgresult_t CGameMode::SendMsg(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ShowImageStateBody.inc" "${show_image_state}")
file(READ "../../src/gamemode/GameModePacket.cpp" show_image_packet_source)
extract_body(show_image_packet "${show_image_packet_source}" "void HandleShowImage(CGameMode& mode, const PacketView& packet)" "void HandleAutoSpellList(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ShowImagePacketBody.inc" "${show_image_packet}")
add_executable(show_image_checks ShowImageTests.cpp)
target_compile_features(show_image_checks PRIVATE cxx_std_17)
target_include_directories(show_image_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME show_image_checks COMMAND show_image_checks)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/gamemode/GameMode.cpp ../../src/gamemode/GameModePacket.cpp)
file(READ "../../src/gamemode/GameModePacket.cpp" guild_skill_packet_source)
string(FIND "${guild_skill_packet_source}" "namespace {\n\nconstexpr const char* kGuildSkillIdNames[16]" guild_skill_table_start)
string(FIND "${guild_skill_packet_source}" "const char* GetGuildSkillIdName(int skillId)" guild_skill_name_start)
if(guild_skill_table_start LESS 0 OR guild_skill_name_start LESS guild_skill_table_start)
    message(FATAL_ERROR "Cannot locate production guild skill id-name table boundaries")
endif()
if(guild_skill_table_start LESS 0 OR guild_skill_name_start LESS guild_skill_table_start)
    message(FATAL_ERROR "Cannot locate guild skill name table")
endif()
math(EXPR guild_skill_table_length "${guild_skill_name_start} - ${guild_skill_table_start}")
string(SUBSTRING "${guild_skill_packet_source}" ${guild_skill_table_start} ${guild_skill_table_length} guild_skill_table)
extract_body(guild_skill_name_helper "${guild_skill_packet_source}" "const char* GetGuildSkillIdName(int skillId)" "void HandleGuildSkillInfo(")
extract_body(guild_skill_packet "${guild_skill_packet_source}" "void HandleGuildSkillInfo(CGameMode& mode, const PacketView& packet)" "void HandleShowImage(")
set(guild_skill_packet "${guild_skill_table}\n${guild_skill_name_helper}\n${guild_skill_packet}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/GuildSkillPacketBody.inc" "${guild_skill_packet}")
file(READ "../../src/session/Session.cpp" guild_skill_session_source)
extract_body(guild_skill_session "${guild_skill_session_source}" "void CSession::ClearGuildSkillItems()" "void CSession::InvalidateUI()")
set(guild_skill_session_body "${guild_skill_session}")
extract_body(guild_skill_session "${guild_skill_session_source}" "bool CSession::SetGuildSkillItem(const PLAYER_SKILL_INFO& skillInfo)" "void CSession::SetHomunSkillItem(")
string(APPEND guild_skill_session_body "\n${guild_skill_session}")
extract_body(guild_skill_session "${guild_skill_session_source}" "const std::list<PLAYER_SKILL_INFO>& CSession::GetGuildSkillItems() const" "const std::list<PLAYER_SKILL_INFO>& CSession::GetHomunSkillItems() const")
string(APPEND guild_skill_session_body "\n${guild_skill_session}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/GuildSkillSessionBodies.inc" "${guild_skill_session_body}")
add_executable(guild_skill_checks GuildSkillTests.cpp)
target_compile_features(guild_skill_checks PRIVATE cxx_std_17)
target_include_directories(guild_skill_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME guild_skill_checks COMMAND guild_skill_checks)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/gamemode/GameModePacket.cpp ../../src/session/Session.cpp)
file(READ "../../src/ui/UIWindow.cpp" ui_invalidate_source)
extract_body(ui_invalidate_children "${ui_invalidate_source}" "void UIWindow::InvalidateChildren()" "void UIWindow::InvalidateWallPaper()")
file(READ "../../src/ui/UIWindowMgr.cpp" ui_manager_invalidate_source)
extract_body(ui_manager_invalidate "${ui_manager_invalidate_source}" "void UIWindowMgr::InvalidateUpdateNeededUI()" "void UIWindowMgr::DrawWallpaperToDC(")
file(READ "../../src/session/Session.cpp" session_invalidate_source)
extract_body(session_invalidate "${session_invalidate_source}" "void CSession::InvalidateUI()" "void CSession::ClearActiveStatusIcons()")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/UIInvalidateBodies.inc" "${ui_invalidate_children}\n${ui_manager_invalidate}\n${session_invalidate}")
add_executable(ui_invalidate_checks UIInvalidateTests.cpp)
target_compile_features(ui_invalidate_checks PRIVATE cxx_std_17)
target_include_directories(ui_invalidate_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME ui_invalidate_checks COMMAND ui_invalidate_checks)
add_test(NAME ui_invalidate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_ui_invalidate_source.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/ui/UIWindow.cpp ../../src/ui/UIWindowMgr.cpp ../../src/session/Session.cpp)
file(READ "../../src/render/Prim.cpp" math_source)
extract_body(matrix_mult "${math_source}" "void MatrixMult(" "void MatrixInverse(")
extract_body(matrix_rotate_x "${math_source}" "void MatrixAppendXRotation(" "void MatrixAppendYRotation(")
extract_body(matrix_rotate_y "${math_source}" "void MatrixAppendYRotation(" "void MatrixAppendZRotation(")
extract_body(matrix_rotate_z "${math_source}" "void MatrixAppendZRotation(" "// --- Intersection Tests")
set(matrix_rotations "${matrix_rotate_x}\n${matrix_rotate_y}\n${matrix_rotate_z}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ParticleMatrixBodies.inc" "${matrix_mult}\n${matrix_rotations}")
file(READ "../../src/world/RagEffect.cpp" effect_source)
extract_body(weather_map_lower_name "${mode_source}" "std::string ToLowerAsciiMapName(" "bool MatchMapName(")
extract_body(weather_map_match_name "${mode_source}" "bool MatchMapName(" "int ResolveRefWeatherCloudEffectId(")
extract_body(weather_cloud_effect_id "${mode_source}" "int ResolveRefWeatherCloudEffectId(" "int ResolveRefMapClearColor(")
extract_body(weather_map_clear_color "${mode_source}" "int ResolveRefMapClearColor(" "void ApplyRefFogForMap(")
set(weather_map_helpers "${weather_map_lower_name}\n${weather_map_match_name}\n${weather_cloud_effect_id}\n${weather_map_clear_color}")
extract_body(weather_map_launch "${mode_source}" "void LaunchMapWeatherEffect(CGameMode& mode)" "void UpdateMapAudio(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/WeatherMapBodies.inc"
    "${weather_map_helpers}\n${weather_map_launch}")
extract_body(project_point "${effect_source}" "bool ProjectPoint(" "float ResolveEffectPixelRatio(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ProjectPointBody.inc" "${project_point}")
extract_body(attr_wave_tables "${session_source}" "constexpr const char* kEnemyHitNormalWaves[]" "struct JobTokenEntry")
extract_body(attr_wave_selector "${session_source}" "const char* CSession::GetAttrWaveName(" "const char* CSession::GetJobHitWaveName(")
# Instrument the production selector's RNG with the same silent sequence as emitters.
string(REPLACE "std::rand()" "SilentRand()" attr_wave_selector "${attr_wave_selector}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AttrWaveBodies.inc" "${attr_wave_tables}\n${attr_wave_selector}")
file(READ "../../src/res/Res.cpp" resource_source)
extract_body(resource_lock "${resource_source}" "int CRes::Lock()" "int CRes::Unlock(")
extract_body(resource_unlock "${resource_source}" "int CRes::Unlock()" "bool CRes::LoadFromBuffer(")
file(READ "../../src/world/RagEffect.h" effect_header)
extract_body(primitive_enum "${effect_header}" "enum EFFECTPRIMID {" "struct EffectBandState")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/PrimitiveEnum.inc" "${primitive_enum}")
extract_body(segment_types "${effect_header}" "struct EffectSegmentState" "constexpr int kRagEffectPortal")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/EffectSegmentTypes.inc" "${segment_types}")
extract_body(gi_types "${effect_header}" "struct EffectGi" "struct EffectSegmentState")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/EffectGiTypes.inc" "${gi_types}")
extract_body(segment_shift_circle "${effect_source}" "void ShiftCircleSegments(" "// HighPriest.exe.c:466090-466174")
extract_body(segment_shift_reference "${effect_source}" "void ShiftReferenceSegments(" "// Ref/RagEffectPrim.cpp:1660-1698")
extract_body(segment_init_reference "${effect_source}" "void InitReferenceSegments(" "// HighPriest.exe.c:55640-55652")
set(segment_shift "${segment_shift_circle}\n${segment_shift_reference}\n${segment_init_reference}")
extract_body(spline_primitive "${effect_source}" "bool UpdateReferenceParticleSpline(" "#include \"StatusStateIconMap.h\"")
extract_body(warp_band "${effect_source}" "void ConfigureHealBand(" "void CRagEffect::LaunchParticleUp(")
extract_body(warp_helper "${effect_source}" "void LaunchWarpCasting(" "void CRagEffect::ApplyPortalWind(")
extract_body(portal_wind_emitter "${effect_source}" "void CRagEffect::ApplyPortalWind(int variant)" "bool CRagEffect::ProcessPortalWind389()")
extract_body(portal_wind402_handler "${effect_source}" "void CRagEffect::ApplyPortalWind402()" "void CRagEffect::ApplyBlackDevil(")
extract_body(blackdevil_emitter "${effect_source}" "void CRagEffect::ApplyBlackDevil(const char* textureName)" "// Ref/RagEffect2.cpp:33659-33923 (0x635F90), effect 321.")
extract_body(flower_cast_emitter "${effect_source}" "void CRagEffect::ApplyFlowerCasting(const char* textureName, float size, int variant)" "void CRagEffect::ApplyDefender(")
extract_body(gi_explosion_emitter "${effect_source}" "void CRagEffect::ApplyGiExplosion(const char* textureName)" "void CRagEffect::ApplyAcidDemon(")
extract_body(forest_light_emitter "${effect_source}" "void CRagEffect::ApplyForestLight(const char* textureName, int variant)" "void CRagEffect::ApplyStin4()")
extract_body(poison_helper "${effect_source}" "void CRagEffect::ApplyPoison(int variant)" "void CRagEffect::ApplyBlind(int level)")
extract_body(throw_item_emitter "${effect_source}" "void CRagEffect::ApplyThrowItem(const char* textureName, int variant)" "bool CRagEffect::ProcessThrowItemEffect()")
extract_body(throw_item_dispatcher "${effect_source}" "bool CRagEffect::ProcessThrowItemEffect()" "bool CRagEffect::ProcessHealBlue()")
extract_body(gumgang_effect_handler "${effect_source}" "bool CRagEffect::ProcessGumGangEffect()" "bool CRagEffect::ProcessBash3DEffect()")
extract_body(pokjuk_sound301_handler "${effect_source}" "void CRagEffect::ProcessPokJukSound301()" "bool CRagEffect::OnProcess2Legacy()")
extract_body(ready_portal_emitter_base "${effect_source}" "void CRagEffect::ApplyReadyPortal()" "void CRagEffect::ApplyPortal()")
extract_body(portal_emitter "${effect_source}" "void CRagEffect::ApplyPortal()" "void CRagEffect::ApplyReadyPortal2()")
set(ready_portal_emitter "${ready_portal_emitter_base}\n${portal_emitter}")
extract_test_region(ice_arrow_emitter "${effect_source}" ice_arrow_family)
extract_body(potion_pillar_emitter "${effect_source}" "void CRagEffect::ApplyPotionPillar(" "bool CRagEffect::ProcessPotionConcentration(")
extract_body(potion_concentration_emitter "${effect_source}" "bool CRagEffect::ProcessPotionConcentration()" "bool CRagEffect::ApplyPotionBerserk()")
extract_body(potion_berserk_emitter "${effect_source}" "bool CRagEffect::ApplyPotionBerserk()" "double EffectHeading(")
set(potion_emitters "${potion_pillar_emitter}\n${potion_concentration_emitter}\n${potion_berserk_emitter}")
extract_body(detoxication_handler "${effect_source}" "bool CRagEffect::ProcessDetoxication21()" "bool CRagEffect::ProcessYufitel93()")
extract_body(spearbmr_handler "${effect_source}" "void CRagEffect::SpawnSpearBmr()" "// TEST_BODY_END spear_bmr_handler")
extract_body(turnundead_handler "${effect_source}" "void CRagEffect::SpawnTurnUndead()" "// TEST_BODY_END turn_undead_handler")
extract_body(waterball2_handler "${effect_source}" "void CRagEffect::SpawnWaterBall2()" "// TEST_BODY_END waterball2_handler")
extract_body(waterball116_handler "${effect_source}" "bool CRagEffect::ProcessWaterBall116()" "// TEST_BODY_END waterball116_handler")
extract_body(fireivy118_handler "${effect_source}" "bool CRagEffect::ProcessFireIvy118()" "// TEST_BODY_END fireivy118_handler")
extract_body(firepillar138_handler "${effect_source}" "bool CRagEffect::ProcessFirePillar138()" "// TEST_BODY_END firepillar138_handler")
extract_body(banjjakii165_handler "${effect_source}" "bool CRagEffect::ProcessBanjjakii165()" "// TEST_BODY_END banjjakii165_handler")
extract_body(revive140_handler "${effect_source}" "bool CRagEffect::ProcessRevive140()" "// TEST_BODY_END revive140_handler")
extract_body(aspersio86_handler "${effect_source}" "bool CRagEffect::ProcessAspersio86()" "// TEST_BODY_END aspersio86_handler")
extract_body(stormgust89_handler "${effect_source}" "bool CRagEffect::ProcessStormGust89()" "// TEST_BODY_END stormgust89_handler")
extract_body(magicrod244_handler "${effect_source}" "bool CRagEffect::ProcessMagicRod244()" "// TEST_BODY_END magicrod244_handler")
extract_body(shieldcharge246_handler "${effect_source}" "bool CRagEffect::ProcessShieldCharge246()" "// TEST_BODY_END shieldcharge246_handler")
extract_body(strip_equipment269_272_handler "${effect_source}" "bool CRagEffect::ProcessStripEquipment269To272()" "// TEST_BODY_END strip_equipment269_272_handler")
extract_body(airtexture416_handler "${effect_source}" "bool CRagEffect::ProcessAirTexture416()" "// TEST_BODY_END airtexture416_handler")
extract_body(hitline_effect_handler "${effect_source}" "bool CRagEffect::ProcessHitLineEffect(short f1, bool applyBodyLight)" "// TEST_BODY_END hitline_effect")
extract_body(tripleattack_emitter "${effect_source}" "void CRagEffect::SpawnTripleAttack(int variant)" "bool CRagEffect::ProcessTripleAttackEffects()")
extract_body(tripleattack_handler "${effect_source}" "bool CRagEffect::ProcessTripleAttackEffects()" "bool CRagEffect::ProcessSoulParticle506()")
extract_body(soulparticle506_handler "${effect_source}" "bool CRagEffect::ProcessSoulParticle506()" "bool CRagEffect::ProcessEffect502()")
extract_body(effect502_handler "${effect_source}" "bool CRagEffect::ProcessEffect502()" "bool CRagEffect::ProcessEffect505()")
extract_body(effect505_handler "${effect_source}" "bool CRagEffect::ProcessEffect505()" "void CRagEffect::SpawnSma2()")
extract_body(sma_emitter "${effect_source}" "void CRagEffect::SpawnSma(int variant)" "bool CRagEffect::ProcessStinSmaEffects()")
extract_body(effect605_handler "${effect_source}" "bool CRagEffect::ProcessEffect605()" "bool CRagEffect::ProcessEffect563()")
extract_body(effect563_handler "${effect_source}" "bool CRagEffect::ProcessEffect563()" "bool CRagEffect::ProcessDamageNumberEffects()")
extract_body(tier3_str_handler "${effect_source}" "bool CRagEffect::ProcessTier3StrEffect()" "void CRagEffect::ApplyActorLight()")
extract_body(actor_light_handler "${effect_source}" "void CRagEffect::ApplyActorLight()" "void CRagEffect::ApplyBodyLight449()")
extract_body(body_light442_handler "${effect_source}" "void CRagEffect::ApplyBodyLight442()" "void CRagEffect::ApplyBodyLight446()")
extract_body(body_light446_handler "${effect_source}" "void CRagEffect::ApplyBodyLight446()" "void CRagEffect::ApplyEffectTexture(")
extract_body(body_light449_handler "${effect_source}" "void CRagEffect::ApplyBodyLight449()" "void CRagEffect::ApplyBodyLight368()")
extract_body(damage_number_effects_handler "${effect_source}" "bool CRagEffect::ProcessDamageNumberEffects()" "bool CRagEffect::ProcessNpcStopEffects()")
extract_body(sma2_552_emitter "${effect_source}" "void CRagEffect::SpawnSma2()" "bool CRagEffect::ProcessSma2Effect552()")
extract_body(sma2_552_handler "${effect_source}" "bool CRagEffect::ProcessSma2Effect552()" "void CRagEffect::SpawnParticlePath556()")
extract_body(particlepath556_emitter "${effect_source}" "void CRagEffect::SpawnParticlePath556()" "bool CRagEffect::ProcessParticlePath556()")
extract_body(particlepath556_handler "${effect_source}" "bool CRagEffect::ProcessParticlePath556()" "bool CRagEffect::ProcessEffect537()")
extract_body(effect537_handler "${effect_source}" "bool CRagEffect::ProcessEffect537()" "bool CRagEffect::ProcessEffect273()")
extract_body(effect599_handler "${effect_source}" "bool CRagEffect::ProcessEffect599()" "bool CRagEffect::ProcessEffect602()")
extract_body(effect602_handler "${effect_source}" "bool CRagEffect::ProcessEffect602()" "void CRagEffect::SpawnDemonstration302()")
extract_body(effect273_handler "${effect_source}" "bool CRagEffect::ProcessEffect273()" "void CRagEffect::SpawnPressure365()")
extract_body(effect440_handler "${effect_source}" "bool CRagEffect::ProcessEffect440()" "// PE HighPriest.exe.c:420778/421365")
extract_body(soulbreaker361_emitter "${effect_source}" "void CRagEffect::SpawnSoulBreaker361()" "bool CRagEffect::ProcessSoulBreaker361Effect()")
extract_body(soulbreaker361_handler "${effect_source}" "bool CRagEffect::ProcessSoulBreaker361Effect()" "bool CRagEffect::ProcessTarotEffect()")
extract_body(pressure365_emitter "${effect_source}" "void CRagEffect::SpawnPressure365()" "void CRagEffect::SpawnGroundShake365()")
extract_body(groundshake365_emitter "${effect_source}" "void CRagEffect::SpawnGroundShake365()" "bool CRagEffect::ProcessEffect365()")
extract_body(effect365_handler "${effect_source}" "bool CRagEffect::ProcessEffect365()" "bool CRagEffect::ProcessSlimPotionEffects(")
extract_body(demonstration302_emitter "${effect_source}" "void CRagEffect::SpawnDemonstration302()" "bool CRagEffect::ProcessDemonstration302()")
extract_body(demonstration302_handler "${effect_source}" "bool CRagEffect::ProcessDemonstration302()" "void CRagEffect::SpawnMaple333()")
extract_body(maple333_emitter "${effect_source}" "void CRagEffect::SpawnMaple333()" "bool CRagEffect::ProcessMaple333()")
extract_body(maple333_handler "${effect_source}" "bool CRagEffect::ProcessMaple333()" "void CRagEffect::SpawnEntry6()")
extract_body(entry6_emitter "${effect_source}" "void CRagEffect::SpawnEntry6()" "bool CRagEffect::ProcessEntry6()")
extract_body(entry6_handler "${effect_source}" "bool CRagEffect::ProcessEntry6()" "void CRagEffect::SpawnWarp8()")
extract_body(warp8_emitter "${effect_source}" "void CRagEffect::SpawnWarp8()" "bool CRagEffect::ProcessWarp8()")
extract_body(warp8_handler "${effect_source}" "bool CRagEffect::ProcessWarp8()" "void CRagEffect::SpawnEnhance9()")
extract_body(enhance9_emitter "${effect_source}" "void CRagEffect::SpawnEnhance9()" "bool CRagEffect::ProcessEnhance9()")
extract_body(enhance9_handler "${effect_source}" "bool CRagEffect::ProcessEnhance9()" "bool CRagEffect::ProcessLord90()")
extract_body(exit7_handler "${effect_source}" "bool CRagEffect::ProcessExit7()" "bool CRagEffect::ProcessEndure11()")
string(REPLACE "rand()" "SilentRand()" exit7_handler "${exit7_handler}")
extract_body(lord90_handler "${effect_source}" "bool CRagEffect::ProcessLord90()" "bool CRagEffect::ProcessIceWall74()")
extract_body(icewall74_handler "${effect_source}" "bool CRagEffect::ProcessIceWall74()" "bool CRagEffect::ProcessGlassWall13()")
extract_body(glasswall13_handler "${effect_source}" "bool CRagEffect::ProcessGlassWall13()" "bool CRagEffect::ProcessMeteorStorm92()")
extract_body(meteorstorm92_handler "${effect_source}" "bool CRagEffect::ProcessMeteorStorm92()" "void CRagEffect::SpawnNpcStop(int variant)")
extract_body(particleupdater "${effect_source}" "void UpdateParticleUpPrimitive(CEffectPrim* prim)" "void RenderParticleUp(")
extract_body(kyrie112_handler "${effect_source}" "bool CRagEffect::ProcessKyrie112()" "// TEST_BODY_END kyrie112_handler")
extract_body(repairweapon101_handler "${effect_source}" "bool CRagEffect::ProcessRepairWeapon101()" "// TEST_BODY_END repairweapon101_handler")
extract_body(maxpower104_handler "${effect_source}" "bool CRagEffect::ProcessMaxPower104()" "// TEST_BODY_END maxpower104_handler")
extract_body(intimidate227_handler "${effect_source}" "bool CRagEffect::ProcessIntimidate227()" "// TEST_BODY_END intimidate227_handler")
extract_body(effect380_handler "${effect_source}" "bool CRagEffect::ProcessEffect380()" "// TEST_BODY_END effect380_handler")
extract_body(effect403_handler "${effect_source}" "bool CRagEffect::ProcessEffect403()" "// TEST_BODY_END effect403_handler")
extract_body(effect413_handler "${effect_source}" "bool CRagEffect::ProcessEffect413()" "// TEST_BODY_END effect413_handler")
extract_body(effect445_handler "${effect_source}" "bool CRagEffect::ProcessEffect445()" "// TEST_BODY_END effect445_handler")
extract_body(effect457_handler "${effect_source}" "bool CRagEffect::ProcessEffect457()" "// TEST_BODY_END effect457_handler")
extract_body(effect466_handler "${effect_source}" "bool CRagEffect::ProcessEffect466()" "// TEST_BODY_END effect466_handler")
extract_body(effect501_handler "${effect_source}" "bool CRagEffect::ProcessEffect501()" "// TEST_BODY_END effect501_handler")
extract_body(effect704_handler "${effect_source}" "bool CRagEffect::ProcessEffect704()" "// TEST_BODY_END effect704_handler")
extract_body(bottom2_effect137_handler "${effect_source}" "bool CRagEffect::ProcessBottom2Effect137()" "bool CRagEffect::ProcessCloaking120()")
extract_body(bottom114_handler "${effect_source}" "bool CRagEffect::ProcessBottom114()" "bool CRagEffect::ProcessBottom2Effect137()")
extract_body(soulstrike15_handler "${effect_source}" "bool CRagEffect::ProcessSoulStrike15(int variant)" "bool CRagEffect::ProcessDetoxication21()")
extract_body(effect650_handler "${effect_source}" "bool CRagEffect::ProcessEffect650()" "bool CRagEffect::ProcessEffect651()")
extract_body(effect651_handler "${effect_source}" "bool CRagEffect::ProcessEffect651()" "bool CRagEffect::ProcessLevel99_2Effect()")
string(REPLACE "std::rand()" "SilentRand()" effect651_handler "${effect651_handler}")
extract_body(effect695_handler "${effect_source}" "bool CRagEffect::ProcessEffect695MapFireworks()" "bool CRagEffect::ProcessLevel99Effect()")
extract_body(effect_texture_animation_handler "${effect_source}" "bool CRagEffect::ProcessEffectTextureAnimation()" "bool CRagEffect::ProcessTier3StrEffect()")
extract_body(status_state_emitter "${effect_source}" "void CRagEffect::SpawnStatusState()" "void CRagEffect::SendMsg(")
file(READ "../../src/world/GameActor.cpp" actor_effect_state_source)
extract_body(actor_effect_state_body "${actor_effect_state_source}" "void CGameActor::ProcessEffectState()" "CMsgEffect* CGameActor::SpawnMsgSprite(")
extract_body(sphere_wind_helper "${effect_source}" "void CRagEffect::ApplySphereWind(const char* textureName, short time, int variant)" "// Ref/dhxj/RagEffect.cpp:10587+; canonical CRagEffect::EarthSpike")
extract_body(cloaking120_handler "${effect_source}" "bool CRagEffect::ProcessCloaking120()" "void CRagEffect::ApplyKyrieGuard()")
extract_test_region(effect133_134_terminal_handler "${effect_source}" effect133_134_terminal_handler)
extract_test_region(warp_emitter "${effect_source}" warp_emitter_family)
extract_test_region(agility_emitters "${effect_source}" agility_emitter_family)
extract_body(blessing_emitter "${effect_source}" "void CRagEffect::SpawnBlessing()" "// Ref/RagEffect.cpp:7721-7761")
extract_test_region(smoke_torch_emitters "${effect_source}" smoke_torch_emitter_family)
string(REPLACE "rand()" "SilentRand()" smoke_torch_emitters "${smoke_torch_emitters}")
extract_test_region(poison_emitters "${effect_source}" poison_emitter_family)
extract_test_region(sight_emitters "${effect_source}" sight_emitter_family)
string(REPLACE "rand()" "SilentRand()" sight_emitters "${sight_emitters}")
extract_test_region(fireball_emitter "${effect_source}" fireball_emitter_family)
extract_body(exit2_emitter "${effect_source}" "void CRagEffect::ApplyExit2(const char* textureName)" "void CRagEffect::ApplyHeal2(const char* textureName, int variant)")
string(REPLACE "kPrimTypeHeal" "PP_HEAL" exit2_emitter "${exit2_emitter}")
extract_body(heal2_emitter "${effect_source}" "void CRagEffect::ApplyHeal2(const char* textureName, int variant)" "void CRagEffect::ApplyHeal(const char* textureName, int variant)")
string(REPLACE "static_cast<EFFECTPRIMID>(kPrimTypeHeal)" "PP_HEAL" heal2_emitter "${heal2_emitter}")
string(REPLACE "rand()" "SilentRand()" heal2_emitter "${heal2_emitter}")
extract_body(heal_blue_handler "${effect_source}" "bool CRagEffect::ProcessHealBlue()" "void CRagEffect::ApplyTeleportation2(const char* textureName, short time)")
extract_body(defender_emitter "${effect_source}" "void CRagEffect::ApplyDefender(const char* textureName)" "void CRagEffect::ApplyMapPillar(const char* textureName, float size, bool wide)")
string(REPLACE "kPrimTypeCasting2" "PP_3DCASTING_2" defender_emitter "${defender_emitter}")
extract_body(volcano_emitter "${effect_source}" "void CRagEffect::ApplyVolcano(const char* textureName, short variant, bool fastSpin)" "void CRagEffect::ApplyBottomVolcano(const char* textureName, float size)")
string(REPLACE "static_cast<EFFECTPRIMID>(kPrimTypeGi2)" "PP_GI_2" volcano_emitter "${volcano_emitter}")
extract_body(map_pillar_emitter "${effect_source}" "void CRagEffect::ApplyMapPillar(const char* textureName, float size, bool wide)" "void CRagEffect::ApplyProvidence(const char* textureName)")
string(REPLACE "static_cast<EFFECTPRIMID>(kPrimTypeMapPillar)" "PP_MAPPILLAR" map_pillar_emitter "${map_pillar_emitter}")
extract_body(providence_emitter "${effect_source}" "void CRagEffect::ApplyProvidence(const char* textureName)" "void CRagEffect::ApplyVolcano(const char* textureName, short variant, bool fastSpin)")
string(REPLACE "static_cast<EFFECTPRIMID>(kPrimTypeMapPillar)" "PP_MAPPILLAR" providence_emitter "${providence_emitter}")
extract_body(wind224_emitter "${effect_source}" "void CRagEffect::ApplyWind()" "void CRagEffect::ApplyStormKick(int variant)")
string(REPLACE "static_cast<EFFECTPRIMID>(kPrimTypeWind)" "PP_WIND" wind224_emitter "${wind224_emitter}")
extract_body(half_sphere_emitter "${effect_source}" "void CRagEffect::ApplyHalfSphere(const char* textureName)" "void CRagEffect::ApplyAttackEnergy(const char* textureName)")
extract_body(attack_energy_emitter "${effect_source}" "void CRagEffect::ApplyAttackEnergy(const char* textureName)" "void CRagEffect::ApplyElectric(short baseRise)")
string(REPLACE "kPrimTypeHalfSphere" "PP_HALFSPHERE" half_sphere_emitter "${half_sphere_emitter}")
string(REPLACE "kPrimTypeAttackEnergy" "PP_ATTACKENERGY" attack_energy_emitter "${attack_energy_emitter}")
extract_body(bash3d_emitter "${effect_source}" "void CRagEffect::SpawnBash3D(" "void CRagEffect::ApplyColorPaper(")
string(REPLACE "rand()" "SilentRand()" bash3d_emitter "${bash3d_emitter}")
extract_body(kouenka_emitter "${effect_source}" "void CRagEffect::ApplyKouenka()" "void CRagEffect::ApplySphereWind(")
string(REPLACE "rand()" "SilentRand()" kouenka_emitter "${kouenka_emitter}")
extract_body(earth_spike_emitter "${effect_source}" "void CRagEffect::ApplyEarthSpike(char variant)" "void CRagEffect::ApplyMagnum2(")
string(REPLACE "rand()" "SilentRand()" earth_spike_emitter "${earth_spike_emitter}")
extract_body(chemical_rise_angle "${effect_source}" "short Chemical2RiseAngle(float dx, float dz)" "void CRagEffect::ApplyChemical2(short time)")
extract_body(chemical3_emitter "${effect_source}" "void CRagEffect::ApplyChemical3(short time)" "void CRagEffect::ApplyChemicalProtection(")
string(REPLACE "rand()" "SilentRand()" chemical3_emitter "${chemical3_emitter}")
extract_body(stin4_caller "${effect_source}" "void CRagEffect::ApplyStin4()" "void CRagEffect::SpawnStin4(")
extract_test_region(stin4_emitter "${effect_source}" stin4_emitter_family)
string(REPLACE "rand()" "SilentRand()" stin4_emitter "${stin4_emitter}")
extract_test_region(stin5_emitter "${effect_source}" stin5_emitter_family)
string(REPLACE "rand()" "SilentRand()" stin5_emitter "${stin5_emitter}")
extract_test_region(rg_coin3_emitter "${effect_source}" rg_coin3_emitter_family)
string(REPLACE "rand()" "SilentRand()" rg_coin3_emitter "${rg_coin3_emitter}")
extract_body(bash3d5_caller "${effect_source}" "void CRagEffect::ApplyBash3D5()" "void CRagEffect::SpawnChookgi(")
extract_body(chookgi_emitter "${effect_source}" "void CRagEffect::SpawnChookgi(" "void CRagEffect::ApplyBottomMagnus(")
extract_body(bottom_magnus_emitter "${effect_source}" "void CRagEffect::ApplyBottomMagnus(const char* textureName, int variant)" "void CRagEffect::ApplyThrowItem(")
string(REPLACE "rand()" "SilentRand()" chookgi_emitter "${chookgi_emitter}")
extract_body(ground_sample_emitter "${effect_source}" "void CRagEffect::ProcessGroundSample()" "bool CRagEffect::ProcessBottomHermode517()")
extract_test_region(bluefall_emitter "${effect_source}" bluefall_emitter_family)
extract_body(bluefall_band_setup "${effect_source}" "void ConfigureBlueFallBand(" "bool UpdateWeatherCloudPrimitive(")
extract_body(prim_cloud_update "${effect_source}" "bool UpdateWeatherCloudPrimitive(CEffectPrim* prim)" "bool UpdateWaterfallPrimitive(")
extract_body(cloud_texture "${effect_source}" "CTexture* ResolveWeatherCloudTexture(int mapVariant)" "std::array<CTexture*, 3> ResolveBlueFallTextures(")
extract_body(weather_cloud_emitter "${effect_source}" "void CRagEffect::SpawnWeatherCloud()" "void CRagEffect::ProcessGroundSample()")
extract_body(sight_projection "${effect_source}" "void ProjectReferenceEffectVertex(" "void SubmitReferenceEffectTriangle(")
extract_body(ruwach_emitter "${effect_source}" "void CRagEffect::SpawnRuwach()" "bool EffectTicksWhenNoFrameTime(")
extract_test_region(angel_emitters "${effect_source}" angel_emitter_family)
extract_body(slash1_update_primitive "${effect_source}" "void UpdateSlash1Primitive(" "void RenderSlash1Primitive(")
extract_body(slash1_render_primitive "${effect_source}" "void RenderSlash1Primitive(" "void UpdateSuperAngelPrimitive(")
set(slash_primitive "${slash1_update_primitive}\n${slash1_render_primitive}")
extract_test_region(angel_render "${effect_source}" reference_particle_render_family)
extract_test_region(particle_render "${effect_source}" particle_render_helper_family)
extract_body(air_texture_renderer "${effect_source}" "void RenderAirTexture(" "void RenderHitLine3(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AirTextureRendererBody.inc" "${air_texture_renderer}")
extract_body(hitline3_renderer "${effect_source}" "void RenderHitLine3(" "void RenderHitLine(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/HitLine3RendererBody.inc" "${hitline3_renderer}")
extract_body(soullink_renderer "${effect_source}" "void RenderSoulLink(" "void RenderSoulParticle(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SoulLinkRendererBody.inc" "${soullink_renderer}")
extract_body(soulparticle_renderer "${effect_source}" "void RenderSoulParticle(" "void RenderHermode(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SoulParticleRendererBody.inc" "${soulparticle_renderer}")
extract_body(hermode_renderer "${effect_source}" "void RenderHermode(" "void UpdateChemicalPrimitive(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/HermodeRendererBody.inc" "${hermode_renderer}")
extract_test_region(circle_render "${effect_source}" circle_render_helper_family)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CircleRenderBodies.inc" "${circle_render}")
extract_body(casting_colors "${effect_source}" "unsigned int CastingSizeRgb(" "void CastingLowPolygonRise(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CastingColorBodies.inc" "${casting_colors}")
extract_test_region(casting_low_rise "${effect_source}" casting_low_render_family)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CastingLowRiseBody.inc" "${casting_low_rise}")
extract_body(electric_path "${effect_source}" "void ElectricPathPoint(" "void RenderElectric(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ElectricPathBody.inc" "${electric_path}")
extract_body(electric2_path "${effect_source}" "void Electric2PathPoint(" "void RenderElectric2(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/Electric2PathBody.inc" "${electric2_path}")
extract_body(wind_sample "${effect_source}" "void WindRenderSample(" "void RenderWind(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/WindSampleBody.inc" "${wind_sample}")
extract_body(aura_renderer "${effect_source}" "void Render3DAura(" "void RenderMagnum2(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AuraRendererBody.inc" "${aura_renderer}")
extract_body(aura2_renderer "${effect_source}" "void Render3DAura2(" "void Render3DAura(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/Aura2RendererBody.inc" "${aura2_renderer}")
extract_body(grand_cross2_corners "${effect_source}" "void GrandCross2Corners(" "void RenderGrandCross2(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/GrandCross2CornersBody.inc" "${grand_cross2_corners}")
extract_test_region(line_link_geometry "${effect_source}" line_link_render_family)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/LineLinkGeometryBody.inc" "${line_link_geometry}")
extract_test_region(intimidate_render_geometry "${effect_source}" intimidate_render_family)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/IntimidateRenderGeometryBody.inc" "${intimidate_render_geometry}")
extract_test_region(gumgang_render_geometry "${effect_source}" gumgang_render_family)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/GumGangRenderGeometryBody.inc" "${gumgang_render_geometry}")
extract_body(tei_hit1_geometry "${effect_source}" "void TeiHit1Corners(" "struct CloudRenderStyle")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/TeiHit1GeometryBody.inc" "${tei_hit1_geometry}")
extract_test_region(cloud_render_geometry "${effect_source}" cloud_render_family)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CloudRenderGeometryBody.inc" "${cloud_render_geometry}")
file(READ "../../src/render/Renderer.cpp" renderer_source)
extract_test_region(reference_triangle_queue "${renderer_source}" reference_triangle_queue)
string(REPLACE "CRenderer::" "SilentRenderer::" reference_triangle_queue "${reference_triangle_queue}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ReferenceTriangleQueueBodies.inc" "${reference_triangle_queue}")
file(READ "../../src/res/ActRes.h" act_header)
extract_body(act_types "${act_header}" "struct CSprClip" "class CActRes")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ParticleActTypes.inc" "${act_types}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ParticleRenderBodies.inc" "${particle_render}")
extract_test_region(job_emitters "${effect_source}" job_emitter_family)
extract_body(entry2 "${effect_source}" "void CRagEffect::ApplyEntry2()" "bool CRagEffect::ApplyJobLvUp50(")
file(READ "../../src/res/EzEffectRes.h" str_types_source)
extract_body(str_types "${str_types_source}" "struct KAC_XFORMDATA" "class CEZeffectRes")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/StrTypes.inc" "${str_types}")
extract_body(effect_render "${effect_source}" "void CRagEffect::Render(matrix*" "CRagEffect* CreateWorldRagEffect(")
extract_body(chookgi_renderer "${effect_source}" "void RenderChookGi(CEffectPrim& prim" "void CEffectPrim::Render(matrix*")
extract_test_region(str_render "${effect_source}" str_render_family)
extract_body(str_delta "${effect_source}" "void AddXformDelta(" "std::string ResolveStrName(")
extract_body(effect_catalog_type "${effect_source}" "struct RagEffectCatalogEntry {" "#include \"RagEffectCatalog.generated.inc\"")
extract_body(effect_catalog_name_resolver "${effect_source}" "std::string ResolveCatalogStrName(" "int ResolveEffectRenderFlags(")
extract_body(effect_texture_emitter "${effect_source}" "void CRagEffect::ApplyEffectTexture(" "bool CRagEffect::ProcessEffectTextureSet(")
extract_body(tarot_effect_handler "${effect_source}" "bool CRagEffect::ProcessTarotEffect()" "bool CRagEffect::ProcessEffectTextureAnimation()")
extract_body(sphere_wind2_emitter "${effect_source}" "void CRagEffect::SpawnSphereWind2(short time)" "bool CRagEffect::ProcessSphereWind2Effect()")
string(REPLACE "prim->m_gi.size()" "4" sphere_wind2_emitter "${sphere_wind2_emitter}")
extract_body(sphere_wind2_handler "${effect_source}" "bool CRagEffect::ProcessSphereWind2Effect()" "void CRagEffect::SpawnSoulBreaker2(short time)")
string(REPLACE "CGameActor* player =" "CRenderObject* player =" sphere_wind2_handler "${sphere_wind2_handler}")
extract_body(soulbreaker2_emitter "${effect_source}" "void CRagEffect::SpawnSoulBreaker2(short time)" "void CRagEffect::SpawnPeongUp(")
string(REPLACE "prim->m_gi.size()" "4" soulbreaker2_emitter "${soulbreaker2_emitter}")
extract_body(soulbreaker2_handler "${effect_source}" "bool CRagEffect::ProcessSoulBreaker2Effect()" "// Ref/RagEffect2.cpp:32055-32096; canonical PE helper at HighPriest.exe.c:457147.")
string(REPLACE "CGameActor* player =" "CRenderObject* player =" soulbreaker2_handler "${soulbreaker2_handler}")
extract_body(beginasura_emitter "${effect_source}" "void CRagEffect::SpawnBeginAsura(int variant, bool secondBurst)" "// Ref/RagEffect2.cpp:32149; canonical helper at HighPriest.exe.c:443146.")
extract_body(beginasura_handler "${effect_source}" "bool CRagEffect::ProcessBeginAsuraEffect()" "bool CRagEffect::ProcessBeginAsura2Effect()")
string(REPLACE "CGameActor* player =" "CRenderObject* player =" beginasura_handler "${beginasura_handler}")
extract_body(energy_drain2_emitter "${effect_source}" "void CRagEffect::SpawnEnergyDrain2()" "// Ref/RagEffect2.cpp:26377; canonical helper at HighPriest.exe.c:433598.")
extract_body(energy_drain3_emitter "${effect_source}" "void CRagEffect::SpawnEnergyDrain3()" "bool CRagEffect::ProcessEnergyDrain2Effect()")
extract_body(energy_drain2_handler "${effect_source}" "bool CRagEffect::ProcessEnergyDrain2Effect()" "bool CRagEffect::ProcessEnergyDrain3Effect()")
extract_body(energy_drain3_handler "${effect_source}" "bool CRagEffect::ProcessEnergyDrain3Effect()" "bool CRagEffect::ProcessSoulBreaker2Effect()")
extract_body(line_link_emitter "${effect_source}" "void CRagEffect::SpawnLineLink(int variant)" "bool CRagEffect::ProcessLineLinkEffects()")
extract_body(line_link_handler "${effect_source}" "bool CRagEffect::ProcessLineLinkEffects()" "void CRagEffect::SpawnTripleAttack(")
string(REPLACE "prim->m_gi.size()" "4" line_link_emitter "${line_link_emitter}")
extract_body(peong_up_emitter "${effect_source}" "void CRagEffect::SpawnPeongUp(const char* textureName, short variant)" "void CRagEffect::SpawnPeong(const char* textureName, int time)")
string(REPLACE "prim->m_gi.size()" "4" peong_up_emitter "${peong_up_emitter}")
extract_body(peong_emitter "${effect_source}" "void CRagEffect::SpawnPeong(const char* textureName, int time)" "bool CRagEffect::ProcessPeongEffects()")
string(REPLACE "prim->m_gi.size()" "4" peong_emitter "${peong_emitter}")
extract_body(peong_handler "${effect_source}" "bool CRagEffect::ProcessPeongEffects()" "void CRagEffect::SpawnBeginAsura(")
extract_body(effect_texture_set_handler "${effect_source}" "bool CRagEffect::ProcessEffectTextureSet()" "bool CRagEffect::ProcessEffect348()")
extract_body(effect348_handler "${effect_source}" "bool CRagEffect::ProcessEffect348()" "void CRagEffect::ApplyLightSphere(")
extract_body(lightsphere_emitter "${effect_source}" "void CRagEffect::ApplyLightSphere(" "bool CRagEffect::ProcessEffect695MapFireworks()")
extract_test_region(str_process "${effect_source}" str_process_family)
extract_test_region(clip_wave_str_helper "${effect_source}" clip_wave_str_helper)
extract_body(blitzbeat115_handler "${effect_source}" "bool CRagEffect::ProcessBlitzBeat115()" "bool CRagEffect::ProcessBottom114()")
extract_body(particle_up_burst_emitter "${effect_source}" "void CRagEffect::ApplyParticleUpBurst()" "void LaunchWarpCasting(")
extract_body(effect_sprite_str_helper "${effect_source}" "bool CRagEffect::ProcessEffectSpriteAndStr()" "bool CRagEffect::ProcessClipWaveStrEffect()")
extract_body(effect_sprite_handler "${effect_source}" "bool CRagEffect::ProcessEffectSprite(int spriteVariant)" "bool CRagEffect::ProcessEffectSpriteAndStr()")
string(REPLACE "bool CRagEffect::ProcessEffectSprite(int spriteVariant)" "bool CRagEffect::ProcessEffectSpriteProduction(int spriteVariant)" effect_sprite_handler "${effect_sprite_handler}")
extract_test_region(land_protector_handler "${effect_source}" land_protector_family)
extract_test_region(ghost_effect_handler "${effect_source}" ghost_effect_family)
extract_test_region(waterfall_effects_handler "${effect_source}" waterfall_effects349_356_family)
extract_test_region(vallentine_effect_handler "${effect_source}" vallentine_effect_family)
extract_test_region(portal3_effect_handler "${effect_source}" portal3_effects561_562_family)
extract_test_region(tanji_effect_handler "${effect_source}" tanji_effect_family)
extract_test_region(basilica_effect_handler "${effect_source}" basilica_effect_family)
extract_test_region(rainbow410_effect_handler "${effect_source}" rainbow410_effect_family)
extract_test_region(hitline3zero428_handler "${effect_source}" hitline3zero428_family)
extract_test_region(hitline3basic434_handler "${effect_source}" hitline3basic434_family)
extract_test_region(pokjuk297_handler "${effect_source}" pokjuk297_family)
extract_test_region(foot447_handler "${effect_source}" foot447_family)
extract_test_region(foot448_handler "${effect_source}" foot448_family)
extract_test_region(foot326_327_handler "${effect_source}" foot326_327_family)
extract_test_region(callzone340_handler "${effect_source}" callzone340_family)
extract_test_region(slim_potion_effects497_499_handler "${effect_source}" slim_potion_effects497_499_family)
extract_test_region(bottom_hermode517_handler "${effect_source}" bottom_hermode517_family)
extract_body(bottom_volcano_emitter "${effect_source}" "void CRagEffect::ApplyBottomVolcano(" "void CRagEffect::ApplyGravitation(")
extract_body(gravitation_emitter "${effect_source}" "void CRagEffect::ApplyGravitation()" "void CRagEffect::ApplyWind()")
extract_test_region(beginasura1_467_handler "${effect_source}" beginasura1_467_family)
extract_test_region(beginasura1_variants468_473_handler "${effect_source}" beginasura1_variants468_473_family)
extract_test_region(bottom_vertical_handler "${effect_source}" bottom_vertical_family)
extract_test_region(bottom_music_handler "${effect_source}" bottom_music_family)
file(READ "../../src/world/World.cpp" world_source)
extract_body(primitive_init "${effect_source}" "void CEffectPrim::Init(" "static_assert(offsetof(EffectGi, height)")
extract_test_region(primitive_update_family "${effect_source}" primitive_update_family)
extract_body(cylinder_update "${effect_source}" "bool UpdateCylinderPrimitive(CEffectPrim* prim)" "bool UpdateParticleOrbitPrimitive(")
extract_test_region(primitive_lifetime "${effect_source}" primitive_lifecycle_family)
extract_body(effect_gate "${effect_source}" "bool EffectTicksWhenNoFrameTime(" "void ConfigureHealBand(")
extract_test_region(str_termination "${effect_source}" str_termination_family)
extract_test_region(casting_update "${effect_source}" casting_update_family)
extract_body(table_angle "${effect_source}" "int TableAngle(int angle)\n{" "void RotateCornerTowardScreen(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CastingUpdateBody.inc" "int TableAngle(int angle);\n${casting_update}\n${table_angle}")
extract_body(paired_saint_emitter "${effect_source}" "void CRagEffect::ApplyPairedSaintCasting(const char* textureName, int variant, bool playWave, float waveYOffset)" "void CRagEffect::ApplyCoupleCasting(")
extract_body(saint_emitter "${effect_source}" "void CRagEffect::ApplySaintCasting(" "void CRagEffect::ApplyBottomOut(")
extract_body(couple_casting "${effect_source}" "void CRagEffect::ApplyCoupleCasting(" "void CRagEffect::ApplyHeartCasting(")
extract_test_region(begin_emitter "${effect_source}" begin_spell_family)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CastingEmitterBodies.inc" "${paired_saint_emitter}\n${saint_emitter}\n${begin_emitter}")
extract_body(str_only_dispatch "${effect_source}" "std::optional<bool> CRagEffect::ProcessReferenceStrOnly()" "bool CRagEffect::ProcessEffectFrame()")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/StrOnlyDispatchBody.inc" "${str_only_dispatch}")
extract_body(effect_tick "${effect_source}" "u8 CRagEffect::OnProcess()" "// Legacy RSW map-emitter scheduling.")
extract_body(effect_dtor "${effect_source}" "CRagEffect::~CRagEffect()" "void CRagEffect::LoadEzEffect(")
extract_test_region(effect_launch "${effect_source}" effect_launch_family)
extract_test_region(grim_tooth "${effect_source}" grim_tooth_family)
extract_body(effect_messages "${effect_source}" "void CRagEffect::SendMsg(" "void CRagEffect::Render(")
extract_test_region(actor_detach "${actor_source}" actor_effect_detach_family)
extract_body(world_update "${world_source}" "void CWorld::UpdateGameObjects()" "CGameActor* CWorld::GetGameActorByAID(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc"
    "${resource_lock}\n${resource_unlock}\n${effect_gate}\n${primitive_lifetime}\n${primitive_init}\n${segment_shift}\nint TableAngle(int angle);\n${primitive_update_family}\n${effect_tick}\n${effect_dtor}\n${effect_launch}\n${effect_messages}\n${actor_detach}\n${world_update}\n${grim_tooth}\n${str_delta}\n${str_process}\n${str_termination}\n${entry2}\n${str_render}\n${chookgi_renderer}\n${effect_render}\n${spline_primitive}\n${slash_primitive}\n${angel_render}\n${job_emitters}\n${angel_emitters}\n${ruwach_emitter}\n${smoke_torch_emitters}\n${poison_emitters}\n${blessing_emitter}\n${sight_projection}\n${agility_emitters}\n${sight_emitters}\n${warp_band}\n${warp_helper}\n${blackdevil_emitter}\n${flower_cast_emitter}\n${gi_explosion_emitter}\n${forest_light_emitter}\n${poison_helper}\n${pokjuk_sound301_handler}\n${warp_emitter}\n${ready_portal_emitter}\n${potion_emitters}\n${detoxication_handler}\n${ice_arrow_emitter}\n${fireball_emitter}\n${exit2_emitter}\n${defender_emitter}\n${volcano_emitter}\n${half_sphere_emitter}\n${attack_energy_emitter}\n${bash3d_emitter}\n${kouenka_emitter}\n${earth_spike_emitter}\n${chemical_rise_angle}\n${chemical3_emitter}\n${stin4_caller}\n${stin4_emitter}\n${stin5_emitter}\n${rg_coin3_emitter}\n${bash3d5_caller}\n${chookgi_emitter}\n${ground_sample_emitter}\n${prim_cloud_update}\n${cloud_texture}\n${weather_cloud_emitter}\n${spearbmr_handler}\n${turnundead_handler}\n${waterball2_handler}\n${revive140_handler}\n${aspersio86_handler}\n${stormgust89_handler}\n${magicrod244_handler}\n${shieldcharge246_handler}\n${strip_equipment269_272_handler}\n${kyrie112_handler}\n${repairweapon101_handler}\n${maxpower104_handler}\n${intimidate227_handler}\n${effect380_handler}\n${effect403_handler}\n${effect413_handler}\n${effect445_handler}\n${effect457_handler}\n${effect466_handler}\n${effect501_handler}\n${effect704_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${map_pillar_emitter}\n${providence_emitter}\n${wind224_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${heal2_emitter}\n${heal_blue_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${bottom2_effect137_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${bottom114_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${soulstrike15_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect650_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect651_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect695_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect_texture_animation_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${status_state_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${actor_effect_state_body}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${sphere_wind_helper}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${cloaking120_handler}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/render/Renderer.cpp ../../src/render/Prim.cpp ../../src/res/ActRes.h ../../src/world/RagEffect.cpp ../../src/world/RagEffect.h ../../src/world/GameActor.cpp ../../src/world/World.cpp ../../src/res/EzEffectRes.h ../../src/res/Res.cpp)
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${airtexture416_handler}\n${hitline_effect_handler}\n${tripleattack_emitter}\n${tripleattack_handler}\n${soulparticle506_handler}\n${effect502_handler}\n${effect505_handler}\n${sma2_552_emitter}\n${sma2_552_handler}\n${particlepath556_emitter}\n${particlepath556_handler}\n${effect537_handler}\n${soulbreaker361_emitter}\n${soulbreaker361_handler}\n${effect273_handler}\n${pressure365_emitter}\n${groundshake365_emitter}\n${effect365_handler}\n${demonstration302_emitter}\n${demonstration302_handler}\n${maple333_emitter}\n${maple333_handler}\n${entry6_emitter}\n${entry6_handler}\n${warp8_emitter}\n${warp8_handler}\n${enhance9_emitter}\n${enhance9_handler}\n${exit7_handler}\n${lord90_handler}\n${icewall74_handler}\n${glasswall13_handler}\n${meteorstorm92_handler}\n${effect440_handler}\n${effect599_handler}\n${effect602_handler}\n${particleupdater}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${waterball116_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${throw_item_emitter}\n${throw_item_dispatcher}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${bottom_magnus_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${portal_wind_emitter}\n${portal_wind402_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${gumgang_effect_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${gravitation_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect348_handler}\n${lightsphere_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${cylinder_update}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${fireivy118_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${firepillar138_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${banjjakii165_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${clip_wave_str_helper}\n${blitzbeat115_handler}\n${particle_up_burst_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect_sprite_str_helper}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect_sprite_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${land_protector_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${ghost_effect_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${waterfall_effects_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${vallentine_effect_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${portal3_effect_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${bottom_vertical_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${bottom_music_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${tanji_effect_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${basilica_effect_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${rainbow410_effect_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${hitline3zero428_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${hitline3basic434_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${pokjuk297_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${foot447_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${foot448_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${foot326_327_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${callzone340_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${slim_potion_effects497_499_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${bottom_hermode517_handler}\n${bottom_volcano_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\nconstexpr float kWaterfallSegmentHeight = 40.0f;\n${bluefall_band_setup}\n${bluefall_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${beginasura1_467_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${beginasura1_variants468_473_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${sma_emitter}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${couple_casting}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect605_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect563_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${tier3_str_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${actor_light_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${body_light442_handler}\n${body_light446_handler}\n${body_light449_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${damage_number_effects_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect133_134_terminal_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${effect_catalog_type}\n${effect_catalog_name_resolver}\n${effect_texture_emitter}\n${effect_texture_set_handler}\n${tarot_effect_handler}\n${sphere_wind2_emitter}\n${sphere_wind2_handler}\n${soulbreaker2_emitter}\n${soulbreaker2_handler}\n${peong_up_emitter}\n${peong_emitter}\n${peong_handler}\n${beginasura_emitter}\n${beginasura_handler}\n${energy_drain2_emitter}\n${energy_drain3_emitter}\n${energy_drain2_handler}\n${energy_drain3_handler}")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/EffectLifecycleBodies.inc" "\n${line_link_emitter}\n${line_link_handler}")
add_executable(effect_lifecycle_checks EffectLifecycleTests.cpp)
target_compile_features(effect_lifecycle_checks PRIVATE cxx_std_17)
target_include_directories(effect_lifecycle_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" ../../src/world)
if(MSVC)
    target_link_options(effect_lifecycle_checks PRIVATE /STACK:8388608)
endif()
add_test(NAME effect_lifecycle_checks COMMAND effect_lifecycle_checks)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/gamemode/GameMode.cpp)

add_executable(particle_render_checks ParticleRenderTests.cpp)
target_compile_features(particle_render_checks PRIVATE cxx_std_17)
target_include_directories(particle_render_checks PRIVATE ../../src "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME particle_render_checks COMMAND particle_render_checks)

# Compile the production guild skill row refresh body against silent hooks, and
# compare its behavior/order to the executable dispatch and Ref decompilation.
file(READ "../../src/ui/GuildSkillWndRows.cpp" guild_skill_refresh_source)
extract_body(guild_skill_refresh_body "${guild_skill_refresh_source}" "void GuildSkillWndRows::RefreshFromGuildSkillItems(" "\n}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/GuildSkillWndRefreshBody.inc" "${guild_skill_refresh_body}\n}")
add_executable(guild_skill_wnd_refresh_checks ../GuildSkillWndRefreshTests.cpp)
target_compile_features(guild_skill_wnd_refresh_checks PRIVATE cxx_std_17)
target_include_directories(guild_skill_wnd_refresh_checks PRIVATE ../../src "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(guild_skill_wnd_refresh_checks PRIVATE
    GUILD_SKILL_EXE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../../HighPriest.exe\"
    GUILD_SKILL_DECOMP_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../../HighPriest.exe.c\"
    GUILD_SKILL_REF_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/UIGuildWnd.cpp\")
add_test(NAME guild_skill_wnd_refresh_checks COMMAND guild_skill_wnd_refresh_checks)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/ui/GuildSkillWndRows.cpp
    ../../src/ui/GuildSkillWndRows.h
    ../../HighPriest.exe
    ../../HighPriest.exe.c
    ../../Ref/UIGuildWnd.cpp)

file(READ "../../src/gamemode/GameModePacket.cpp" auto_spell_packet_source)
extract_body(auto_spell_packet "${auto_spell_packet_source}" "void HandleAutoSpellList(" "void DecodePosDir(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AutoSpellPacketBody.inc" "${auto_spell_packet}")
add_executable(auto_spell_packet_checks AutoSpellPacketTests.cpp)
target_compile_features(auto_spell_packet_checks PRIVATE cxx_std_17)
target_include_directories(auto_spell_packet_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME auto_spell_packet_checks COMMAND auto_spell_packet_checks)
add_test(NAME auto_spell_packet_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_autospell_packet.py")
add_test(NAME spirits_packet_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_spirits_packet_source.py")
add_test(NAME spirits2_packet_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_spirits2_packet_source.py")
add_executable(spirit_packet_lifecycle_checks SpiritPacketLifecycleTests.cpp)
target_compile_features(spirit_packet_lifecycle_checks PRIVATE cxx_std_17)
target_include_directories(spirit_packet_lifecycle_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME spirit_packet_lifecycle_checks COMMAND spirit_packet_lifecycle_checks)

# Integrate the production spirit packet handlers with the production CHOOKGI
# emitter against a silent primitive/resource/RNG harness.
file(READ "../../src/world/RagEffect.cpp" spirit_chookgi_source)
extract_body(spirit_chookgi_emitter "${spirit_chookgi_source}" "void CRagEffect::SpawnChookgi(" "void CRagEffect::ApplyBottomMagnus(")
string(REPLACE "rand()" "SilentRand()" spirit_chookgi_emitter "${spirit_chookgi_emitter}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SpiritChookgiBodies.inc"
    "${skill_packet_actor_lookup}\n${spirit_has_effect_state_flag}\n${spirit_effect_exists}\n${spirit_handle_one}\n${spirit_chookgi_emitter}")
add_executable(spirit_chookgi_integration_checks SpiritChookgiIntegrationTests.cpp)
target_compile_features(spirit_chookgi_integration_checks PRIVATE cxx_std_17)
target_include_directories(spirit_chookgi_integration_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" ../../src/world)
add_test(NAME spirit_chookgi_integration_checks COMMAND spirit_chookgi_integration_checks)
add_test(NAME spirit_chookgi_integration_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_spirit_chookgi_integration.py")
add_executable(devotion_packet_lifecycle_checks DevotionPacketLifecycleTests.cpp)
target_compile_features(devotion_packet_lifecycle_checks PRIVATE cxx_std_17)
target_include_directories(devotion_packet_lifecycle_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME devotion_packet_lifecycle_checks COMMAND devotion_packet_lifecycle_checks)
add_test(NAME devotion_packet_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_devotion_packet_source.py")
add_executable(recovery_resurrection_packet_checks RecoveryResurrectionPacketTests.cpp)
target_compile_features(recovery_resurrection_packet_checks PRIVATE cxx_std_17)
target_include_directories(recovery_resurrection_packet_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME recovery_resurrection_packet_checks COMMAND recovery_resurrection_packet_checks)
add_test(NAME recovery_resurrection_packet_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_recovery_resurrection_packet.py")
add_test(NAME prim2dflash_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim2dflash.py")
add_test(NAME prim_onprocess_inventory_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../audit_prim_onprocess_inventory.py")
add_test(NAME effectprim_sendmsg_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effectprim_sendmsg.py")
add_test(NAME initsegment_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_initsegment_source.py")
add_test(NAME primitive_anim_motion_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_primitive_anim_motion.py")
add_test(NAME primitive_anim_texture_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_primitive_anim_texture.py")
add_test(NAME groundskill_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_groundskill_source.py")
add_test(NAME process_wba_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_process_wba_source.py")
add_test(NAME force_animation2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_force_animation2_source.py")
add_test(NAME actor_setstate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_setstate_source.py")
add_test(NAME actor_processstate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_processstate_source.py")
add_test(NAME process_effect_state_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_process_effect_state_source.py")
add_test(NAME make_special_attack_effect_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_make_special_attack_effect_source.py")
add_test(NAME mandra_attack_effect_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_mandra_attack_effect_source.py")
add_test(NAME make_party_effect_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_make_party_effect_source.py")
add_test(NAME party_effect_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_party_effect.py")
add_test(NAME cart_revolution_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_cart_revolution.py")
add_test(NAME pc_setstate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_pc_setstate_source.py")
add_test(NAME pc_processstate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_pc_processstate_source.py")
add_test(NAME player_setstate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_player_setstate_source.py")
add_test(NAME npc_setstate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_npc_setstate_source.py")
add_test(NAME npc_sendmsg_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_npc_sendmsg_source.py")
add_test(NAME setattrstate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_setattrstate_source.py")
add_test(NAME wba_acceptance_trace_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_wba_acceptance_trace.py")
add_test(NAME player_processstate_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_player_processstate_source.py")
add_test(NAME processsound_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_processsound_source.py")
add_test(NAME process_damage_situation_scope_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_process_damage_situation_scope.py")
add_test(NAME soundmaker_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_soundmaker_source.py")
add_test(NAME pc_onprocess_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_pc_onprocess_source.py")
add_test(NAME actor_tint_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_tint_source.py")
add_test(NAME grannypc_onprocess_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_grannypc_onprocess_source.py")
add_test(NAME player_onprocess_source_audit
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_player_onprocess_source.py")
add_test(NAME player_premove_route_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_player_premove_route.py")
add_test(NAME runtime_wave_pending_producers_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_runtime_wave_pending_producers.py")
add_test(NAME player_proceed_source_comparison
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_player_proceed_source.py")
add_test(NAME player_pt_skill_route_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_player_pt_skill_route.py")
add_test(NAME player_pt_ground_skill_route_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_player_pt_ground_skill_route.py")
add_test(NAME player_pt_touch_skill_route_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_player_pt_touch_skill_route.py")
add_test(NAME session_average_ping_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_session_average_ping_source.py")
add_test(NAME dummy_effect_launcher_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_dummy_effect_launcher.py")
add_test(NAME actor_sendmsg_effect_cases_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_effect_cases.py")
add_test(NAME actor_sendmsg_color_cases_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_color_cases.py")
add_test(NAME actor_sendmsg_vanish_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_vanish.py")
add_test(NAME actor_sendmsg_position_case_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_position_case.py")
add_test(NAME actor_sendmsg_target_case_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_target_case.py")
add_test(NAME actor_sendmsg_effect_detach_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_effect_detach_case.py")
add_test(NAME actor_sendmsg_inventory_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../audit_actor_sendmsg_inventory.py")
add_test(NAME actor_sendmsg_aura_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_aura_case.py")
add_test(NAME actor_sendmsg_move_path_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_move_path_case.py")
add_test(NAME actor_sendmsg_spawn_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_spawn_case.py")
add_test(NAME actor_sendmsg_identity_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_identity_case.py")
add_test(NAME actor_sendmsg_move_update_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_move_update_case.py")
add_test(NAME actor_sendmsg_move_subcell_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_move_subcell_case.py")
add_test(NAME actor_sendmsg_emotion_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_emotion_case.py")
add_test(NAME actor_sendmsg_pk_effect_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_pk_effect_case.py")
add_test(NAME actor_sendmsg_crusis_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_crusis_case.py")
add_test(NAME actor_sendmsg_status_effect_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_status_effect_case.py")
add_test(NAME actor_sendmsg_chat_prohibit_release_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_chat_prohibit_release_case.py")
add_test(NAME actor_sendmsg_cursor_ice_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_actor_sendmsg_cursor_ice_effect_cases.py")
add_test(NAME actor_am_action_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_am_action_source_case.py")
add_test(NAME actor_am_skill_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_am_skill_source_case.py")
add_test(NAME skill_action_info_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_action_info_source.py")
set(skill_action_info_oracle "${CMAKE_CURRENT_BINARY_DIR}/SkillActionInfoSourceOracle.cpp")
add_custom_command(
    OUTPUT "${skill_action_info_oracle}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_action_info_source.py"
            --emit-oracle "${skill_action_info_oracle}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_action_info_source.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/Session2.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../HighPriest.exe.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/session/SkillActionInfo.cpp"
    VERBATIM)
add_executable(skill_action_info_runtime_parity_checks
    SkillActionInfoParityTests.cpp
    "${skill_action_info_oracle}"
    ../../src/session/SkillActionInfo.cpp)
target_compile_features(skill_action_info_runtime_parity_checks PRIVATE cxx_std_17)
target_include_directories(skill_action_info_runtime_parity_checks PRIVATE ../../src)
add_test(NAME skill_action_info_runtime_parity COMMAND skill_action_info_runtime_parity_checks)
set(skill_action_info2_oracle "${CMAKE_CURRENT_BINARY_DIR}/SkillActionInfo2SourceOracle.cpp")
add_custom_command(
    OUTPUT "${skill_action_info2_oracle}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_action_info2_source.py"
            --emit-oracle "${skill_action_info2_oracle}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_action_info2_source.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/Session2.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../HighPriest.exe.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/session/SkillActionInfo2.cpp"
    VERBATIM)
add_executable(skill_action_info2_parity_checks
    SkillActionInfo2ParityTests.cpp
    "${skill_action_info2_oracle}"
    ../../src/session/SkillActionInfo2.cpp)
target_compile_features(skill_action_info2_parity_checks PRIVATE cxx_std_17)
target_include_directories(skill_action_info2_parity_checks PRIVATE ../../src)
add_test(NAME skill_action_info2_runtime_parity COMMAND skill_action_info2_parity_checks)
add_test(NAME skill_action_info2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_action_info2_source.py")
add_test(NAME clif_skill_nodamage_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_clif_skill_nodamage_source.py")
add_test(NAME clif_skill_damage_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_clif_skill_damage_source.py")
add_test(NAME clif_skill_poseffect_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_clif_skill_poseffect_source.py")
add_test(NAME clif_skill_setunit_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_clif_skill_setunit_source.py")
add_test(NAME clif_skill_disappear_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_clif_skill_disappear_source.py")
add_test(NAME clif_skill_update_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_clif_skill_update_source.py")
add_test(NAME skill_cast_ack_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_cast_ack_source.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../HighPriest.exe.c
    ../../Ref/GameModePacket.cpp
    ../../Ref/GameActor.cpp
    ../../Ref/GameMode.cpp
    ../../Ref/HP_Battle.cpp
    ../../Ref/Session2.cpp
    ../../src/gamemode/GameMode.cpp
    ../../src/gamemode/GameModePacket.cpp
    ../../src/world/GameActor.cpp
    ../../src/world/MsgEffect.cpp
    ../../Ref/eAthena_src_2011/map/clif.c
    ../../Ref/RunningServer/packet_db.txt
    ../../src/network/GronPacket.cpp
    ../../src/world/RagEffect.h
    ../../src/world/RagEffect.cpp
    ../../Ref/GameWorld.cpp
    ../../Ref/Skill.cpp
    ../../src/world/World.cpp
    check_clif_skill_disappear_source.py
    ../../src/world/SkillUnit.cpp
    check_clif_skill_update_source.py
    ../../src/gamemode/GameModePacket.cpp
    check_skill_cast_ack_source.py
    check_prim2dflash.py
    check_spirits_packet_source.py
    check_spirits2_packet_source.py
    check_devotion_packet_source.py)
add_executable(ui_spell_list_receiver_checks UISpellListWndReceiverTests.cpp)
target_compile_features(ui_spell_list_receiver_checks PRIVATE cxx_std_17)
add_test(NAME ui_spell_list_receiver_checks COMMAND ui_spell_list_receiver_checks)
add_test(NAME effect_coverage_audit
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/audit_effect_coverage.py")
add_test(NAME effect_dispatch_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect_dispatch_source.py")
add_test(NAME bash3d_callers_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_bash3d_callers.py")
add_test(NAME effect_sprite_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect_sprite.py")
add_test(NAME effect603_firehit_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect603_firehit.py")
add_test(NAME effects123_132_grim_tooth_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_grim_tooth123_132.py")
add_test(NAME effect142_heavens_drive_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect142_heavens_drive.py")
add_test(NAME effects20_125_enchant_poison_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect125_enchant_poison.py")
add_test(NAME effects109_110_str_init_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect109_110_str_init.py")
add_test(NAME effects143_144_146_152_str_init_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect143_144_146_152_str_init.py")
add_test(NAME effect128_overthrust_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect128_overthrust.py")
add_test(NAME hitdark_effects180_184_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_hitdark_caller.py")
add_test(NAME effect159_top_rank_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_top_rank.py")
add_test(NAME effect162_snow_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_snow_effect.py")
add_test(NAME effect163_sakura_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_sakura_effect.py")
add_test(NAME effects121_122_sonic_blow_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect121_122_sonic_blow.py")
add_test(NAME effects635_636_str_prefix_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect635_636_str_prefix.py")
add_test(NAME effects593_598_food_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect593_598_food_str.py")
add_test(NAME effects565_569_moonlight_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect565_569_moonlight_str.py")
add_test(NAME effects491_492_507_508_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect491_492_507_508_str.py")
add_test(NAME effect695_map_fireworks_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect695_map_fireworks.py")
add_test(NAME elemental_change_casting_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_elemental_change_casting.py")
add_test(NAME aspersio86_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_aspersio86.py")
add_test(NAME stormgust89_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_stormgust89.py")
add_test(NAME kyrie112_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_kyrie112.py")
add_test(NAME repairweapon101_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_repairweapon101.py")
add_test(NAME maxpower104_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_maxpower104.py")
add_test(NAME intimidate227_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_intimidate227.py")
add_test(NAME shockwave145_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_shockwave145.py")
add_test(NAME magicrod244_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_magicrod244.py")
add_test(NAME shieldcharge246_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_shieldcharge246.py")
add_test(NAME strip_equipment269_272_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_strip_equipment269_272.py")
add_test(NAME airtexture416_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_airtexture416.py")
add_test(NAME prim3dline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dline.py")
add_test(NAME prim3dsphere_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dsphere.py")
add_test(NAME prim3dtexturebillboard_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dtexturebillboard.py")
add_test(NAME hitline_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_hitline_render.py")
add_test(NAME hitline3_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_hitline3_render.py")
add_test(NAME airtexture_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_airtexture_render.py")
add_test(NAME electric_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_electric_render.py")
add_test(NAME electric2_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_electric2_render.py")
add_test(NAME sma2_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_sma2_render.py")
add_test(NAME sma_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_sma_render.py")
add_test(NAME stin_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_stin_render.py")
add_test(NAME stin5_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_stin5_render.py")
add_test(NAME wind_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_wind_render.py")
add_test(NAME colorpaper_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_colorpaper_render.py")
add_test(NAME magnum2_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_magnum2_render.py")
add_test(NAME effect339_magnum2_emitter_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect339_magnum2_emitter.py")
add_test(NAME aura_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_aura_render.py")
add_test(NAME aura2_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_aura2_render.py")
add_test(NAME cross_texture_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_cross_texture_render.py")
add_test(NAME 3dtexture_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_3dtexture_render.py")
add_test(NAME prim3dquadhorn_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dquadhorn.py")
add_test(NAME prim3dquadhorn2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dquadhorn2.py")
add_test(NAME prim3dline_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dline_initializer.py")
add_test(NAME prim3dsphere_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dsphere_initializer.py")
add_test(NAME prim3dcircle_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dcircle_initializer.py")
add_test(NAME prim3dtexture_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dtexture_initializer.py")
add_test(NAME prim2dflash_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim2dflash_initializer.py")
add_test(NAME prim3dflash_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dflash_initializer.py")
add_test(NAME prim3dcylinderhalf_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dcylinderhalf_initializer.py")
add_test(NAME prim3dcylinder_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dcylinder_initializer.py")
add_test(NAME prim2dcircle_initializer_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim2dcircle_initializer.py")
add_test(NAME prim3dparticle_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dparticle_render.py")
add_test(NAME prim3dsegment_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dsegment_render.py")
add_test(NAME prim3dcircle_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dcircle_render.py")
add_test(NAME prim3dcylinder_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dcylinder_render.py")
add_test(NAME prim2dcircle_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim2dcircle_render.py")
add_test(NAME prim2dtexture_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim2dtexture_render.py")
add_test(NAME prim2dquad_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim2dquad_render.py")
add_test(NAME hitline_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_hitline_caller.py")
add_test(NAME hitline4_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_hitline4_caller.py")
add_test(NAME hitline5_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_hitline5_caller.py")
add_test(NAME hitline3_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_hitline3_caller.py")
add_test(NAME teihit_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_teihit_callers.py")
add_test(NAME chimto_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_chimto_callers.py")
add_test(NAME stin_sma_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_stin_sma_callers.py")
add_test(NAME shield_boomerang_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_shield_boomerang_callers.py")
add_test(NAME rg_coin_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_rg_coin_callers.py")
add_test(NAME grandcross_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_grandcross_callers.py")
add_test(NAME linelink_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_linelink_callers.py")
add_test(NAME tripleattack_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_tripleattack_callers.py")
add_test(NAME soulparticle506_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_soulparticle506.py")
add_test(NAME effect502_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect502.py")
add_test(NAME effect505_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect505.py")
add_test(NAME sma2_552_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_sma2_552.py")
add_test(NAME particlepath556_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_particlepath556.py")
add_test(NAME effect537_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect537.py")
add_test(NAME effect599_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect599.py")
add_test(NAME effect602_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect602.py")
add_test(NAME effect273_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect273.py")
add_test(NAME effect440_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect440.py")
add_test(NAME effect365_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect365.py")
add_test(NAME effect302_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect302.py")
add_test(NAME effect300_chemical_protection_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect300_chemical_protection.py")
add_test(NAME effect303_chemical2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect303_chemical2.py")
add_test(NAME effect301_pokjuk_sound_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_pokjuk_sound301.py")
add_test(NAME effect333_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect333.py")
add_test(NAME effect6_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect6.py")
add_test(NAME effect8_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect8.py")
add_test(NAME effect9_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect9.py")
add_test(NAME effect95_quagmire_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect95_quagmire.py")
add_test(NAME effect96_fire_pillar_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect96_fire_pillar.py")
add_test(NAME effect97_fire_pillar_bomb_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect97_fire_pillar_bomb.py")
add_test(NAME effect108_freezing_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect108_freezing.py")
add_test(NAME effect111_spring_trap_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect111_spring_trap.py")
add_test(NAME effect113_magnus_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect113_magnus.py")
add_test(NAME effects124_127_poison_react_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect124_127_poison_react.py")
add_test(NAME effect129_venom_splasher_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect129_venom_splasher.py")
add_test(NAME effects130_250_twohand_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect130_250_twohand.py")
add_test(NAME effect131_autocounter_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect131_autocounter.py")
add_test(NAME effects133_134_terminal_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect133_134_terminal_str.py")
add_test(NAME effect135_ice_crash_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect135_ice_crash.py")
add_test(NAME effect136_slowp_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect136_slowp.py")
add_test(NAME effect139_sandman_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect139_sandman.py")
add_test(NAME effect141_pneuma_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect141_pneuma.py")
add_test(NAME effect153_concentration_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect153_concentration.py")
add_test(NAME effect154_refine_success_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect154_refine_success.py")
add_test(NAME effect155_refine_failed_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect155_refine_failed.py")
add_test(NAME effect158_job_level_up_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect158_job_level_up.py")
add_test(NAME effect156_job_change_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect156_job_change.py")
add_test(NAME effect157_level_up_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect157_level_up.py")
add_test(NAME effect197_sleep_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect197_sleep.py")
add_test(NAME effect235_cp949_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect235_cp949_str.py")
add_test(NAME effect608_cook_success_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect608_cook_success.py")
add_test(NAME effect609_cook_fail_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect609_cook_fail.py")
add_test(NAME effect646_str_ignored_result_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect646_str_ignored_result.py")
add_test(NAME effect649_str_ignored_result_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect649_str_ignored_result.py")
add_test(NAME effect670_str_ignored_result_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect670_str_ignored_result.py")
add_test(NAME effect699_str_ignored_result_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect699_str_ignored_result.py")
add_test(NAME effect668_dragonfear_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect668_dragonfear.py")
add_test(NAME effect669_wideb_poison_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect669_wideb_poison.py")
add_test(NAME effect677_cwound_wideb_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect677_cwound_wideb.py")
add_test(NAME effect102_crash_earth_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect102_crash_earth.py")
add_test(NAME effect103_weapon_perfection_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect103_weapon_perfection.py")
add_test(NAME effect106_blastmine_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect106_blastmine.py")
add_test(NAME effect107_claymore_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect107_claymore.py")
add_test(NAME effect90_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect90.py")
add_test(NAME effect74_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect74.py")
add_test(NAME effect13_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect13.py")
add_test(NAME effect92_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect92.py")
add_test(NAME soulbreaker361_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_soulbreaker361.py")
add_test(NAME npcstop_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_npcstop_callers.py")
add_test(NAME guard_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_guard_callers.py")
add_test(NAME twilight_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_twilight_callers.py")
add_test(NAME particle_time_spread_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_particle_time_spread_callers.py")
add_test(NAME spherewind2_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_spherewind2_caller.py")
add_test(NAME soulbreaker2_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_soulbreaker2_caller.py")
add_test(NAME peong_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_peong_callers.py")
add_test(NAME begin_asura_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_begin_asura_callers.py")
add_test(NAME energy_drain_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_energy_drain_callers.py")
add_test(NAME portal_wind_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_portal_wind_caller.py")
add_test(NAME blur_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_blur_callers.py")
add_test(NAME runstop_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_runstop_caller.py")
add_test(NAME cartboost_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_cartboost_caller.py")
add_test(NAME exit_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_exit_caller.py")
add_test(NAME endure_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_endure_caller.py")
add_test(NAME soulstrike_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_soulstrike_callers.py")
add_test(NAME detoxication_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_detoxication_caller.py")
add_test(NAME yufitel_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_yufitel_callers.py")
add_test(NAME flasher_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_flasher_caller.py")
add_test(NAME removetrap_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_removetrap_caller.py")
add_test(NAME blitzbeat_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_blitzbeat_caller.py")
add_test(NAME blood_drain_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_blood_drain.py")
add_test(NAME bottom_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_bottom_caller.py")
add_test(NAME bottom2_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_bottom2_caller.py")
add_test(NAME bowlingbash_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_bowlingbash_caller.py")
add_test(NAME cartrevolution_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_cartrevolution_caller.py")
add_test(NAME cloaking_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_cloaking_inline.py")
add_test(NAME coin_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_coin_caller.py")
add_test(NAME coldhit_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_coldhit_caller.py")
add_test(NAME concentration_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_concentration_caller.py")
add_test(NAME cone_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_cone_caller.py")
add_test(NAME crashearth_orphan_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_crashearth_orphan.py")
add_test(NAME cure_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_cure_inline.py")
add_test(NAME curseattack_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_curseattack_inline.py")
add_test(NAME darkbreath_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_darkbreath_inline.py")
add_test(NAME decagility_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_decagility_caller.py")
add_test(NAME detecting_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_detecting_caller.py")
add_test(NAME dragon_smoke_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_dragon_smoke.py")
add_test(NAME lockon_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_lockon_caller.py")
add_test(NAME spearbmr_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_spearbmr_caller.py")
add_test(NAME turnundead_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_turnundead_caller.py")
add_test(NAME waterball2_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_waterball2_caller.py")
add_test(NAME revive_caller_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_revive_caller.py")
add_test(NAME effect198_noop_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect198_noop.py")
add_test(NAME effect380_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect380_inline.py")
add_test(NAME effect403_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect403_inline.py")
add_test(NAME effect413_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect413_inline.py")
add_test(NAME effect445_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect445_inline.py")
add_test(NAME effect457_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect457_inline.py")
add_test(NAME effect466_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect466_inline.py")
add_test(NAME effect501_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect501_inline.py")
add_test(NAME effect704_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect704_inline.py")
add_test(NAME effect105_inline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect105_inline.py")
add_test(NAME effect116_waterball_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect116_waterball.py")
add_test(NAME effect118_fireivy_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect118_fireivy.py")
add_test(NAME effect138_firepillar_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect138_firepillar.py")
add_test(NAME effect165_banjjakii_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect165_banjjakii.py")
add_test(NAME effect171_venomdust2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect171_venomdust2.py")
add_test(NAME effect167_169_str_mapping_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect167_169_str_mapping.py")
add_test(NAME effect183_hit2blue_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect183_hit2blue.py")
add_test(NAME effect185_suicide_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect185_suicide_str.py")
add_test(NAME effect181_mentalbreak_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect181_mentalbreak_str.py")
add_test(NAME effect182_186_str_mapping_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect182_186_str_mapping.py")
add_test(NAME effect187_yunta2_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect187_yunta2_str.py")
add_test(NAME effect188_yunta3_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect188_yunta3_str.py")
add_test(NAME effect189_195_str_mappings_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect189_195_str_mappings.py")
add_test(NAME effect213_214_str_mappings_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect213_214_str_mappings.py")
add_test(NAME effect199_pong_str_variants_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect199_pong_str_variants.py")
add_test(NAME effect204_210_clipwave_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect204_210_clipwave_str.py")
add_test(NAME effect208_211_potion_str_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect208_211_potion_str.py")
add_test(NAME effect242_404_land_protector_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect242_404_land_protector.py")
add_test(NAME effect265_412_tanji_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect265_412_tanji.py")
add_test(NAME effect374_basilica_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect374_basilica.py")
add_test(NAME effect375_actor_light_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect375_actor_light.py")
add_test(NAME effect442_body_light_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect442_body_light.py")
add_test(NAME effects446_449_body_light_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect446_449_body_light.py")
add_test(NAME effect454_dark_casting_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect454_dark_casting.py")
add_test(NAME effect487_black_devil_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect487_black_devil.py")
add_test(NAME effect488_flower_cast_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect488_flower_cast.py")
add_test(NAME effect392_kyrie_guard_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect392_kyrie_guard.py")
add_test(NAME effects390_391_clipwave_callers_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect390_391_clipwave_callers.py")
add_test(NAME effect394_sphere_wind_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect394_sphere_wind.py")
add_test(NAME effects397_398_level99_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect397_398_level99.py")
add_test(NAME effect401_napalm_hit_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect401_napalm_hit.py")
add_test(NAME effect402_portal_wind_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect402_portal_wind.py")
add_test(NAME effect514_gi_explosion_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect514_gi_explosion.py")
add_test(NAME effects523_536_tarot_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects523_536_tarot.py")
add_test(NAME effect493_enchant_poison2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect493_enchant_poison2.py")
add_test(NAME effect512_chemical3_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect512_chemical3.py")
add_test(NAME effects539_541_throw_items_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect539_throw_items.py")
add_test(NAME effect581_forest_light_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect581_forest_light.py")
add_test(NAME effect582_super_angel_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect582_super_angel.py")
add_test(NAME effect673_poison_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect673_poison.py")
add_test(NAME effect590_job_lv_up2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect590_job_lv_up2.py")
add_test(NAME effect337_job_lv_up_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect337_job_lv_up.py")
add_test(NAME effect410_rainbow_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect410_rainbow.py")
add_test(NAME effect428_hitline3zero_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect428_hitline3zero.py")
add_test(NAME effect434_hitline3basic_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect434_hitline3basic.py")
add_test(NAME effect297_pokjuk_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect297_pokjuk.py")
add_test(NAME effects298_299_throw_item_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_throw_item298_299.py")
add_test(NAME gumgang_emitter_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_gumgang_emitter.py")
add_test(NAME effect447_foot_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect447_foot.py")
add_test(NAME effect448_foot_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect448_foot.py")
add_test(NAME effect326_327_foot_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect326_327_foot.py")
add_test(NAME effect340_callzone_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect340_callzone.py")
add_test(NAME effects557_560_bluefall_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects557_560_bluefall.py")
add_test(NAME effects497_499_slim_potion_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects497_499_slim_potion.py")
add_test(NAME effect513_ground_sample_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect513_ground_sample.py")
add_test(NAME effect517_bottom_hermode_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect517_bottom_hermode.py")
add_test(NAME effect551_sma_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect551_sma.py")
add_test(NAME effects671_672_land_protector_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects671_672_land_protector.py")
add_test(NAME effect348_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect348.py")
add_test(NAME effect405_bottom_magnus_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect405_bottom_magnus.py")
add_test(NAME effect620_bottom_volcano_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect620_bottom_volcano.py")
add_test(NAME effect310_particle_up_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect310_particle_up.py")
add_test(NAME effects515_516_592_weather_cloud_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects515_516_592_weather_cloud.py")
add_test(NAME effect622_thunder_storm2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_thunder_storm2_caller.py")
add_test(NAME effect624_stin5_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_stin5_caller.py")
add_test(NAME effect554_texture_set_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect554_texture_set.py")
add_test(NAME effect601_sight2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect601_sight2.py")
add_test(NAME effect24_fireball_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect24_fireball.py")
add_test(NAME effect50_firesplashhit_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect50_firesplashhit.py")
add_test(NAME effect48_spray_pond_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect48_spray_pond.py")
add_test(NAME effect47_torch_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect47_torch.py")
add_test(NAME effect46_sandwind_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect46_sandwind.py")
add_test(NAME effect45_firefly_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect45_firefly.py")
add_test(NAME effect44_smoke_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect44_smoke.py")
add_test(NAME effect42_blessing_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect42_blessing.py")
add_test(NAME effect41_angelus_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect41_angelus.py")
add_test(NAME effect40_signum_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect40_signum.py")
add_test(NAME effect39_aqua_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect39_aqua.py")
add_test(NAME effect34_teleportation_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect34_teleportation.py")
add_test(NAME effect33_ruwach_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect33_ruwach.py")
add_test(NAME effect17_magnum_break_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect17_magnum_break.py")
add_test(NAME effect12_saint_casting_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect12_saint_casting.py")
add_test(NAME effect0_hit1_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect0_hit1.py")
add_test(NAME effect1_hit2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect1_hit2.py")
add_test(NAME effect2_hit3_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect2_hit3.py")
add_test(NAME effect3_hit4_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect3_hit4.py")
add_test(NAME effects4_5_hit5_hit6_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects4_5_hit5_hit6.py")
add_test(NAME effect7_exit_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect7_exit.py")
add_test(NAME effects23_25_str_only_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects23_25_str_only.py")
add_test(NAME effect29_str_only_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect29_str_only.py")
add_test(NAME effect49_fire_hit_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect49_fire_hit.py")
add_test(NAME effects52_64_70_str_only_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects52_64_70_str_only.py")
add_test(NAME effect61_warp_zone_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect61_warp_zone.py")
add_test(NAME effects223_225_236_238_261_263_volcano_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects223_225_236_238_261_263_volcano.py")
add_test(NAME effects231_247_259_260_mappillar_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects231_247_259_260_mappillar.py")
add_test(NAME effect224_wind_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect224_wind.py")
add_test(NAME effect248_providence_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect248_providence.py")
add_test(NAME effect253_heal_blue_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect253_heal_blue.py")
add_test(NAME effect10_coin_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect10_coin.py")
add_test(NAME effect18_steal_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect18_steal.py")
add_test(NAME effect16_bash_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect16_bash.py")
add_test(NAME effect14_heal_sp_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect14_heal_sp.py")
add_test(NAME effect21_detoxication_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect21_detoxication.py")
add_test(NAME effect26_ice_arrow_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect26_ice_arrow.py")
add_test(NAME effect27_frost_diver_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect27_frost_diver.py")
add_test(NAME effect28_frost_diver2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect28_frost_diver2.py")
add_test(NAME effect31_fire_arrow_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect31_fire_arrow.py")
add_test(NAME effect32_napalm_beat_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect32_napalm_beat.py")
add_test(NAME effects35_36_portal_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects35_36_portal.py")
add_test(NAME effects37_38_43_agility_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects37_38_43_agility.py")
add_test(NAME effects436_437_sphere_energy_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect436_437_sphere_energy.py")
add_test(NAME effect119_detecting_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect119_detecting.py")
add_test(NAME effects222_252_defender_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect222_252_defender.py")
add_test(NAME effects312_313_320_325_heal_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects312_313_320_325_heal.py")
add_test(NAME effect314_exit2_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect314_exit2.py")
add_test(NAME effects689_691_696_texture_animation_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects689_691_696_texture_animation.py")
add_test(NAME effects610_611_texture_cues_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects610_611_texture_cues.py")
add_test(NAME effects358_360_ghost_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects358_360_ghost.py")
add_test(NAME effects349_356_waterfall_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects349_356_waterfall.py")
add_test(NAME effects363_364_509_519_545_vallentine_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects363_364_509_519_545_vallentine.py")
add_test(NAME effects561_562_portal3_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects561_562_portal3.py")
add_test(NAME effect522_gravitation_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect522_gravitation.py")
add_test(NAME effect650_map_magic_zone_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect650_map_magic_zone.py")
add_test(NAME effect651_map_particle_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect651_map_particle.py")
add_test(NAME effects342_564_couple_casting_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects342_564_couple_casting.py")
add_test(NAME effect605_stop_message_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect605_stop_message.py")
add_test(NAME effect563_mode_message_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect563_mode_message.py")
add_test(NAME effects652_654_damage_number_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effects653_654_damage_number.py")
add_test(NAME effect467_beginasura1_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect467_beginasura1.py")
add_test(NAME effect468_473_beginasura1_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect468_473_beginasura1.py")
add_test(NAME effect277_294_bottom_vertical_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect277_294_bottom_vertical.py")
add_test(NAME effect281_288_bottom_music_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect281_288_bottom_music.py")
add_test(NAME skill_cast_cancel_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_cast_cancel_source.py")
add_test(NAME status_state_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_status_state.py")
add_test(NAME skill_packet_actor_lookup_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_packet_actor_lookup.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/gamemode/GameModePacket.cpp
    ../../HighPriest.exe
    ../../HighPriest.exe.c
    ../../Ref/GameModePacket.cpp
    ../../Ref/RunningServer/packet_db.txt
    ../../src/ui/UISpellListWnd.cpp
    ../../src/ui/UISpellListWnd.h
    UISpellListWndReceiverTests.cpp)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../HighPriest.exe
    ../../HighPriest.exe.c
    ../../Ref/RagEffect.cpp
    ../../Ref/RagEffect2.cpp
    ../../src/world/RagEffect.cpp
    ../../src/world/RagEffectOnProcess2Cases.generated.inc
    ../../src/world/RagEffectOnProcess3Cases.generated.inc
    check_hitline_render.py
    check_sma2_render.py
    check_sma_render.py
    check_stin_render.py
    check_stin5_render.py
    check_wind_render.py
    check_colorpaper_render.py
    check_magnum2_render.py
    check_aura_render.py
    check_aura2_render.py
    check_cross_texture_render.py
    check_3dtexture_render.py
    check_prim3dquadhorn.py
    check_prim3dquadhorn2.py
    check_prim3dline_initializer.py
    check_prim3dsphere_initializer.py
    check_prim3dcircle_initializer.py
    check_prim3dtexture_initializer.py
    check_prim2dflash_initializer.py
    check_prim3dflash_initializer.py
    check_prim3dcylinderhalf_initializer.py
    check_prim3dcylinder_initializer.py
    check_prim2dcircle_initializer.py
    check_hitline_caller.py
    check_hitline4_caller.py
    check_hitline5_caller.py
    check_hitline3_caller.py
    check_teihit_callers.py
    check_chimto_callers.py
    check_stin_sma_callers.py
    check_effect551_sma.py
    check_effects671_672_land_protector.py
    check_effect348.py
    check_effect405_bottom_magnus.py
    check_effect620_bottom_volcano.py
    check_effect310_particle_up.py
    check_effects515_516_592_weather_cloud.py
    check_thunder_storm2_caller.py
    check_stin5_caller.py
    check_effect554_texture_set.py
    check_effect601_sight2.py
    check_effect24_fireball.py
    check_effect50_firesplashhit.py
    check_effect48_spray_pond.py
    check_effect47_torch.py
    check_effect46_sandwind.py
    check_effect45_firefly.py
    check_effect44_smoke.py
    check_effect42_blessing.py
    check_effect41_angelus.py
    check_effect40_signum.py
    check_effect39_aqua.py
    check_effect34_teleportation.py
    check_effect33_ruwach.py
    check_effect32_napalm_beat.py
    check_effect16_bash.py
    check_effect17_magnum_break.py
    check_effect12_saint_casting.py
    check_effect0_hit1.py
    check_effect1_hit2.py
    check_effect2_hit3.py
    check_effect3_hit4.py
    check_effects4_5_hit5_hit6.py
    check_effect7_exit.py
    check_effects23_25_str_only.py
    check_effect29_str_only.py
    check_effect49_fire_hit.py
    check_effects52_64_70_str_only.py
    check_effect61_warp_zone.py
    check_effects223_225_236_238_261_263_volcano.py
    check_effects231_247_259_260_mappillar.py
    check_effect224_wind.py
    check_effect248_providence.py
    check_effect253_heal_blue.py
    check_effect10_coin.py
    check_effect18_steal.py
    check_effect14_heal_sp.py
    check_effect21_detoxication.py
    check_effect26_ice_arrow.py
    check_effect27_frost_diver.py
    check_effect28_frost_diver2.py
    check_effect31_fire_arrow.py
    check_effects35_36_portal.py
    check_effects37_38_43_agility.py
    check_effect436_437_sphere_energy.py
    check_effect119_detecting.py
    check_effect222_252_defender.py
    check_effects312_313_320_325_heal.py
    check_effect314_exit2.py
    check_effects689_691_696_texture_animation.py
    check_effects610_611_texture_cues.py
    check_effects358_360_ghost.py
    check_effects349_356_waterfall.py
    check_effects363_364_509_519_545_vallentine.py
    check_effects561_562_portal3.py
    check_effects342_564_couple_casting.py
    check_effect605_stop_message.py
    check_effect563_mode_message.py
    check_effects653_654_damage_number.py
    check_shield_boomerang_callers.py
    check_rg_coin_callers.py
    check_grandcross_callers.py
    check_linelink_callers.py
    check_tripleattack_callers.py
    check_soulparticle506.py
    check_effect502.py
    check_effect505.py
    check_sma2_552.py
    check_particlepath556.py
    check_effect537.py
    check_effect599.py
    check_effect602.py
    check_effect273.py
    check_effect440.py
    check_effect365.py
    check_effect302.py
    check_effect333.py
    check_effect6.py
    check_effect8.py
    check_effect9.py
    check_effect95_quagmire.py
    check_effect96_fire_pillar.py
    check_effect97_fire_pillar_bomb.py
    check_effect108_freezing.py
    check_effect111_spring_trap.py
    check_effect113_magnus.py
    check_effect124_127_poison_react.py
    check_effect129_venom_splasher.py
    check_effect130_250_twohand.py
    check_effect131_autocounter.py
    check_effect133_134_terminal_str.py
    check_effect135_ice_crash.py
    check_effect136_slowp.py
    check_effect139_sandman.py
    check_effect141_pneuma.py
    check_effect153_concentration.py
    check_effect154_refine_success.py
    check_effect155_refine_failed.py
    check_effect158_job_level_up.py
    check_effect156_job_change.py
    check_effect157_level_up.py
    check_effect197_sleep.py
    check_effect235_cp949_str.py
    check_effect102_crash_earth.py
    check_effect103_weapon_perfection.py
    check_effect106_blastmine.py
    check_effect107_claymore.py
    check_effect90.py
    check_effect74.py
    check_effect13.py
    check_effect92.py
    check_soulbreaker361.py
    check_npcstop_callers.py
    check_guard_callers.py
    check_twilight_callers.py
    check_particle_time_spread_callers.py
    check_spherewind2_caller.py
    check_soulbreaker2_caller.py
    check_peong_callers.py
    check_begin_asura_callers.py
    check_skill_packet_actor_lookup.py
    replay/SkillPacketReplayTests.cpp
    replay/SkillPacketRuntimeReplay.cpp
    ../../src/gamemode/GameModePacket.cpp
    check_energy_drain_callers.py
    check_portal_wind_caller.py
    check_effect402_portal_wind.py
    check_effects523_536_tarot.py
    check_effect493_enchant_poison2.py
    check_effect512_chemical3.py
    check_effect539_throw_items.py
    check_effect581_forest_light.py
    check_effect582_super_angel.py
    check_effect673_poison.py
    check_effect590_job_lv_up2.py
    check_blur_callers.py
    check_runstop_caller.py
    check_cartboost_caller.py
    check_exit_caller.py
    check_endure_caller.py
    check_soulstrike_callers.py
    check_detoxication_caller.py
    check_yufitel_callers.py
    check_flasher_caller.py
    check_removetrap_caller.py
    check_blitzbeat_caller.py
    check_blood_drain.py
    check_bottom_caller.py
    check_bottom2_caller.py
    check_bowlingbash_caller.py
    check_cartrevolution_caller.py
    check_cloaking_inline.py
    check_coin_caller.py
    check_coldhit_caller.py
    check_concentration_caller.py
    check_cone_caller.py
    check_crashearth_orphan.py
    check_cure_inline.py
    check_curseattack_inline.py
    check_darkbreath_inline.py
    check_decagility_caller.py
    check_detecting_caller.py
    check_effect_sprite.py
    check_effect138_firepillar.py
    check_effect165_banjjakii.py
    check_effect171_venomdust2.py
    check_effect167_169_str_mapping.py
    check_effect183_hit2blue.py
    check_effect185_suicide_str.py
    check_effect181_mentalbreak_str.py
    check_effect182_186_str_mapping.py
    check_effect187_yunta2_str.py
    check_effect188_yunta3_str.py
    check_effect189_195_str_mappings.py
    check_effect213_214_str_mappings.py
    check_effect199_pong_str_variants.py
    check_effect204_210_clipwave_str.py
    check_effect208_211_potion_str.py
    check_effect695_map_fireworks.py
    check_grim_tooth123_132.py
    check_effect142_heavens_drive.py
    check_effect125_enchant_poison.py
    check_effect128_overthrust.py
    check_hitdark_caller.py
    check_top_rank.py
    check_effect121_122_sonic_blow.py
    check_elemental_change_casting.py
    check_party_effect.py
    check_cart_revolution.py)

# Compile the exact actor status dispatch, sprite creation/release, and owner
# message code against a silent in-memory world/actor fixture.
file(READ "../../src/world/GameActor.cpp" msg_effect_actor_source)
file(READ "../../src/world/MsgEffect.cpp" msg_effect_source)
extract_body(msg_status_spawn "${msg_effect_actor_source}"
    "CMsgEffect* CGameActor::SpawnMsgSprite(" "void ReleaseMsgSprite(")
extract_body(msg_status_release "${msg_effect_actor_source}"
    "void ReleaseMsgSprite(" "bool CGameActor::ProcessStatusMsgEffectMessage(")
extract_body(msg_status_dispatch "${msg_effect_actor_source}"
    "bool CGameActor::ProcessStatusMsgEffectMessage(" "void CGameActor::SendMsg(")
extract_body(msg_owner_dispatch "${msg_effect_source}"
    "bool CMsgEffect::ProcessOwnerMessage(" "void CMsgEffect::SendMsg(")
extract_body(msg_effect_send "${msg_effect_source}"
    "void CMsgEffect::SendMsg(" "void CMsgEffect::Render(")
extract_body(damage_number_clamp "${msg_effect_actor_source}"
    "int ClampDamageNumber(int value)" "int CountDamageDigits(")
extract_body(make_damage_number "${msg_effect_actor_source}"
    "void MakeDamageNumber(CGameActor& actor" "bool FindActivePathSegment(")
string(FIND "${msg_effect_actor_source}" "void CGameActor::SendMsg(" resurrection_sendmsg_start)
string(LENGTH "${msg_effect_actor_source}" msg_effect_actor_source_length)
math(EXPR resurrection_sendmsg_length "${msg_effect_actor_source_length} - ${resurrection_sendmsg_start}")
string(SUBSTRING "${msg_effect_actor_source}" ${resurrection_sendmsg_start} ${resurrection_sendmsg_length} resurrection_sendmsg_source)
string(FIND "${resurrection_sendmsg_source}" "    case 98:" resurrection_receiver_start)
string(FIND "${resurrection_sendmsg_source}" "    case 88: {" resurrection_receiver_end)
if(resurrection_receiver_start LESS 0 OR resurrection_receiver_end LESS resurrection_receiver_start)
    message(FATAL_ERROR "Could not isolate CGameActor::SendMsg resurrection case 98")
endif()
math(EXPR resurrection_receiver_length "${resurrection_receiver_end} - ${resurrection_receiver_start}")
string(SUBSTRING "${resurrection_sendmsg_source}" ${resurrection_receiver_start} ${resurrection_receiver_length} resurrection_receiver_case)
string(FIND "${resurrection_sendmsg_source}" "    case 88: {" damage_number_receiver_start)
string(FIND "${resurrection_sendmsg_source}" "    case 89:" damage_number_receiver_end)
if(damage_number_receiver_start LESS 0 OR damage_number_receiver_end LESS damage_number_receiver_start)
    message(FATAL_ERROR "Could not isolate CGameActor::SendMsg damage-number case 88")
endif()
math(EXPR damage_number_receiver_length "${damage_number_receiver_end} - ${damage_number_receiver_start}")
string(SUBSTRING "${resurrection_sendmsg_source}" ${damage_number_receiver_start} ${damage_number_receiver_length} damage_number_receiver_case)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/MsgEffectStateLifecycleBodies.inc"
    "${msg_status_spawn}\n${msg_status_release}\n${msg_status_dispatch}\n${msg_owner_dispatch}\n${msg_effect_send}\n"
    "${damage_number_clamp}\n${make_damage_number}\n"
    "void CGameActor::ApplyResurrectionReceiverCase() { switch (98) {\n${resurrection_receiver_case}\ndefault: break; } }\n"
    "void CGameActor::ApplyDamageNumberReceiverCase(CGameObject* src, msgparam_t par1, msgparam_t par2, msgparam_t par3) { switch (88) {\n${damage_number_receiver_case}\ndefault: break; } }\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/world/GameActor.cpp ../../src/world/MsgEffect.cpp)
add_executable(msg_effect_state_lifecycle_checks MsgEffectStateLifecycleTests.cpp)
target_compile_features(msg_effect_state_lifecycle_checks PRIVATE cxx_std_17)
target_include_directories(msg_effect_state_lifecycle_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME msg_effect_state_lifecycle_checks COMMAND msg_effect_state_lifecycle_checks)
add_test(NAME msg_effect_state_lifecycle_source_check
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_msg_effect_state_lifecycle.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/world/GameActor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/GameActor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/Npc.cpp")
add_test(NAME recovery_number_receiver_source_check
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_recovery_number_receiver.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/world/GameActor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/GameActor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Ref/GameActorMsgHandler.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../HighPriest.exe.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/RecoveryResurrectionPacketTests.cpp")
