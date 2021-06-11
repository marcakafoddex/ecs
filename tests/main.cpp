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

// ECS itself
#include <Ecs.hh>

// example components
#include "components/DrawComponent.hh"
#include "components/PositionComponent.hh"
#include "components/TimerComponent.hh"

// test header
#include "greatest.h"

// C++ stuff
#include <iostream>
#include <random>

namespace {

	/* Simple random helpers. */
	static std::default_random_engine e;		// NOTE: no seed, so the same "randomness" every time it's run
	float get_randomF() {
		static std::uniform_real_distribution<float> dis(0, 1); // rage 0 - 1
		return dis(e);
	}
	unsigned get_randomU() {
		static std::uniform_int_distribution<unsigned> dis(0, std::numeric_limits<unsigned>::max()); // rage 0 - 1
		return dis(e);
	}

	// define test archetypes
	using VectorStorageArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::Vector, PositionComponent>;
	using FixedArrayStorageArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::FixedSizedArray<4>::Type, PositionComponent, TimerComponent>;
	using TestArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::FixedSizedArray<4>::Type, PositionComponent, TimerComponent, DrawComponent>;

}

TEST ecsBasicEcsConstructionTest() {
	ecs::Ecs ecs("Test");
	PASS();
}

TEST ecsBasicRegistrationTest() {
	ecs::Ecs ecs("Test");

	// register car archetype as id 1, and find it
	auto& vsp = ecs.registerArchetype<VectorStorageArchetype>("vs", 1);
	ASSERT(&vsp == &ecs.findArchetype<VectorStorageArchetype>());

	// register ghost archetype as id 2, and find it
	auto& fasp = ecs.registerArchetype<FixedArrayStorageArchetype>("fas", 2);
	ASSERT(&fasp == &ecs.findArchetype<FixedArrayStorageArchetype>());

	// register test archetype as id 1, should fail due to duplicate id
	try {
		ecs.registerArchetype<TestArchetype>("test", 1);
		FAILm("Duplicate id should have been thrown");
	} catch (ecs::DoubleId&) {
		// this is what we want!
	} catch (...) {
		FAILm("Unexpected exception thrown");
	}

	// register the same car archetype (with a different id), should fail cause you cannot register
	// the exact same component configuration twice atm
	try {
		ecs.registerArchetype<VectorStorageArchetype>("test", 3);
	} catch (ecs::DuplicateArchetypeException&) {
		// this is what we expect!
	} catch (...) {
		FAILm("Unexpected exception thrown");
	}
	PASS();
}

TEST ecsBasicCreateRemoveTest() {
	ecs::Ecs ecs("Test");
	ecs::Entity e;

	// create an entity that is valid
	auto& archetype = ecs.registerArchetype<FixedArrayStorageArchetype>("fas", 1);
	e = archetype.createEntity();
	ASSERT(!e.empty());
	ASSERT(e.fullyValidate());

	// now remove it, empty should now be true, but fully validate should return false now
	auto copy = e;
	e.remove();
	ASSERT(e.empty());
	ASSERT(!e.fullyValidate());

	// for the copy we didn't call remove on, empty should now still be false
	// but fully validate should also be false
	ASSERT(!copy.empty());
	ASSERT(!copy.fullyValidate());

	PASS();
}

TEST ecsVectorStorageTest() {
	ecs::Ecs ecs("Test");
	ecs::Entity e;

	// no space reserved yet, never auto relocates, this should fail
	auto& archetype = ecs.registerArchetype<VectorStorageArchetype>("vs", 1);
	e = archetype.createEntity();	
	ASSERT(e.empty());			

	// allocate some space and test
	archetype.reserve(4);			
	for (int i = 0; i < 4; ++i) {
		e = archetype.createEntity();
		ASSERT(!e.empty());			
	}
	e = archetype.createEntity();
	ASSERT(e.empty());			

	PASS();
}

TEST ecsFixedArrayStorageTest() {
	ecs::Ecs ecs("Test");
	ecs::Entity e;

	// should always have capacity for exactly 4 entities
	auto& archetype = ecs.registerArchetype<FixedArrayStorageArchetype>("fas", 1);
	for (int i = 0; i < 4; ++i) {
		e = archetype.createEntity();
		ASSERT(!e.empty());			
	}
	e = archetype.createEntity();
	ASSERT(e.empty());			

	PASS();
}

TEST ecsReuseEmptySlotsTest() {
	ecs::Ecs ecs("Test");
	std::vector<ecs::Entity> entities;

	// allocate capacity for 4 entities
	// we're "randomly" gonna create and remove entities, making sure we'll never
	// have more than 4... every create HAS to always succeed, every remove as well
	// this tests the "reusing open slots" feature in the archetype
	auto& archetype = ecs.registerArchetype<VectorStorageArchetype>("vs", 1);
	archetype.reserve(4);
	uint32_t numCreate = 0, numDelete = 0, wasEmpty = 0, wasFull = 0;
	for (size_t i = 0; i < 1000; ++i) {
		const bool doCreate = entities.size() < 4 && (entities.empty() || (get_randomU() % 2));
		if (doCreate) {
			ASSERT(entities.size() < 4);
			auto e = archetype.createEntity();
			ASSERT(e.fullyValidate());
			entities.push_back(e);
			++numCreate;
			if (entities.size() == 4)
				++wasFull;
		} else {
			ASSERT(!entities.empty());
			const size_t index = get_randomU() % entities.size();
			auto e = entities[index];
			ASSERT(e.fullyValidate());
			e.remove();
			ASSERT(!e.fullyValidate());
			entities.erase(entities.begin() + index);
			++numDelete;
			if (entities.empty())
				++wasEmpty;
		}
	}
	// std::cout << "Num creates: " << numCreate << " deletes: " << numDelete << " wasFull: " << wasFull << " wasEmpty: " << wasEmpty << std::endl;
	PASS();
}

/* Suites can group multiple tests with common setup. */
SUITE(basic) {
	RUN_TEST(ecsBasicEcsConstructionTest);
	RUN_TEST(ecsBasicRegistrationTest);
	RUN_TEST(ecsBasicCreateRemoveTest);
	RUN_TEST(ecsVectorStorageTest);
	RUN_TEST(ecsFixedArrayStorageTest);
	RUN_TEST(ecsReuseEmptySlotsTest);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
	GREATEST_MAIN_BEGIN();
	greatest_set_verbosity(1);	/* always have verbose output */
	RUN_SUITE(basic);
	GREATEST_MAIN_END();		/* display results */
	return 0;
}
