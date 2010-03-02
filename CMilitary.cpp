#include "CMilitary.h"

#include <limits>

#include "headers/HEngine.h"

#include "CRNG.h"
#include "CAI.h"
#include "CUnit.h"
#include "CGroup.h"
#include "CTaskHandler.h"
#include "CThreatMap.h"
#include "CIntel.h"
#include "CWishList.h"
#include "CUnitTable.h"
#include "CConfigParser.h"

CMilitary::CMilitary(AIClasses *ai): ARegistrar(200, std::string("military")) {
	this->ai = ai;
}

CMilitary::~CMilitary()
{
	for(int i = 0; i < groups.size(); i++)
		delete groups[i];	
}

void CMilitary::remove(ARegistrar &group) {
	LOG_II("CMilitary::remove group(" << group.key << ")")
	
	// NOTE: we do not destroy group to prevent unnecessary memory allocations
	free.push(lookup[group.key]); // remember index of free group
	
	lookup.erase(group.key);
	activeScoutGroups.erase(group.key);
	activeAttackGroups.erase(group.key);
	mergeScouts.erase(group.key);
	mergeGroups.erase(group.key);
	
	for (std::map<int,CGroup*>::iterator i = assemblingGroups.begin(); i != assemblingGroups.end(); i++) {
		if (i->second->key == group.key) {
			assemblingGroups.erase(group.key);
			break;
		}
	}
	
	// NOTE: CMilitary is registered inside group, so the next lines 
	// are senseless because records.size() == 0 always
	std::list<ARegistrar*>::iterator i;
	for (i = records.begin(); i != records.end(); i++)
		(*i)->remove(group);

	group.unreg(*this);
}

void CMilitary::addUnit(CUnit &unit) {
	LOG_II("CMilitary::addUnit " << unit)
	
	assert(unit.group == NULL);

	unsigned int c = unit.type->cats;

	if (c&MOBILE) {
		CGroup *group;
		if (c&SCOUTER) {
			/* A scout is initially alone */
			group = requestGroup(SCOUT);
		}
		else {
			/* If there is a new factory, or the current group is busy, request
			   a new group  */
			std::map<int,CGroup*>::iterator i = assemblingGroups.find(unit.builtBy);
			if (i == assemblingGroups.end()	|| i->second->busy) {
				group = requestGroup(ENGAGE);
				assemblingGroups[unit.builtBy] = group;
			} else {
				group = i->second;
			}
		}
		group->addUnit(unit);
	}
}

CGroup* CMilitary::requestGroup(groupType type) {
	int index = 0;
	CGroup *group = NULL;

	/* Create a new slot */
	if (free.empty()) {
		group = new CGroup(ai);
		groups.push_back(group);
		index = groups.size() - 1;
	}
	/* Use top free slot from stack */
	else {
		index = free.top(); free.pop();
		group = groups[index];
		group->reset();
	}

	lookup[group->key] = index;
	
	group->reg(*this);

	switch(type) {
		case SCOUT:
			activeScoutGroups[group->key] = group;
			break;
		case ENGAGE:
			activeAttackGroups[group->key] = group;
			break;
	}

	return group;
}

int CMilitary::selectTarget(float3 &ourPos, float radius, bool scout, std::vector<int> &targets) {
	int target = -1;
	float estimate = std::numeric_limits<float>::max();
	float factor = scout ? 10000 : 1;
	float unitDamageK;

	/* First sort the targets on distance */
	for (unsigned i = 0; i < targets.size(); i++) {
		int tempTarget = targets[i];
		const UnitDef *ud = ai->cbc->GetUnitDef(tempTarget);

		if (ud == NULL || ai->cbc->IsUnitCloaked(tempTarget))
			continue;

		float unitMaxHealth = ai->cbc->GetUnitMaxHealth(tempTarget);
		if (unitMaxHealth > EPS)
			unitDamageK = (unitMaxHealth - ai->cbc->GetUnitHealth(tempTarget)) / unitMaxHealth;
		else
			unitDamageK = 0.0f;
		
		const unsigned int ecats = UC(ud->id);
		float3 epos = ai->cbc->GetUnitPos(tempTarget);
		float dist = ourPos.distance2D(epos);
		dist += (factor * ai->threatmap->getThreat(epos, radius)) - unitDamageK * 150.0f;
		if (!scout) {
			if (ecats&SCOUTER) dist += 10000.0f;
		}
		
		if(dist < estimate) {
			estimate = dist;
			target = tempTarget;
		}
	}

	return target;
}

void CMilitary::prepareTargets(std::vector<int> &all, std::vector<int> &harass) {
	std::map<int, CTaskHandler::AttackTask*>::iterator j;
	std::vector<int> targets;

	occupiedTargets.clear();
	for (j = ai->tasks->activeAttackTasks.begin(); j != ai->tasks->activeAttackTasks.end(); j++)
		occupiedTargets.push_back(j->second->target);
	
	targets.insert(targets.end(), ai->intel->factories.begin(), ai->intel->factories.end());
	targets.insert(targets.end(), ai->intel->attackers.begin(), ai->intel->attackers.end());
	targets.insert(targets.end(), ai->intel->rest.begin(), ai->intel->rest.end());
	targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	if (targets.empty()) {
		targets.insert(targets.end(), ai->intel->energyMakers.begin(), ai->intel->energyMakers.end());
		targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
		targets.insert(targets.end(), ai->intel->restUnarmedUnits.begin(), ai->intel->restUnarmedUnits.end());
	}

	filterOccupiedTargets(targets, all);
	targets.clear();

	targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	targets.insert(targets.end(), ai->intel->energyMakers.begin(), ai->intel->energyMakers.end());
	targets.insert(targets.end(), ai->intel->factories.begin(), ai->intel->factories.end());
	targets.insert(targets.end(), ai->intel->restUnarmedUnits.begin(), ai->intel->restUnarmedUnits.end());
	
	filterOccupiedTargets(targets, harass);
	targets.clear();
}

void CMilitary::filterOccupiedTargets(std::vector<int> &source, std::vector<int> &dest) {
	for (size_t i = 0; i < source.size(); i++) {
		bool isOccupied = false;
		int target = source[i];
		
		for (size_t j = 0; j < occupiedTargets.size(); j++) {
			if (target == occupiedTargets[j]) {
				isOccupied = true;
				break;
			}
		}

		if (!isOccupied) 
			dest.push_back(target);
	}
}

void CMilitary::update(int frame) {
	int target = 0;

	std::vector<int> all, harras;
	
	prepareTargets(all, harras);

	std::map<int, CGroup*>::iterator i, k;
	
	/* Give idle scouting groups a new attack plan */
	for (i = activeScoutGroups.begin(); i != activeScoutGroups.end(); i++) {
		CGroup *group = i->second;

		/* This group is busy, don't bother */
		if (group->busy || !ai->unittable->canPerformTask(*group->firstUnit()))
			continue;

		// NOTE: if once target is not found it will never appear during
		// current loop execution
		if (target >= 0) {
			float3 pos = group->pos();
			target = selectTarget(pos, 300.0f, true, harras);
			/* There are no harras targets available */
			if (target < 0)
				target = selectTarget(pos, 300.0f, true, all);
		}

		/* Nothing available */
		if (target < 0) {
			if (group->units.size() < MAX_SCOUTS_IN_GROUP)
				mergeScouts[group->key] = group;
		}
		else {
			float3 tpos = ai->cbc->GetUnitPos(target);
			float threat = ai->threatmap->getThreat(tpos, 0.0f);
			if (threat < group->strength) {
				mergeScouts.erase(group->key);
				ai->tasks->addAttackTask(target, *group);
				break;
			}
			else {
				//LOG_II("CMilitary::update target at (" << tpos << ") is too dangerous (threat=" << threat << ") for scout Group(" << group->key << "):strength(" << group->strength << ")")
				if (group->units.size() < MAX_SCOUTS_IN_GROUP)
					mergeScouts[group->key] = group;
			}
		}

		// prevent merging of more than 2 groups
		/*
		if(mergeScouts.size() >= 2)
			break;
		*/
	}

	/* Merge the scout groups that were not strong enough */
	if (mergeScouts.size() >= 2) {
		std::map<int,CGroup*> merge;
		for (std::map<int,CGroup*>::iterator base = mergeScouts.begin(); base != mergeScouts.end(); base++) {
			for (std::map<int,CGroup*>::iterator compare = mergeScouts.begin(); compare != mergeScouts.end(); compare++) {
				if (base->second->key != compare->second->key) {
					if (base->second->pos().distance2D(compare->second->pos()) < 1000.0f) {
						merge[base->first] = base->second;
						merge[compare->first] = compare->second;
					}
				}
				if (!merge.empty())
					break;
			}
		}
		
		if (!merge.empty())
			ai->tasks->addMergeTask(merge);

		mergeScouts.clear();
	}

	/* Give idle, strong enough groups a new attack plan */
	for (i = activeAttackGroups.begin(); i != activeAttackGroups.end(); i++) {
		CGroup *group = i->second;

		/* This group is busy, don't bother */
		if (group->busy || !ai->unittable->canPerformTask(*group->firstUnit()))
			continue;

		/* Determine if this group is the assembling group */
		bool isAssembling = false;
		for (k = assemblingGroups.begin(); k != assemblingGroups.end(); k++) {
			if (k->second->key == group->key) {
				isAssembling = true;
				break;
			}
		}

		/* Select a target */
		float3 pos = group->pos();
		int target = selectTarget(pos, 0.0f, false, all);

		/* There are no targets available, assist an attack */
		if (target == -1) {
			// FIXME: there is a chance that group with one unit only
			// will go assisting attack task, which is LOL
			if (!ai->tasks->activeAttackTasks.empty()) {
				float minStrength = std::numeric_limits<float>::max();
				ATask* task = NULL;
				std::map<int, CTaskHandler::AttackTask*>::iterator i;
				for (i = ai->tasks->activeAttackTasks.begin(); i != ai->tasks->activeAttackTasks.end(); i++) {
					if (i->second->group->strength < minStrength) {
						task = i->second;
						minStrength = i->second->group->strength;
					}
				}
				if (task) {
					ai->tasks->addAssistTask(*task, *group);
					mergeGroups.erase(group->key);
				}
			}
			break;
		}

		/* Determine if the group has the strength */
		float3 targetpos    = ai->cbc->GetUnitPos(target);
		bool isStrongEnough = group->strength >= ai->threatmap->getThreat(targetpos, 0.0f);
		bool isSizeEnough   = group->units.size() >= ai->cfgparser->getMinGroupSize(group->techlvl);

		if (!isAssembling && !isStrongEnough)
			mergeGroups[group->key] = group;

		if ((isAssembling && isSizeEnough) || (!isAssembling && isStrongEnough)) {
			mergeGroups.erase(group->key);
			ai->tasks->addAttackTask(target, *group);
			break;
		}
	}

	/* Merge the groups that were not strong enough */
	if (mergeGroups.size() >= 2) {
		ai->tasks->addMergeTask(mergeGroups);
		mergeGroups.clear();
	}
	
	/* Always have enough scouts */
	if (activeScoutGroups.size() < ai->cfgparser->getMinScouts())
		// TODO: LAND cat should vary between LAND|SEA|AIR actually depending
		// on map type
		ai->wishlist->push(LAND | SCOUTER, HIGH);

	/* Always build some unit */
	ai->wishlist->push(requestUnit(), NORMAL);
}

unsigned CMilitary::requestUnit() {
	float r = rng.RandFloat();
	std::multimap<float, unitCategory>::iterator i;
	float sum = 0.0f;
	for (i = ai->intel->roulette.begin(); i != ai->intel->roulette.end(); i++) {
		sum += i->first;
		if (r <= sum) {
			// TODO: LAND cat should vary between LAND|SEA|AIR actually 
			// depending on map type
			return LAND | MOBILE | i->second;
		}
	}
	return LAND | MOBILE | ASSAULT; // unreachable code :)
}

int CMilitary::idleScoutGroupsNum() {
	int result = 0;
	std::map<int, CGroup*>::iterator i;
	for(i = activeScoutGroups.begin(); i != activeScoutGroups.end(); i++)
		if(!i->second->busy)
			result++;
	return result;
}
