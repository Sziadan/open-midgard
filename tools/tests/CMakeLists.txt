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
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SoundBodies.inc" "${sound_body}\n${update_body}")
file(READ "../../src/session/Session.cpp" session_source)
extract_body(combo_body "${session_source}" "u32 CSession::GetForceAnimEndTick(" "// Ref CSession::GetWeaponType")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ComboBodies.inc" "${combo_body}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/gamemode/GameModePacket.cpp ../../src/gamemode/GameMode.cpp ../../src/session/Session.cpp)
add_executable(server_sound_checks ServerSoundTests.cpp)
target_compile_features(server_sound_checks PRIVATE cxx_std_17)
target_include_directories(server_sound_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME server_sound_checks COMMAND server_sound_checks)
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
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/NotifyEffect3Body.inc" "${effect_status_rng}\n${notify_effect3_handler}")
add_executable(notify_effect3_checks NotifyEffect3Tests.cpp)
target_compile_features(notify_effect3_checks PRIVATE cxx_std_17)
target_include_directories(notify_effect3_checks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME notify_effect3_checks COMMAND notify_effect3_checks)
file(READ "../../src/gamemode/GameModePacket.cpp" talkbox_source)
extract_body(talkbox_chat_contents "${talkbox_source}" "void HandleTalkboxChatContents(CGameMode& mode, const PacketView& packet)" "void HandleChangeGuild(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/TalkboxChatContentsBody.inc" "${talkbox_chat_contents}")
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
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ChangeGuildPacketBody.inc" "${change_guild_packet}")
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
extract_body(revive140_handler "${effect_source}" "bool CRagEffect::ProcessRevive140()" "// TEST_BODY_END revive140_handler")
extract_body(effect380_handler "${effect_source}" "bool CRagEffect::ProcessEffect380()" "// TEST_BODY_END effect380_handler")
extract_body(effect403_handler "${effect_source}" "bool CRagEffect::ProcessEffect403()" "// TEST_BODY_END effect403_handler")
extract_body(effect413_handler "${effect_source}" "bool CRagEffect::ProcessEffect413()" "// TEST_BODY_END effect413_handler")
extract_body(effect445_handler "${effect_source}" "bool CRagEffect::ProcessEffect445()" "// TEST_BODY_END effect445_handler")
extract_body(effect457_handler "${effect_source}" "bool CRagEffect::ProcessEffect457()" "// TEST_BODY_END effect457_handler")
extract_body(effect466_handler "${effect_source}" "bool CRagEffect::ProcessEffect466()" "// TEST_BODY_END effect466_handler")
extract_body(effect501_handler "${effect_source}" "bool CRagEffect::ProcessEffect501()" "// TEST_BODY_END effect501_handler")
extract_body(effect704_handler "${effect_source}" "bool CRagEffect::ProcessEffect704()" "// TEST_BODY_END effect704_handler")
extract_test_region(warp_emitter "${effect_source}" warp_emitter_family)
extract_test_region(agility_emitters "${effect_source}" agility_emitter_family)
extract_body(blessing_emitter "${effect_source}" "void CRagEffect::SpawnBlessing()" "// Ref/RagEffect.cpp:7721-7761")
extract_test_region(smoke_torch_emitters "${effect_source}" smoke_torch_emitter_family)
string(REPLACE "rand()" "SilentRand()" smoke_torch_emitters "${smoke_torch_emitters}")
extract_test_region(poison_emitters "${effect_source}" poison_emitter_family)
extract_test_region(sight_emitters "${effect_source}" sight_emitter_family)
string(REPLACE "rand()" "SilentRand()" sight_emitters "${sight_emitters}")
extract_test_region(fireball_emitter "${effect_source}" fireball_emitter_family)
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
string(REPLACE "rand()" "SilentRand()" chookgi_emitter "${chookgi_emitter}")
extract_body(ground_sample_emitter "${effect_source}" "void CRagEffect::ProcessGroundSample()" "void CRagEffect::SpawnBlueFall()")
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
extract_test_region(str_process "${effect_source}" str_process_family)
file(READ "../../src/world/World.cpp" world_source)
extract_body(primitive_init "${effect_source}" "void CEffectPrim::Init(" "static_assert(offsetof(EffectGi, height)")
extract_test_region(primitive_update_family "${effect_source}" primitive_update_family)
extract_test_region(primitive_lifetime "${effect_source}" primitive_lifecycle_family)
extract_body(effect_gate "${effect_source}" "bool EffectTicksWhenNoFrameTime(" "void ConfigureHealBand(")
extract_test_region(str_termination "${effect_source}" str_termination_family)
extract_test_region(casting_update "${effect_source}" casting_update_family)
extract_body(table_angle "${effect_source}" "int TableAngle(int angle)\n{" "void RotateCornerTowardScreen(")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CastingUpdateBody.inc" "int TableAngle(int angle);\n${casting_update}\n${table_angle}")
extract_body(saint_emitter "${effect_source}" "void CRagEffect::ApplySaintCasting(" "void CRagEffect::ApplyBottomOut(")
extract_test_region(begin_emitter "${effect_source}" begin_spell_family)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CastingEmitterBodies.inc" "${saint_emitter}\n${begin_emitter}")
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
    "${resource_lock}\n${resource_unlock}\n${effect_gate}\n${primitive_lifetime}\n${primitive_init}\n${segment_shift}\nint TableAngle(int angle);\n${primitive_update_family}\n${effect_tick}\n${effect_dtor}\n${effect_launch}\n${effect_messages}\n${actor_detach}\n${world_update}\n${grim_tooth}\n${str_delta}\n${str_process}\n${str_termination}\n${entry2}\n${str_render}\n${chookgi_renderer}\n${effect_render}\n${spline_primitive}\n${slash_primitive}\n${angel_render}\n${job_emitters}\n${angel_emitters}\n${ruwach_emitter}\n${smoke_torch_emitters}\n${poison_emitters}\n${blessing_emitter}\n${sight_projection}\n${agility_emitters}\n${sight_emitters}\n${warp_band}\n${warp_helper}\n${warp_emitter}\n${ready_portal_emitter}\n${potion_emitters}\n${detoxication_handler}\n${ice_arrow_emitter}\n${fireball_emitter}\n${bash3d_emitter}\n${kouenka_emitter}\n${earth_spike_emitter}\n${chemical_rise_angle}\n${chemical3_emitter}\n${stin4_caller}\n${stin4_emitter}\n${stin5_emitter}\n${rg_coin3_emitter}\n${bash3d5_caller}\n${chookgi_emitter}\n${ground_sample_emitter}\n${prim_cloud_update}\n${cloud_texture}\n${weather_cloud_emitter}\n${spearbmr_handler}\n${turnundead_handler}\n${waterball2_handler}\n${revive140_handler}\n${effect380_handler}\n${effect403_handler}\n${effect413_handler}\n${effect445_handler}\n${effect457_handler}\n${effect466_handler}\n${effect501_handler}\n${effect704_handler}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ../../src/render/Renderer.cpp ../../src/render/Prim.cpp ../../src/res/ActRes.h ../../src/world/RagEffect.cpp ../../src/world/RagEffect.h ../../src/world/World.cpp ../../src/res/EzEffectRes.h ../../src/res/Res.cpp)
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
add_test(NAME devotion_packet_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_devotion_packet_source.py")
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
add_test(NAME effect695_map_fireworks_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_effect695_map_fireworks.py")
add_test(NAME prim3dline_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dline.py")
add_test(NAME prim3dtexturebillboard_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_prim3dtexturebillboard.py")
add_test(NAME hitline_render_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_hitline_render.py")
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
add_test(NAME skill_cast_cancel_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_skill_cast_cancel_source.py")
add_test(NAME status_state_source_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_status_state.py")
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
    ../../Ref/RagEffect.cpp
    ../../Ref/RagEffect2.cpp
    ../../src/world/RagEffect.cpp
    ../../src/world/RagEffectOnProcess3Cases.generated.inc
    check_hitline_render.py
    check_hitline_caller.py
    check_hitline4_caller.py
    check_hitline5_caller.py
    check_hitline3_caller.py
    check_teihit_callers.py
    check_chimto_callers.py
    check_stin_sma_callers.py
    check_shield_boomerang_callers.py
    check_rg_coin_callers.py
    check_grandcross_callers.py
    check_linelink_callers.py
    check_tripleattack_callers.py
    check_npcstop_callers.py
    check_guard_callers.py
    check_twilight_callers.py
    check_particle_time_spread_callers.py
    check_spherewind2_caller.py
    check_soulbreaker2_caller.py
    check_peong_callers.py
    check_begin_asura_callers.py
    check_energy_drain_callers.py
    check_portal_wind_caller.py
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
    check_effect695_map_fireworks.py)
