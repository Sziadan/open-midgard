"""Compile production-body regressions without the shared test project's extractors."""
import argparse
import os
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    packet = (ROOT / 'src/gamemode/GameModePacket.cpp').read_text(encoding='utf-8')
    mode = (ROOT / 'src/gamemode/GameMode.cpp').read_text(encoding='utf-8')
    cursor = (ROOT / 'src/gamemode/CursorRenderer.cpp').read_text(encoding='utf-8')
    equip = (ROOT / 'src/ui/UIEquipWnd.cpp').read_text(encoding='utf-8')
    item = (ROOT / 'src/ui/UIItemWnd.cpp').read_text(encoding='utf-8')
    session = (ROOT / 'src/session/Session.cpp').read_text(encoding='utf-8')
    actor = (ROOT / 'src/world/GameActor.cpp').read_text(encoding='utf-8')
    # Instant announcements cannot launch a local pose/effect before a result.
    cast = function(packet, 'void HandleSkillCastAck(')
    guard = cast.index('if (castTimeMs <= 0 && mode.m_world && sourceActor == mode.m_world->m_player)')
    assert cast.index('return;', guard) < cast.index('sourceActor->SetState(')
    assert 'router.Register(0x013B, HandleActionFailure);' in packet
    assert 'router.Register(0x013C, HandleEquipAmmo);' in packet
    reference = (ROOT / 'Ref/GameModePacket.cpp').read_text(encoding='utf-8', errors='replace')
    failure_ref = function(reference, 'void __thiscall CGameMode::Zc_action_failure(')
    failure_local = function(packet, 'void HandleActionFailure(')
    cases = lambda body: [int(value, 0) for value in re.findall(r'case\s+(0x[0-9A-Fa-f]+|\d+)\s*:', body)]
    assert cases(failure_ref) == cases(failure_local) == [0, 1, 2, 3]
    pickup = function(packet, 'void HandleItemPickupAck(')
    assert 'itemInfo.m_location = 0x8000;' in pickup
    assert 'itemInfo.m_wearLocation = 0x8000;' not in pickup
    # Both inventory activation implementations use equipment requests for ammo.
    for name in ['void UIItemWnd::OnLBtnDblClk(', 'void UIItemWnd::TryActivateItemAtPoint(']:
        body = function(item, name)
        assert 'item->m_itemType == 10' in body
        assert 'GameMsg_RequestEquipInventoryItem' in body
    assert 'GameMsg_RequestEquipInventoryItem' in function(equip, 'void UIEquipWnd::DragAndDrop(')
    assert 'GameMsg_RequestUnequipInventoryItem' in function(item, 'void UIItemWnd::DragAndDrop(')
    assert '{ 0x8000, 240, 179 }' in equip
    assert 'assignByMask(0x8000);' in equip
    equipment_filter = function(equip, 'bool IsEquipTabType(')
    assert 'case 10:' in equipment_filter

    with tempfile.TemporaryDirectory(prefix='ro_ammo_cursor_') as directory:
        build = pathlib.Path(directory)
        (build / 'AmmoPacketBodies.inc').write_text('\n'.join(function(packet, name) for name in [
            'bool BuildNormalInventoryItem(', 'void HandleEquipAmmo(', 'void HandleActionFailure(']), encoding='utf-8')
        (build / 'AmmoRequestBodies.inc').write_text('\n'.join(function(mode, name) for name in [
            'bool RequestEquipInventoryItem(', 'bool RequestUnequipInventoryItem(',
            'bool RequestUseInventoryItem(', 'bool RequestShortcutSlotUse(']), encoding='utf-8')
        (build / 'CursorFrameBody.inc').write_text(function(cursor, 'bool GetModeCursorClientPos('), encoding='utf-8')
        (build / 'InstantCastBody.inc').write_text(function(packet, 'void HandleSkillCastAck('), encoding='utf-8')
        slots_start = equip.index('constexpr std::array<EquipSlotDefLocal,')
        slots_end = equip.index('}};', slots_start) + 3
        (build / 'AmmoSlotBodies.inc').write_text(equip[slots_start:slots_end] + '\n' + equipment_filter + '\n'
            + function(equip, 'std::vector<const ITEM_INFO*> UIEquipWnd::BuildSlotAssignments()'), encoding='utf-8')
        (build / 'AmmoRefreshBody.inc').write_text(function(session, 'void CSession::ClearEquipmentInventoryItems()'), encoding='utf-8')
        (build / 'EquipmentOptionLabelBodies.inc').write_text('\n'.join(function(equip, name) for name in [
            'bool IsLocalPlayerPushCartEquipped()', 'const char* GetLocalPlayerRemoveOptionLabel()']), encoding='utf-8')
        tabs_start = item.index('bool IsUsableTabType(')
        tabs_end = item.index('bool CanUseInventoryItemOnDoubleClick(', tabs_start)
        (build / 'AmmoInventoryViewBodies.inc').write_text(item[tabs_start:tabs_end] + '\n'
            + function(item, 'int UIItemWnd::GetInventoryItemCount()') + '\n'
            + function(item, 'std::vector<const ITEM_INFO*> UIItemWnd::GetFilteredItems()'), encoding='utf-8')
        executable = build / 'ammo_cursor_checks.exe'
        source = str(ROOT / 'tools/tests/AmmoCursorRegressionTests.cpp')
        if pathlib.Path(args.cxx).stem.lower() == 'cl':
            command = [args.cxx, '/std:c++17', '/EHsc', '/UNDEBUG', f'/I{build}', source, f'/Fe:{executable}']
        else:
            command = [args.cxx, '-std=c++17', '-UNDEBUG', f'-I{build}', source, '-o', str(executable)]
        subprocess.run(command, check=True, cwd=build)
        environment = os.environ.copy()
        environment['PATH'] = str(pathlib.Path(args.cxx).resolve().parent) + os.pathsep + environment.get('PATH', '')
        subprocess.run([str(executable)], check=True, env=environment)
        click_start = mode.index('    case GameMsg_LButtonDown: {')
        click_end = mode.index('    case GameMsg_LButtonUp:', click_start)
        click = mode[click_start:click_end].replace('    case GameMsg_LButtonDown:',
            'int CGameMode::Click(msgparam_t wparam,msgparam_t lparam)', 1)
        (build / 'SkillTargetClickBodies.inc').write_text('\n'.join([
            function(mode, 'bool HasPendingSkillUse('), function(mode, 'bool ShouldUseTurnOnlyGroundClick('), click]), encoding='utf-8')
        click_source = str(ROOT / 'tools/tests/SkillTargetClickTests.cpp')
        click_executable = build / 'skill_target_click_checks.exe'
        if pathlib.Path(args.cxx).stem.lower() == 'cl':
            click_command = [args.cxx, '/std:c++17', '/EHsc', '/UNDEBUG', f'/I{build}', click_source, f'/Fe:{click_executable}']
        else:
            click_command = [args.cxx, '-std=c++17', '-UNDEBUG', f'-I{build}', click_source, '-o', str(click_executable)]
        subprocess.run(click_command, check=True, cwd=build)
        subprocess.run([str(click_executable)], check=True, env=environment)
        (build / 'CastingPoseBodies.inc').write_text('\n'.join([
            function(actor, 'bool IsTransientActionActive('),
            function(actor, 'void CGameActor::ProcessSkillRechargeGageOverlay('),
            function(packet, 'void HandleSkillCastCancel(')]), encoding='utf-8')
        cast_source = str(ROOT / 'tools/tests/CastingPoseTests.cpp')
        cast_executable = build / 'casting_pose_checks.exe'
        if pathlib.Path(args.cxx).stem.lower() == 'cl':
            cast_command = [args.cxx, '/std:c++17', '/EHsc', '/UNDEBUG', f'/I{build}', cast_source, f'/Fe:{cast_executable}']
        else:
            cast_command = [args.cxx, '-std=c++17', '-UNDEBUG', f'-I{build}', cast_source, '-o', str(cast_executable)]
        subprocess.run(cast_command, check=True, cwd=build)
        subprocess.run([str(cast_executable)], check=True, env=environment)
        effects = (ROOT / 'src/world/RagEffect.cpp').read_text(encoding='utf-8')
        effects_header = (ROOT / 'src/world/RagEffect.h').read_text(encoding='utf-8')
        (build / 'WeatherCloudGi.inc').write_text(function(effects_header, 'struct EffectGi {') + ';', encoding='utf-8')
        (build / 'WeatherCloudCycleBody.inc').write_text(function(effects, 'bool UpdateWeatherCloudPrimitive('), encoding='utf-8')
        weather_source = str(ROOT / 'tools/tests/WeatherCloudCycleTests.cpp')
        weather_executable = build / 'weather_cloud_cycle_checks.exe'
        if pathlib.Path(args.cxx).stem.lower() == 'cl':
            weather_command = [args.cxx, '/std:c++17', '/EHsc', '/UNDEBUG', f'/I{build}', weather_source, f'/Fe:{weather_executable}']
        else:
            weather_command = [args.cxx, '-std=c++17', '-UNDEBUG', f'-I{build}', weather_source, '-o', str(weather_executable)]
        subprocess.run(weather_command, check=True, cwd=build)
        subprocess.run([str(weather_executable)], check=True, env=environment)
    print('Ammo UI routing, packet registration and instant-cast ordering checks passed')


if __name__ == '__main__':
    main()
