#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdint>

namespace skilltree {
struct Requirement { int id = 0; int level = 0; };
struct Skill {
    int id = 0, cap = 0, baseLevel = 0, jobLevel = 0, accessGroup = -1;
    std::string name, symbol;
    bool passive = false, quest = false, grantedOnly = false;
    std::vector<Requirement> requires;
};
struct Snapshot {
    int points = 0, baseLevel = 0, jobLevel = 0;
    std::map<int, int> levels;
    std::set<int> blocked;
    bool enforceServerGroups = false;
    std::set<int> availableGroups;
};
class Model {
public:
    std::map<int, Skill> skills;
    Snapshot confirmed;
    std::vector<int> draft;
    std::map<int, int> selected;
    std::string message;
    bool Load(const std::vector<Skill>& definitions);
    void Sync(const Snapshot& snapshot);
    int Learned(int id) const;
    int Projected(int id) const;
    int Pending(int id) const;
    int Remaining() const;
    int Selected(int id) const;
    void Select(int id, int delta);
    bool Unlocked(int id, bool projected = true) const;
    bool GroupAvailable(int id) const;
    bool CanStage(int id) const;
    bool Stage(int id);
    void Reset();
    void ConfirmFront();
    std::map<int, int> Requirements(int id) const;
private:
    bool Gates(int id) const;
    int Next(int id, std::set<int>& visiting) const;
    bool ValidGraph(int id, std::set<int>& visiting, std::set<int>& done) const;
};
// One request in flight. Poll returns a skill ID only when a new request should
// be sent; repeated snapshots/acks cannot send the same request twice.
class ApplyQueue {
public:
    bool active = false, balanceUncertain = false, frontConfirmed = false;
    int waitingSkill = 0, expectedLevel = 0, startPoints = 0, applied = 0, total = 0;
    std::uint64_t since = 0;
    bool Begin(Model& model, std::uint64_t now);
    int Poll(Model& model, const std::set<int>& upgradeable, std::uint64_t now);
    void Stop(Model& model, const std::string& message, bool clear = true);
};
struct Position { int x = 0, y = 0; };
std::vector<Skill> VisibleSkills(const std::vector<Skill>& definitions, const std::map<int,int>& learned);
struct Geometry {
    static constexpr int width = 104, height = 98, margin = 8;
    static constexpr int columnGap = 14, rowGap = 22;
    static constexpr int iconSize = 32, iconX = (width-iconSize)/2, iconY = 29;
    static constexpr int arrowSize = 20, arrowY = 65, leftX = 4, rightX = width-24;
};
// Node-local hit testing uses the same geometry exported to QML.
int HitPart(int x, int y);
std::map<int, Position> Layout(const Model& model, int viewportWidth = 764);
}
