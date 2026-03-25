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

int StepCost(int fromX, int fromY, int toX, int toY)
{
	return (fromX != toX && fromY != toY) ? 14 : 10;
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
	(void)dx;
	(void)dy;

	if (!pathInfo) {
		return false;
	}
	pathInfo->Reset();

	if (!m_map || !IsWalkable(sx, sy) || !IsWalkable(cellX, cellY)) {
		return false;
	}
	if (sx == cellX && sy == cellY) {
		pathInfo->m_cells.push_back(PathCell{ sx, sy, startTime });
		return true;
	}

	std::vector<std::pair<int, int>> directPath;
	{
		int stepX = Sign(cellX - sx);
		int stepY = Sign(cellY - sy);
		int walkX = sx;
		int walkY = sy;
		bool directPathValid = true;

		while (walkX != cellX || walkY != cellY) {
			walkX += stepX;
			walkY += stepY;
			if (walkX == cellX) {
				stepX = 0;
			}
			if (walkY == cellY) {
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
		pathInfo->m_cells.push_back(PathCell{ sx, sy, startTime });
		unsigned int currentTime = startTime;
		for (const auto& step : directPath) {
			currentTime += static_cast<unsigned int>((StepCost(sx, sy, step.first, step.second) * speedFactor) / 10);
			pathInfo->m_cells.push_back(PathCell{ step.first, step.second, currentTime });
			sx = step.first;
			sy = step.second;
		}
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

	enqueueOrRelax(sx, sy, 0, 10 * ((std::abs)(cellX - sx) + (std::abs)(cellY - sy)), sx, sy);

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

		if (current.x == cellX && current.y == cellY) {
			goalNode = &current;
			break;
		}

		const int dist = current.dist + 10;
		const int cost = current.cost;
		int dc[4] = { 0, 0, 0, 0 };
		int freeMask = 0;

		if (current.y + 1 < m_map->m_height && IsWalkable(current.x, current.y + 1)) {
			freeMask |= 1;
			dc[0] = (current.y >= cellY) ? 20 : 0;
			enqueueOrRelax(current.x, current.y + 1, dist, cost + dc[0], current.x, current.y);
		}
		if (current.x - 1 >= 0 && IsWalkable(current.x - 1, current.y)) {
			freeMask |= 2;
			dc[1] = (current.x <= cellX) ? 20 : 0;
			enqueueOrRelax(current.x - 1, current.y, dist, cost + dc[1], current.x, current.y);
		}
		if (current.y - 1 >= 0 && IsWalkable(current.x, current.y - 1)) {
			freeMask |= 4;
			dc[2] = (current.y <= cellY) ? 20 : 0;
			enqueueOrRelax(current.x, current.y - 1, dist, cost + dc[2], current.x, current.y);
		}
		if (current.x + 1 < m_map->m_width && IsWalkable(current.x + 1, current.y)) {
			freeMask |= 8;
			dc[3] = (current.x >= cellX) ? 20 : 0;
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
	int walkX = cellX;
	int walkY = cellY;
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
	pathInfo->m_cells.push_back(PathCell{ sx, sy, startTime });
	unsigned int currentTime = startTime;
	int prevX = sx;
	int prevY = sy;
	for (const auto& step : reversed) {
		const unsigned int stepTime = static_cast<unsigned int>((StepCost(prevX, prevY, step.first, step.second) * speedFactor) / 10);
		currentTime += stepTime;
		pathInfo->m_cells.push_back(PathCell{ step.first, step.second, currentTime });
		prevX = step.first;
		prevY = step.second;
	}
	return true;
}
