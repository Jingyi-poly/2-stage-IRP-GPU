#include "irp/model/Vehicle.h"

Vehicle::Vehicle(int depotNumber,double maxRouteTime,double vehicleCapacity): //specificDepotnumber/signal,MaximumrowTimeandVehicleCapacity.
depotNumber(depotNumber), maxRouteTime(maxRouteTime), vehicleCapacity(vehicleCapacity)
{}
Vehicle::~Vehicle(void){}