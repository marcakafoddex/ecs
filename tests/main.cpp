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

// define test archetypes
using VectorStorageArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::Vector, PositionComponent>;
using FixedArrayStorageArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::FixedSizedArray<4>::Type, PositionComponent, TimerComponent>;
using TestArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::FixedSizedArray<4>::Type, PositionComponent, TimerComponent, DrawComponent>;

TEST ecsBasicConstructionTest() {
	ecs::Ecs ecs("Test");
	PASS();
}

TEST ecsBasicRegistrationTest() {
	ecs::Ecs ecs("Test");

	// register car archetype as id 1, and find it
	auto& vsp = ecs.registerArchetype<VectorStorageArchetype>("vsp", 1);
	ASSERT(&vsp == &ecs.findArchetype<VectorStorageArchetype>());

	// register ghost archetype as id 2, and find it
	auto& fasp = ecs.registerArchetype<FixedArrayStorageArchetype>("fasp", 2);
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

TEST ecsVectorStorageTest() {
	ecs::Ecs ecs("Test");
	ecs::Entity e;

	// no space reserved yet, never auto relocates, this should fail
	auto& archetype = ecs.registerArchetype<VectorStorageArchetype>("ghost", 1);
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
	auto& archetype = ecs.registerArchetype<FixedArrayStorageArchetype>("car", 1);
	for (int i = 0; i < 4; ++i) {
		e = archetype.createEntity();
		ASSERT(!e.empty());			
	}
	e = archetype.createEntity();
	ASSERT(e.empty());			

	PASS();
}

/* Suites can group multiple tests with common setup. */
SUITE(basic) {
	RUN_TEST(ecsBasicConstructionTest);
	RUN_TEST(ecsBasicRegistrationTest);
	RUN_TEST(ecsVectorStorageTest);
	RUN_TEST(ecsFixedArrayStorageTest);
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
