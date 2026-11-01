#include "SkillTreeModel.h"
#include <algorithm>
#include <functional>
#include <cstdlib>

namespace skilltree {
bool Model::ValidGraph(int id, std::set<int>& visiting, std::set<int>& done) const
{
    if (done.count(id)) return true;
    const auto it = skills.find(id);
    if (it == skills.end() || !visiting.insert(id).second) return false;
    for (const auto& requirement : it->second.requires) {
        auto parent = skills.find(requirement.id);
        if (parent == skills.end() || requirement.level < 1 || requirement.level > parent->second.cap
            || !ValidGraph(requirement.id, visiting, done)) return false;
    }
    visiting.erase(id); done.insert(id); return true;
}
bool Model::Load(const std::vector<Skill>& definitions)
{
    skills.clear(); draft.clear(); selected.clear(); confirmed = {}; message.clear();
    for (const auto& skill : definitions) {
        if (skill.id <= 0 || skill.cap <= 0 || !skills.emplace(skill.id, skill).second) {
            message = "Invalid skill catalog."; skills.clear(); return false;
        }
    }
    std::set<int> visiting, done;
    for (const auto& [id, skill] : skills) {
        if (!ValidGraph(id, visiting, done)) {
            message = "Invalid prerequisite graph."; skills.clear(); return false;
        }
    }
    return true;
}
int Model::Learned(int id) const { const auto it = confirmed.levels.find(id); return it == confirmed.levels.end() ? 0 : it->second; }
int Model::Pending(int id) const { return static_cast<int>(std::count(draft.begin(), draft.end(), id)); }
int Model::Projected(int id) const { return Learned(id) + Pending(id); }
int Model::Remaining() const { return std::max(0, confirmed.points - static_cast<int>(draft.size())); }
int Model::Selected(int id) const { auto it = selected.find(id); return it == selected.end() ? Learned(id) : it->second; }
void Model::Sync(const Snapshot& snapshot)
{
    confirmed = snapshot;
    for (const auto& [id, skill] : skills) {
        const int level = Projected(id);
        auto it = selected.find(id);
        if (it == selected.end() || (it->second == 0 && Learned(id) > 0)) selected[id] = Learned(id);
        else it->second = level == 0 ? 0 : std::clamp(it->second, 1, level);
    }
}
void Model::Select(int id, int delta)
{
    if (Projected(id) > 0) selected[id] = std::clamp(Selected(id) + delta, 1, Projected(id));
}
bool Model::Gates(int id) const
{
    auto it = skills.find(id);
    return it != skills.end() && GroupAvailable(id) && !it->second.quest && !it->second.grantedOnly && !confirmed.blocked.count(id)
        && confirmed.baseLevel >= it->second.baseLevel && confirmed.jobLevel >= it->second.jobLevel;
}
bool Model::GroupAvailable(int id) const
{
    const auto it = skills.find(id);
    return it != skills.end() && (!confirmed.enforceServerGroups || it->second.accessGroup < 0
        || confirmed.availableGroups.count(it->second.accessGroup));
}
bool Model::Unlocked(int id, bool projected) const
{
    if (!Gates(id)) return false;
    for (const auto& requirement : skills.at(id).requires)
        if ((projected ? Projected(requirement.id) : Learned(requirement.id)) < requirement.level) return false;
    return true;
}
int Model::Next(int id, std::set<int>& visiting) const
{
    if (!Gates(id) || Projected(id) >= skills.at(id).cap || !visiting.insert(id).second) return 0;
    int next = id;
    for (const auto& req : skills.at(id).requires) {
        if (Projected(req.id) >= req.level) continue;
        const int candidate = Next(req.id, visiting);
        if (!candidate) { visiting.erase(id); return 0; }
        if (next == id) next = candidate;
    }
    visiting.erase(id); return next;
}
bool Model::CanStage(int id) const
{
    std::set<int> visiting;
    return Remaining() > 0 && Next(id, visiting) != 0;
}
bool Model::Stage(int id)
{
    if (Remaining() <= 0) { message = "No unassigned skill points."; return false; }
    if (!GroupAvailable(id)) {
        message = "This job's skills are not available from the server yet. Apply earlier-job points first.";
        return false;
    }
    std::set<int> visiting;
    const int recipient = Next(id, visiting);
    if (!recipient) { message = "This skill is capped or blocked by a job, quest, or skill restriction."; return false; }
    draft.push_back(recipient);
    if (Selected(recipient) == 0) selected[recipient] = 1;
    message = "Planned " + skills.at(recipient).name + " Lv. " + std::to_string(Projected(recipient));
    if (recipient != id) message += " for " + skills.at(id).name;
    return true;
}
void Model::Reset()
{
    draft.clear(); Sync(confirmed); message = "Unapplied points reset.";
}
void Model::ConfirmFront()
{
    if (!draft.empty()) draft.erase(draft.begin());
    Sync(confirmed);
}
std::map<int, int> Model::Requirements(int id) const
{
    std::map<int, int> result;
    std::set<int> visited;
    std::vector<int> todo{id};
    while (!todo.empty()) {
        const int current = todo.back(); todo.pop_back();
        auto it = skills.find(current);
        if (it == skills.end() || !visited.insert(current).second) continue;
        for (const auto& req : it->second.requires) {
            result[req.id] = std::max(result[req.id], req.level); todo.push_back(req.id);
        }
    }
    return result;
}
void ApplyQueue::Stop(Model& model, const std::string& message, bool clear)
{
    active = false; waitingSkill = 0;
    if (clear) model.Reset();
    model.message = message;
}
bool ApplyQueue::Begin(Model& model, std::uint64_t now)
{
    if (active || balanceUncertain || model.draft.empty()) return false;
    Model check = model; check.draft.clear();
    for (const int id : model.draft) {
        if (!check.Unlocked(id) || check.Projected(id) >= check.skills.at(id).cap || check.Remaining() <= 0) {
            model.message = "Draft no longer valid. Reset and assign the points again."; return false;
        }
        check.draft.push_back(id);
    }
    active = true; applied = 0; total = static_cast<int>(model.draft.size()); since = now; waitingSkill = 0;
    model.message = "Applying points. Waiting for server confirmation...";
    return true;
}
int ApplyQueue::Poll(Model& model, const std::set<int>& upgradeable, std::uint64_t now)
{
    if (balanceUncertain && model.confirmed.points <= startPoints - 1) balanceUncertain = false;
    if (!active) return 0;
    if (waitingSkill) {
        if (model.Learned(waitingSkill) > expectedLevel) {
            Stop(model, "Unexpected server skill change. Remaining draft cleared."); return 0;
        }
        const bool learned = model.Learned(waitingSkill) >= expectedLevel;
        if (learned && !frontConfirmed) { model.ConfirmFront(); ++applied; frontConfirmed = true; }
        if (learned && model.confirmed.points <= startPoints - 1) {
            waitingSkill = 0; since = now;
        } else if (now - since > 7000) {
            balanceUncertain = learned;
            Stop(model, "Apply stopped after " + std::to_string(applied)
                + " confirmed point(s). Remaining draft cleared; waiting for server state.");
        }
        return 0;
    }
    if (model.draft.empty()) { Stop(model, "All planned points confirmed.", false); return 0; }
    const int next = model.draft.front();
    if (model.Learned(next) >= model.skills.at(next).cap) {
        Stop(model, "Skill level changed during Apply. Remaining draft cleared."); return 0;
    }
    if (!model.Unlocked(next, false) || !upgradeable.count(next) || model.confirmed.points <= 0) {
        if (now - since > 7000) Stop(model, "Apply stopped: server did not unlock the next skill. Remaining draft cleared.");
        return 0;
    }
    frontConfirmed = false; waitingSkill = next; expectedLevel = model.Learned(next) + 1;
    startPoints = model.confirmed.points; since = now;
    return next;
}
std::vector<Skill> VisibleSkills(const std::vector<Skill>& definitions, const std::map<int,int>& learned)
{
    std::vector<Skill> visible;
    for (const auto& skill : definitions) {
        auto level = learned.find(skill.id);
        if (!skill.grantedOnly || (level != learned.end() && level->second > 0)) visible.push_back(skill);
    }
    return visible;
}
int HitPart(int x, int y)
{
    const auto inside = [=](int left, int top, int width, int height) {
        return x >= left && x < left+width && y >= top && y < top+height;
    };
    if (inside(Geometry::leftX,Geometry::arrowY,Geometry::arrowSize,Geometry::arrowSize)) return -1;
    if (inside(Geometry::rightX,Geometry::arrowY,Geometry::arrowSize,Geometry::arrowSize)) return 1;
    if (inside(Geometry::iconX,Geometry::iconY,Geometry::iconSize,Geometry::iconSize)) return 2;
    return 0;
}
std::map<int, Position> Layout(const Model& model, int viewportWidth)
{
    std::map<int, Position> positions;
    std::map<int, int> depths, children;
    std::function<int(int)> depth = [&](int id) {
        if (depths.count(id)) return depths[id];
        int value = 0;
        for (const auto& req : model.skills.at(id).requires) value = std::max(value, depth(req.id) + 1);
        return depths[id] = value;
    };
    for (const auto& [id, skill] : model.skills)
        for (const auto& req : skill.requires) ++children[req.id];
    std::vector<int> branches, leaves, independent;
    for (const auto& [id, skill] : model.skills) {
        if (children[id]) branches.push_back(id);
        else if (skill.requires.empty()) independent.push_back(id);
        else leaves.push_back(id);
    }
    const auto order = [&](int a, int b) {
        return depth(a) != depth(b) ? depth(a) < depth(b) : a < b;
    };
    std::stable_sort(branches.begin(),branches.end(),order);
    std::stable_sort(leaves.begin(),leaves.end(),order);
    const int columns = std::max(1,(viewportWidth-2*Geometry::margin+Geometry::columnGap)
        /(Geometry::width+Geometry::columnGap));
    const int first = !independent.empty() && columns > 1 ? 1 : 0;
    std::set<std::pair<int,int>> occupied;
    std::map<int,Position> cells;
    const auto place = [&](int id, bool leaf) {
        int row = 0, preferred = first, parentColumns = 0;
        const auto& requirements = model.skills.at(id).requires;
        for (const auto& req : requirements) {
            row = std::max(row,cells.at(req.id).y+1);
            parentColumns += cells.at(req.id).x;
        }
        if (!requirements.empty()) preferred = parentColumns/static_cast<int>(requirements.size());
        int column = preferred;
        // Terminal siblings share a vertical lane beneath their prerequisites.
        // Branching nodes spread across the available columns, wrapping down.
        for (;;) {
            int candidate = -1, distance = columns+1;
            for (int c=first;c<columns;++c) {
                if (leaf && c!=preferred) continue;
                if (!occupied.count({c,row}) && std::abs(c-preferred)<distance) {
                    candidate=c;distance=std::abs(c-preferred);
                }
            }
            if (candidate>=0) { column=candidate;break; }
            ++row;
        }
        occupied.insert({column,row});cells[id]={column,row};
    };
    for (int id : branches) place(id,false);
    for (int id : leaves) place(id,true);
    int independentRow = 0;
    for (int id : independent) {
        while (occupied.count({0,independentRow})) ++independentRow;
        occupied.insert({0,independentRow});cells[id]={0,independentRow++};
    }
    for (const auto& [id,cell] : cells)
        positions[id]={Geometry::margin+cell.x*(Geometry::width+Geometry::columnGap),
            Geometry::margin+cell.y*(Geometry::height+Geometry::rowGap)};
    return positions;
}
}
