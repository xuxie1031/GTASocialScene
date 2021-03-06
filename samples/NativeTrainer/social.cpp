#include "social.h"

std::vector<SocialBus> socialBusArr;

static int argMinStreetPivots(std::vector<Vector3> &streetPivots, Vector3 pt)
{
	int idx = -1;
	float minDist = 1e9;
	for (int i = 0; i < streetPivots.size(); i++)
	{
		float dist = SYSTEM::VDIST(pt.x, pt.y, pt.z, streetPivots[i].x, streetPivots[i].y, streetPivots[i].z);
		if (dist < minDist)
		{
			minDist = dist;
			idx = i;
		}
	}
	return idx;
}


/* Social Bus */
SocialBus::SocialBus(std::vector<std::tuple<Vector3, Vector3, Vector3> > &busRoutePtsTuples, std::map<Vector3, WPT*> &busStopPt2WPTPtrMap, std::map<Vector3, float> &busStartPt2HeadingMap, std::map<Vector3, float> &busEndPt2ExitRadiusMap, Vector3 centerPt) :
	centerPt(centerPt)
{
	PropertySample(busRoutePtsTuples);
	pedWaitPtPtr = busStopPt2WPTPtrMap[stopPt];
	float startHeading = busStartPt2HeadingMap[startPt];
	exitRadius = busEndPt2ExitRadiusMap[endPt];

	// create bus
	Hash busModel = GAMEPLAY::GET_HASH_KEY("BUS");
	if (STREAMING::IS_MODEL_IN_CDIMAGE(busModel) && STREAMING::IS_MODEL_VALID(busModel))
	{
		STREAMING::REQUEST_MODEL(busModel);
		while (!STREAMING::HAS_MODEL_LOADED(busModel))	WAIT(0);
		WAIT(100);
	}
	bus = VEHICLE::CREATE_VEHICLE(busModel, startPt.x, startPt.y, startPt.z, startHeading, false, true);

	// create bus driver
	driver = PED::CREATE_RANDOM_PED_AS_DRIVER(bus, true);

	EventOneStart();
}

SocialBus::~SocialBus()
{
	PED::DELETE_PED(&driver);
	VEHICLE::DELETE_VEHICLE(&bus);
}

Entity SocialBus::GetEntity() const
{
	return bus;
}

std::tuple<Vector3, Vector3, Vector3> SocialBus::GetRoutePts() const
{
	return std::make_tuple(startPt, stopPt, endPt);
}

int SocialBus::GetAvailableSeatID() const
{
	std::vector<int> seatIDs{ 0, 1, 3 };

	for (int i = 0; i < seatIDs.size(); i++)
		if (VEHICLE::IS_VEHICLE_SEAT_FREE(bus, i))
			return seatIDs[i];

	return -1;
}

UINT SocialBus::GetStateIndicator() const
{
	return stateIndicator;
}

bool SocialBus::TaskScheduler()
{
	bool transit = false;
	switch (stateIndicator)
	{
	case 1:
		transit = EventOneEnd();
		break;
	case 2:
		transit = EventTwoEnd();
		break;
	case 3:
		transit = EventThreeEnd();
		break;
	default:
		break;
	}

	return transit;
}

void SocialBus::PropertySample(std::vector<std::tuple<Vector3, Vector3, Vector3> > &busRoutePtsTuples)
{
	std::random_device rd;

	std::mt19937 genBusRoutePtsTuplesIdx(rd());
	std::uniform_int_distribution<int> distributionBusRoutePtsTuplesIdx(0, busRoutePtsTuples.size() - 1);

	std::tie(startPt, stopPt, endPt) = busRoutePtsTuples[distributionBusRoutePtsTuplesIdx(genBusRoutePtsTuplesIdx)];
}

void SocialBus::EventOneStart()
{
	AI::TASK_VEHICLE_DRIVE_TO_COORD(driver, bus, stopPt.x, stopPt.y, stopPt.z, 10.0, 1, ENTITY::GET_ENTITY_MODEL(bus), 786603, 0.5, true);
	currentTickCount = GetTickCount();
	stateIndicator = 1;
}

bool SocialBus::EventOneEnd()
{
	Vector3 busPose = ENTITY::GET_ENTITY_COORDS(bus, true);
	if (SYSTEM::VDIST(busPose.x, busPose.y, busPose.z, stopPt.x, stopPt.y, stopPt.z) < closeDist && VEHICLE::IS_VEHICLE_STOPPED(bus) && VEHICLE::GET_VEHICLE_NUMBER_OF_PASSENGERS(bus) == 0)
	{
		EventTwoStart();
		return true;
	}

	if (GetTickCount() - currentTickCount > driveDurationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}

void SocialBus::EventTwoStart()
{
	currentTickCount = GetTickCount();
	stateIndicator = 2;
}

bool SocialBus::EventTwoEnd()
{
	if (GetTickCount() - currentTickCount > stopDuration)
	{
		if (VEHICLE::GET_VEHICLE_NUMBER_OF_PASSENGERS(bus) >= numPassengerMax || !(pedWaitPtPtr->occupy))
		{
			EventThreeStart();
			return true;
		}
	}

	return false;
}

void SocialBus::EventThreeStart()
{
	AI::TASK_VEHICLE_DRIVE_TO_COORD(driver, bus, endPt.x, endPt.y, endPt.z, 10.0, 1, ENTITY::GET_ENTITY_MODEL(bus), 786603, 0.5, true);
	currentTickCount = GetTickCount();
	stateIndicator = 3;
}

bool SocialBus::EventThreeEnd()
{
	Vector3 busPose = ENTITY::GET_ENTITY_COORDS(bus, true);
	if (SYSTEM::VDIST(busPose.x, busPose.y, busPose.z, centerPt.x, centerPt.y, centerPt.z) > exitRadius)
	{
		stateIndicator = 0;
		return true;
	}

	if (GetTickCount() - currentTickCount > driveDurationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}

/* Social Passenger */
SocialPassenger::SocialPassenger(SocialBus* socialBus, std::vector<Vector3> &streetPivots, Vector3 centerPt) :
	socialBus(socialBus), centerPt(centerPt)
{
	this->streetPivots = streetPivots;
	Vehicle bus = socialBus->GetEntity();

	// create passenger
	int seatId = socialBus->GetAvailableSeatID();
	Hash passengerModel = GAMEPLAY::GET_HASH_KEY("player_zero");
	passenger = PED::CREATE_PED_INSIDE_VEHICLE(bus, 0, passengerModel, seatId, false, true);

	EventOneStart();
}

SocialPassenger::~SocialPassenger()
{
	PED::DELETE_PED(&passenger);
}

UINT SocialPassenger::GetStateIndicator() const
{
	return stateIndicator;
}

bool SocialPassenger::TaskScheduler()
{
	bool transit = false;
	switch (stateIndicator)
	{
	case 1:
		transit = EventOneEnd();
		break;
	case 2:
		transit = EventTwoEnd();
		break;
	default:
		break;
	}

	return transit;
}

void SocialPassenger::EventOneStart()
{
	currentTickCount = GetTickCount();
	stateIndicator = 1;
}

bool SocialPassenger::EventOneEnd()
{
	Vehicle bus = socialBus->GetEntity();
	Vector3 busStartPt, busStopPt, busEndPt;
	std::tie(busStartPt, busStopPt, busEndPt) = socialBus->GetRoutePts();
	Vector3 busPose = ENTITY::GET_ENTITY_COORDS(bus, true);
	if (SYSTEM::VDIST(busPose.x, busPose.y, busPose.z, busStopPt.x, busStopPt.y, busStopPt.z) < busCloseDist && VEHICLE::IS_VEHICLE_STOPPED(bus))
	{
		EventTwoStart();
		return true;
	}

	if (GetTickCount() - currentTickCount > busDurationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}

void SocialPassenger::EventTwoStart()
{
	AI::TASK_WANDER_STANDARD(passenger, 10.0, 10);
	stateIndicator = 2;
}

bool SocialPassenger::EventTwoEnd()
{
	Vector3 passengerPose = ENTITY::GET_ENTITY_COORDS(passenger, true);
	int streetID = argMinStreetPivots(streetPivots, passengerPose);
	if (SYSTEM::VDIST(passengerPose.x, passengerPose.y, passengerPose.z, centerPt.x, centerPt.y, centerPt.z) > SYSTEM::VDIST(streetPivots[streetID].x, streetPivots[streetID].y, streetPivots[streetID].z, centerPt.x, centerPt.y, centerPt.z))
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}

/* Social Wait Ped */
SocialWaitPed::SocialWaitPed(std::map<Vector3, WR*> &pedStartPt2WRPtrMap, std::vector<Vector3> &pedStartPts, std::vector<Vector3> &streetPivots, Vector3 centerPt) :
	centerPt(centerPt)
{
	PropertySample(pedStartPts);
	waitRegion = pedStartPt2WRPtrMap[startPt];
	this->streetPivots = streetPivots;
	currentPtIdx = -1;
	isRun = false;

	// create wait ped
	Hash waitPedModel = GAMEPLAY::GET_HASH_KEY("player_zero");
	waitPed = PED::CREATE_PED(0, waitPedModel, startPt.x, startPt.y, startPt.z, 0, false, true);
	socialBus = NULL;

	EventOneStart();
}

SocialWaitPed::~SocialWaitPed()
{
	PED::DELETE_PED(&waitPed);
}

void SocialWaitPed::PropertySample(std::vector<Vector3> &pedStartPts)
{
	std::random_device rd;

	std::mt19937 genPedStartPtsIdx(rd());
	std::uniform_int_distribution<int> distributionPedStartPtsIdx(0, pedStartPts.size() - 1);
	startPt = pedStartPts[distributionPedStartPtsIdx(genPedStartPtsIdx)];
}

Entity SocialWaitPed::GetEntity() const
{
	return waitPed;
}

UINT SocialWaitPed::GetStateIndicator() const
{
	return stateIndicator;
}

void SocialWaitPed::GetAvailableSocialBus(std::vector<SocialBus> &socialBusArr)
{
	Vector3 waitPedPose = ENTITY::GET_ENTITY_COORDS(waitPed, true);
	for (int i = 0; i < socialBusArr.size(); i++)
	{
		Hash busModel = GAMEPLAY::GET_HASH_KEY("BUS");
		Vehicle closeBus = VEHICLE::GET_CLOSEST_VEHICLE(waitPedPose.x, waitPedPose.y, waitPedPose.z, 50.0, busModel, 0); // need to test validation
		if (socialBusArr[i].GetEntity() == closeBus)
		{
			socialBus = &socialBusArr[i];
		}
	}
}

bool SocialWaitPed::TaskScheduler()
{
	bool transit = false;

	switch (stateIndicator)
	{
	case 1:
	{
		GetAvailableSocialBus(socialBusArr);
		if (!isRun && socialBus && VEHICLE::IS_VEHICLE_STOPPED(socialBus->GetEntity()))
		{
			AI::TASK_GO_STRAIGHT_TO_COORD(waitPed, waitRegion->offBusPt.x, waitRegion->offBusPt.y, waitRegion->offBusPt.z, 2.5, -1, waitRegion->waitHeading, 0.0);
			isRun = true;
		}

		transit = EventOneEnd();
		break;
	}
	case 2:
	{
		int forwardIdx = waitRegion->getForwardIdx();
		if (currentPtIdx > forwardIdx && !waitRegion->waitPts[forwardIdx].occupy)
		{
			waitRegion->waitPts[forwardIdx].occupy = true;
			AI::TASK_GO_STRAIGHT_TO_COORD(waitPed, waitRegion->waitPts[forwardIdx].coord.x, waitRegion->waitPts[forwardIdx].coord.y, waitRegion->waitPts[forwardIdx].coord.z, 1.0, -1, waitRegion->waitHeading, 0.0);
			waitRegion->waitPts[currentPtIdx].occupy = false;
			currentPtIdx = forwardIdx;
		}

		transit = EventTwoEnd();
		break;
	}
	case 3:
	{
		transit = EventThreeEnd();
		break;
	}
	default:
		break;
	}

	return transit;
}

void SocialWaitPed::EventOneStart()
{
	AI::TASK_GO_STRAIGHT_TO_COORD(waitPed, waitRegion->offBusPt.x, waitRegion->offBusPt.y, waitRegion->offBusPt.z, 1.0, -1, waitRegion->waitHeading, 0.0);
	currentTickCount = GetTickCount();
	stateIndicator = 1;
}

bool SocialWaitPed::EventOneEnd()
{
	Vector3 waitPedPose = ENTITY::GET_ENTITY_COORDS(waitPed, true);
	if (SYSTEM::VDIST(waitPedPose.x, waitPedPose.y, waitPedPose.z, waitRegion->offBusPt.x, waitRegion->offBusPt.y, waitRegion->offBusPt.z) < closeRegionDist)
	{
		EventTwoStart();
		return true;
	}

	if (GetTickCount() - currentTickCount > durationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}

void SocialWaitPed::EventTwoStart()
{
	int forwardIdx = waitRegion->getForwardIdx();
	currentPtIdx = forwardIdx;
	waitRegion->waitPts[currentPtIdx].occupy = true;
	AI::TASK_GO_STRAIGHT_TO_COORD(waitPed, waitRegion->waitPts[currentPtIdx].coord.x, waitRegion->waitPts[currentPtIdx].coord.y, waitRegion->waitPts[currentPtIdx].coord.z, 1.0, -1, waitRegion->waitHeading, 0.2);
	currentTickCount = GetTickCount();
	stateIndicator = 2;
}

bool SocialWaitPed::EventTwoEnd()
{
	if (currentPtIdx == 0)
	{
		if (socialBus && VEHICLE::IS_VEHICLE_STOPPED(socialBus->GetEntity()))
		{
			int seatID = socialBus->GetAvailableSeatID();
			if (seatID >= 0)
			{
				waitRegion->waitPts[0].occupy = false;
				EventThreeStart(seatID);
				return true;
			}
		}
	}

	if (GetTickCount() - currentTickCount > waitDurationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}

void SocialWaitPed::EventThreeStart(int seatID)
{
	AI::TASK_ENTER_VEHICLE(waitPed, socialBus->GetEntity(), -1, seatID, 1.0, 1, 0);
	currentTickCount = GetTickCount();
	stateIndicator = 3;
}

bool SocialWaitPed::EventThreeEnd()
{
	if (PED::IS_PED_IN_VEHICLE(waitPed, socialBus->GetEntity(), false))
	{
		EventFourStart();
		return true;
	}

	if (GetTickCount() - currentTickCount > durationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}

void SocialWaitPed::EventFourStart()
{
	stateIndicator = 4;
}

bool SocialWaitPed::EventFourEnd()
{
	if(!PED::IS_PED_IN_ANY_VEHICLE(waitPed, false))		// bus entity not exist anymore
	{
		stateIndicator = 0;
		return true;
	}

	if (GetTickCount() - currentTickCount > busDurationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}