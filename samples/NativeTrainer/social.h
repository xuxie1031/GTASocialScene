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
}BBX;

typedef struct waitpoint {
	Vector3 coord;
	bool occupy;
}WPT;


// Social Bus
class SocialBus
{
public:
	SocialBus(std::vector<std::tuple<Vector3, Vector3, Vector3> > &busRoutePtTuples, std::map<Vector3, WPT*> &Vec2WPTPtrMap, std::map<Vector3, float> &Vec2HeadingMap, std::map<Vector3, float> &Vec2ExitRadiusMap, Vector3 centerPt);
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
	SocialPassenger(SocialBus* socialBus, std::map<Vector3, Vector3> &offBusPtMap, std::vector<Vector3> &pedAccessPts, std::vector<Vector3> &streetPivots, Vector3 centerPt);
	~SocialPassenger();

	bool TaskScheduler();

	UINT GetStateIndicator() const;

private:
	void PropertySample(std::vector<Vector3> &pedAccessPts);

	// Event Utilities
	// Event Transition
	// (1->2), (2->3), 0(dead)
	// 1: warp passenger into bus seat
	void EventOneStart();
	bool EventOneEnd();

	// 2: passenger gets off bus to stop area
	void EventTwoStart();
	bool EventTwoEnd();

	// 3: passenger goes to end point
	void EventThreeStart();
	bool EventThreeEnd();

	Vector3 offBusPt;
	Vector3 endPt;
	Vector3 centerPt;
	const float busCloseDist = 8.0;
	const float closeDist = 0.1;
	const float exitRadius = 1000.0;
	const DWORD busDurationMax = 600000;
	const DWORD durationMax = 1200000;

	// Task Utilites
	DWORD currentTickCount;
	UINT stateIndicator;
	std::vector<Vector3> streetPivots;
	SocialBus* socialBus;
	Ped passenger;
};