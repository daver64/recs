# RECS - Really Explicit Component System

A lightweight, header-only Entity Component System (ECS) designed for high performance and SIMD-friendly data access.

## Features

- **Header-only**: Single file, no build system required
- **Archetype-based**: Entities with identical component sets are grouped for optimal cache locality
- **SIMD-friendly**: Chunk iteration provides direct array pointers for vectorized processing
- **Data-oriented**: Components stored in Structure of Arrays (SoA) layout
- **Type-safe**: Compile-time component type checking
- **Minimal overhead**: Fixed-size array storage, no hashing in hot paths
- **Generation-based entity IDs**: Safe entity handles that detect use-after-free
- **Query system**: Flexible filtering with include/exclude patterns
- **Resource management**: Global singleton storage for game state
- **Event system**: Callbacks for component lifecycle events
- **Move semantics**: Efficient World transfer with proper RAII

## Quick Start

```cpp
#include "recs.h"
using namespace recs;

// Define components as plain structs
struct Position { float x, y; };
struct Velocity { float vx, vy; };
struct Dead {};  // Tag component (zero-size marker)

int main() {
    World world;
    
    // Create entities
    Entity player = world.create();
    Entity enemy = world.create();
    
    // Add components (with optional initialization)
    world.add<Position>(player, 0.0f, 0.0f);
    world.add<Velocity>(player, 1.0f, 0.5f);
    world.add<Position>(enemy);
    
    // Direct component access
    if (auto* pos = world.get<Position>(player)) {
        pos->x = 10.0f;
    }
    
    // Check component presence
    if (world.has<Velocity>(player)) {
        std::cout << "Player can move!\n";
    }
    
    // Update systems
    world.for_each<Position, Velocity>([](Position& p, Velocity& v) {
        p.x += v.vx;
        p.y += v.vy;
    });
    
    // Query with exclusions
    world.query<Position>()
        .exclude<Dead>()
        .each([](Position& p) {
            // Only alive entities
            p.x += 1.0f;
        });
    
    // SIMD-friendly chunk iteration
    world.for_each_chunk<Position, Velocity>(
        [](Position* pos, Velocity* vel, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                pos[i].x += vel[i].vx;
                pos[i].y += vel[i].vy;
            }
        }
    );
    
    // Resource management
    struct GameTime { float delta; };
    world.set_resource<GameTime>(0.016f);
    
    // Event system
    world.on_component_added<Velocity>([](Entity e) {
        std::cout << "Entity can now move!\n";
    });
    
    // Batch operations
    auto enemies = world.create_batch(100);
    world.destroy_batch(enemies);
    
    // Debug info
    world.print_memory_usage();
    
    return 0;
}
```

## API Reference

### World

The main container for all entities and components.

```cpp
World world;
```

#### Entity Management

```cpp
Entity create()                    // Create a new entity
void destroy(Entity e)             // Destroy an entity and all its components
bool alive(Entity e) const         // Check if entity handle is still valid
```

#### Component Management

```cpp
template<typename... Cs>
void add(Entity e)                 // Add one or more components to an entity

template<typename C, typename... Args>
void add(Entity e, Args&&... args) // Add a component with initialization

template<typename... Cs>
void remove(Entity e)              // Remove one or more components from an entity
```

**Note**: Adding or removing components triggers archetype migration, which may invalidate iterators.

**Examples**:
```cpp
// Add components with default initialization
world.add<Position, Velocity>(e);

// Add component with custom initialization
world.add<Position>(e, 10.0f, 20.0f);  // Position{10.0f, 20.0f}
```

#### Component Access

```cpp
template<typename C>
C* get(Entity e)                   // Get mutable pointer to component (nullptr if not present)

template<typename C>
const C* get(Entity e) const       // Get const pointer to component

template<typename C>
bool has(Entity e) const           // Check if entity has component
```

**Example**:
```cpp
if (auto* pos = world.get<Position>(e)) {
    pos->x += 10.0f;
}

if (world.has<Velocity>(e)) {
    // Entity has velocity component
}
```

#### Iteration

##### Entity-Level Iteration

```cpp
template<typename... Cs, typename Fn>
void for_each(Fn&& fn)
```

Iterate over all entities that have the specified components. The callback receives references to each component.

**Example**:
```cpp
world.for_each<Position, Velocity>([](Position& p, Velocity& v) {
    p.x += v.vx;
    p.y += v.vy;
});
```

##### Chunk Iteration (SIMD-Friendly)

```cpp
template<typename... Cs, typename Fn>
void for_each_chunk(Fn&& fn)
```

Iterate over contiguous chunks of components. The callback receives raw pointers to component arrays and the count. Ideal for SIMD operations.

**Example**:
```cpp
world.for_each_chunk<Position, Velocity>(
    [](Position* pos, Velocity* vel, size_t count) {
        // Process 'count' elements in tight loop
        // Perfect for auto-vectorization or explicit SIMD
        for (size_t i = 0; i < count; ++i) {
            pos[i].x += vel[i].vx;
            pos[i].y += vel[i].vy;
        }
    }
);
```

#### Query Builder

For more complex queries with exclusion filters:

```cpp
template<typename... Cs>
Query<Cs...> query()               // Create a query for components Cs...
```

**Example**:
```cpp
// Iterate over entities with Position but without Dead tag
world.query<Position>()
    .exclude<Dead>()
    .each([](Position& p) {
        p.x += 1.0f;
    });

// Multiple exclusions
world.query<Position, Velocity>()
    .exclude<Dead, Frozen>()
    .each([](Position& p, Velocity& v) {
        p.x += v.vx;
    });
```

#### Batch Operations

Efficient bulk entity creation and destruction:

```cpp
std::vector<Entity> create_batch(size_t count)  // Create multiple entities at once
void destroy_batch(const std::vector<Entity>&)  // Destroy multiple entities
```

**Example**:
```cpp
auto enemies = world.create_batch(100);
for (Entity e : enemies) {
    world.add<Position, Enemy>(e);
}

// Later...
world.destroy_batch(enemies);
```

#### Resource Management

Store and access global singleton data:

```cpp
template<typename R, typename... Args>
void set_resource(Args&&... args)  // Create/update a resource

template<typename R>
R& get_resource()                  // Get mutable resource reference

template<typename R>
const R& get_resource() const      // Get const resource reference

template<typename R>
bool has_resource() const          // Check if resource exists
```

**Example**:
```cpp
struct GameTime {
    float delta;
    float total;
};

world.set_resource<GameTime>(0.016f, 0.0f);

// Access in systems
GameTime& time = world.get_resource<GameTime>();
time.total += time.delta;
```

#### Event System

Register callbacks for component lifecycle events:

```cpp
template<typename C>
void on_component_added(std::function<void(Entity)>)    // Called when C is added

template<typename C>
void on_component_removed(std::function<void(Entity)>)  // Called when C is removed
```

**Example**:
```cpp
world.on_component_added<Health>([](Entity e) {
    std::cout << "Entity " << e.id << " gained health!\n";
});

world.on_component_removed<Health>([](Entity e) {
    std::cout << "Entity " << e.id << " lost health!\n";
});
```

#### Debug & Introspection

Runtime statistics and memory usage:

```cpp
size_t get_entity_count() const    // Count of alive entities
size_t get_archetype_count() const // Number of unique archetypes
void print_memory_usage() const    // Print detailed memory statistics
```

**Example**:
```cpp
std::cout << "Active entities: " << world.get_entity_count() << "\n";
std::cout << "Archetypes: " << world.get_archetype_count() << "\n";
world.print_memory_usage();
// Output:
// === ECS Memory Usage ===
// Entities: 100000
// Archetypes: 3
// Component data: 1562.5 KB
// Entity metadata: 390.625 KB
```

### Entity

Entity handles use a generation counter to detect use-after-free bugs.

```cpp
struct Entity {
    uint32_t id;         // Index in entity array
    uint32_t generation; // Increments on destroy, invalidates old handles
};
```

### Configuration

```cpp
#define RECS_MAX_COMPONENTS 64  // Maximum number of unique component types (default: 64)
#include "recs.h"
```

## Design Principles

### Archetype-Based Storage

Entities with the same set of components are stored together in "archetypes". When you add or remove components, entities migrate between archetypes. This provides:

- **Cache locality**: Components are stored contiguously in memory
- **Efficient queries**: Only iterate over archetypes that match the query
- **Predictable performance**: No pointer chasing or indirection

### Structure of Arrays (SoA)

Components are stored in separate arrays rather than arrays of structs:

```
Archetype [Position, Velocity]:
  entities:  [E1, E2, E3, ...]
  Position:  [P1, P2, P3, ...]
  Velocity:  [V1, V2, V3, ...]
```

This layout is optimal for:
- CPU cache utilization
- SIMD vectorization
- Prefetching

### Generation-Based Entity IDs

Entity handles include a generation counter. When an entity is destroyed, its generation increments:

```cpp
Entity e = world.create();  // {id: 0, generation: 0}
world.destroy(e);           // generation becomes 1
world.alive(e);             // returns false - generation mismatch
```

This prevents:
- Use-after-free bugs
- Accessing recycled entity IDs by mistake

## Performance Characteristics

- **Entity creation**: O(1) amortized
- **Entity destruction**: O(1) with swap-and-pop
- **Component add/remove**: O(K) where K = number of components (archetype migration)
- **Iteration**: O(N) where N = matching entities, excellent cache performance
- **Component lookup**: O(1) with fixed-size array indexing

## Limitations

- Maximum 64 unique component types by default (configurable)
- No runtime component type registration
- Components must be default-constructible
- Adding/removing components during iteration invalidates iterators
- World is movable but non-copyable (unique ownership)
- Tag components (zero-size) still consume component ID slots

## Advanced Features

### Tag Components

Zero-size structs work as marker components:

```cpp
struct Player {};
struct Enemy {};
struct Dead {};

world.add<Player>(e);
if (world.has<Dead>(e)) {
    // Entity is marked as dead
}
```

### Move Semantics

World supports move operations for efficient transfer:

```cpp
World create_world() {
    World world;
    // ... setup ...
    return world;  // Move, not copy
}

World world1;
World world2 = std::move(world1);  // world1 is now empty
```

## Examples

See [test.cpp](test.cpp) for comprehensive examples including:
- Entity lifecycle management
- Component operations
- Both iteration styles
- Archetype migration
- Generation-based safety
- Stress testing with 100k+ entities

See [examples.cpp](examples.cpp) for demonstrations of all features:
- Component access API (`get`, `has`)
- Query builder with exclusions
- Batch operations
- Resource management
- Event system
- Tag components
- Debug & introspection
- Move semantics
- Const iteration

Run the examples:
```bash
g++ -std=c++17 -O3 -Iinclude examples.cpp -o examples
./examples
```

## Building

Header-only library - just include it:

```bash
g++ -std=c++17 -O3 -Iinclude your_code.cpp -o your_app
```

Requires C++17 or later.

Build and run tests:
```bash
g++ -std=c++17 -O3 -Iinclude test.cpp -o recs_test
./recs_test
```

## License

See [LICENSE](LICENSE) file.
