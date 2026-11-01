#pragma once

#include "GameActor.h"
#include "World.h"

void RenderFrozenActorEffectPass(CMsgEffect& effect, matrix* viewMatrix,
    const CWorld::BillboardScreenEntry& owner);

bool QueueQueuedMsgEffectsQuads();
bool GetQueuedMsgEffectsBounds(RECT* outRect);
void ClearQueuedMsgEffects();
bool HasQueuedMsgEffects();
bool HasActiveMsgEffects();
bool RenderAttachedActorEffectPass(CMsgEffect& effect,
	matrix* viewMatrix,
	float ownerDepthKey,
	float ownerScreenY,
	bool afterOwner,
	u32 frameId);
