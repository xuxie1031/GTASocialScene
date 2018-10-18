#include "social.h"

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
SocialBus::SocialBus(std::vector<std::tuple<Vector3, Vector3, Vector3> > &busRoutePtsTuples, std::map<Vector3, WPT*> &Vec2WPTPtrMap, std::map<Vector3, float> &Vec2HeadingMap, std::map<Vector3, float> &Vec2ExitRadiusMap, Vector3 centerPt) :
	centerPt(centerPt)
{
	PropertySample(busRoutePtsTuples);
	pedWaitPtPtr = Vec2WPTPtrMap[stopPt];
	float startHeading = Vec2HeadingMap[startPt];
	exitRadius = Vec2ExitRadiusMap[endPt];

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
	Hash driverModel = GAMEPLAY::GET_HASH_KEY("player_zero");
	if (STREAMING::IS_MODEL_IN_CDIMAGE(driverModel) && STREAMING::IS_MODEL_VALID(driverModel))
	{
		STREAMING::REQUEST_MODEL(driverModel);
		while (!STREAMING::HAS_MODEL_LOADED(driverModel))	WAIT(0);
		WAIT(100);
	}
	driver = PED::CREATE_PED_INSIDE_VEHICLE(bus, 0, driverModel, -1, false, true);

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
	if (SYSTEM::VDIST(busPose.x, busPose.y, busPose.z, stopPt.x, stopPt.y, stopPt.z) < closeDist && VEHICLE::IS_VEHICLE_STOPPED(bus))
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
SocialPassenger::SocialPassenger(SocialBus* socialBus, std::map<Vector3, Vector3> &offBusPtMap, std::vector<Vector3> &pedAccessPts, std::vector<Vector3> &streetPivots, Vector3 centerPt):
	socialBus(socialBus), centerPt(centerPt)
{
	PropertySample(pedAccessPts);
	this->streetPivots = streetPivots;

	Vehicle bus = socialBus->GetEntity();
	Vector3 busStartPt, busStopPt, busEndPt;
	std::tie(busStartPt, busStopPt, busEndPt) = socialBus->GetRoutePts();
	offBusPt = offBusPtMap[busStopPt];

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

void SocialPassenger::PropertySample(std::vector<Vector3> &pedAccessPts)
{
	std::random_device rd;

	std::mt19937 genPedAccessPtsIdx(rd());
	std::uniform_int_distribution<int> distributionPedAccessPtsIdx(0, pedAccessPts.size() - 1);
	endPt = pedAccessPts[distributionPedAccessPtsIdx(genPedAccessPtsIdx)];
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
	case 3:
		transit = EventThreeEnd();
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
	AI::TASK_FOLLOW_NAV_MESH_TO_COORD(passenger, offBusPt.x, offBusPt.y, offBusPt.z, 1.0, -1, 0.1, false, 0);
	currentTickCount = GetTickCount();
	stateIndicator = 2;
}

bool SocialPassenger::EventTwoEnd()
{
	Vector3 passengerPose = ENTITY::GET_ENTITY_COORDS(passenger, true);
	if (SYSTEM::VDIST(passengerPose.x, passengerPose.y, passengerPose.z, offBusPt.x, offBusPt.y, offBusPt.z) < closeDist)
	{
		EventThreeStart();
		return true;
	}

	if (GetTickCount() - currentTickCount > durationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}

void SocialPassenger::EventThreeStart()
{
	AI::TASK_WANDER_STANDARD(passenger, 10.0, 10);
	currentTickCount = GetTickCount();
	stateIndicator = 3;
}

bool SocialPassenger::EventThreeEnd()
{
	Vector3 passengerPose = ENTITY::GET_ENTITY_COORDS(passenger, true);
	int streetID = argMinStreetPivots(streetPivots, passengerPose);
	if (SYSTEM::VDIST(passengerPose.x, passengerPose.y, passengerPose.z, centerPt.x, centerPt.y, centerPt.z) > SYSTEM::VDIST(streetPivots[streetID].x, streetPivots[streetID].y, streetPivots[streetID].z, centerPt.x, centerPt.y, centerPt.z))
	{
		stateIndicator = 0;
		return true;
	}

	if (GetTickCount() - currentTickCount > durationMax)
	{
		stateIndicator = 0;
		return true;
	}

	return false;
}