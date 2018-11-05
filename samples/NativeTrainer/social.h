#pragma once

#include "../../inc/natives.h"
#include "../../inc/enums.h"
#include "../../inc/types.h"

#include <tuple>
#include <map>
#include <random>

#define MAXWAITPEDNUM 3

typedef struct boundingbox {
	std::pair<float, float> xlim;
	std::pair<float, float> ylim;
	std::pair<float, float> zlim;
	Vector3 bbPt1;
	Vector3 bbPt2;
}BBX;

typedef struct waitpoint {
	Vector3 coord;
	bool occupy;
}WPT;

typedef struct waitregion {
	WPT waitPts[MAXWAITPEDNUM];
	Vector3 offBusPt;
	float waitHeading;

	int getForwardIdx()
	{
		for (int i = 0; i < MAXWAITPEDNUM; i++)
			if (!waitPts[i].occupy)
				return i;
		return MAXWAITPEDNUM;
	}
}WR;

// Social Bus
class SocialBus
{
public:
	SocialBus(std::vector<std::tuple<Vector3, Vector3, Vector3> > &busRoutePtTuples, std::map<Vector3, WPT*> &busStopPt2WPTPtrMap, std::map<Vector3, float> &busStartPt2HeadingMap, std::map<Vector3, float> &busExitPt2ExitRadiusMap, Vector3 centerPt);
	~SocialBus();

	bool TaskScheduler();

	Entity GetEntity() const;
	std::tuple<Vector3, Vector3, Vector3> GetRoutePts() const;
	int GetAvailableSeatID() const;
	UINT GetStateIndicator() const;

private:
	void PropertySample(std::vector<std::tuple<Vector3, Vector3, Vector3> > &busRoutePtTuples);

	// Event Utlities
	// Event Transition 
	// (1->2), (2->3), 0(dead)
	// 1: bus drives from start point to stop point
	void EventOneStart();
	bool EventOneEnd();

	// 2: bus stops at stop point
	void EventTwoStart();
	bool EventTwoEnd();

	// 3: bus drives from stop point to end point
	void EventThreeStart();
	bool EventThreeEnd();

	WPT* pedWaitPtPtr;
	Vector3 startPt;
	Vector3 stopPt;
	Vector3 endPt;
	Vector3 centerPt;
	float exitRadius;
	const float closeDist = 8.0;
	const DWORD stopDuration = 60000;
	const DWORD driveDurationMax = 600000;
	const UINT numPassengerMax = 3;

	// Task Utilites
	DWORD currentTickCount;
	UINT stateIndicator;
	Vehicle bus;
	Ped driver;
};

// Social Passenger
class SocialPassenger
{
public:
	SocialPassenger(SocialBus* socialBus, std::vector<Vector3> &streetPivots, Vector3 centerPt);
	~SocialPassenger();

	bool TaskScheduler();

	UINT GetStateIndicator() const;

private:
	// Event Utilities
	// Event Transition
	// (1->2), (2->3), 0(dead)
	// 1: warp passenger into bus seat
	void EventOneStart();
	bool EventOneEnd();

	// 2: passenger gets off bus and wander
	void EventTwoStart();
	bool EventTwoEnd();

	Vector3 centerPt;
	const float busCloseDist = 8.0;
	const float closeDist = 0.1;
	const DWORD busDurationMax = 600000;
	const DWORD durationMax = 1200000;

	// Task Utilites
	DWORD currentTickCount;
	UINT stateIndicator;
	std::vector<Vector3> streetPivots;
	SocialBus* socialBus;
	Ped passenger;
};

// Social WaitPed
class SocialWaitPed
{
public:
	SocialWaitPed(std::map<Vector3, WR*> &Vec2WRPtrMap, std::vector<Vector3> &pedStartPts, std::vector<Vector3> &streetPivots, Vector3 centerPt);
	~SocialWaitPed();

	bool TaskScheduler();

	Entity GetEntity() const;
	UINT GetStateIndicator() const;
	void GetAvailableSocialBus(std::vector<SocialBus> &socialBusArr);

private:
	void PropertySample(std::vector<Vector3> &pedStartPts);

	// Event Utilites
	// Event Transition
	// (1->2), (2->3), (3->4), 0(dead)
	// 1: wait ped goes from start point to wait region
	void EventOneStart();
	bool EventOneEnd();

	// 2: wait ped moves to last stop point in wait region
	void EventTwoStart();
	bool EventTwoEnd();

	// 3: wait ped gets on stopped bus
	void EventThreeStart(int seatID);
	bool EventThreeEnd();

	// 4: wait ped in bus moves to bus end point
	void EventFourStart();
	bool EventFourEnd();

	WR* waitRegionPtr;
	Vector3 startPt;
	Vector3 centerPt;
	const float closeDist = 0.1;
	const float closeRegionDist = 10.0;
	const float busDurationMax = 600000;
	const float durationMax = 1200000;
	const float waitDurationMax = 7200000;

	// Task Utilities
	DWORD currentTickCount;
	UINT stateIndicator;
	std::vector<Vector3> streetPivots;
	WR* waitRegion;
	int currentPtIdx;
	bool isRun;
	SocialBus* socialBus;
	Ped waitPed;
};

//Global Scene
class GlobalScene {
public:
	GlobalScene();
	~GlobalScene();

	//Social Bus
	void SceneAddSocialBus(SocialBus socialBus);
};