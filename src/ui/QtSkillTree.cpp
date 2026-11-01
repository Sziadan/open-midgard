#include "QtSkillTree.h"
#if RO_ENABLE_QT6_UI
#include "UISkillListWnd.h"
#include "qtui/SkillTreeGeometry.h"
#include "UIWindowMgr.h"
#include "UiScale.h"
#include "session/Session.h"
#include "skill/Skill.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>
#include <chrono>
#include <functional>
#include <windows.h>

namespace {
std::uint64_t Now() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
QString Text(const std::string& value) { return QString::fromStdString(value); }
const QJsonObject& Catalog()
{
    static const QJsonObject catalog = [] {
        QFile file(QStringLiteral(":/qtui/data/skill_tree_catalog.json"));
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    return catalog;
}
}

QtSkillTree::QtSkillTree(UISkillListWnd& owner) : m_owner(owner) { Tick(); }
void QtSkillTree::LoadJob()
{
    m_catalogError.clear(); m_jobName.clear(); m_positions.clear();
    std::vector<skilltree::Skill> definitions;
    if (Catalog().value("schema").toInt() != 1) m_catalogError = "Skill tree catalog is missing or incompatible.";
    for (const auto& value : Catalog().value("jobs").toArray()) {
        auto job = value.toObject();
        if (job.value("id").toInt() != m_job) continue;
        m_jobName = job.value("name").toString().toStdString();
        for (const auto& entry : job.value("skills").toArray()) {
            const auto item = entry.toObject();
            skilltree::Skill skill;
            skill.id = item.value("id").toInt(); skill.cap = item.value("cap").toInt();
            skill.name = item.value("name").toString().toStdString(); skill.symbol = item.value("symbol").toString().toStdString();
            skill.baseLevel = item.value("baseLevel").toInt(); skill.jobLevel = item.value("jobLevel").toInt();
            skill.accessGroup = item.value("accessGroup").toInt(-1);
            skill.passive = item.value("passive").toBool(); skill.quest = item.value("quest").toBool();
            skill.grantedOnly = item.value("grantedOnly").toBool();
            for (const auto& requirement : item.value("requires").toArray()) {
                auto req = requirement.toObject(); skill.requires.push_back({req.value("id").toInt(), req.value("level").toInt()});
            }
            definitions.push_back(std::move(skill));
        }
        break;
    }
    if (definitions.empty() && m_catalogError.empty()) m_catalogError = "No skill tree is defined for this job in the server catalog.";
    std::set<int> defined;
    for (const auto& skill : definitions) defined.insert(skill.id);
    for (const auto& learned : g_session.GetSkillItems()) {
        if (learned.level <= 0 || defined.count(learned.SKID)) continue;
        skilltree::Skill granted;
        granted.id = learned.SKID; granted.cap = learned.level; granted.quest = true; granted.grantedOnly = true;
        granted.passive = learned.type == 0; granted.symbol = learned.skillIdName; granted.name = learned.skillName;
        definitions.push_back(std::move(granted));
    }
    std::map<int,int> levels;
    for (const auto& skill : g_session.GetSkillItems()) levels[skill.SKID] = skill.level;
    if (!m_model.Load(skilltree::VisibleSkills(definitions,levels))) m_catalogError = m_model.message;
    RECT client{}; GetClientRect(g_hMainWnd, &client);
    const float scale = GetConfiguredUiScaleFactor();
    const int width = std::clamp(static_cast<int>((client.right - client.left) / scale) - 24, 280, 780);
    m_fullHeight = std::clamp(static_cast<int>((client.bottom - client.top) / scale) - 24, 220, 580);
    m_owner.Resize(width, m_collapsed ? 17 : m_fullHeight);
    int x = m_owner.m_x, y = m_owner.m_y;
    g_windowMgr.ClampWindowToClient(&x, &y, m_owner.m_w, m_owner.m_h); m_owner.Move(x, y);
    m_scrollX = m_scrollY = m_hover = m_pressed = 0;
    Layout();
}
void QtSkillTree::Layout()
{
    m_positions = skilltree::Layout(m_model, m_owner.m_w - 16);
    m_contentWidth = m_contentHeight = 0;
    for (const auto& [id, position] : m_positions) {
        m_contentWidth = std::max(m_contentWidth, position.x + skilltree::Geometry::width + skilltree::Geometry::margin);
        m_contentHeight = std::max(m_contentHeight, position.y + skilltree::Geometry::height + skilltree::Geometry::margin);
    }
    // Small job trees should not leave several empty columns in a wide frame.
    m_owner.Resize(std::max(std::min(480,m_owner.m_w),m_contentWidth+16),m_owner.m_h);
    ClampScroll();
}
void QtSkillTree::ClampScroll()
{
    m_scrollX = std::clamp(m_scrollX, 0, std::max(0, m_contentWidth - (m_owner.m_w - 16)));
    m_scrollY = std::clamp(m_scrollY, 0, std::max(0, m_contentHeight - (m_fullHeight - 142)));
}
void QtSkillTree::Stop(const std::string& message, bool clear)
{
    m_apply.Stop(m_model, message, clear);
}
void QtSkillTree::Tick()
{
    const auto* character = g_session.GetSelectedCharacterInfo();
    const int job = character ? character->job : g_session.m_playerJob;
    if (m_job != job || m_character != static_cast<int>(g_session.m_gid)) {
        const bool changed = m_job != -1;
        m_job = job; m_character = static_cast<int>(g_session.m_gid);
        m_apply = {};
        LoadJob();
        if (changed) m_model.message = "Character/job changed. Unapplied points cleared.";
    }
    // New quest/equipment/copy skills can arrive after the window was opened.
    // Rebuild their read-only nodes without accepting a draft based on old state.
    bool grantsChanged = false;
    for (const auto& [id, definition] : m_model.skills) {
        const auto* learned = g_session.GetSkillItemBySkillId(id);
        if (definition.grantedOnly && (!learned || learned->level <= 0)) grantsChanged = true;
    }
    for (const auto& skill : g_session.GetSkillItems()) {
        if (skill.level > 0 && !m_model.skills.count(skill.SKID)) grantsChanged = true;
    }
    if (grantsChanged) {
        const bool hadDraft = m_apply.active || !m_model.draft.empty();
        m_apply = {};
        LoadJob();
        if (hadDraft) m_model.message = "Granted skills changed. Unapplied points cleared.";
    }
    skilltree::Snapshot snapshot;
    snapshot.points = g_session.GetPlayerSkillPointCount();
    snapshot.baseLevel = character ? character->level : 0; snapshot.jobLevel = character ? character->joblevel : 0;
    snapshot.enforceServerGroups = true;
    for (const auto& skill : g_session.GetSkillItems()) {
        snapshot.levels[skill.SKID] = skill.level;
        const auto definition = m_model.skills.find(skill.SKID);
        if (definition != m_model.skills.end() && !definition->second.quest && !definition->second.grantedOnly
            && (skill.upgradable || skill.level >= definition->second.cap))
            snapshot.availableGroups.insert(definition->second.accessGroup);
        if (skill.level > 0 && !skill.upgradable && definition != m_model.skills.end() && skill.level < definition->second.cap)
            snapshot.blocked.insert(skill.SKID);
    }
    if (!m_apply.active && !m_model.draft.empty()
        && (snapshot.levels != m_model.confirmed.levels || snapshot.points != m_model.confirmed.points
            || snapshot.baseLevel != m_model.confirmed.baseLevel || snapshot.jobLevel != m_model.confirmed.jobLevel))
        Stop("Server skill state changed. Unapplied points reset.");
    m_model.Sync(snapshot);
    std::set<int> upgradeable;
    for (const auto& skill : g_session.GetSkillItems()) if (skill.upgradable) upgradeable.insert(skill.SKID);
    const int next = m_apply.Poll(m_model, upgradeable, Now());
    if (next && !g_modeMgr.SendMsg(CGameMode::GameMsg_RequestUpgradeSkillLevel, next, 0, 0))
        Stop("Unable to send skill upgrade. Remaining draft cleared.");
}
void QtSkillTree::Apply()
{
    m_apply.Begin(m_model, Now());
}
bool QtSkillTree::InViewport(int x, int y) const
{
    return !m_collapsed && x >= m_owner.m_x + 8 && x < m_owner.m_x + m_owner.m_w - 8
        && y >= m_owner.m_y + 50 && y < m_owner.m_y + m_owner.m_h - 92;
}
int QtSkillTree::Hit(int x, int y, int* part) const
{
    if (!InViewport(x,y)) return 0;
    x = x - m_owner.m_x - 8 + m_scrollX; y = y - m_owner.m_y - 50 + m_scrollY;
    for (const auto& [id, position] : m_positions) {
        const int localX = x - position.x, localY = y - position.y;
        if (localX < 0 || localX >= skilltree::Geometry::width || localY < 0 || localY >= skilltree::Geometry::height) continue;
        if (part) {
            *part = skilltree::HitPart(localX,localY);
        }
        return id;
    }
    return 0;
}
bool QtSkillTree::Press(int x, int y)
{
    const int localX=x-m_owner.m_x, localY=y-m_owner.m_y;
    m_pressed = 0; m_dragged = false; m_pan = false;
    if (localY < 17 && localX >= m_owner.m_w - 34) { m_pressedPart = localX >= m_owner.m_w - 18 ? 10 : 11; return true; }
    if (m_collapsed) return false;
    if (localY >= m_owner.m_h - 31 && localY < m_owner.m_h - 7) {
        if (localX >= m_owner.m_w-164 && localX < m_owner.m_w-88) { m_pressedPart=12; return true; }
        if (localX >= m_owner.m_w-84 && localX < m_owner.m_w-8) { m_pressedPart=13; return true; }
    }
    m_pressedPart = 0; m_pressed = Hit(x,y,&m_pressedPart);
    m_startX=x; m_startY=y; m_originX=m_scrollX; m_originY=m_scrollY;
    if (m_pressed) return true;
    if (InViewport(x,y)) { m_pan = true; return true; }
    return false;
}
bool QtSkillTree::Release(int x, int y)
{
    const int part=m_pressedPart, pressed=m_pressed;
    m_pressed=0; m_pressedPart=0;
    if (m_pan) { m_pan=false; return true; }
    if (m_dragged) { m_dragged=false; return true; }
    if (part >= 10) {
        const int localX=x-m_owner.m_x, localY=y-m_owner.m_y;
        if (part==10 && localY<17 && localX>=m_owner.m_w-18) m_owner.SetShow(0);
        else if (part==11 && localY<17 && localX>=m_owner.m_w-34 && localX<m_owner.m_w-18) {
            m_collapsed=!m_collapsed; m_owner.Resize(m_owner.m_w,m_collapsed?17:m_fullHeight);
        } else if (localY>=m_owner.m_h-31 && localY<m_owner.m_h-7) {
            if (part==12 && localX>=m_owner.m_w-164 && localX<m_owner.m_w-88) Apply();
            else if (part==13 && !m_apply.active && localX>=m_owner.m_w-84 && localX<m_owner.m_w-8) m_model.Reset();
        }
        return true;
    }
    int releasePart=0;
    if (pressed && Hit(x,y,&releasePart)==pressed && part==releasePart && !m_apply.active) {
        if (part==-1 || part==1) m_model.Select(pressed,part);
        else if (part==2 && !m_apply.balanceUncertain) m_model.Stage(pressed);
        return true;
    }
    return pressed != 0;
}
bool QtSkillTree::Move(int x, int y)
{
    m_hover=Hit(x,y);
    if (m_pan) { m_scrollX=m_originX-(x-m_startX); m_scrollY=m_originY-(y-m_startY); ClampScroll(); return true; }
    if (m_pressed && m_pressedPart==2 && !m_dragged && (x-m_startX)*(x-m_startX)+(y-m_startY)*(y-m_startY)>=16) {
        m_dragged=true;
        const auto& definition=m_model.skills.at(m_pressed);
        const int level=m_model.Selected(m_pressed);
        if (definition.passive) m_model.message="Passive skills cannot be assigned to a shortcut.";
        else if (level<=0 || level>m_model.Learned(m_pressed)) m_model.message="Apply these points before using this level.";
        else if (CGameMode* mode=g_modeMgr.GetCurrentGameMode()) {
            mode->m_dragType=static_cast<int>(DragType::ShortcutSkill); mode->m_dragInfo=DRAG_INFO{};
            mode->m_dragInfo.type=mode->m_dragType; mode->m_dragInfo.source=static_cast<int>(DragSource::SkillListWindow);
            mode->m_dragInfo.skillId=m_pressed; mode->m_dragInfo.skillLevel=level;
        }
    }
    return m_pressed!=0;
}
void QtSkillTree::CancelCapture() { m_pressed=0; m_pressedPart=0; m_pan=false; m_dragged=false; m_hover=0; }
void QtSkillTree::Wheel(int delta) { m_scrollY-=delta>0?70:-70; ClampScroll(); }
void QtSkillTree::Describe(int x, int y)
{
    const int id=Hit(x,y); if (!id) return;
    PLAYER_SKILL_INFO info{};
    if (const auto* learned=g_session.GetSkillItemBySkillId(id)) info=*learned;
    const auto& skill=m_model.skills.at(id);
    info.SKID=id; info.m_isValid=1; info.skillName=skill.name; info.skillIdName=skill.symbol;
    info.level=m_model.Learned(id); info.skillMaxLv=skill.cap;
    g_skillMgr.EnsureLoaded();
    if (const auto* metadata=g_skillMgr.GetSkillMetadata(id)) info.descriptionLines=metadata->descriptionLines;
    g_windowMgr.ShowSkillDescribeWindow(info,x+12,y+12);
}
QVariantMap QtSkillTree::Data() const
{
    QVariantMap data;
    const int reservation = m_apply.active && m_apply.waitingSkill && m_apply.frontConfirmed
        && m_model.confirmed.points >= m_apply.startPoints ? 1 : 0;
    data["geometry"]=SkillTreeGeometryForQt();
    data["treeMode"]=true; data["title"]=QStringLiteral("Skill Tree - ")+Text(m_jobName);
    data["collapsed"]=m_collapsed; data["points"]=std::max(0,m_model.Remaining()-reservation); data["pending"]=static_cast<int>(m_model.draft.size());
    data["applying"]=m_apply.active; data["canApply"]=!m_apply.active&&!m_apply.balanceUncertain&&!m_model.draft.empty();
    data["canReset"]=!m_apply.active&&!m_model.draft.empty();
    data["status"]=Text(!m_catalogError.empty()?m_catalogError:m_apply.balanceUncertain?"Waiting for the server point balance. Reopen after synchronization.":m_model.message);
    if (m_apply.active) data["status"]=QString("Applying %1 / %2 - waiting for server confirmation").arg(m_apply.applied).arg(m_apply.total);
    data["viewHeight"]=std::max(1,m_fullHeight-142); data["contentWidth"]=m_contentWidth; data["contentHeight"]=m_contentHeight;
    data["scrollX"]=m_scrollX; data["scrollY"]=m_scrollY;
    const auto requirements=m_model.Requirements(m_hover);
    QVariantList nodes, edges;
    for (const auto& [id, skill] : m_model.skills) {
        const auto position=m_positions.at(id);
        QVariantMap node;
        node["id"]=id; node["name"]=Text(skill.name); node["symbol"]=Text(skill.symbol);
        node["x"]=position.x-m_scrollX; node["y"]=position.y-m_scrollY;
        node["learned"]=m_model.Learned(id); node["level"]=m_model.Projected(id); node["selected"]=m_model.Selected(id);
        node["pending"]=m_model.Pending(id)>0; node["locked"]=m_model.Learned(id)==0&&!m_model.Unlocked(id);
        node["available"]=!m_apply.active&&!m_apply.balanceUncertain&&m_model.Remaining()>0&&m_model.Unlocked(id)&&m_model.Projected(id)<skill.cap;
        node["hovered"]=id==m_hover; node["prerequisite"]=requirements.count(id)!=0;
        node["required"]=requirements.count(id)?requirements.at(id):0;
        node["passive"]=skill.passive; node["cap"]=skill.cap;
        node["jobBlocked"]=!m_model.GroupAvailable(id);
        nodes.push_back(node);
        for (const auto& req : skill.requires) {
            const auto parent=m_positions.at(req.id);
            edges.push_back(QVariantMap{{"x1",parent.x+skilltree::Geometry::width/2-m_scrollX},{"y1",parent.y+skilltree::Geometry::height-m_scrollY},
                {"x2",position.x+skilltree::Geometry::width/2-m_scrollX},{"y2",position.y-m_scrollY},
                {"detour",position.y-parent.y>skilltree::Geometry::height+skilltree::Geometry::rowGap},
                {"highlight",(id==m_hover||requirements.count(id))&&requirements.count(req.id)},
                {"met",m_model.Projected(req.id)>=req.level},{"level",req.level}});
        }
    }
    data["nodes"]=nodes; data["edges"]=edges;
    QString tooltip;
    if (m_hover && m_model.skills.count(m_hover)) {
        const auto& skill=m_model.skills.at(m_hover);
        tooltip=Text(skill.name)+QString(" | Learned %1 / cap %2").arg(m_model.Learned(m_hover)).arg(skill.cap);
        if (skill.quest || skill.grantedOnly) tooltip+=" | Quest/granted skill";
        if (skill.passive) tooltip+=" | Passive";
        if (skill.baseLevel) tooltip+=QString(" | Base Lv. %1").arg(skill.baseLevel);
        if (skill.jobLevel) tooltip+=QString(" | Job Lv. %1").arg(skill.jobLevel);
        if (!m_model.GroupAvailable(m_hover)) tooltip+="\nNot available from server yet. Apply earlier-job skill points first.";
        for (const auto& [id, level] : requirements) tooltip+=QString("\n%1: %2 / %3 required").arg(Text(m_model.skills.at(id).name)).arg(m_model.Projected(id)).arg(level);
    }
    data["tooltip"]=tooltip;
    return data;
}
#endif
