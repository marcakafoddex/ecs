# C++ Entity Component System

Overview:
- no external dependencies, just a C++17 compiler
  - tested with MSVC 2019, clang and gcc
  - test suite uses 2021-06-11 version of https://github.com/silentbicycle/greatest
- bitmask based components
  - currently hard limit of 64 different components 
- uses archetypes to define types of entities
  - defined at compile time only
  - archetypes cannot be altered at runtime
- ```std::vector``` based storage for cache friendly component iteration
  - also features statically sized array's for storage if a hard limit on entity count is desired
  - supports custom storage systems if desired
- supports (de)serialization of the entire ECS state
  - supports "best effort" solving when handling missing or new components when loading data from a different archetype configuration
- CMake build system, can be used to generate static and shared builds
- does not require a base class for components
  - requires a few static methods for compile time information in the component type
- offers a generic Entity class that can hold an entity of any archetype
  - features simple get (pointer), or fetch (reference, throw if non-existent) interface for all components on this entity class

# Building instructions

You need CMake to build this project.

```
mkdir build
cd build
cmake ..
make
cd bin
./ecs-example
```

# Components

Overview:
- a component is a struct or class that can be included in an archetype
- if a method with the following signature exists: ```void setEntity(const ecs::Entity&)```, then it will be called at construction time automatically to inform the component about the entity it belongs to, if so desired
- components can be configured to not be destructed and (re)constructed when reused, to save time (see ecs::ComponentFlagNoCleanComponent)
- components can be configured to have a method call before destruction (see ecs::ComponentFlagCallPreDestroy)
- components can be configured to serialize as a POD type (see ecs::ComponentFlagSerializeAsPODType)
- components can be configured to never serialize, in which case load/save don't need to exist (see ecs::ComponentFlagNeverSerialize)

For example:
```C++
struct PositionComponent {
  /* Mandatory static function that returns the name of this component. This is serialized, and later used to recognize the component. */
  static const char* staticName() { return "Position"; }

  /* Compile time static information function for the ECS system. We define the component mask, current serialization version and flags with this. */
  constexpr static uint64_t staticComponentInfo(ecs::StaticComponentInfo info) {
    switch (info) {
    case ecs::StaticComponentInfo::Version:            return 1;                          // serialization version, increase when changing
    case ecs::StaticComponentInfo::Mask:               return 0x0000000000000001;         // unique single bit mask that represents this component
    case ecs::StaticComponentInfo::Flags:              return ecs::ComponentFlagDefaults; // flags for our component
    case ecs::StaticComponentInfo::RequiredComponents: return 0;                          // mask of components we require to always be paired with us
    default:                                           return 0;
    }
  }

  /* Serialization function for this component. Writes data into the given stream. No need to write version information, this is done by the ECS automatically. */
  void save(ecs::IStream& stream, void* /*userdata*/) const {
    stream.write(position);
    stream.write(acceleration);
    stream.write(speed);
  }

  /* Deserialization function for this component. If you have serialized data with different version, then use the given version to determine what data to load. */
  void load(ecs::IStream& stream, void* /*userdata*/, uint8_t /*version*/) {
    stream.read(position);
    stream.read(acceleration);
    stream.read(speed);
  }

  /* Add your own methods here */

  /* Our data */
  float position;
  float acceleration;
  float speed;
  /* ... */
};

```

# Archetypes

Archetypes are combinations of components that make up an entity.
- archetypes are assembled **at compile time only**
  - hence, **at runtime**, you **cannot** remove a component from or add a component to an entity!
- archetypes store their data in a custom storage container
  - based on std::vector
  - or on std::array
  - or on whatever you want
  - the **storage always has a certain capacity**, which is **not always automatically increased** when required!
- archetypes are only known within an ECS instance after they have been registered
- archetypes can be made to auto compress (remove unneeded space) or auto reserve (allocate more space in the vectors when a certain size threshold is met)

For example:
```C++
class PositionComponent { /* ... */ };
class DrawComponent { /* ... */ };
class TimerComponent { /* ... */ };

using CarArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::FixedSizedArray<4>::Type, PositionComponent, DrawComponent>;
using GhostArchetype = ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::Vector, PositionComponent, DrawComponent, TimerComponent>;

ecs::Ecs ecs;
ecs.registerArchetype<CarArchetype>("car", /* unique id */ 1);
ecs.registerArchetype<GhostArchetype>("ghost", /* unique id */ 2);
```

There are a lot of methods you can override in this Archetype base class. For example, if you want to automatically reserve component instances for 256 entities, you could do:
```C++
class CarAchetype : public ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::FixedSizedArray<4>::Type, PositionComponent, DrawComponent> {
public:
  CarArchetype() : Archetype("car", /* id */ 1) {
    reserve(256);
  }
}
```

If you want to update e.g. a quad tree whenever a new entity is created, you could do:
```C++
class CarAchetype : public ecs::Archetype<ecs::ArchetypeFlagDefaults, ecs::storage::FixedSizedArray<4>::Type, PositionComponent, DrawComponent> {
public:
  CarArchetype() : Archetype("car", /* id */ 1) {}
  EntityId_t create(EntityId_t requestedIndex = EntityInvalidIndex) override {
    auto id = Archetype::create(requestedIndex);
    if (id != ecs::EntityInvalidId) {
      PositionComponent* pc = at<PositionComponent>(id);
      /* update quad tree */
    }
    return id;
  }

}
```

# (De)Serialization

You can (de)serialize the entire ECS, with all its entities and components into a custom stream. 
- implement the ecs::IStream interface, which is super straightforward
- no ecs::IStream implementation comes with this code, but one that reads and writes to file is easy to build
- when deserializing **archetypes are recognized by their ids**, if you try to load old data where different archetypes had different ids, **chaos will ensue**
- when you change the layout of a component, e.g. when you add or remove a component, the code does a best effort attempt at recognizing what you want
- when you change the serialization of a component, properly increase its serialization version (see ```staticComponentInfo```), and handle old versions in your ```load``` accordingly

# Allocation behavior

Archetypes have separate storages for internal state, and all of their components. These storages all have the same capacity. When a request for a new entity comes in and there are no more free entries to be reused, remaining capacity is used. **If there is no more capacity left, then an empty entity is returned!** This is to avoid causing dangling pointers during iteration. When you are iterating over components, you might need to construct new entities as a result of state from these components. However, if you create a component that would require a reallocation on the storages, then your iteration state will become invalid. For this reason, reserving more storage is an explicit call that you need to do manually.

Basically you can always reserve more space safely, if you are not inside an iteration.

For example, if you need to create a bunch of entities, and you can't (or won't) reserve beforehand, you can do this:
```C++
for (unsigned i = 0; i < ...; ++i) {
  auto entity = someArchetype.createEntity();
  if (entity.empty()) {
    someArchetype.enlarge();    // enlarge the capacity, i.e. reserve more space
    entity = someArchetype.createEntity();
    assert(!entity.empty());
  }
  /* ... use entity ... */
}
```
