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

#if defined(WIN32)
#	define _CRT_SECURE_NO_WARNINGS
#	include <Windows.h>
#endif

#include <Ecs.hh>
#include <Advanced.hh>
#include <Call.hh>
#include <cstdio>
#include <cstdarg>

DAE_NAMESPACE_BEGIN

	namespace {
		const uint32_t streamVersion = 2;
		class BadStreamVersion : public std::exception {};
		class DoubleId : public std::exception {};
	}

	void Entity::remove() const {
		if (m_archetype) {
			m_archetype->remove(m_id);
			m_archetype = nullptr;
			m_id = EntityInvalidId;
		}
	}

	Ecs::Ecs(const char* name) {
		reset();
		setName(name);
	}

	Ecs::~Ecs() {
		setName(nullptr);
	}

	void Ecs::setName(const char* name) {
		if (m_name)
			delete[] m_name;
		const size_t len = name ? strlen(name) : 0;
		if (len) {
			m_name = new char[len + 1];
			strcpy(m_name, name);
		} else {
			m_name = nullptr;
		}
	}

	void Ecs::setListener(IEcsListener* listener) { 
		m_listener = listener; 
		for (auto& list : m_archetypes)
			list.archetype->setListener(listener);
	}
	
    IArchetype* Ecs::findArchetype(uint64_t mask) {
		for (const auto& list : m_archetypes)
			if (list.mask == mask)
				return list.archetype.get();
		return nullptr;
    }

	void Ecs::performMaintenance() {
		for (const auto& list : m_archetypes)
			list.archetype->performMaintenance();
	}

	IArchetype* Ecs::find(uint64_t mask) {
		for (const auto& list : m_archetypes)
			if (list.mask == mask)
				return list.archetype.get();
		return nullptr;
	}

	void Ecs::insert(uint64_t mask, std::unique_ptr<IArchetype> archetype) {
		if (m_archetypesById[archetype->id()])
			throw DoubleId();
		m_archetypesById[archetype->id()] = archetype.get();					// yes, hacky, but private ptr anyway
		m_archetypes.emplace_back(Entry{ mask, std::move(archetype) });
	}

	void Ecs::save(IStream& stream, void* userdata) const {
		using Type = IEcsListener::SerializationEvent::Type;
		DAE_ECS_DEBUG_LOG("Ecs::save: saving to stream", 0);
		if (m_listener)
			m_listener->serializationEvent({ Type::SaveStart, 0, streamVersion, 0, 0, name() });
		stream.write(&streamVersion, sizeof(streamVersion));
		const uint32_t archetypeCount = (uint32_t)m_archetypes.size();
		DAE_ECS_DEBUG_LOG("%u archetypes", archetypeCount);
		stream.write(&archetypeCount, sizeof(archetypeCount));

		for (const auto& archetype : m_archetypes) {
			DAE_ECS_DEBUG_LOG("saving archetypes id %u %s", archetype.archetype->id(), archetype.archetype->name());
			ArchetypeId_t id = archetype.archetype->id();
			stream.write(&id, sizeof(id));

			const uint64_t archetypeSizeOffset = stream.position();
			uint32_t archetypeSize = 0;
			stream.write(archetypeSize);
			{
				archetype.archetype->save(stream, userdata);
			}
			archetypeSize = (uint32_t)(stream.position() - archetypeSizeOffset - sizeof(archetypeSize));
			stream.setPosition(archetypeSizeOffset);
			stream.write(archetypeSize);
			stream.setPosition(archetypeSizeOffset + archetypeSize + sizeof(archetypeSize));
			DAE_ECS_DEBUG_LOG("Archetype %u %s = %u bytes", archetype.archetype->id(), archetype.archetype->name(), archetypeSize);
		}
		if (m_listener)
			m_listener->serializationEvent({ Type::SaveFinished });
		DAE_ECS_DEBUG_LOG("done", 0);
	}

	void Ecs::load(IStream& stream, void* userdata) {
		using Type = IEcsListener::SerializationEvent::Type;
		DAE_ECS_DEBUG_LOG("Ecs::load: loading from stream", 0);
		uint32_t v;
		stream.read(&v, sizeof(v));
		DAE_ECS_DEBUG_LOG("stream version %u", v);
		if (v > streamVersion) {
			DAE_ECS_DEBUG_LOG("wrong version %u > %u", v, streamVersion);
			throw BadStreamVersion();
		}
		if (m_listener)
			m_listener->serializationEvent({ Type::LoadStart, 0, v, 0, 0, name() });

		// reset all archetypes completely
		for (auto& archetype : m_archetypes)
			archetype.archetype->reset();

		uint32_t archetypeCount;
		stream.read(&archetypeCount, sizeof(archetypeCount));
		DAE_ECS_DEBUG_LOG("%u archetypes in stream", archetypeCount);

		// load all data
		for (uint32_t i = 0; i < archetypeCount; ++i) {
			uint8_t id;
			uint32_t archetypeSize = 0;
			stream.read(id);
			stream.read(archetypeSize);
			IArchetype* archetype = findArchetypeById(id);
			if (!archetype) {
				DAE_ECS_DEBUG_LOG("could not deduce archetype %u/%u with id %u", i+1, archetypeCount, id);
				DAE_ECS_WARN_LOG("unrecognized archetype %u", id);
				stream.skip(archetypeSize);
			} else {
				DAE_ECS_DEBUG_LOG("archetype id %2u/%2u %u is %s = %u bytes", i + 1, archetypeCount, id, archetype->name(), archetypeSize);
				if (m_listener)
					m_listener->serializationEvent({ Type::ArchetypeStart, archetype->id() });
				archetype->load(stream, userdata, v);
				if (m_listener)
					m_listener->serializationEvent({ Type::ArchetypeFinished, archetype->id() });
			}
		}
		DAE_ECS_DEBUG_LOG("finished load", 0);

		// to make sure that the next first of updates (possibly in the same frame) won't immediately
		// cause issues with not being able to create new entities, run the enlarge methods
		// on every archetype
		for (auto& archetype : m_archetypes)
			archetype.archetype->performMaintenance();
		if (m_listener)
			m_listener->serializationEvent({ Type::LoadFinished });
		DAE_ECS_DEBUG_LOG("done", 0);
	}

	void Ecs::reset() {
		m_archetypes.clear();
		for (auto& ptr : m_archetypesById)
			ptr = nullptr;
	}

	namespace detail {
		void validateComponentInfo(const IArchetype::ComponentInfo* info, size_t count) {
			if (!info || !count)
				throw InvalidComponentConfigurationException();

			uint64_t combinedBits = 0;
			for (size_t i = 0; i < count; ++i) {
				// check if all mask bits are powers of two, and mutually exclusive
				const uint64_t n = info[i].mask;
				const bool isPowerOfTwo = (n != 0 && (n & (n - 1)) == 0);
				if (!isPowerOfTwo)
					throw InvalidComponentConfigurationException();
				uint64_t newCombinedBits = combinedBits | n;
				if (newCombinedBits == combinedBits)
					throw InvalidComponentConfigurationException();
				combinedBits = newCombinedBits;

				// and check that names are set and unique
				const char* m = info[i].name;
				if (!m || !*m)
					throw InvalidComponentConfigurationException();
				for (size_t j = i + 1; j < count; ++j) {
					if (m == info[j].name || !strcmp(m, info[j].name))
						throw InvalidComponentConfigurationException();
				}
			}
		}
	}

	namespace {
		char buffer[1024];
		void debugDump(const std::function<void(const char*)>& dumper, const char* message, ...) {
			va_list vl;
			va_start(vl, message);
			vsnprintf(buffer, sizeof(buffer), message, vl);
			va_end(vl);
			buffer[sizeof(buffer) - 1] = '\0';
			dumper(buffer);
		}
	}

	void Ecs::dump(const std::function<void(const char*)>& dumper, DumpMode mode) {
		switch (mode) {
		case DumpMode::OneLine:
		{
			uint32_t numEntities = 0;
			uint32_t memory = 0;
			forEachArchetype([&](IArchetype* archetype) {
				numEntities += (uint32_t)archetype->size();
				memory += (uint32_t)(archetype->capacity() * archetype->singleEntitySize());
			});
			debugDump(dumper, "ECS: %u archetypes, %u entities, %.1fKb", m_archetypes.size(), numEntities, memory / 1024.f);
			break;
		}
		case DumpMode::Normal:
		{
			debugDump(dumper, "--[ECS]------------------------------------------------------");
			uint32_t totalEntities = 0, totalCapacity = 0, totalMemory = 0;
			forEachArchetype([&](IArchetype* archetype) {
				const auto memoryBytes = archetype->singleEntitySize() * archetype->capacity();
				totalEntities += (uint32_t)archetype->size();
				totalCapacity += (uint32_t)archetype->capacity();
				totalMemory += (uint32_t)memoryBytes;
				debugDump(dumper, "%3s %5u/%5u %5.1f%% %6.1fKb %s",
						  archetype->storageDescription(),
						  (uint32_t)archetype->size(),
						  (uint32_t)archetype->capacity(),
						  archetype->capacity() ? 100.f * (float)archetype->size() / (float)archetype->capacity() : 0.f,
						  memoryBytes / 1024.f,
						  archetype->name());
			});
			debugDump(dumper, "    %5u/%5u %5.1f%% %6.1fKb TOTALS", totalEntities, totalCapacity, totalCapacity ? 100.f * (float)totalEntities / (float)totalCapacity : 0.f, totalMemory / 1024.f);
			break;
		}
		}
	}

    uint64_t Ecs::countEntities() const {
		uint64_t count = 0;
		for (const auto& archetype : m_archetypes)
			count += archetype.archetype->size();
		return count;
    }

#if DAE_ECS_DEBUG
	namespace debug {
		void log(const char* filename, uint32_t line, const char* format, ...) {
			const char* const bs = strrchr(filename, (int)'\\');
			const char* const fs = strrchr(filename, (int)'/');
			const char* const slash = (bs > fs) ? bs : fs;
			
			char buf[1024];
			char* start = buf;
			start += snprintf(buf, sizeof(buf), "ECS DEBUG: %s(%u): ", slash + 1, line);
			const size_t left = sizeof(buf) - (start - buf);

			va_list vl;
			va_start(vl, format);
			start += vsnprintf(start, left - 1, format, vl);
			va_end(vl);
			*start++ = '\n';
			*start++ = '\0';

#if defined(WIN32)
			OutputDebugString(buf);
#else
			fprintf(stdout, "%s", buf);
#endif
		}
	}
#endif

	namespace warn {
		void log(const char* filename, uint32_t line, const char* format, ...) {
			const char* const bs = strrchr(filename, (int)'\\');
			const char* const fs = strrchr(filename, (int)'/');
			const char* const slash = (bs > fs) ? bs : fs;

			char buf[1024];
			char* start = buf;
			start += snprintf(buf, sizeof(buf), "ECS WARNING: %s(%u): ", slash + 1, line);
			const size_t left = sizeof(buf) - (start - buf);

			va_list vl;
			va_start(vl, format);
			start += vsnprintf(start, left - 1, format, vl);
			va_end(vl);
			*start++ = '\n';
			*start++ = '\0';

#if defined(WIN32)
			OutputDebugString(buf);
#else
			fprintf(stdout, "ECS WARNING: %s", buf);
#endif
		}
	}

DAE_NAMESPACE_END
