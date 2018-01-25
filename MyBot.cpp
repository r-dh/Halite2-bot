#include "hlt/hlt.hpp"
#include "hlt/navigation.hpp"
//#include "hlt/mission.hpp"
#include <vector>

# define M_PI           3.14159265358979323846
//TODO: make ships NOT crash into eachother early game when docking planet
//Separation https://www.youtube.com/watch?v=7HVVBONfLuE&t=130s

//findNearestPlanet must be split and refactored, too many parameters

enum MissionType {
	GUARD,
	ATTACK,
	COLONIZE,
	NONE
};

struct TargetPlanet {
	hlt::Planet target;
	double distance;
	TargetPlanet(hlt::Planet p, double d) : target(p), distance(d) {}
};
struct TargetShip {
	hlt::Ship target;
	double distance;
	TargetShip(hlt::Ship s, double d) : target(s), distance(d) {}
};

struct myShip {
	hlt::Ship ship;
	//rdh::Mission mission;
};

hlt::Planet findNearestPlanet(const hlt::Map &map, const hlt::Location location, const unsigned int notFromPlayer, bool otherPlanets = true, bool onlyFreePlanets = false) {

	TargetPlanet nearestPlanet(map.planets.at(0), 9999);

	for (const hlt::Planet& planet : map.planets) {
		double distance = location.get_distance_to(planet.location);
		/*std::ostringstream logger;
		logger << "Processing planet [" << nearestPlanet.target.entity_id << "] distance: " << distance;
		hlt::Log::log(logger.str());*/

		if (onlyFreePlanets && planet.owned) continue;
		if (otherPlanets && planet.is_full() && planet.owned && planet.owner_id == notFromPlayer) continue; //planet.is_full() is a quickfix

		//MAJOR CHANGE: favor planets 10% further if they have more docks
		//ORIGINAL VERSION: distance < nearestPlanet.distance or 
		if (distance <= (nearestPlanet.distance*1.1) && planet.docking_spots > nearestPlanet.target.docking_spots) {
			nearestPlanet = TargetPlanet(planet, distance);
		} else if (distance < nearestPlanet.distance) {
			nearestPlanet = TargetPlanet(planet, distance);
		}

	}

	std::ostringstream logger;
	logger << "Nearest planet found [" << nearestPlanet.target.entity_id << "]";
	hlt::Log::log(logger.str());

	//if (nearestPlanet.distance == 9999) return nullptr;
	return nearestPlanet.target;
}

void logShipAndPlanetInfo(const hlt::Ship ship, const hlt::Planet planet) {
	std::ostringstream logger;
	logger << "Ship [" << ship.entity_id << "] at " << ship.location << " versus planet ["
		<< planet.entity_id << "] at " << planet.location << " at distance " << ship.location.get_distance_to(planet.location);
	hlt::Log::log(logger.str());
}
//For some reason this doesn't compile on the halite servers
void moveShip(const hlt::Map &map, const hlt::Ship &ship, const hlt::Location &location, std::vector<hlt::Move> &moves) {
	const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(map, ship, location,
		hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, M_PI / 180.0);

	if (move.second) {
		moves.push_back(move.first);
	}
}

void dockShip(const hlt::EntityId shipId, const hlt::EntityId planetId, std::vector<hlt::Move> &moves) {
	moves.push_back(hlt::Move::dock(shipId, planetId));
}

int main() {
	const hlt::Metadata metadata = hlt::initialize("Eiron_v4.1");
	const hlt::PlayerId player_id = metadata.player_id;

	const hlt::Map& initial_map = metadata.initial_map;

	// We now have 1 full minute to analyse the initial map.
	std::ostringstream initial_map_intelligence;
	initial_map_intelligence
		<< "width: " << initial_map.map_width
		<< "; height: " << initial_map.map_height
		<< "; players: " << initial_map.ship_map.size()
		<< "; my ships: " << initial_map.ship_map.at(player_id).size()
		<< "; planets: " << initial_map.planets.size();
	hlt::Log::log(initial_map_intelligence.str());

	std::vector<hlt::Move> moves;
	std::unordered_map<hlt::EntityId, hlt::Planet> shipCommanded;

	int turns = 0;
	hlt::EntityId missionaryShip = 99999;

	for (;;) {
		moves.clear();
		const hlt::Map map = hlt::in::get_map();
		++turns;

		for (const hlt::Ship& ship : map.ships.at(player_id)) {
			if (ship.docking_status != hlt::ShipDockingStatus::Undocked) {
				continue;
			}
		
			///Find all enemy ships
			std::vector<hlt::Ship> enemyShips;
			//enemyShips.reserve(map.ship_map.size() - map.ships.at(player_id).size()); //map.ship_map.size() gives amount of players, not amount of total ships
			for (std::pair<hlt::PlayerId, std::vector<hlt::Ship>> v : map.ships) {
				if (v.first != player_id) {
					enemyShips.insert(enemyShips.end(), v.second.begin(), v.second.end());
				}
			}

			//Calculate if enemyship within range
			hlt::Ship enemy;
			bool attackEnemy = false;
			for (hlt::Ship s : enemyShips) {
				double x = (ship.location.pos_x - s.location.pos_x) * (ship.location.pos_x - s.location.pos_x);
				double y = (ship.location.pos_y - s.location.pos_y) * (ship.location.pos_y - s.location.pos_y);
				int distance = sqrt(x + y);

				//Missionary ship should be a bit more blind
				if ((missionaryShip != ship.entity_id || distance < 10) && distance < 20) { //Attack first enemy detected within range (current implementation: NOT necessarily the closest enemy)
					enemy = s;
					attackEnemy = true;
					break;
				}
			}

			//If enemyShip within radius: A l'attack!
			if (attackEnemy) {
				const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(map, ship, ship.location.get_closest_point(enemy.location, 1),
					hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, M_PI / 180.0);
				if (move.second) moves.push_back(move.first);				
				continue;
			}

			const hlt::Planet nearestPlanet = findNearestPlanet(map, ship.location, player_id);
			const hlt::Planet planet = map.get_planet(nearestPlanet.entity_id); //Don't copy directly, get from map

			//logShipAndPlanetInfo(ship, planet); //DEBUG

			///Colonize nearest planet if empty or not full
			if (!planet.owned || (planet.owner_id == player_id && !planet.is_full())) {
				if (ship.can_dock(planet)) {
					std::ostringstream logger;
					int dockedShips = planet.docked_ships.size();
					logger << "Ship [" << ship.entity_id << "] docking planet [" << planet.entity_id << "] capa: " << dockedShips << "/" << planet.docking_spots << " owned: " << planet.owned;
					hlt::Log::log(logger.str());
					
					//dockShip(ship.entity_id, planet.entity_id, moves);
					moves.push_back(hlt::Move::dock(ship.entity_id, planet.entity_id));

					continue;
				} else {
					//moveShip(map, ship, ship.location.get_closest_point(planet.location, planet.radius), moves);  //only works locally, not on Halite servers, so instead I c/p'd the method here...

					const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(map, ship, ship.location.get_closest_point(planet.location, planet.radius - 0.1),
						hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, M_PI / 180.0);
					if (move.second) moves.push_back(move.first);
					continue;
				}
			}

			///Closest planet is now an enemy

			//Assign one ship to go colonize an empty planet at turns 40 and 80
			if (turns >= 40 && missionaryShip == 99999) missionaryShip = ship.entity_id;
			if (turns == 60 && missionaryShip != 99998) missionaryShip = 99999;
			if (turns >= 80 && missionaryShip == 99999) missionaryShip = ship.entity_id; 

			if (missionaryShip == ship.entity_id) {
				const hlt::Planet nearestEmptyPlanet = findNearestPlanet(map, ship.location, player_id, true, true); //returns a random full planet if no empty planets
				if (nearestEmptyPlanet.owned == 0) {
					const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(map, ship, ship.location.get_closest_point(nearestEmptyPlanet.location, nearestEmptyPlanet.radius),
						hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, M_PI / 180.0);
					if (move.second) moves.push_back(move.first);
					hlt::Log::log("@@@@@@SNEAKY SNEAKY@@@@@@");
					//logShipAndPlanetInfo(ship, nearestEmptyPlanet);
					continue;
				} else {
					hlt::Log::log("@@@@@@No more empty planets!@@@@@@");
					missionaryShip = 99998;
				}

			}

			///IF ENEMY: ATTACK
			if (planet.owned && planet.owner_id != player_id) {
				const hlt::Ship dockedEnemyShip = map.get_ship(planet.owner_id, planet.docked_ships.at(0));
				//moveShip(map, ship, ship.location.get_closest_point(dockedEnemyShip.location, 1), moves); //maybe check rotation
				const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(map, ship, ship.location.get_closest_point(dockedEnemyShip.location, dockedEnemyShip.radius),
					hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, M_PI / 180.0);
				if (move.second) moves.push_back(move.first);
			}
			continue;
		}

		if (!hlt::out::send_moves(moves)) {
			hlt::Log::log("send_moves failed; exiting");
			break;
		}
	}
}
