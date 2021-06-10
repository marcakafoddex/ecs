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

#include <Advanced.hh>

#include "components/TimerComponent.hh"

/* The timer system goes over all archetypes that have a timer component, and does
 * its things on it. This system example works a little different from the position
 * system. It uses a fixed-size archetype iterator. This archetype iterator
 * finds all archetypes that have a certain component combination ONCE, and stores
 * the pointers directly. If you have a large number of archetypes and you know
 * that the system will only use a low amount, this system is a bit faster.
 * 
 * The downside is that you have to manually indicate the amount of archetypes at
 * compile time. If there are less than you specified, nothing will happen (but
 * you used useless memory), if there are more, at runtime an assert will fail.
 */
class TimerSystem {
public:
	TimerSystem(ecs::Ecs* ecs) {
		m_iterator.initialize(*ecs);
	}
	void update(float delta) {
		m_iterator.iterate([delta](TimerComponent& pc) {
			pc.update(delta);
		});
	}
private:
	ecs::ComponentIterator<1, TimerComponent> m_iterator;
};
