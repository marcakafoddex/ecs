/*

MIT License

Copyright (c) 2021 Marc "Foddex" Oude Kotte

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <Ecs.hh>
#include <Advanced.hh>
#include <Call.hh>

#include <chrono>
#include <random>
#include <thread>

#include "systems/PositionSystem.hh"
#include "systems/TimerSystem.hh"
#include "systems/DrawSystem.hh"
#include "systems/WinnerSystem.hh"

/* Declare our car archetype, a car only has a position and can be drawn. We will never have more than 4 car, so
 * we can limit the storage to a fixed sized array of 4 instead of a vector.
 * Note that you this doesn't mean you HAVE to have 4 cars always, just that you can never have MORE than 4.
 */
using CarArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::FixedSizedArray<4>::Type, PositionComponent, DrawComponent>;

/* Declare our ghost archetype, a ghost has a position, can be drawn and has a maximum life time. 
 * There is no limit to the amount of ghosts we can have, so we use a vector for storage.
 */
using GhostArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::Vector, PositionComponent, DrawComponent, TimerComponent>;

/* Simple random helpers. */
static std::default_random_engine e;
float get_randomF() {
	static std::uniform_real_distribution<float> dis(0, 1); // rage 0 - 1
	return dis(e);
}
unsigned get_randomU() {
	static std::uniform_int_distribution<unsigned> dis(0, std::numeric_limits<unsigned>::max()); // rage 0 - 1
	return dis(e);
}

/* Helper function that sets up a car or a ghost */
void setupEntity(ecs::Entity entity, float baseAcceleration, char ch) {
	// NOTE: we could use fetch as well, since we're pretty sure the entities we receive
	// here will be valid, and thus nullptr will never be returned, and we'd get a reference.
	// But in case someone starts messing with the code, i wrote this a bit more robust.
	PositionComponent* pc = entity.get<PositionComponent>();
	if (pc) {
		pc->position = 0;
		pc->speed = 0;
		pc->acceleration = 0.5f * get_randomF() + baseAcceleration;		// random acceleration between 0.5 and 1.0
	}
	DrawComponent* dc = entity.get<DrawComponent>();
	if (dc) {
		dc->ch = ch;
	}
	// not all entities have a timer component in this setup, so here we definitely have
	// to use get and not fetch
	TimerComponent* tc = entity.get<TimerComponent>();
	if (tc) {
		tc->timeLeft = 5 + get_randomF() * 5;							// disappear between 5 and 10 seconds
	}
}

int main(int argc, char** argv) {
	/* Setup our basic ecs */
	ecs::Ecs ecs("ExampleEcs");

	/* register our archetypes with the ECS, giving them a name and a unique id */
	auto& cars = ecs.registerArchetype<CarArchetype>("car", 1);
	auto& ghosts = ecs.registerArchetype<GhostArchetype>("ghost", 2);

	/* Spawn 4 cars */
	for (unsigned i = 0; i < 4; ++i)
		setupEntity(cars.createEntity(), 0, '1' + i);

	/* Spawn random amount of ghosts */
	unsigned numGhosts = numGhosts = 5 + get_randomU() % 10;
	ghosts.reserve(numGhosts);						// for safety reasons archetypes never allocate during a createEntity call, so reserve beforehand
	for (unsigned i = 0; i < numGhosts; ++i)
		setupEntity(ghosts.createEntity(), 0.5f, get_randomU() % 2 ? 'G' : 'g');

	/* Setup our systems that will update our components */
	std::cout << "This non interactive game shows 4 race cars (1-4) and a bunch of ghosts (gG)" << std::endl;
	std::cout << "competing on a very straight race track. Ghosts are fast but disappear, cars" << std::endl;
	std::cout << "will never disappear. Exciting right! Let's wait and see what's happens!" << std::endl;
	PositionSystem positionSystem(&ecs);
	TimerSystem timerSystem(&ecs);
	DrawSystem drawSystem(&ecs);
	WinnerSystem winnerSystem(&ecs);
	ecs::Entity winner;
	auto last = std::chrono::system_clock::now();
	while (true) {
		// calculate frame time
		const auto now = std::chrono::system_clock::now();
		const std::chrono::duration<float> delta = now - last;
		last = now;

		// update our systems accordingly
		positionSystem.update(delta.count());
		timerSystem.update(delta.count());
		drawSystem.update();
		winner = winnerSystem.update();
		if (!winner.empty())
			break;

		// try to run at 4 FPS (although on Windows this is not very precise usually)
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}

	/* Display the winner! */
	std::cout << std::endl;
	if (winner.archetype() == &cars) {
		std::cout << "Car " << winner.fetch<DrawComponent>().ch << " won!" << std::endl;
	} else {
		std::cout << "Oh no, a ghost won!!" << std::endl;
	}
	return 0;
}
