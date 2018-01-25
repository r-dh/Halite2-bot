#include "mission.hpp"


namespace rdh {

	class Mission {
	private:
		int maxSquadSize;
		MissionType mType;
		hlt::Planet targetPlaneet;
		hlt::Ship targetShip;

	public:
		Mission() {
			mType = MissionType::NONE;
		}

		Mission(MissionType type, int SquadSize, hlt::Planet planet) {
			maxSquadSize = SquadSize;
			mType = type;
			targetPlaneet = planet;
		}

		Mission(MissionType type, int SquadSize, hlt::Ship ship) {
			maxSquadSize = SquadSize;
			mType = type;
			targetShip = ship;
		}

		//Main logic will assign their ships according to mission priority and location
		int getSquadSize() { 
			return maxSquadSize;
		}

		MissionType getType() {
			return mType;
		}

		hlt::Location targetLocation() {
			switch (mType) {
			case rdh::GUARD:
 
				break;
			case rdh::ATTACK:
				break;
			case rdh::COLONIZE:
				break;
			default:
				break;
			}
		}


	};

}