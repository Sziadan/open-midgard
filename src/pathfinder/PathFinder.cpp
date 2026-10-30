#include "PathFinder.h"

#include "world/GameActor.h"
#include "world/World.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

namespace {

struct OpenNode {
	int x;
	int y;
	int cost;
	unsigned int serial;
};

struct OpenNodeGreater {
	bool operator()(const OpenNode& a, const OpenNode& b) const
	{
		if (a.cost != b.cost) {
			return a.cost > b.cost;
		}
		return a.serial > b.serial;
	}
};

struct SearchNode {
	int x = 0;
	int y = 0;
	int dist = std::numeric_limits<int>::max();
	int cost = std::numeric_limits<int>::max();
	int prevX = 0;
	int prevY = 0;
	bool hasPrev = false;
	bool isClosed = false;
	bool discovered = false;
};

int NodeKey(int x, int y)
{
	return (y << 16) ^ (x & 0xFFFF);
}

int Sign(int value)
{
	if (value < 0) {
		return -1;
	}
	if (value > 0) {
		return 1;
	}
	return 0;
	}

int StepMilliseconds(int speedFactor, bool diagonal)
{
	const int speed = speedFactor > 0 ? speedFactor : 150;
	if (!diagonal) {
		return speed;
	}
	// Ref CPathFinder::BuildResultPath: diagonal steps add (int)(speed * 1.414).
	return static_cast<int>(static_cast<double>(speed) * 1.414);
}

// Ref FindPath stores the 0..15 sub-cell as the path start, then the first
// step lasts speed * distance from that point to the next cell. Later steps
// are one speed unit, or speed*1.414 on a diagonal.
void StampPathTimes(CPathInfo* pathInfo,
	unsigned int startTime,
	int sx,
	int sy,
	int subCellX,
	int subCellY,
	int speedFactor,
	const std::vector<std::pair<int, int>>& steps)
{
	pathInfo->m_cells.clear();
	pathInfo->m_cells.push_back(PathCell{ sx, sy, startTime });
	if (steps.empty()) {
		return;
	}

	double startPointX = static_cast<double>(subCellX) * 0.0625;
	double startPointY = static_cast<double>(subCellY) * 0.0625;
	if (sx > 0) {
		startPointX = static_cast<double>(static_cast<int>(startPointX + static_cast<double>(sx) - 0.5));
	}
	if (sy > 0) {
		startPointY = static_cast<double>(static_cast<int>(startPointY + static_cast<double>(sy) - 0.5));
	}

	const double speed = static_cast<double>(speedFactor > 0 ? speedFactor : 150);
	const double deltaX = static_cast<double>(steps.front().first) - startPointX;
	const double deltaY = static_cast<double>(steps.front().second) - startPointY;
	unsigned int currentTime = startTime + static_cast<unsigned int>(speed * std::sqrt(deltaX * deltaX + deltaY * deltaY));
	pathInfo->m_cells.push_back(PathCell{ steps.front().first, steps.front().second, currentTime });

	int prevX = steps.front().first;
	int prevY = steps.front().second;
	for (size_t index = 1; index < steps.size(); ++index) {
		const bool diagonal = steps[index].first != prevX && steps[index].second != prevY;
		currentTime += static_cast<unsigned int>(StepMilliseconds(speedFactor, diagonal));
		pathInfo->m_cells.push_back(PathCell{ steps[index].first, steps[index].second, currentTime });
		prevX = steps[index].first;
		prevY = steps[index].second;
	}
}

} // namespace

CPathFinder g_pathFinder;

CPathFinder::CPathFinder()
	: m_map(nullptr)
{
}

CPathFinder::~CPathFinder()
{
}

void CPathFinder::Reset()
{
}

void CPathFinder::SetMap(C3dAttr* newMap)
{
	m_map = newMap;
}

bool CPathFinder::IsWalkable(int x, int y) const
{
	if (!m_map || x < 0 || y < 0 || x >= m_map->m_width || y >= m_map->m_height) {
		return false;
	}

	const CAttrCell& cell = m_map->m_cells[static_cast<size_t>(y) * static_cast<size_t>(m_map->m_width) + static_cast<size_t>(x)];
	return cell.flag == 0;
}

bool CPathFinder::IsConnected(int sx, int sy, int dx, int dy) const
{
	if (!IsWalkable(dx, dy)) {
		return false;
	}

	const int deltaX = dx - sx;
	const int deltaY = dy - sy;
	if ((std::abs)(deltaX) > 1 || (std::abs)(deltaY) > 1) {
		return false;
	}
	if (deltaX != 0 && deltaY != 0) {
		return IsWalkable(sx + deltaX, sy) && IsWalkable(sx, sy + deltaY);
	}
	return true;
}

bool CPathFinder::FindPath(unsigned int startTime,
	int sx,
	int sy,
	int cellX,
	int cellY,
	int dx,
	int dy,
	int speedFactor,
	CPathInfo* pathInfo)
{
	if (!pathInfo) {
		return false;
	}
	pathInfo->Reset();

	// Ref: sx/sy is the start cell, cellX/cellY is the 0..15 sub-cell inside
	// it, and dx/dy is the destination cell. A same-cell move is not a path.
	if (!m_map || !IsWalkable(sx, sy) || !IsWalkable(dx, dy) || (sx == dx && sy == dy)) {
		return false;
	}

	std::vector<std::pair<int, int>> directPath;
	{
		int stepX = Sign(dx - sx);
		int stepY = Sign(dy - sy);
		int walkX = sx;
		int walkY = sy;
		bool directPathValid = true;

		while (walkX != dx || walkY != dy) {
			walkX += stepX;
			walkY += stepY;
			if (walkX == dx) {
				stepX = 0;
			}
			if (walkY == dy) {
				stepY = 0;
			}
			if (!IsWalkable(walkX, walkY)) {
				directPath.clear();
				directPathValid = false;
				break;
			}
			directPath.push_back({walkX, walkY});
		}

		if (directPathValid) {
			StampPathTimes(pathInfo, startTime, sx, sy, cellX, cellY, speedFactor, directPath);
			return true;
		}
	}

	std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeGreater> open;
	std::unordered_map<int, SearchNode> nodes;
	unsigned int nextSerial = 0;

	auto enqueueOrRelax = [&](int x, int y, int dist, int cost, int prevX, int prevY) {
		const int key = NodeKey(x, y);
		SearchNode& node = nodes[key];
		if (node.discovered && node.dist <= dist) {
			return;
		}

		node.x = x;
		node.y = y;
		node.dist = dist;
		node.cost = cost;
		node.prevX = prevX;
		node.prevY = prevY;
		node.hasPrev = !(x == sx && y == sy);
		node.isClosed = false;
		node.discovered = true;
		open.push(OpenNode{ x, y, cost, nextSerial++ });
	};

	enqueueOrRelax(sx, sy, 0, 10 * ((std::abs)(dx - sx) + (std::abs)(dy - sy)), sx, sy);

	SearchNode* goalNode = nullptr;
	while (!open.empty()) {
		const OpenNode currentOpen = open.top();
		open.pop();

		auto currentIt = nodes.find(NodeKey(currentOpen.x, currentOpen.y));
		if (currentIt == nodes.end()) {
			continue;
		}

		SearchNode& current = currentIt->second;
		if (!current.discovered || current.isClosed || current.cost != currentOpen.cost) {
			continue;
		}

		if (current.x == dx && current.y == dy) {
			goalNode = &current;
			break;
		}

		const int dist = current.dist + 10;
		const int cost = current.cost;
		int dc[4] = { 0, 0, 0, 0 };
		int freeMask = 0;

		if (current.y + 1 < m_map->m_height && IsWalkable(current.x, current.y + 1)) {
			freeMask |= 1;
			dc[0] = (current.y >= dy) ? 20 : 0;
			enqueueOrRelax(current.x, current.y + 1, dist, cost + dc[0], current.x, current.y);
		}
		if (current.x - 1 >= 0 && IsWalkable(current.x - 1, current.y)) {
			freeMask |= 2;
			dc[1] = (current.x <= dx) ? 20 : 0;
			enqueueOrRelax(current.x - 1, current.y, dist, cost + dc[1], current.x, current.y);
		}
		if (current.y - 1 >= 0 && IsWalkable(current.x, current.y - 1)) {
			freeMask |= 4;
			dc[2] = (current.y <= dy) ? 20 : 0;
			enqueueOrRelax(current.x, current.y - 1, dist, cost + dc[2], current.x, current.y);
		}
		if (current.x + 1 < m_map->m_width && IsWalkable(current.x + 1, current.y)) {
			freeMask |= 8;
			dc[3] = (current.x >= dx) ? 20 : 0;
			enqueueOrRelax(current.x + 1, current.y, dist, cost + dc[3], current.x, current.y);
		}

		if ((freeMask & (2 | 1)) == (2 | 1) && IsWalkable(current.x - 1, current.y + 1)) {
			enqueueOrRelax(current.x - 1, current.y + 1, dist + 4, cost + dc[1] + dc[0] - 6, current.x, current.y);
		}
		if ((freeMask & (2 | 4)) == (2 | 4) && IsWalkable(current.x - 1, current.y - 1)) {
			enqueueOrRelax(current.x - 1, current.y - 1, dist + 4, cost + dc[1] + dc[2] - 6, current.x, current.y);
		}
		if ((freeMask & (8 | 4)) == (8 | 4) && IsWalkable(current.x + 1, current.y - 1)) {
			enqueueOrRelax(current.x + 1, current.y - 1, dist + 4, cost + dc[3] + dc[2] - 6, current.x, current.y);
		}
		if ((freeMask & (8 | 1)) == (8 | 1) && IsWalkable(current.x + 1, current.y + 1)) {
			enqueueOrRelax(current.x + 1, current.y + 1, dist + 4, cost + dc[3] + dc[0] - 6, current.x, current.y);
		}

		current.isClosed = true;
	}

	if (!goalNode) {
		return false;
	}

	std::vector<std::pair<int, int>> reversed;
	int walkX = dx;
	int walkY = dy;
	while (!(walkX == sx && walkY == sy)) {
		const auto it = nodes.find(NodeKey(walkX, walkY));
		if (it == nodes.end() || !it->second.hasPrev) {
			return false;
		}

		reversed.push_back({ walkX, walkY });
		walkX = it->second.prevX;
		walkY = it->second.prevY;
	}

	std::reverse(reversed.begin(), reversed.end());
	StampPathTimes(pathInfo, startTime, sx, sy, cellX, cellY, speedFactor, reversed);
	return !pathInfo->m_cells.empty();
}
