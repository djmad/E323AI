#include "Military.h"

CMilitary::CMilitary(AIClasses *ai) {
	this->ai = ai;
	currentGroup = 10;
	currentScout = 1;
}

void CMilitary::init(int unit) {
	createNewGroup(groups, currentGroup);
}

int CMilitary::selectTarget(float3 &ourPos, std::vector<int> &targets, std::vector<int> &occupied) {
	int target = -1;
	std::map<float, int> M;

	/* First sort the targets on distance */
	for (unsigned i = 0; i < targets.size(); i++) {
		int t = targets[i];
		float3 epos = ai->cheat->GetUnitPos(t);
		if (epos == NULLVECTOR) continue;
		float dist = (epos-ourPos).Length2D();
		M[dist] = t;
	}

	/* Select a non taken target */
	for (std::map<float,int>::iterator i = M.begin(); i != M.end(); i++) {
		target = i->second;
		bool isOccupied = false;
		for (unsigned j = 0; j < occupied.size(); j++) {
			if (target == occupied[j]) {
				isOccupied = true;
				break;
			}
		}
		if (isOccupied) continue;
		else break;
	}
	return target;
}

int CMilitary::selectHarrasTarget(int scout) {
	std::vector<int> occupiedTargets;
	ai->tasks->getMilitaryTasks(HARRAS, occupiedTargets);
	float3 pos = ai->call->GetUnitPos(scout);
	std::vector<int> targets;
	targets.insert(targets.end(), ai->intel->metalMakers.begin(), ai->intel->metalMakers.end());
	targets.insert(targets.end(), ai->intel->mobileBuilders.begin(), ai->intel->mobileBuilders.end());
	return selectTarget(pos, targets, occupiedTargets);
}

int CMilitary::selectAttackTarget(int group) {
	std::vector<int> occupiedTargets;
	ai->tasks->getMilitaryTasks(ATTACK, occupiedTargets);
	int target = -1;
	float3 pos = getGroupPos(group);

	/* Select an energyMaking target */
	target = selectTarget(pos, ai->intel->energyMakers, occupiedTargets);
	if (target != -1) return target;

	/* Select a mobile builder target */
	target = selectTarget(pos, ai->intel->mobileBuilders, occupiedTargets);
	if (target != -1) return target;

	/* Select a factory target */
	target = selectTarget(pos, ai->intel->factories, occupiedTargets);
	if (target != -1) return target;

	/* Select an offensive target */
	target = selectTarget(pos, ai->intel->attackers, occupiedTargets);
	if (target != -1) return target;

	return target;
}

void CMilitary::update(int frame) {
	/* Always have enough scouts */
	if (scouts.size() < ai->intel->metalMakers.size())
		ai->wl->push(SCOUT, NORMAL);

	/* Give idle groups a new attack plan */
	std::map<int, std::map<int, bool> >::iterator i;
	for (i = groups.begin(); i != groups.end(); i++) {
		bool isCurrentGroup  = (i->first == currentGroup);
		bool isBusy = (ai->tasks->militaryplans.find(i->first) != ai->tasks->militaryplans.end());

		if (isCurrentGroup || isBusy) continue;

		int target = selectAttackTarget(i->first);
		if (target != -1) {
			float3 goal = ai->cheat->GetUnitPos(target);
			float enemyStrength = ai->threatMap->getThreat(goal, 100.0f);

			/* If we can confront the enemy, do so */
			if (i->second.strength >= enemyStrength*1.2f) {
				/* Add the taskplan */
				ai->tasks->addMilitaryPlan(ATTACK, i->first, target);

				/* Bootstrap the path */
				float3 start = getGroupPos(i->first);
				ai->pf->addPath(i->first, start, goal); 
				break;
			}
		}
	}

	/* If we have a scout, harras the enemy */
	if (!scouts.empty()) {
		std::map<int, bool>::iterator s;
		int scout = -1;
		for (s = scouts.begin(); s != scouts.end(); s++)
			if (!s->second)
				scout = s->first;

		if (scout != -1) {
			int target = selectHarrasTarget(scout);
			if (target != -1) {
				ai->tasks->addMilitaryPlan(HARRAS, scout, target);
				float3 start = ai->call->GetUnitPos(scout);
				float3 goal  = ai->cheat->GetUnitPos(target);
				ai->pf->addPath(scout, start, goal);
				scouts[scout] = true;
			}
		}
	} else ai->wl->push(SCOUT, HIGH);
	
	/* If enemy offensive is within our perimeter, attack it */
	if (ai->intel->enemyInbound()) {
		/* Regroup our offensives such that we have enough power to engage */

		/* If we cannot get the power, put factories on HIGH priority for offensives */
	}

	/* Else pick a target, decide group power required and attack */
	else {
		/* Pick a target */
		if (groups[currentGroup].size() >= MINGROUPSIZE) {
			int target = selectAttackTarget(currentGroup);
			if (target != -1) {
				float3 goal = ai->cheat->GetUnitPos(target);
				float enemyStrength = ai->threatMap->getThreat(goal, 100.0f);

				/* If we can confront the enemy, do so */
				if (groups[currentGroup].strength >= enemyStrength*1.2f) {
					/* Add the taskplan */
					ai->tasks->addMilitaryPlan(ATTACK, currentGroup, target);

					/* Bootstrap the path */
					float3 start = groups[currentGroup].pos();
					ai->pf->addPath(currentGroup, start, goal); 

					/* Create new group */
					createNewGroup(groups, currentGroup);
				}
			}
		}
	}

	/* Always build some unit */
	ai->wl->push(randomUnit(), NORMAL);
}

void CMilitary::createNewGroup(std::map<int, CGroup> &groups, int &group) {
	CGroup G(ai, group);
	groups[group]  = G;
	group         += 10;
}

void CMilitary::addToGroup(int unit) {
	groups[currentGroup].add(unit);
	units[unit] = currentGroup;
}

void CMilitary::removeFromGroup(int unit) {
	int group = units[unit];
	groups[group].remove(unit);
	units.erase(unit);
	if (groups[group].empty() && group != currentGroup) {
		groups.erase(group);
		ai->tasks->militaryplans.erase(group);
		ai->pf->removePath(group);
	}
}

unsigned CMilitary::randomUnit() {
	float r = rng.RandFloat();
	if (r > 0.1 && r < 0.6)
		return MOBILE|ARTILLERY;
	else if(r >= 0.6)
		return MOBILE|ANTIAIR;
	else 
		return MOBILE|SCOUT;
}
