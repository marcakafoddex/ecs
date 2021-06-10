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

DAE_NAMESPACE_BEGIN

	/* 
	 * Component iterator that once - during initialization - determines what archetypes to iterate through,
	 * storing their pointers from the ECS, and then using that to iterate.
	 * 
	 * This class can only improve the performance if they are PERMANENTLY STORED. They have to initialized AFTER all
	 * archetypes have been registered.
	 * 
	 * The maximum should be set as low as possible (preferably at the exact number on case by case basis) to 
	 * reduce unnecessary memory overload.
	 */
	template<size_t _Max, typename... _IterateComponents>
	class ComponentIterator {
	public:
		static const size_t Max = _Max;
		ComponentIterator() {
			for (auto& a : m_archetypes)
				a = nullptr;
		}
		void initialize(Ecs& ecs) {
			const uint64_t componentMask = detail::BitwiseOr<_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask)...>::value;
			size_t count = 0;
			ecs.forEachArchetype([&](IArchetype* archetype) {
				if ((archetype->mask() & componentMask) == componentMask) {
					DAE_ECS_ASSERT(count < Max && "ComponentIterator found found more archetypes than it was configured for at compile time");
					m_archetypes[count++] = archetype;
				}
			});
		}
		template<typename _Predicate>
		void iterate(const _Predicate& pr) {
			std::tuple<_IterateComponents*...> tuple;
			const auto set = [](auto& tgt, void* ptr) { tgt = reinterpret_cast<decltype(tgt)>(ptr); };
			for (auto archetype : m_archetypes) {
				if (!archetype)
					return;
				const EntityState_t* state = archetype->stateBegin();
				const EntityState_t* end = archetype->stateEnd();
				if (state != end) {
					(set(std::get<_IterateComponents*>(tuple), archetype->componentBegin(_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask))), ...);
					do {
						if (!detail::emptyFromState(*state++))
							pr(*std::get<_IterateComponents*>(tuple)...);
						(++std::get<_IterateComponents*>(tuple), ...);
					} while (state != end);
				}
			}
		}
		template<typename _Predicate>
		void iterateEntity(const _Predicate& pr) {
			std::tuple<_IterateComponents*...> tuple;
			const auto set = [](auto& tgt, void* ptr) { tgt = reinterpret_cast<decltype(tgt)>(ptr); };
			for (auto archetype : m_archetypes) {
				if (!archetype)
					return;
				const EntityState_t* state = archetype->stateBegin();
				const EntityState_t* end = archetype->stateEnd();
				if (state != end) {
					const bool copyable = archetype->allowsEntities();
					EntityId_t index = 0;
					(set(std::get<_IterateComponents*>(tuple), archetype->componentBegin(_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask))), ...);
					do {
						if (!detail::emptyFromState(*state))
							pr(Entity{ detail::idFromIndexAndState(index, *state), archetype, copyable }, *std::get<_IterateComponents*>(tuple)...);
						(++std::get<_IterateComponents*>(tuple), ...);
						++index;
						++state;
					} while (state != end);
				}
			}
		}

	private:
		IArchetype* m_archetypes[Max];
	};

DAE_NAMESPACE_END
