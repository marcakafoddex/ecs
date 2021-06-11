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
#include "components/DrawComponent.hh"
#include "systems/WinnerSystem.hh"

/* The position system goes over all archetypes that have a position component and a draw
 * component, and draws the 'game'.
 */
class DrawSystem {
public:
	DrawSystem(ecs::Ecs* ecs)
		: m_ecs(ecs) 
	{}
	void update(unsigned* boosts, size_t count) {
		char buffer[WinnerSystem::WinnerPosition + 1];
		for (auto& c : buffer) c = '-';

		m_ecs->forEach<PositionComponent, DrawComponent>([&](PositionComponent& pc, DrawComponent& dc) {
			const int p = (int)std::round(pc.position);
			if (p >= 0 && p < WinnerSystem::WinnerPosition)
				buffer[p] = dc.ch;
		});
		buffer[WinnerSystem::WinnerPosition] = 0;
		std::cout << buffer << " boosts: [";
		for (size_t i = 0; i < count; ++i) {
			std::cout << (i ? ", " : "") << boosts[i];
		}
		std::cout << "]\r" << std::flush;
	}
private:
	ecs::Ecs* m_ecs;
};
