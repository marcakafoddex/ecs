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

#include <iostream>

#include "components/PositionComponent.hh"

/* The winner system goes over all archetypes with a position and simply checks if any
 * position is beyond a certain value. In that case that's the winning entity.
 */
class WinnerSystem {
public:
	enum {
		WinnerPosition = 80,
	};
	WinnerSystem(ecs::Ecs* ecs)
		: m_ecs(ecs) {}
	ecs::Entity update() {
		ecs::Entity winner;
		m_ecs->forEachWithEntity<PositionComponent>([&](const ecs::Entity& entity, PositionComponent& pc) {
			if (pc.position >= WinnerPosition)
				winner = entity;
		});
		return winner;
	}
private:
	ecs::Ecs* m_ecs;
};
