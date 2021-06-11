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

// test streams
#include "Streams.hh"

// test header
#include "greatest.h"

// C++ stuff
#include <iostream>
#include <random>
#include <set>

GREATEST_MAIN_DEFS();

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

	template<typename _T> 
	std::vector<ecs::Entity> getAllEntitiesForArchetype(ecs::Ecs& ecs) {
		std::vector<ecs::Entity> entities;
		ecs.findArchetype<_T>().forEachEntity([&](const ecs::Entity& e) { entities.push_back(e); });
		return entities;
	}
}

/* This test just checks if creating an empty ECS object works. */
TEST ecsBasicEcsConstructionTest() {
	ecs::Ecs ecs("Test");
	PASS();
}

/* This test checks if registering archetypes works, if you can find them again after registering,
 * and that certain errors from the user are handled as expected (like double registration).
 */
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

/* This test checks if creating an entity works, if after removing it the validation routines
 * work as expected, and that copies of the entity also work as expected after removal.
 */
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

/* This test checks if vector storage behaves as expected: no capacity so no entities before
 * the first reserve (or enlarge) call.
 * Then it should only return as many entities as it was given capacity for.
 */
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

/* This test checks if array storage behaves as expected: fixed capacity so no need
 * for any reserve/enlarge calls.
 * It should only return as many entities as it was given capacity for.
 */
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

/* This test checks if the archetype correctly reuses slots.
 * Created entities should always be valid, even though their ids should always
 * be different from earlier ids (due to versioning).
 * Will (semi) randomly create and remove, testing that removed slots are reused.
 */
TEST ecsReuseEmptySlotsTest() {
	ecs::Ecs ecs("Test");
	std::vector<ecs::Entity> entities;
	std::set<ecs::Entity> removedEntities;

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
			ASSERT(removedEntities.find(e) == removedEntities.end());
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
			removedEntities.insert(e);
			entities.erase(entities.begin() + index);
			++numDelete;
			if (entities.empty())
				++wasEmpty;
		}
	}
	// std::cout << "Num creates: " << numCreate << " deletes: " << numDelete << " wasFull: " << wasFull << " wasEmpty: " << wasEmpty << std::endl;
	PASS();
}

/* This test checks if basic serialization works. It will setup a bunch of entities,
 * then remove a bunch, and then serialize the ECS to a in-memory stream.
 * It will then create a second ECS, load the memory stream into that second ECS,
 * and do an entity-by-entity, value-by-value comparison of all components.
 */
TEST ecsBasicSerializationTest() {
	ecs::Ecs ecs("Test");

	// setup archetypes
	auto& vs = ecs.registerArchetype<VectorStorageArchetype>("vs", 1);
	auto& fas = ecs.registerArchetype<FixedArrayStorageArchetype>("fas", 2);

	// reserve space for the vector driven one
	vs.reserve(16);

	// create randomized entities in both archetypes
	size_t numEntities = 0;
	for (auto archetype : std::vector<ecs::IArchetype*>{&vs, &fas}) {
		for (int i = 0; i < 4; ++i) {
			auto e = archetype->createEntity();
			if (!e.empty()) {
				auto& pc = e.fetch<PositionComponent>();
				pc.position = get_randomF();
				pc.speed = i * i;
				pc.acceleration = i % 2;
				if (auto tc = e.get<TimerComponent>())
					tc->timeLeft = i;
				++numEntities;
			}
		}
	}

	// remove some entities again
	ecs.forEachWithEntity<PositionComponent>([&](const ecs::Entity& e, PositionComponent& pc) {
		if (pc.position < 0.25f) {
			e.remove();
			--numEntities;
		}
	});
	ASSERT(vs.size() + fas.size() == numEntities);

	// serialize data
	MemoryStream memoryStream;
	ecs.save(memoryStream, /* userdata not used */ nullptr);
	// std::cout << "Serialized ECS contains " << memoryStream.data().size() << " bytes" << std::endl;

	// setup second ECS
	ecs::Ecs ecs2("Test2");
	auto& vs2 = ecs2.registerArchetype<VectorStorageArchetype>("vs", 1);
	auto& fas2 = ecs2.registerArchetype<FixedArrayStorageArchetype>("fas", 2);
	memoryStream.setPosition(0);
	ecs2.load(memoryStream, /* userdata not used */ nullptr);

	// now compare entities
	using EntVec = std::vector<ecs::Entity>;
	EntVec vs1ents = getAllEntitiesForArchetype<VectorStorageArchetype>(ecs);
	EntVec vs2ents = getAllEntitiesForArchetype<VectorStorageArchetype>(ecs2);
	EntVec fas1ents = getAllEntitiesForArchetype<FixedArrayStorageArchetype>(ecs);
	EntVec fas2ents = getAllEntitiesForArchetype<FixedArrayStorageArchetype>(ecs2);

	for (auto pair : std::vector<std::pair<EntVec*, EntVec*>>{{&vs1ents, &vs2ents}, {&fas1ents, &fas2ents}}) {
		ASSERT(pair.first->size() == pair.second->size());
		for (size_t i = 0; i < pair.first->size(); ++i) {
			PositionComponent& pc1 = (*pair.first)[i].fetch<PositionComponent>();
			PositionComponent& pc2 = (*pair.second)[i].fetch<PositionComponent>();
			ASSERT(&pc1 != &pc2);
			ASSERT(pc1.position == pc2.position);
			ASSERT(pc1.speed == pc2.speed);
			ASSERT(pc1.acceleration == pc2.acceleration);

			TimerComponent* tc1 = (*pair.first)[i].get<TimerComponent>();
			TimerComponent* tc2 = (*pair.second)[i].get<TimerComponent>();
			ASSERT(!!tc1 == !!tc2);
			if (tc1) {
				ASSERT(tc1 != tc2);
				ASSERT(tc1->timeLeft == tc2->timeLeft);
				ASSERT(tc1->self == (*pair.first)[i]);
				ASSERT(tc2->self == (*pair.second)[i]);
			}
		}
	}
		
	PASS();
}

/* Define test suites */

SUITE(basic) {
	RUN_TEST(ecsBasicEcsConstructionTest);
	RUN_TEST(ecsBasicRegistrationTest);
	RUN_TEST(ecsBasicCreateRemoveTest);
	RUN_TEST(ecsVectorStorageTest);
	RUN_TEST(ecsFixedArrayStorageTest);
	RUN_TEST(ecsReuseEmptySlotsTest);
}

SUITE(serialization) {
	RUN_TEST(ecsBasicSerializationTest);
}

/* Main application */

int main(int argc, char **argv)
{
	GREATEST_MAIN_BEGIN();
	greatest_set_verbosity(1);	/* always have verbose output */
	RUN_SUITE(basic);
	RUN_SUITE(serialization);
	GREATEST_MAIN_END();		/* display results */
	return 0;
}
