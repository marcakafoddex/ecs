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

#pragma once

#include <Ecs.hh>

struct TimerComponent {
	/* Mandatory static function that returns the name of this component. This is serialized, and later used
	 * to recognize the component.
	 */
	static const char* staticName() { return "Timer"; }

	/* Compile time static information function for the ECS system. We define the component mask, current
	 * serialization version and flags with this.
	 */
	constexpr static uint64_t staticComponentInfo(ecs::StaticComponentInfo info) {
		switch (info) {
		case ecs::StaticComponentInfo::Version:				return 1;									// serialization version
		case ecs::StaticComponentInfo::Mask:				return 0x0000000000000002;					// unique single bit mask that represents this component
		case ecs::StaticComponentInfo::Flags:				return ecs::ComponentFlagDefaults;			// flags for our component
		case ecs::StaticComponentInfo::RequiredComponents:	return 0;									// mask of components we require to always be paired with us
		default:											return 0;
		}
	}

	/* Serialization function for this component. Writes data into the given stream. No need to write version information,
	 * this is done by the ECS automatically.
	 */
	void save(ecs::IStream& stream, void* /*userdata*/) const {
		stream.write(timeLeft);
	}

	/* Deserialization function for this component. If you have serialized data with different version, then use the
	 * given version to determine what data to load.
	 */
	void load(ecs::IStream& stream, void* /*userdata*/, uint8_t /*version*/) {
		stream.read(timeLeft);
	}

	/* Update function that will update the state of this component. */
	void update(float delta) {
		timeLeft -= delta;
		if (timeLeft <= 0)
			self.remove();
	}

	/* This function gets called automatically by the ECS upon construction of the component. 
	 * It has to have exactly this (const ref) signature (and be public) or it won't be called!
	 */
	void setEntity(const ecs::Entity& entity) { 
		self = entity; 
	}

	/* Our data */
	float timeLeft;
	ecs::Entity self;
};
