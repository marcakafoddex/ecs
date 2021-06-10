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

#include <cstdint>
#include <cstring>
#include <tuple>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <limits>

#if !defined(DAE_ECS_API)
#	if defined(WIN32)
#		if defined(ECS_STATIC)
#			define DAE_ECS_API
#		elif defined(ECS_BUILD_DLL)
#			define DAE_ECS_API			__declspec(dllexport)
#		else
#			define DAE_ECS_API			__declspec(dllimport)
#		endif
#	else
#		define DAE_ECS_API
#	endif
#endif

#if !defined(DAE_ECS_NATVIS_DEBUG)
#	if !defined(DEBUG) && _MSC_VER
#		define DAE_ECS_NATVIS_DEBUG 1
#	else
#		define DAE_ECS_NATVIS_DEBUG 0
#	endif
#endif

#if !defined(DAE_ECS_DEBUG)
#	define DAE_ECS_DEBUG 0				// set to 1 temporarily to get debug output when loading/saving 
#endif
#if DAE_ECS_DEBUG
#	define DAE_ECS_DEBUG_LOG(format, ...)		debug::log(__FILE__, __LINE__, format, __VA_ARGS__)
#else
#	define DAE_ECS_DEBUG_LOG(...)
#endif

#if !defined(DAE_ECS_WARN_LOG)
#	define DAE_ECS_WARN_LOG(format, ...)		warn::log(__FILE__, __LINE__, format, __VA_ARGS__)
#endif

#if !defined(DAE_ECS_ASSERT)
#	include <cassert>
#	define DAE_ECS_ASSERT assert
#endif

#if !defined(DAE_NAMESPACE)
#	define DAE_NAMESPACE 1
#	define DAE_NAMESPACE_NAME ecs
#endif
#if DAE_NAMESPACE
#	define DAE_NAMESPACE_BEGIN namespace DAE_NAMESPACE_NAME {
#	define DAE_NAMESPACE_END }
#else
#	define DAE_NAMESPACE_BEGIN
#	define DAE_NAMESPACE_END
#endif

#undef min
#undef max

DAE_NAMESPACE_BEGIN

	class DuplicateArchetypeException : public std::exception {};
	class UnregisteredArchetypeException : public std::exception {};
	class InvalidEntityException : public std::exception {};
	class InvalidIndexException : public std::exception {};
	class InvalidRequestedIndexException : public std::exception {};
	class InvalidDataStreamException : public std::exception {};
	class InvalidPodDataVersionException : public std::exception {};
	class InvalidComponentConfigurationException : public std::exception {};
	class MissingRequiredComponentsException : public std::exception {};
	class TooLargeComponentException : public std::exception {};
	class CannotSkipComponentException : public std::exception {};

	using ArchetypeId_t = uint8_t;
	using EntityId_t = uint32_t;
	using EntityIndexVersion_t = uint8_t;
	using EntityState_t = EntityIndexVersion_t;
	enum {
		EntityIndexVersionShift		= 24,				// how many bits to shift right to get version value
		EntityIndexVersionBitLength = 7,				// how many bits is the version value?
		EntityIndexVersionStart		= 1,				// version we start at, wraps from 127 to 0, but then bumped to 1, as 0 is invalid
		EntityIndexVersionMask		= 0x7f000000,
		EntityIndexMask				= 0x00ffffff,
		EntityInvalidId				= 0,				// cause version 0 will never exist
		EntityInvalidIndex			= (EntityId_t)-1,	// only an invalid index, not an invalid id!
	};

	enum class StaticComponentInfo {
		Mask,					// should return 64-bits user defined component mask (single bit) for the component
		Flags,					// should return 64-bits combination of ComponentFlagXXX defaults 
		Version,				// should return 64-bits (max 255) serialization version
		RequiredComponents,		// should return 64-bits user defined mask of required components that always have to accompany a component in an archetype, can be 0 to indicate no requirements
	};

	class Entity;
	class Ecs;
	class IArchetype;
	template<uint64_t, template<typename> class, typename...> class Archetype;

	enum : uint64_t {
		// component flags
		ComponentFlagDefaults					= 0x00,			// value for "no flags", basically
		ComponentFlagNoCleanComponent			= 0x01,			// if set, every time an entity is removed, it's components are "cleaned", meaning reconstructed (so destructor called, then constructor)
		ComponentFlagCallPreDestroy				= 0x02,			// if set, before destroy, the preDestroy function on the component is called
		ComponentFlagSerializeAsPODType			= 0x04,			// if set, the entire component stack will be serialized as POD type from memory in 1 go, only for pure data (non-string, non-complex type) components
		ComponentFlagNeverSerialize				= 0x08,			// if set, the component will never be serialized (it will still be present, but not streamed)
		ComponentFlag_FirstFree					= 0x10,			// custom flags can be added starting from this bit

		// archetype flags
		ArchetypeFlagDefaults					= 0x00,			// value for "no flags", basically
		ArchetypeFlagCompressableNoEntities		= 0x01,			// if set, the archetype's vectors can be compressed, but entities can not be created for it (cause their indexes cannot be updated)
		ArchetypeFlagAutoCompressNCalls			= 0x02,			// if set, the archetype will automatically compress (if ArchetypeFlagCompressableNoEntities is also set) every N calls to maintenance()
		ArchetypeFlagAutoCompressFreeThreshold	= 0x04,			// if set, the archetype will automatically compress (if ArchetypeFlagCompressableNoEntities is also set) once a certain percentage of slots has become empty (e.g. 25% empty -> auto compress)
		ArchetypeFlagAutoReserveNLeft			= 0x08,			// if set, the archetype will automatically try to reserve (if storage can reallocate) more memory for slots once the storage has a certain amount of slots left in its capacity
		ArchetypeFlagAutoReserveFullThreshold	= 0x10,			// if set, the archetype will automatically try to reserve (if storage can reallocate) more memory for slots once the storage has reached a certain size (e.g. >75% means double capacity)
		ArchetypeFlagNeverSerialize				= 0x20,			// if set, the archetype will not serialize (save to or load from stream), ever
		ArchetypeFlagWithCreateDeleteTracking	= 0x40,			// if set, creates and deletes are tracked, and the archetype will build a vector with change data
		ArchetypeFlag_FirstFree					= 0x80,			// custom flags can be added starting from this bit
	};

	class IStream {
	protected:
		virtual ~IStream() = default;
	public:
		/* ECS needs this interface to work */
		virtual void write(const void* data, size_t count) = 0;		// must write given data, or throw
		virtual void read(void* data, size_t count) = 0;			// must read given data, or throw
		virtual uint64_t position() const = 0;						// must return current write/read position
		virtual void setPosition(uint64_t pos) = 0;					// must change the read/write position to the given position, or throw
		virtual void skip(uint64_t pos) { setPosition(position() + pos); }

		/* Convenience helper */
		template<typename _T> void write(const _T& t) { write(&t, sizeof(t)); }
		template<typename _T> void read(_T& t) { read(&t, sizeof(t)); }
	};

	enum class ChangeType {
		Create = 0,
		Delete,
	};
	struct Change {
		EntityId_t	id;
		ChangeType	type;
	};

#if DAE_ECS_DEBUG
	namespace debug {
		DAE_ECS_API void log(const char* filename, uint32_t line, const char* format, ...);
	}
#endif

	namespace warn {
		DAE_ECS_API void log(const char* filename, uint32_t line, const char* format, ...);
	}

	namespace storage {
		template<typename _T>
		class Vector {
			std::vector<_T>		m_vector;
		public:
			using value_type = _T;
			static const char* description() { return "vec"; }
			static constexpr bool canReallocate() { return true; }
			using iterator = _T*;
			using const_iterator = const _T*;
			inline iterator begin() { return m_vector.data(); }
			inline iterator end() { return m_vector.data() + m_vector.size(); }
			inline const_iterator begin() const { return m_vector.data(); }
			inline const_iterator end() const { return m_vector.data() + m_vector.size(); }
			inline _T& operator[](size_t index) { return m_vector[index]; }
			inline const _T& operator[](size_t index) const { return m_vector[index]; }
			inline size_t size() const { return m_vector.size(); }
			inline size_t capacity() const { return m_vector.capacity(); }
			inline bool empty() const { return m_vector.empty(); }
			inline _T& push_back(const _T& val) { 
				m_vector.push_back(val); 
				return m_vector.back(); 
			}
			inline _T& push_back(_T&& val) {
				m_vector.emplace_back(std::move(val));
				return m_vector.back();
			}
			template<typename... _Args> inline _T& emplace_back(_Args&&... args) { 
				m_vector.emplace_back(std::forward<_Args>(args)...); 
				return m_vector.back(); 
			}
			inline _T& back() { return m_vector.back(); }
			inline const _T& back() const { return m_vector.back(); }
			inline void pop_back() { m_vector.pop_back(); }
			inline void clear() { m_vector.clear(); }
			inline void reserve(size_t capacity) { m_vector.reserve(capacity); }
			inline void resize(size_t newSize) { m_vector.resize(newSize); }
		};

		template<typename _T, size_t _N>
		class Array {
			uint8_t		m_array[_N * sizeof(_T)];
			_T*			m_end = reinterpret_cast<_T*>(m_array);
		public:
			using value_type = _T;
			static const char* description() { return "arr"; }
			Array() = default;
			~Array() {
				while (!empty())
					pop_back();
			}
			static constexpr bool canReallocate() { return false; }
			using iterator = _T*;
			using const_iterator = const _T*;
			inline iterator begin() { return reinterpret_cast<_T*>(m_array); }
			inline iterator end() { return m_end; }
			inline const_iterator begin() const { return reinterpret_cast<const_iterator>(m_array); }
			inline const_iterator end() const { return m_end; }
			inline _T& operator[](size_t index) { return begin()[index]; }
			inline const _T& operator[](size_t index) const { return begin()[index]; }
			inline size_t size() const { return m_end - begin(); }
			inline size_t capacity() const { return _N; }
			inline bool empty() const { return begin() == m_end; }
			inline _T& push_back(const _T& val) {
				DAE_ECS_ASSERT(size() < capacity());
				return *(m_end++) = val;
			}
			inline _T& push_back(_T&& val) {
				DAE_ECS_ASSERT(size() < capacity());
				return *(m_end++) = std::move(val);
			}
			template<typename... _Args> inline _T& emplace_back(_Args&&... args) { 
				DAE_ECS_ASSERT(size() < capacity());
				return *new(m_end++)_T(std::forward<_Args>(args)...);
			}
			inline _T& back() {
				DAE_ECS_ASSERT(!empty());
				return *(m_end - 1);
			}
			inline const _T& back() const {
				DAE_ECS_ASSERT(!empty());
				return *(m_end - 1);
			}
			inline void pop_back() { 
				DAE_ECS_ASSERT(!empty());
				(--m_end)->~_T();
			}
			inline void clear() {
				for (auto it = begin(), itEnd = end(); it < itEnd; ++it)
					it->~_T();
				m_end = begin();
			}
			inline void reserve(size_t) {}
			inline void resize(size_t newSize) {
				while (size() < newSize)
					new(m_end++)_T();
				while (size() > newSize)
					pop_back();
			}
		};

		template<size_t N> struct FixedSizedArray {
			template<typename _T>
			using Type = Array<_T, N>;
		};
	}

	namespace detail {
		// state helpers
		inline EntityIndexVersion_t versionFromState(EntityState_t state) {
			return state & (EntityIndexVersionMask >> EntityIndexVersionShift);
		}
		inline bool emptyFromState(EntityState_t state) {
			return !!(state >> EntityIndexVersionBitLength);
		}
		inline EntityId_t indexFromId(EntityId_t id) {
			return id & EntityIndexMask;
		}
		inline EntityIndexVersion_t versionFromId(EntityId_t id) {
			return (id & EntityIndexVersionMask) >> EntityIndexVersionShift;
		}
		inline EntityId_t idFromIndexAndVersion(EntityId_t index, EntityIndexVersion_t version) {
			return index | (EntityId_t(version) << EntityIndexVersionShift);
		}
		inline EntityId_t idFromIndexAndState(EntityId_t index, EntityState_t state) {
			return index | (EntityId_t(versionFromState(state)) << EntityIndexVersionShift);
		}
		inline EntityState_t stateFromVersionAndEmpty(EntityIndexVersion_t version, bool empty) {
			return version | (EntityIndexVersion_t(!!empty) << EntityIndexVersionBitLength);
		}

		// uint64 value bitwise-or template
		template<uint64_t _Head, uint64_t... _Tail> struct BitwiseOr { enum : uint64_t { value = _Head | BitwiseOr<_Tail...>::value }; };
		template<uint64_t _Head> struct BitwiseOr<_Head> { enum : uint64_t { value = _Head }; };

		// uint64 value boolean-and template
		template<uint64_t _Head, uint64_t... _Tail> struct And { enum { value = _Head && And<_Tail...>::value }; };
		template<uint64_t _Head> struct And<_Head> { enum { value = _Head }; };

		// extends-from with multiple classes template
		template<typename _Base, typename... _List> struct ExtendsFrom { enum { value = And<std::is_base_of<_Base, _List>::value...>::value }; };

		// uint64_t mask based component begin iterator template
		template<typename _Tuple, template<typename> class _Storage, typename _Head, typename... _Tail> struct ComponentBeginImpl {
			static void* get(_Tuple& tuple, uint64_t mask) {
				if (_Head::staticComponentInfo(StaticComponentInfo::Mask) == mask)
					return std::get<_Storage<_Head>>(tuple).begin();
				return ComponentBeginImpl<_Tuple, _Storage, _Tail...>::get(tuple, mask);
			}
		};
		template<typename _Tuple, template<typename> class _Storage, typename _Head> struct ComponentBeginImpl<_Tuple, _Storage, _Head> {
			static void* get(_Tuple& tuple, uint64_t mask) {
				if (_Head::staticComponentInfo(StaticComponentInfo::Mask) == mask)
					return std::get<_Storage<_Head>>(tuple).begin();
				return nullptr;
			}
		};
		template<typename _Tuple, template<typename> class _Storage, typename... _Components> struct ComponentBegin {
			static void* get(_Tuple& tuple, uint64_t mask) {
				return ComponentBeginImpl<_Tuple, _Storage, _Components...>::get(tuple, mask);
			}
		};

		// component cleaner by calling destructor + constructor if component wants it - template
		template<typename _Component, bool _WantsClean> struct ComponentCleanerImpl {
			static void call(_Component* component, const _Component& defaultComponent) {
				component->~_Component();
				new(component)_Component(defaultComponent);
			}
		};
		template<typename _Component> struct ComponentCleanerImpl<_Component, true> {
			static void call(_Component*, const _Component&) {}
		};
		template<typename _Component> struct ComponentCleaner {
			static void call(_Component* component, const _Component& defaultComponent) {
				ComponentCleanerImpl<_Component, _Component::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagNoCleanComponent>::call(component, defaultComponent);
			}
		};
		// component preDestroy caller if component wants it - template
		template<typename _Component, bool _WantsClean> struct PreDestroyCallerImpl {
			static void call(_Component* component) { component->preDestroy(); }
		};
		template<typename _Component> struct PreDestroyCallerImpl<_Component, false> {
			static void call(_Component*) {}
		};
		template<typename _Component> struct PreDestroyCaller {
			static void call(_Component* component) {
				PreDestroyCallerImpl<_Component, !!(_Component::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagCallPreDestroy)>::call(component);
			}
		};

		// void setEntity(const Entity&) caller if it's present in the component template
		template<typename _Component> struct HasSetEntityMethod {
			template<typename _U, void(_U::*)(const Entity&)> struct SFINAE {};
			template<typename _U> static char Test(SFINAE<_U, &_U::setEntity>*);
			template<typename _U> static int Test(...);
			static const bool value = sizeof(Test<_Component>(0)) == sizeof(char);
		};
		template<typename _Component, bool> struct SetEntityImpl {
			static void call(_Component& comp, const Entity& entity) { comp.setEntity(entity); }
		};
		template<typename _Component> struct SetEntityImpl<_Component, false> {
			static void call(_Component&, const Entity&) {}
		};
		template<typename _Tuple, template<typename> class _Storage, typename _Head, typename... _Tail> struct SetEntity {
			static void set(_Tuple& tuple, const Entity& entity);
		};
		template<typename _Tuple, template<typename> class _Storage, typename _Head> struct SetEntity<_Tuple, _Storage, _Head> {
			static void set(_Tuple& tuple, const Entity& entity);
		};

		// copy component
		template<typename _Tuple, template<typename> class _Storage, typename _Head, typename... _Tail> 
		struct CopyComponents {
			static void copy(_Tuple& tuple, EntityId_t target, EntityId_t source) {
				auto& storage = std::get<_Storage<_Head>>(tuple);
				storage[target] = storage[source];
				CopyComponents<_Tuple, _Storage, _Tail...>::copy(tuple, target, source);
			}
		};
		template<typename _Tuple, template<typename> class _Storage, typename _Head> 
		struct CopyComponents<_Tuple, _Storage, _Head> {
			static void copy(_Tuple& tuple, EntityId_t target, EntityId_t source) {
				auto& storage = std::get<_Storage<_Head>>(tuple);
				storage[target] = storage[source];
			}
		};
		
		// compression implementation
		template<bool _Allowed, uint64_t _Flags, template<typename> class _Storage, typename... _Components> struct ArchetypeHelper {
			static void compress(Archetype<_Flags, _Storage, _Components...>& archetype);																// _Allowed as in, can not compress and has entities
			static Entity create(Archetype<_Flags, _Storage, _Components...>& archetype);																// _Allowed as in, can not compress and has entities
			static Entity duplicate(Archetype<_Flags, _Storage, _Components...>& archetype, Entity entity);												// _Allowed as in, can not compress and has entities
			static void save(IStream& stream, void* userdata, const Archetype<_Flags, _Storage, _Components...>& archetype);							// _Allowed as in, can serialize
			static void saveSingle(IStream& stream, void* userdata, const Archetype<_Flags, _Storage, _Components...>& archetype, EntityId_t index);	// _Allowed as in, can serialize
			static void load(IStream& stream, void* userdata, Archetype<_Flags, _Storage, _Components...>& archetype, uint32_t version);				// _Allowed as in, can serialize
			static void loadSingle(IStream& stream, void* userdata, Archetype<_Flags, _Storage, _Components...>& archetype, EntityId_t index);			// _Allowed as in, can serialize
		};
		template<uint64_t _Flags, template<typename> class _Storage, typename... _Components> struct ArchetypeHelper<false, _Flags, _Storage, _Components...> {
			static void compress(Archetype<_Flags, _Storage, _Components...>&);																			// _Allowed as in, can not compress and has entities
			static Entity create(Archetype<_Flags, _Storage, _Components...>&);																			// _Allowed as in, can not compress and has entities
			static Entity duplicate(Archetype<_Flags, _Storage, _Components...>& archetype, Entity);													// _Allowed as in, can not compress and has entities
			static void save(IStream&, void*, const Archetype<_Flags, _Storage, _Components...>&) {}													// _Allowed as in, can serialize
			static void saveSingle(IStream&, void*, const Archetype<_Flags, _Storage, _Components...>&, EntityId_t) {}									// _Allowed as in, can serialize
			static void load(IStream&, void*, Archetype<_Flags, _Storage, _Components...>&, uint32_t) {}												// _Allowed as in, can serialize
			static void loadSingle(IStream&, void*, Archetype<_Flags, _Storage, _Components...>&, EntityId_t) {}										// _Allowed as in, can serialize
		};

		// auto compress after N calls
		template<bool _Enabled> class AutoCompressNCalls {
			uint32_t		m_nCalls = 10000;			// completely arbitrary, applications should set this
			uint32_t		m_currentCalls = 0;
		public:
			inline bool compressNCalls() {
				if (m_currentCalls++ < m_nCalls)
					return false;
				m_currentCalls = 0;
				return true;
			}
			inline void setAutoCompressNCalls(uint32_t ncalls) { m_nCalls = ncalls; }
		};
		template<> struct AutoCompressNCalls<false> {
			constexpr bool compressNCalls() { return false; }
		};

		// auto compress after threshold size
		template<bool _Enabled> class AutoCompressFreeThreshold {
			float			m_threshold = 0.25f;		// completely arbitrary, applications should set this
		public:
			inline bool compressFreeThreshold(float freeRatio) { return freeRatio >= m_threshold; }
			inline void setAutoCompressThreshold(float threshold) { m_threshold = threshold; }
		};
		template<> struct AutoCompressFreeThreshold<false> {
			inline constexpr bool compressFreeThreshold(float) { return false; }
		};

		// auto reserve when less than N left
		template<bool _Enabled> class AutoReserveNLeft {
			uint32_t		m_nLeft = 1;				// completely arbitrary, applications should set this
		public:
			inline bool reserveNLeft(uint32_t left) { return left <= m_nLeft; }
			inline void setAutoReserveNLeft(uint32_t nleft) { m_nLeft = nleft; }
		};
		template<> struct AutoReserveNLeft<false> {
			inline constexpr bool reserveNLeft(uint32_t) { return false; }
		};


		// auto reserve when more than threshold full
		template<bool _Enabled> class AutoReserveFullThreshold {
			float		m_threshold = 0.75f;			// completely arbitrary, applications should set this
		public:
			inline bool reserveFullThreshold(float fullRatio) { return fullRatio >= m_threshold; }
			inline void setAutoReserveFullThreshold(float threshold) { m_threshold = threshold; }
		};
		template<> struct AutoReserveFullThreshold<false> {
			inline constexpr bool reserveFullThreshold(float) { return false; }
		};

		// saving a component into a stream
		template<bool _ShouldBeSerialized, bool _PodType, typename _Component> struct StreamHelper {
			void save(IStream&, void*, const _Component*, const _Component*, const EntityState_t*) {}
			void load(IStream&, void*, _Component*, _Component*, const EntityState_t*, uint8_t) {}
		};
		template<typename _Component> struct StreamHelper<true, false, _Component> {
			void save(IStream& stream, void* userdata, const _Component* begin, const _Component* end, const EntityState_t* state) {
				for (; begin < end; ++begin)
					if (!emptyFromState(*state++))
						begin->save(stream, userdata);
			}
			void load(IStream& stream, void* userdata, _Component* begin, _Component* end, const EntityState_t* state, uint8_t version) {
				for (; begin < end; ++begin)
					if (!emptyFromState(*state++))
						begin->load(stream, userdata, version);
			}
		};
		template<typename _Component> struct StreamHelper<true, true, _Component> {
			void save(IStream& stream, void*, const _Component* begin, const _Component* end, const EntityState_t*) {
				stream.write(begin, sizeof(_Component) * (end - begin));
			}
			void load(IStream& stream, void*, _Component* begin, _Component* end, const EntityState_t*, uint8_t version) {
				if (version != static_cast<uint8_t>(_Component::staticComponentInfo(StaticComponentInfo::Version)))
					throw InvalidPodDataVersionException();		// fix this by adding a conversion function for old to new in the type!
				stream.read(begin, sizeof(_Component) * (end - begin));
			}
		};

		// storage serialize helpers
		template<typename _T>
		void writeStorage(IStream& stream, const _T& vector) {
			const uint32_t count = static_cast<uint32_t>(vector.size());
			stream.write(&count, sizeof(count));
			stream.write(vector.begin(), sizeof(typename _T::value_type) * count);
		}

		template<typename _T>
		void readStorage(IStream& stream, _T& vector) {
			uint32_t count;
			stream.read(&count, sizeof(count));
			vector.clear();				// yes, reset all contents fully!!
			vector.resize(count);
			stream.read(vector.begin(), sizeof(typename _T::value_type) * count);
		}

		// change tracking
		template<bool _HaveChangeTracking>
		class ChangeTracker {
		public:
			void registerCreatedEntity(EntityId_t) {}
			void registerDeletedEntity(EntityId_t) {}
			void resetTrackedEntities() {}
			std::pair<Change*, Change*> trackedEntityChanges() { return {}; }
			void enableEntityTracking(bool) {}
		};

		template<>
		class ChangeTracker<true> {
		public:
			ChangeTracker() { m_changes.reserve(16); }
			void resetTrackedEntities() { 
				m_changes.clear(); 
			}
			std::pair<Change*, Change*> trackedEntityChanges() { return std::make_pair(m_changes.data(), m_changes.data() + m_changes.size()); }
			void enableEntityTracking(bool enabled) { m_enabled = enabled; }
		protected:
			void registerCreatedEntity(EntityId_t index) {
				if (m_enabled)
					m_changes.push_back({ index, ChangeType::Create });
			}
			void registerDeletedEntity(EntityId_t index) {
				if (m_enabled)
					m_changes.push_back({ index, ChangeType::Delete });
			}
		private:
			std::vector<Change> m_changes;
			bool				m_enabled = true;
		};
	}

	class IEcsListener {
	protected:
		virtual ~IEcsListener() = default;
	public:
		struct SerializationEvent {
			enum class Type : uint8_t {
				LoadStart = 1,
				LoadFinished,
				SaveStart,
				SaveFinished,
				ArchetypeStart,
				ArchetypeFinished,
				SaveComponent,
				LoadComponent,
			};
			Type			type;
			ArchetypeId_t	archetype = 0;
			uint32_t		version = 0;			// for whatever this event applies to, component, ECS, etc.
			uint32_t		componentCount = 0;
			uint64_t		componentMask = 0;
			const char*		name = nullptr;			// for whatever this event applies to, component, ECS, etc.
		};

		virtual void registeredArchetype(IArchetype* archetype) = 0;
		virtual void serializationEvent(const SerializationEvent& event) = 0;
	};

	class IArchetype {
	public:
		struct ComponentInfo {
			const char* name;
			uint32_t version;
			uint64_t mask;
			uint64_t flags;
			uint64_t requiredComponents;
		};

		virtual ~IArchetype() = default;

		// current state
		virtual size_t size() const = 0;
		virtual size_t capacity() const = 0;
		virtual void* componentBegin(uint64_t mask) = 0;
		virtual const EntityState_t* stateBegin() const = 0;
		virtual const EntityState_t* stateEnd() const = 0;
		virtual bool allowsEntities() const = 0;
		virtual bool validateId(EntityId_t id) const = 0;
		virtual bool extractIndex(EntityId_t id, EntityId_t& index) const = 0;

		// changing entities
		virtual EntityId_t create(EntityId_t requestedIndex = EntityInvalidIndex) = 0;
		virtual Entity createEntity() = 0;
		virtual Entity duplicateEntity(Entity entity) = 0;
		virtual void remove(EntityId_t index) = 0;
		virtual void removeEntity(Entity entity) = 0;
		virtual void compress() = 0;
		virtual void enlarge() = 0;
		virtual void performMaintenance() = 0;
		virtual void reset() = 0;

		// serialization
		virtual void save(IStream& stream, void* userdata) const = 0;
		virtual void load(IStream& stream, void* userdata, uint32_t version) = 0;
		virtual void saveSingle(IStream& stream, void* userdata, EntityId_t index) const = 0;
		virtual void loadSingle(IStream& stream, void* userdata, EntityId_t index) = 0;

		// tracking
		virtual std::pair<Change*, Change*> trackedEntityChanges() = 0;
		virtual void resetTrackedEntities() = 0;
		virtual void enableEntityTracking(bool enabled) = 0;

		// basic settings
		virtual ArchetypeId_t id() const = 0;
		virtual uint64_t mask() const = 0;
		virtual uint64_t flags() const = 0;
		virtual const char* name() const = 0;
		virtual const char* storageDescription() const = 0;
		virtual const ComponentInfo* componentInformation(uint64_t mask) const = 0;
		virtual size_t componentCount() const = 0;
		virtual const ComponentInfo* componentAt(size_t index) const = 0;
		virtual size_t singleEntitySize() const = 0;		/* sum of all components */

		// listener
		virtual void setListener(IEcsListener* listener) = 0;

		// in Visual Studio debug mode, add this member so we can show entity archetypes in natvis
#if DAE_ECS_NATVIS_DEBUG
	protected:
		const char* m_dbgName = nullptr;
#endif
	};

	namespace detail {
		DAE_ECS_API void validateComponentInfo(const IArchetype::ComponentInfo* info, size_t count);
	}

	class Entity {
	public:
		inline Entity() = default;
		inline Entity(EntityId_t id, IArchetype* list) : m_archetype(list), m_id(id) {
			DAE_ECS_ASSERT(!m_archetype || m_archetype->allowsEntities());
		}
#if !defined(NDEBUG)
		inline Entity(const Entity& rhs) : m_archetype(rhs.m_archetype), m_id(rhs.m_id), m_copyable(rhs.m_copyable) {
			DAE_ECS_ASSERT(rhs.m_copyable);
		}
		inline Entity& operator=(const Entity& rhs) {
			DAE_ECS_ASSERT(rhs.m_copyable);
			m_archetype = rhs.m_archetype;
			m_id = rhs.m_id;
			m_copyable = rhs.m_copyable;
			return *this;
		}
#else
		inline Entity(const Entity&) = default;
		inline Entity& operator=(const Entity& rhs) = default;
#endif
		inline Entity(EntityId_t id, IArchetype* list, bool copyable)
			: m_archetype(list)
			, m_id(id)
#if !defined(NDEBUG)
			, m_copyable(copyable)
#endif
		{
			(void)copyable;
		}
		inline Entity(Entity&&) = default;
		inline Entity& operator=(Entity&&) = default;

		inline bool operator==(const Entity& rhs) const { return m_archetype == rhs.m_archetype && m_id == rhs.m_id; }
		inline bool operator!=(const Entity& rhs) const { return m_archetype != rhs.m_archetype || m_id != rhs.m_id; }
		inline bool operator<(const Entity& rhs) const {
			if (m_archetype < rhs.m_archetype)
				return true;
			if (m_archetype > rhs.m_archetype)
				return false;
			return m_id < rhs.m_id;
		}
		inline IArchetype* archetype() const { return m_archetype; }
		inline ArchetypeId_t archetypeId() const { return m_archetype ? m_archetype->id() : ArchetypeId_t{}; }
		inline EntityId_t id() const { return m_id; }
		inline bool empty() const { return !m_archetype; }
		inline bool fullyValidate() const { return m_archetype && m_archetype->validateId(m_id); }

		/* Returns the component of the given type, if it's in the entity's archetype. Otherwise it
		 * returns nullptr.
		 */
		template<typename _Component> _Component* get() const;

		/* Returns the component of the given type. Checks with a standard C DAE_ECS_ASSERT that it exists. 
		 * Returns reference to component.
		 */
		template<typename _Component> inline _Component& fetch() const {
			_Component* component = this->get<_Component>();
			DAE_ECS_ASSERT(component);
			return *component;
		}

		/* Removes the entity from its archetype. */
		DAE_ECS_API void remove() const;

	private:
		mutable IArchetype* m_archetype = nullptr;
		mutable EntityId_t m_id = EntityInvalidId;
#if !defined(NDEBUG)
		bool m_copyable = true;
#endif
	};

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	class Archetype 
		: public IArchetype 
		, public detail::AutoCompressNCalls			<(_Flags & (ArchetypeFlagCompressableNoEntities | ArchetypeFlagAutoCompressNCalls))			== (ArchetypeFlagCompressableNoEntities | ArchetypeFlagAutoCompressNCalls)>
		, public detail::AutoCompressFreeThreshold	<(_Flags & (ArchetypeFlagCompressableNoEntities | ArchetypeFlagAutoCompressFreeThreshold))	== (ArchetypeFlagCompressableNoEntities | ArchetypeFlagAutoCompressFreeThreshold)>
		, public detail::AutoReserveNLeft			<(_Flags & ArchetypeFlagAutoReserveNLeft)			&& (detail::And<_Storage<_Components>::canReallocate()...>::value)>
		, public detail::AutoReserveFullThreshold	<(_Flags & ArchetypeFlagAutoReserveFullThreshold)	&& (detail::And<_Storage<_Components>::canReallocate()...>::value)>
		, public detail::ChangeTracker				<!!(_Flags & ArchetypeFlagWithCreateDeleteTracking)>
	{
	public:
		using StorageTuple = std::tuple<_Storage<_Components>...>;		// all component storage objects for this archetype are in this single tuple
		using StorageState = _Storage<EntityState_t>;					// entity state bytes are in here
		using StorageFree = _Storage<EntityId_t>;						// free entity id's are in here
		using DefaultsTuple = std::tuple<_Components...>;				// default value for every component is in here
		enum : uint64_t { 
			Flags						= _Flags,
			AllowCompression			= !!(Flags & ArchetypeFlagCompressableNoEntities),
			AllowEntities				= !(Flags & ArchetypeFlagCompressableNoEntities),
			AllowSerialization			= !(Flags & ArchetypeFlagNeverSerialize),
			WithCreateDeleteTracking	= !!(Flags & ArchetypeFlagWithCreateDeleteTracking),
			NumComponents				= sizeof...(_Components),
		};
		Archetype(const char* name, ArchetypeId_t id);
		Archetype(Archetype&& rhs) = default;
		Archetype& operator=(Archetype && rhs) = default;
		~Archetype() = default;
		static constexpr uint64_t staticMask() { return detail::BitwiseOr<_Components::staticComponentInfo(StaticComponentInfo::Mask)...>::value; }
		template<typename... _IterateComponents, typename _Predicate> void forEach(const _Predicate& pr);					// iterates over this archetype's entities passing the specified components
		template<typename... _IterateComponents, typename _Predicate> void forEachWithEntity(const _Predicate& pr);			// iterates over this archetype's entities passing the entity + specified components
		template<typename _Predicate> void forEachEntity(const _Predicate& pr);												// iterates over this archetype's entities passing the entity
		template<typename _Component> _Component* iterateBegin() { return std::get<_Storage<_Component>>(m_tuple).begin(); }
		template<typename _Component> _Component* iterateEnd() { return iterateBegin<_Component>() + m_state.size(); }
		template<typename _Component> _Component* at(EntityId_t id) { 
			EntityId_t index;
			if (!extractIndex(id, index))
				return nullptr;
			return this->template iterateBegin<_Component>() + index;
		}
		const StorageTuple& storageTuple() const { return m_tuple; }
		const StorageState& storageState() const { return m_state; }
		const StorageFree& storageFree() const { return m_free; }
		void reserve(size_t capacity);
		inline DefaultsTuple& componentDefaults() { return m_defaults; }
		inline const DefaultsTuple& componentDefaults() const { return m_defaults; }
		template<typename _Component> inline const _Component& componentDefault() const { return std::get<_Component>(m_defaults); }
		template<typename _Component> inline _Component& componentDefault() { return std::get<_Component>(m_defaults); }
		inline IEcsListener* listener() const { return m_listener; }

		// IArchetype
		ArchetypeId_t id() const override { return m_id; }
		uint64_t mask() const override { return staticMask(); }
		uint64_t flags() const override { return Flags; }
		size_t size() const override { return m_state.size() - m_free.size(); }
		size_t capacity() const override { return m_state.capacity(); }
		const ComponentInfo* componentInformation(uint64_t mask) const override;
		size_t componentCount() const override { return NumComponents; }
		const ComponentInfo* componentAt(size_t index) const override { return &m_componentInfo[index]; }
		EntityId_t create(EntityId_t requestedIndex = EntityInvalidIndex) override;
		Entity createEntity() override;
		Entity duplicateEntity(Entity entity) override;
		void remove(EntityId_t index) override;
		void removeEntity(Entity entity) override;
		void* componentBegin(uint64_t mask) override;
		const EntityState_t* stateBegin() const override { return m_state.begin(); }
		const EntityState_t* stateEnd() const override { return m_state.end(); }
		void compress() override;
		void enlarge() override;
		bool allowsEntities() const override { return !!AllowEntities; }
		void performMaintenance() override;
		const char* name() const override { return m_name; }
		void save(IStream& stream, void* userdata) const override;
		void load(IStream& stream, void* userdata, uint32_t version) override;
		void saveSingle(IStream& stream, void* userdata, EntityId_t index) const override;
		void loadSingle(IStream& stream, void* userdata, EntityId_t index) override;
		std::pair<Change*, Change*> trackedEntityChanges() override { return this->detail::ChangeTracker<!!(_Flags & ArchetypeFlagWithCreateDeleteTracking)>::trackedEntityChanges(); }
		void resetTrackedEntities() override { this->detail::ChangeTracker<!!(_Flags & ArchetypeFlagWithCreateDeleteTracking)>::resetTrackedEntities(); }
		void enableEntityTracking(bool enabled) override { this->detail::ChangeTracker<!!(_Flags & ArchetypeFlagWithCreateDeleteTracking)>::enableEntityTracking(enabled);  }
		bool validateId(EntityId_t id) const override;
		bool extractIndex(EntityId_t id, EntityId_t& index) const override;
		size_t singleEntitySize() const override { return (sizeof(_Components) + ... + 0); }
		const char* storageDescription() const override { return decltype(m_state)::description(); }
		void setListener(IEcsListener* listener) override { m_listener = listener; }
		void reset() override;

	private:
		const char*		m_name = nullptr;
		ArchetypeId_t	m_id;
		StorageTuple	m_tuple;		// all storage in this tuple have the same size + capacity, just like...
		StorageState	m_state;		// ... this vector of state (empty + current version) values
		StorageFree		m_free;			// this vector is of any size (but never more than the above vectors capacity), and keeps track of empty slots indices for re-use
		DefaultsTuple	m_defaults;
		ComponentInfo	m_componentInfo[NumComponents];
		IEcsListener*	m_listener = nullptr;

		template<bool, uint64_t, template<typename> class, typename...> friend struct detail::ArchetypeHelper;
	};

	class Ecs {
	public:
		DAE_ECS_API Ecs(const char* name);
		DAE_ECS_API ~Ecs();

		/* Sets/gets name. String is duplicated internally */
		DAE_ECS_API void setName(const char* name);
		inline const char* name() const { return m_name; }

		/* Sets a listener */
		DAE_ECS_API void setListener(IEcsListener* listener);
		IEcsListener* listener() { return m_listener; }

		/* Register an archetype. The _Archetype type should be an instantiation of the Archetype<...> template. 
		 * It can be a class deriving from said template too.
		 */
		template<typename _Archetype> _Archetype& registerArchetype(const char* name, ArchetypeId_t id);

		/* Fetch the instance for a specific archetype */
		template<typename _Archetype> _Archetype& findArchetype();
		DAE_ECS_API IArchetype* findArchetype(uint64_t mask);

		/* Iterate over all archetypes that have at least the given components.
		 * The predicate should take the specified components in the same order
		 * by reference.
		 */
		template<typename... _IterateComponents, typename _Predicate> void forEach(const _Predicate& pr);

		/* Iterate over all archetypes that have at least the given components.
		 * The predicate should take an Entity first, and then the specified components in the same order
		 * by reference.
		 */
		template<typename... _IterateComponents, typename _Predicate> void forEachWithEntity(const _Predicate& pr);

		/* Gives begin iterators for every component, per archetype.
		 * The predicate should take a uint32_t count that indicates how many components there are,
		 * a const bool* that indicates which ones are empty,
		 * and the specified component iterators in the same order.
		 */
		template<typename... _IterateComponents, typename _Predicate> void iterators(const _Predicate& pr);

		/* Perform maintenance on the archetypes, can be called as often as desired. 
		 * Will automatically performed maintain as set during compile time.
		 */
		DAE_ECS_API void performMaintenance();

		/* Stores/Reads entire ECS state to/from stream. The user flags are passed to the components and can be
		 * set to anything by the calling code, the ECS does nothing with it other than passing it through.
		 * Could be used to indicate e.g. the serialization context.
		 */
		DAE_ECS_API void save(IStream& stream, void* userdata) const;
		DAE_ECS_API void load(IStream& stream, void* userdata);

		/* Resets the entire ECS. All state will be instantly thrown away. */
		DAE_ECS_API void reset();

		/* Fetching an archetype by its id */
		inline IArchetype* findArchetypeById(ArchetypeId_t id) { return m_archetypesById[id]; }

		/* Going over all archetypes. Not useful in real situations, but for debug stuff */
		template<typename _Predicate> void forEachArchetype(const _Predicate& pr) {
			for (const auto& archetype : m_archetypes)
				pr(archetype.archetype.get());
		}

		/* Debug stuff, dump information about the ECS to a dumper function. */
		enum class DumpMode {
			OneLine = 0,				// num archetypes, num entities only
			Normal,						// per archetype short info
		};
		DAE_ECS_API void dump(const std::function<void(const char*)>& dumper, DumpMode dumpMode);

		/* Returns the total amount of entities over all archetypes. Don't use too often. */
		DAE_ECS_API uint64_t countEntities() const;

	private:
		struct Entry {
			uint64_t					mask;
			std::unique_ptr<IArchetype>	archetype;
			bool operator<(const Entry& rhs) const { return mask < rhs.mask; }
		};
		DAE_ECS_API IArchetype* find(uint64_t mask);
		DAE_ECS_API void insert(uint64_t mask, std::unique_ptr<IArchetype> list);

		std::vector<Entry>				m_archetypes;
		IArchetype*						m_archetypesById[256];
		IEcsListener*					m_listener = nullptr;
		char*							m_name = nullptr;
	};

	//======================================================================================================
	// Entity
	//======================================================================================================

	template<typename _Component> 
	_Component* Entity::get() const {
		if (!m_archetype)
			return nullptr;
		EntityId_t index;
		if (!m_archetype->extractIndex(m_id, index))
			return nullptr;
		auto componentBegin = reinterpret_cast<_Component*>(m_archetype->componentBegin(_Component::staticComponentInfo(StaticComponentInfo::Mask)));
		return componentBegin ? componentBegin + index : nullptr;
	}

	//======================================================================================================
	// Archetype
	//======================================================================================================

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	Archetype<_Flags, _Storage, _Components...>::Archetype(const char* name, ArchetypeId_t id)
		: m_name(name)
		, m_id(id)
	{
		ComponentInfo* info = m_componentInfo;
		auto initInfo = [&](auto& storage) {

			using StorageType = std::remove_reference_t<std::remove_cv_t<decltype(storage)>>;
			using ComponentType = typename StorageType::value_type;
			// if this DAE_ECS_ASSERT fails, you're probably out of bits!
			static_assert(ComponentType::staticComponentInfo(StaticComponentInfo::Mask));
			auto& i = *info++;
			i.name = ComponentType::staticName();
			i.mask = ComponentType::staticComponentInfo(StaticComponentInfo::Mask);
			i.flags = ComponentType::staticComponentInfo(StaticComponentInfo::Flags);
			i.version = static_cast<uint8_t>(ComponentType::staticComponentInfo(StaticComponentInfo::Version));
			i.requiredComponents = ComponentType::staticComponentInfo(StaticComponentInfo::RequiredComponents);

			// compile check per component if its required components are built into the archetype
			// if this fails, check what component is currently being compiled in the output (start from the std::get<> call below)
			// one of its required components is missing!
			static_assert((staticMask() & ComponentType::staticComponentInfo(StaticComponentInfo::RequiredComponents)) == ComponentType::staticComponentInfo(StaticComponentInfo::RequiredComponents));
		};
		(initInfo(std::get<_Storage<_Components>>(m_tuple)), ...);
		detail::validateComponentInfo(m_componentInfo, sizeof(m_componentInfo) / sizeof(m_componentInfo[0]));

#if DAE_ECS_NATVIS_DEBUG
		m_dbgName = m_name;
#endif
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	void Archetype<_Flags, _Storage, _Components...>::reserve(size_t capacity) {
		(std::get<_Storage<_Components>>(m_tuple).reserve(capacity), ...);
		m_state.reserve(capacity);
		m_free.reserve(capacity);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	const IArchetype::ComponentInfo* Archetype<_Flags, _Storage, _Components...>::componentInformation(uint64_t mask) const {
		for (const auto& info : m_componentInfo)
			if (info.mask == mask)
				return &info;
		return nullptr;
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	EntityId_t Archetype<_Flags, _Storage, _Components...>::create(EntityId_t requestedIndex) {
		enum {
			CreateNew,
			PopLastFree,
			PopSpecificFree,
		} action;
		if (requestedIndex == EntityInvalidIndex) {
			if (m_free.empty())
				action = CreateNew;
			else
				action = PopLastFree;
		} else {
			if (requestedIndex == m_state.size())
				action = CreateNew;
			else
				action = PopSpecificFree;
		}

		EntityId_t index;
		switch (action) {
		case CreateNew:
			// NOTE: even though they are probably vectors, do _NOT_ let the vectors re-allocate
			// during this call, instead return invalid entities to indicate the archetype is full
			if (m_state.size() == m_state.capacity())
				return (EntityId_t)EntityInvalidId;
			index = (EntityId_t)m_state.size();
			(std::get<_Storage<_Components>>(m_tuple).emplace_back(componentDefault<_Components>()), ...);
			m_state.push_back(EntityIndexVersionStart);
			break;
		case PopLastFree:
			index = m_free.back();
			m_free.pop_back();
			// NOTE: version was already bumped when removing, double check it was marked removed properly
			DAE_ECS_ASSERT(detail::emptyFromState(m_state[index]));
			m_state[index] = detail::versionFromState(m_state[index]);						// strip empty from version
			break;
		case PopSpecificFree:
		default:				// to stop compilers from whining
			int64_t i = (int64_t)(m_free.size() - 1);
			for (; i >= 0; --i)
				if (m_free[i] == requestedIndex)
					break;
			if (i < 0)
				throw InvalidRequestedIndexException();
			index = requestedIndex;
			m_free[i] = m_free.back();
			m_free.pop_back();
			DAE_ECS_ASSERT(detail::emptyFromState(m_state[index]));
			m_state[index] = detail::versionFromState(m_state[index]);						// strip empty from version
			break;
		}

		// build final id
		DAE_ECS_ASSERT(!detail::emptyFromState(m_state[index]));
		const EntityId_t id = index | (m_state[index] << EntityIndexVersionShift);
		DAE_ECS_ASSERT(id != 0);
		this->registerCreatedEntity(id);
		return id;
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	void Archetype<_Flags, _Storage, _Components...>::remove(EntityId_t id) {
		EntityId_t index;
		if (!extractIndex(id, index))														// we're bumping the version after this call, so deleting it a second time will be blocked by this
			return;
		DAE_ECS_ASSERT(!detail::emptyFromState(m_state[index]));
		this->registerDeletedEntity(id);
		EntityIndexVersion_t newVersion = detail::versionFromState(m_state[index] + 1);		// automatically wrap at correct size (in bits)
		if (!newVersion)																	// never allow version 0, so id is never 0
			newVersion = 1;
		m_state[index] = detail::stateFromVersionAndEmpty(newVersion, true);				// apply empty bit
		m_free.push_back(index);
		(detail::PreDestroyCaller<_Components>::call(&std::get<_Storage<_Components>>(m_tuple)[index]), ...);
		(detail::ComponentCleaner<_Components>::call(&std::get<_Storage<_Components>>(m_tuple)[index], componentDefault<_Components>()), ...);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	Entity Archetype<_Flags, _Storage, _Components...>::createEntity() {
		return detail::ArchetypeHelper<AllowEntities, _Flags, _Storage, _Components...>::create(*this);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	Entity Archetype<_Flags, _Storage, _Components...>::duplicateEntity(Entity entity) {
		return detail::ArchetypeHelper<AllowEntities, _Flags, _Storage, _Components...>::duplicate(*this, entity);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void Archetype<_Flags, _Storage, _Components...>::removeEntity(Entity entity) {
#if !defined(NDEBUG)
		if (entity.archetype() != this)
			throw InvalidEntityException();
#endif
		remove(entity.id());
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void* Archetype<_Flags, _Storage, _Components...>::componentBegin(uint64_t mask) {
		return detail::ComponentBegin<StorageTuple, _Storage, _Components...>::get(m_tuple, mask); 
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void Archetype<_Flags, _Storage, _Components...>::compress() {
		detail::ArchetypeHelper<AllowCompression, _Flags, _Storage, _Components...>::compress(*this);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	void Archetype<_Flags, _Storage, _Components...>::enlarge() {
		const size_t newCapacity = capacity() * 2;
		(std::get<_Storage<_Components>>(m_tuple).reserve(newCapacity), ...);
		m_state.reserve(newCapacity);
		m_free.reserve(newCapacity);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void Archetype<_Flags, _Storage, _Components...>::performMaintenance() {
		if (this->compressNCalls() || 
			this->compressFreeThreshold((float)m_free.size() / (float)m_state.capacity()))
			compress();

		if (this->reserveNLeft(static_cast<uint32_t>(capacity() - size())) ||
			this->reserveFullThreshold((float)m_state.size() / (float)m_state.capacity()))
			enlarge();
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	template<typename... _IterateComponents, typename _Predicate> 
	void Archetype<_Flags, _Storage, _Components...>::forEach(const _Predicate& pr) {
		const uint64_t iterateComponentMask = detail::BitwiseOr<_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask)...>::value;
		static_assert((staticMask() & iterateComponentMask) == iterateComponentMask, "Components requested that are not in this archetype");

		using IteratorTuple 				= std::tuple<typename _Storage<_IterateComponents>::iterator...>;
		const EntityState_t* 	stateIt		= m_state.begin();
		const EntityState_t*	stateEnd	= m_state.end();
		
		if (stateIt != stateEnd) {
			IteratorTuple it = { std::get<_Storage<_IterateComponents>>(m_tuple).begin()... };
			do {
				if (!detail::emptyFromState(*stateIt++))
					pr(*std::get<typename _Storage<_IterateComponents>::iterator>(it)...);
				(++std::get<typename _Storage<_IterateComponents>::iterator>(it), ...);
			} while (stateIt != stateEnd);
		}
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	template<typename... _IterateComponents, typename _Predicate> 
	void Archetype<_Flags, _Storage, _Components...>::forEachWithEntity(const _Predicate& pr) {
		const uint64_t iterateComponentMask = detail::BitwiseOr<_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask)...>::value;
		static_assert((staticMask() & iterateComponentMask) == iterateComponentMask, "Components requested that are not in this archetype");

		using IteratorTuple 				= std::tuple<typename _Storage<_IterateComponents>::iterator...>;
		const EntityState_t*	stateIt 	= m_state.begin();
		const EntityState_t*	stateEnd	= m_state.end();
		
		if (stateIt != stateEnd) {
			IteratorTuple it			= { std::get<_Storage<_IterateComponents>>(m_tuple).begin()... };
			const bool copyable	= allowsEntities();
			EntityId_t index = 0;
			do {
				if (!detail::emptyFromState(*stateIt))
					pr(Entity{ detail::idFromIndexAndState(index, *stateIt), this, copyable }, *std::get<typename _Storage<_IterateComponents>::iterator>(it)...);
				(++std::get<typename _Storage<_IterateComponents>::iterator>(it), ...);
				++index;
				++stateIt;
			} while (stateIt != stateEnd);
		}
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
	template<typename _Predicate>
	void Archetype<_Flags, _Storage, _Components...>::forEachEntity(const _Predicate& pr) {
		const EntityState_t* stateIt = m_state.begin();
		const EntityState_t* stateEnd = m_state.end();
		EntityId_t index = 0;
		const bool copyable = allowsEntities();
		for (; stateIt < stateEnd; ++stateIt, ++index) {
			if (detail::emptyFromState(*stateIt))
				continue;
			const auto id = detail::idFromIndexAndState(index, detail::versionFromState(*stateIt));
			pr({ id, this, copyable });
		}
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void Archetype<_Flags, _Storage, _Components...>::save(IStream& stream, void* userdata) const {
		detail::ArchetypeHelper<AllowSerialization, _Flags, _Storage, _Components...>::save(stream, userdata, *this);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void Archetype<_Flags, _Storage, _Components...>::saveSingle(IStream& stream, void* userdata, EntityId_t index) const {
		detail::ArchetypeHelper<AllowSerialization, _Flags, _Storage, _Components...>::saveSingle(stream, userdata, *this, index);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void Archetype<_Flags, _Storage, _Components...>::load(IStream& stream, void* userdata, uint32_t version) {
		detail::ArchetypeHelper<AllowSerialization, _Flags, _Storage, _Components...>::load(stream, userdata, *this, version);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void Archetype<_Flags, _Storage, _Components...>::loadSingle(IStream& stream, void* userdata, EntityId_t index) {
		detail::ArchetypeHelper<AllowSerialization, _Flags, _Storage, _Components...>::loadSingle(stream, userdata, *this, index);
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	bool Archetype<_Flags, _Storage, _Components...>::validateId(EntityId_t id) const {
		const EntityId_t index = id & EntityIndexMask;
		if (index >= m_state.size())
			return false;
		const EntityIndexVersion_t version = detail::versionFromId(id);
		const EntityIndexVersion_t stateVersion = detail::versionFromState(m_state[index]);
		const bool empty = detail::emptyFromState(m_state[index]);
		return index < m_state.size() && version == stateVersion && !empty;
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	bool Archetype<_Flags, _Storage, _Components...>::extractIndex(EntityId_t id, EntityId_t& index) const {
		index = id & EntityIndexMask;
		if (index >= m_state.size())
			return false;
		const bool empty = detail::emptyFromState(m_state[index]);
		if (empty)
			return false;
		const EntityIndexVersion_t version = detail::versionFromId(id);
		const EntityIndexVersion_t stateVersion = detail::versionFromState(m_state[index]);
		return version == stateVersion;
	}

	template<uint64_t _Flags, template<typename> class _Storage, typename ..._Components>
	void Archetype<_Flags, _Storage, _Components...>::reset() {
		// reset component tuples
		auto resetComponent = [](auto& comp) {
			comp.clear();
		};
		(resetComponent(std::get<_Storage<_Components>>(m_tuple)), ...);

		// reset free + state vectors
		m_state.clear();
		m_free.clear();

		// reset tracked entities
		resetTrackedEntities();
	}

	//======================================================================================================
	// Ecs
	//======================================================================================================

	template<typename _Archetype>
	_Archetype& Ecs::registerArchetype(const char* name, ArchetypeId_t id) {
		static_assert(detail::ExtendsFrom<IArchetype, _Archetype>::value, "Requested type inappropriate");
		const uint64_t mask = _Archetype::staticMask();
		if (find(mask))
			throw DuplicateArchetypeException();
		auto archetype = std::make_unique<_Archetype>(name, id);
		auto* res = archetype.get();
		insert(mask, std::move(archetype));
		if (m_listener)
			m_listener->registeredArchetype(res);
		res->setListener(m_listener);
		return *res;
	}

	template<typename _Archetype>
	_Archetype& Ecs::findArchetype() {
		static_assert(detail::ExtendsFrom<IArchetype, _Archetype>::value, "Requested type inappropriate");
		auto archetype = find(_Archetype::staticMask());
		if (!archetype)
			throw UnregisteredArchetypeException();
		return reinterpret_cast<_Archetype&>(*archetype);
	}

	template<typename... _IterateComponents, typename _Predicate> 
	void Ecs::forEach(const _Predicate& pr) {
		const uint64_t componentMask = detail::BitwiseOr<_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask)...>::value;
		std::tuple<_IterateComponents*...> tuple;
		const auto set = [](auto& tgt, void* ptr) { tgt = reinterpret_cast<decltype(tgt)>(ptr); };
		for (const auto& archetype : m_archetypes) {
			if ((archetype.mask & componentMask) == componentMask) {
				(set(std::get<_IterateComponents*>(tuple), archetype.archetype->componentBegin(_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask))), ...);
				for(const EntityState_t* state = archetype.archetype->stateBegin(), *end = archetype.archetype->stateEnd(); state != end; /* done in loop */) {
					if (!detail::emptyFromState(*state++))
						pr(*std::get<_IterateComponents*>(tuple)...);
					(++std::get<_IterateComponents*>(tuple), ...);
				}
			}
		}
	}

	template<typename... _IterateComponents, typename _Predicate> 
	void Ecs::forEachWithEntity(const _Predicate& pr) {
		const uint64_t componentMask = detail::BitwiseOr<_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask)...>::value;
		std::tuple<_IterateComponents*...> tuple;
		const auto set = [](auto& tgt, void* ptr) { tgt = reinterpret_cast<decltype(tgt)>(ptr); };
		for (const auto& entry : m_archetypes) {
			if ((entry.mask & componentMask) == componentMask) {
				const bool copyable = entry.archetype->allowsEntities();
				(set(std::get<_IterateComponents*>(tuple), entry.archetype->componentBegin(_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask))), ...);
				EntityId_t index = 0;
				for(const EntityState_t* state = entry.archetype->stateBegin(), *end = entry.archetype->stateEnd(); state != end; /* done in loop */) {
					if (!detail::emptyFromState(*state))
						pr(Entity{detail::idFromIndexAndState(index, *state), entry.archetype.get(), copyable }, *std::get<_IterateComponents*>(tuple)...);
					++index;
					++state;
					(++std::get<_IterateComponents*>(tuple), ...);
				}
			}
		}
	}

	template<typename... _IterateComponents, typename _Predicate> 
	void Ecs::iterators(const _Predicate& pr) {
		const uint64_t componentMask = detail::BitwiseOr<_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask)...>::value;
		std::tuple<_IterateComponents*...> begin;
		const auto set = [](auto& tgt, void* ptr) { tgt = reinterpret_cast<decltype(tgt)>(ptr); };
		for (const auto& archetype : m_archetypes) {
			if ((archetype.mask & componentMask) == componentMask) {
				const EntityState_t	*state = archetype.archetype->stateBegin(), 
									*end = archetype.archetype->stateEnd();
				uint32_t count = static_cast<uint32_t>(end - state);
				if (count) {
					(set(std::get<_IterateComponents*>(begin), archetype.archetype->componentBegin(_IterateComponents::staticComponentInfo(StaticComponentInfo::Mask))), ...);
					pr(count, state, std::get<_IterateComponents*>(begin)..., archetype.archetype.get());
				}
			}
		}
	}

	//======================================================================================================
	// detail
	//======================================================================================================

	namespace detail {

		template<typename _Tuple, template<typename> class _Storage, typename _Head>
		inline void SetEntity<_Tuple, _Storage, _Head>::set(_Tuple& tuple, const Entity& entity) {
			SetEntityImpl<_Head, HasSetEntityMethod<_Head>::value>::call(std::get<_Storage<_Head>>(tuple)[detail::indexFromId(entity.id())], entity);
		}

		template<typename _Tuple, template<typename> class _Storage, typename _Head, typename ..._Tail>
		inline void SetEntity<_Tuple, _Storage, _Head, _Tail...>::set(_Tuple& tuple, const Entity& entity) {
			SetEntityImpl<_Head, HasSetEntityMethod<_Head>::value>::call(std::get<_Storage<_Head>>(tuple)[detail::indexFromId(entity.id())], entity);
			SetEntity<_Tuple, _Storage, _Tail...>::set(tuple, entity);
		}

		template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		void ArchetypeHelper<false, _Flags, _Storage, _Components...>::compress(Archetype<_Flags, _Storage, _Components...>& archetype) {
			// if compression is not allowed, this special case is the only thing we can do...
			const size_t startCount = archetype.size();
			if (!startCount) {
				archetype.m_free.clear();
				archetype.m_state.clear();
				(std::get<_Storage<_Components>>(archetype.m_tuple).clear(), ...);
				return;
			}
		}

		template<bool _Allowed, uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		void ArchetypeHelper<_Allowed, _Flags, _Storage, _Components...>::compress(Archetype<_Flags, _Storage, _Components...>& archetype) {
			//
			// NOTE: 	with the current setup of what entities are, this call is useless, as it 
			//			makes all entities invalid, and there is no way of finding an entity again...
			//
			if (archetype.m_free.empty())
				return;

			// special case, everything is empty
			const size_t startCount = archetype.size();
			if (!startCount) {
				archetype.m_free.clear();
				archetype.m_state.clear();
				(std::get<_Storage<_Components>>(archetype.m_tuple).clear(), ...);
				return;
			}

			// sort the free indexes so that they are in order
			std::sort(archetype.m_free.begin(), archetype.m_free.end());
			auto freeBegin = archetype.m_free.begin(),
				freeEnd = archetype.m_free.end();
			const auto move = [](auto& tgt, auto& src) { tgt = std::move(src); };

			do {
				// we only get here if there is at least one free index left;
				// while the last free index is at the end of the list, just remove the free index and the entry by lowering the end iterators
				for (;;) {
					const size_t lastFreeIndex = *(freeEnd - 1);
					if (lastFreeIndex != archetype.m_state.size() - 1)
						break;
					--freeEnd;
					archetype.m_state.pop_back();
					(std::get<_Storage<_Components>>(archetype.m_tuple).pop_back(), ...);
					if (freeBegin == freeEnd)
						goto exit;
				}

				// we only get here if the last free index is NOT at the end of the list
				// so move the last entry from the tuple into the LOWEST free slot, this way
				// it will only be moved ONCE
				(move(std::get<_Storage<_Components>>(archetype.m_tuple)[*freeBegin],
					  std::get<_Storage<_Components>>(archetype.m_tuple).back()), ...);
				detail::SetEntity<typename Archetype<_Flags, _Storage, _Components...>::StorageTuple, _Storage, _Components...>::set(archetype.m_tuple, { *freeBegin, &archetype, true /* dangerous */ });
				archetype.m_state[*freeBegin] = false;
				++freeBegin;
				archetype.m_state.pop_back();
				(std::get<_Storage<_Components>>(archetype.m_tuple).pop_back(), ...);
			} while (freeBegin != freeEnd);
		exit:
			archetype.m_free.clear();
			DAE_ECS_ASSERT(startCount == archetype.size());
		}

		template<bool _Allowed, uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		Entity ArchetypeHelper<_Allowed, _Flags, _Storage, _Components...>::create(Archetype<_Flags, _Storage, _Components...>& archetype) {
			const EntityId_t id = archetype.create();
			if (id == EntityInvalidId)
				return {};
			Entity e{ id, &archetype };
			detail::SetEntity<typename Archetype<_Flags, _Storage, _Components...>::StorageTuple, _Storage, _Components...>::set(archetype.m_tuple, e);
			return e;
		}

		template<bool _Allowed, uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		Entity ArchetypeHelper<_Allowed, _Flags, _Storage, _Components...>::duplicate(Archetype<_Flags, _Storage, _Components...>& archetype, Entity other) {
			if (other.empty() || other.archetype() != &archetype)
				return {};
			EntityId_t otherIndex;
			if (!archetype.extractIndex(other.id(), otherIndex))
				return {};
			const EntityId_t id = archetype.create();
			if (id == EntityInvalidId)
				return {};
			detail::CopyComponents<typename Archetype<_Flags, _Storage, _Components...>::StorageTuple, _Storage, _Components...>::copy(archetype.m_tuple, detail::indexFromId(id), otherIndex);

			Entity e{ id, &archetype };
			detail::SetEntity<typename Archetype<_Flags, _Storage, _Components...>::StorageTuple, _Storage, _Components...>::set(archetype.m_tuple, e);
			return e;
		}

		template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		Entity ArchetypeHelper<false, _Flags, _Storage, _Components...>::create(Archetype<_Flags, _Storage, _Components...>&) {
			return {};
		}

		template<uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		Entity ArchetypeHelper<false, _Flags, _Storage, _Components...>::duplicate(Archetype<_Flags, _Storage, _Components...>&, Entity) {
			return {};
		}

		template<bool _Allowed, uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		void ArchetypeHelper<_Allowed, _Flags, _Storage, _Components...>::save(IStream& stream, void* userdata, const Archetype<_Flags, _Storage, _Components...>& archetype) {
			static_assert(!(_Flags & ArchetypeFlagNeverSerialize));
			const size_t stateCount = archetype.m_state.size(); (void)stateCount;
			detail::writeStorage(stream, archetype.m_state);
			detail::writeStorage(stream, archetype.m_free);

			const EntityState_t* stateIt = archetype.stateBegin();
			auto streamComponent = [&](const auto& storage) {
				using StorageType = std::remove_reference_t<std::remove_cv_t<decltype(storage)>>;
				using ComponentType = typename StorageType::value_type;

				// check the amount of components, if empty do absolutely nothing
				const uint32_t count = static_cast<uint32_t>(storage.size());
				if (!count)
					return;

				// write header info
				const char* const compName = ComponentType::staticName();
				DAE_ECS_ASSERT(compName);
				const size_t compNameLen = strlen(compName);
				DAE_ECS_ASSERT(compNameLen);				// must be non-empty
				DAE_ECS_ASSERT(compNameLen <= 255u);		// length must fit in 1 byte
				const uint8_t compNameLen8 = (uint8_t)compNameLen;
				stream.write(&compNameLen8, sizeof(compNameLen8));
				stream.write(compName, compNameLen8);
				const uint8_t compVersion = static_cast<uint8_t>(ComponentType::staticComponentInfo(StaticComponentInfo::Version));
				stream.write(&compVersion, sizeof(compVersion));

				// store position we'll be writing the size
				const auto sizePosition = stream.position();
				uint32_t totalSize = 0;
				stream.write(&totalSize, sizeof(totalSize));
				{
					// serialize the components
					DAE_ECS_ASSERT(count == stateCount); (void)count;
					if (archetype.listener())
						archetype.listener()->serializationEvent({ IEcsListener::SerializationEvent::Type::SaveComponent, 
																 archetype.id(), 
																 static_cast<uint8_t>(ComponentType::staticComponentInfo(StaticComponentInfo::Version)), 
																 (uint32_t)storage.size(),
																 ComponentType::staticComponentInfo(StaticComponentInfo::Mask),
																 ComponentType::staticName() });
					detail::StreamHelper<!(ComponentType::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagNeverSerialize), 
										std::is_pod_v<ComponentType> || !!(ComponentType::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagSerializeAsPODType), 
										ComponentType>()
						.save(stream, userdata, storage.begin(), storage.end(), stateIt);
				}
				
				// overwrite the size
				const auto finalPosition = stream.position();
				{
					auto size = (finalPosition - sizePosition);
					if (size > std::numeric_limits<decltype(totalSize)>::max())
						throw TooLargeComponentException();
					totalSize = (uint32_t)size;
				}
				stream.setPosition(sizePosition);
				stream.write(&totalSize, sizeof(totalSize));
				stream.setPosition(finalPosition);
			};
			(streamComponent(std::get<_Storage<_Components>>(archetype.m_tuple)), ...);
			const uint8_t nul = 0;
			stream.write(&nul, sizeof(nul));	// informs loader no more components coming!
		}

		template<bool _Allowed, uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		void ArchetypeHelper<_Allowed, _Flags, _Storage, _Components...>::saveSingle(IStream& stream, void* userdata, const Archetype<_Flags, _Storage, _Components...>& archetype, EntityId_t id) {
			static_assert(!(_Flags & ArchetypeFlagNeverSerialize));
			EntityId_t index;
			if (!archetype.extractIndex(id, index))
				return;
			const EntityState_t state = (id & EntityIndexVersionMask) >> EntityIndexVersionShift;
			auto streamComponent = [&](const auto& storage) {
				using StorageType = std::remove_reference_t<std::remove_cv_t<decltype(storage)>>;
				using ComponentType = typename StorageType::value_type;
				if (archetype.listener())
					archetype.listener()->serializationEvent({ IEcsListener::SerializationEvent::Type::SaveComponent,
															 archetype.id(),
															 static_cast<uint8_t>(ComponentType::staticComponentInfo(StaticComponentInfo::Version)),
															 (uint32_t)1,
															 ComponentType::staticComponentInfo(StaticComponentInfo::Mask),
															 ComponentType::staticName() });
				detail::StreamHelper<!(ComponentType::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagNeverSerialize), 
									 std::is_pod_v<ComponentType> || !!(ComponentType::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagSerializeAsPODType), 
									 ComponentType>()
					.save(stream, userdata, storage.begin() + index, storage.begin() + (index + 1), &state);
			};
			(streamComponent(std::get<_Storage<_Components>>(archetype.m_tuple)), ...);
		}

		template<bool _Allowed, uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		void ArchetypeHelper<_Allowed, _Flags, _Storage, _Components...>::load(IStream& stream, void* userdata, Archetype<_Flags, _Storage, _Components...>& archetype, uint32_t version) {
			static_assert(!(_Flags & ArchetypeFlagNeverSerialize));
			detail::readStorage(stream, archetype.m_state);
			detail::readStorage(stream, archetype.m_free);

			// first, resize ALL components to the same size as what state is, so that regardless
			// of the component storage being deserialized, it actually has the right size
			const EntityState_t* stateIt = archetype.stateBegin();
			auto resizeComponent = [&](auto& storage) {
				storage.clear();											// this is mega important in case we unserialize over existing data, clear out components first!
				storage.resize(archetype.m_state.size());					// then set required size
			};
			(resizeComponent(std::get<_Storage<_Components>>(archetype.m_tuple)), ...);

			// set entity on all components that are alive
			EntityId_t index = 0;
			for (auto it = stateIt, end = archetype.stateEnd(); it < end; ++it, ++index) {
				if (!detail::emptyFromState(*it)) {
					const EntityId_t id = detail::idFromIndexAndState(index, *it);
					const Entity e{ id, &archetype };
					detail::SetEntity<typename Archetype<_Flags, _Storage, _Components...>::StorageTuple, _Storage, _Components...>::set(archetype.m_tuple, e);
				}
			}

			// now keep going over states until we find an empty name, which signals
			// the end of the stream
			do {
				uint8_t dataCompNameLen = 0;
				stream.read(&dataCompNameLen, sizeof(dataCompNameLen));
				if (!dataCompNameLen)
					break;

				// read the component name, we're going to find this in the archetype
				char dataCompName[256];
				stream.read(dataCompName, dataCompNameLen);
				dataCompName[dataCompNameLen] = 0;

				// read the component serialization version
				uint8_t dataCompVersion = 0;
				stream.read(&dataCompVersion, sizeof(dataCompVersion));

				// read the total binary size, in case we need to skip the data
				bool canSkip = version >= 2;
				uint32_t dataSize = 0;
				if (canSkip) {
					stream.read(&dataSize, sizeof(dataSize));
					dataSize -= sizeof(uint32_t);
				}
				const auto startPos = stream.position();

				// go over all archetypes and see if the name fits
				bool loaded = false;
				auto streamComponent = [&](auto& storage) {
					using StorageType = std::remove_reference_t<std::remove_cv_t<decltype(storage)>>;
					using ComponentType = typename StorageType::value_type;
					
					const char* const compName = ComponentType::staticName();
					DAE_ECS_ASSERT(compName);
					const size_t compNameLen = strlen(compName); (void)compNameLen;
					DAE_ECS_ASSERT(compNameLen);				// must be non-empty
					if (!strcmp(compName, dataCompName)) {
						if (loaded)
							throw InvalidDataStreamException();
						loaded = true;
						if (archetype.listener())
							archetype.listener()->serializationEvent({ IEcsListener::SerializationEvent::Type::LoadComponent,
																	 archetype.id(),
																	 dataCompVersion,
																	 (uint32_t)storage.size(),
																	 ComponentType::staticComponentInfo(StaticComponentInfo::Mask),
																	 ComponentType::staticName() });
						detail::StreamHelper<!(ComponentType::staticComponentInfo(StaticComponentInfo::Flags)& ComponentFlagNeverSerialize),
							std::is_pod_v<ComponentType> || !!(ComponentType::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagSerializeAsPODType),
							ComponentType>()
							.load(stream, userdata, storage.begin(), storage.end(), stateIt, dataCompVersion);
					}
				};
				(streamComponent(std::get<_Storage<_Components>>(archetype.m_tuple)), ...);

				// if we weren't loaded, skip over the data now
				if (!loaded) {
					if (canSkip)
						stream.skip(dataSize);
					else
						throw CannotSkipComponentException();
				} else if (canSkip) {
					// verify the component loader behaved, if not, skip over bytes that weren't read
					const auto curPos = stream.position();
					const auto bytesRead = curPos - startPos;
					if (bytesRead < dataSize) {
						stream.skip(dataSize - bytesRead);
					} else if (bytesRead > dataSize) {
						throw InvalidDataStreamException();
					}
				} else {
					// we have no way of knowing if the loader behaved...
				}
			} while (true);
		}

		template<bool _Allowed, uint64_t _Flags, template<typename> class _Storage, typename... _Components>
		void ArchetypeHelper<_Allowed, _Flags, _Storage, _Components...>::loadSingle(IStream& stream, void* userdata, Archetype<_Flags, _Storage, _Components...>& archetype, EntityId_t id) {
			static_assert(!(_Flags & ArchetypeFlagNeverSerialize));
			EntityId_t index;
			if (!archetype.extractIndex(id, index))
				return;

			const EntityState_t state = (id & EntityIndexVersionMask) >> EntityIndexVersionShift;
			auto streamComponent = [&](auto& storage) {
				using StorageType = std::remove_reference_t<std::remove_cv_t<decltype(storage)>>;
				using ComponentType = typename StorageType::value_type;
				if (archetype.listener())
					archetype.listener()->serializationEvent({ IEcsListener::SerializationEvent::Type::SaveComponent,
															 archetype.id(),
															 static_cast<uint8_t>(ComponentType::staticComponentInfo(StaticComponentInfo::Version)),
															 (uint32_t)1,
															 ComponentType::staticComponentInfo(StaticComponentInfo::Mask),
															 ComponentType::staticName() });
				detail::StreamHelper<!(ComponentType::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagNeverSerialize), 
									 std::is_pod_v<ComponentType> || !!(ComponentType::staticComponentInfo(StaticComponentInfo::Flags) & ComponentFlagSerializeAsPODType), 
									 ComponentType>()
					.load(stream, userdata, storage.begin() + index, storage.begin() + (index + 1), &state, static_cast<uint8_t>(ComponentType::staticComponentInfo(StaticComponentInfo::Version)));
			};
			(streamComponent(std::get<_Storage<_Components>>(archetype.m_tuple)), ...);
		}

	}

DAE_NAMESPACE_END

#include <Advanced.hh>
