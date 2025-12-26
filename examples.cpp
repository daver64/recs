#include <iostream>
#include <cassert>
#include "recs.h"

using namespace recs;

// Components
struct Position { float x, y; };
struct Velocity { float vx, vy; };
struct Health { int hp; };
struct Dead {};    // Tag component
struct Player {};  // Tag component

// Resources
struct GameTime {
    float delta;
    float total;
};

void example_component_access() {
    std::cout << "\n=== Component Access ===\n";
    World world;
    Entity e = world.create();
    
    // Add with initialization
    world.add<Position>(e, 10.0f, 20.0f);
    world.add<Velocity>(e);
    
    // Get component
    if (auto* pos = world.get<Position>(e)) {
        std::cout << "Position: (" << pos->x << ", " << pos->y << ")\n";
        pos->x += 5.0f;
    }
    
    // Check component presence
    std::cout << "Has Position: " << world.has<Position>(e) << "\n";
    std::cout << "Has Health: " << world.has<Health>(e) << "\n";
    
    // Const access
    const World& const_world = world;
    if (const auto* pos = const_world.get<Position>(e)) {
        std::cout << "Const Position: (" << pos->x << ", " << pos->y << ")\n";
    }
}

void example_query_builder() {
    std::cout << "\n=== Query Builder ===\n";
    World world;
    
    // Create some entities
    Entity alive1 = world.create();
    world.add<Position>(alive1, 0.0f, 0.0f);
    world.add<Velocity>(alive1, 1.0f, 1.0f);
    
    Entity alive2 = world.create();
    world.add<Position>(alive2, 10.0f, 10.0f);
    
    Entity dead = world.create();
    world.add<Position>(dead, 5.0f, 5.0f);
    world.add<Dead>(dead);
    
    // Query with exclusion
    std::cout << "Alive entities with Position:\n";
    world.query<Position>()
        .exclude<Dead>()
        .each([](Position& p) {
            std::cout << "  Position: (" << p.x << ", " << p.y << ")\n";
        });
    
    // Multiple exclusions
    std::cout << "All positions (no filter):\n";
    world.for_each<Position>([](Position& p) {
        std::cout << "  Position: (" << p.x << ", " << p.y << ")\n";
    });
}

void example_batch_operations() {
    std::cout << "\n=== Batch Operations ===\n";
    World world;
    
    // Create batch
    auto entities = world.create_batch(5);
    std::cout << "Created " << entities.size() << " entities\n";
    
    // Add components to all
    for (Entity e : entities) {
        world.add<Position>(e, 0.0f, 0.0f);
    }
    
    std::cout << "Entity count: " << world.get_entity_count() << "\n";
    
    // Destroy batch
    world.destroy_batch(entities);
    std::cout << "After destroy, entity count: " << world.get_entity_count() << "\n";
}

void example_resources() {
    std::cout << "\n=== Resources ===\n";
    World world;
    
    // Set resource
    world.set_resource<GameTime>(0.016f, 0.0f);
    
    // Get and modify resource
    GameTime& time = world.get_resource<GameTime>();
    std::cout << "Initial time - Delta: " << time.delta << ", Total: " << time.total << "\n";
    
    // Simulate frames
    for (int i = 0; i < 3; ++i) {
        time.total += time.delta;
        std::cout << "Frame " << i+1 << " - Total time: " << time.total << "\n";
    }
    
    // Check resource existence
    std::cout << "Has GameTime: " << world.has_resource<GameTime>() << "\n";
    std::cout << "Has Position: " << world.has_resource<Position>() << "\n";
}

void example_events() {
    std::cout << "\n=== Event System ===\n";
    World world;
    
    // Register event handlers
    world.on_component_added<Position>([](Entity e) {
        std::cout << "Position added to entity " << e.id << "\n";
    });
    
    world.on_component_removed<Position>([](Entity e) {
        std::cout << "Position removed from entity " << e.id << "\n";
    });
    
    // Trigger events
    Entity e = world.create();
    world.add<Position>(e, 0.0f, 0.0f);  // Triggers on_add
    world.remove<Position>(e);            // Triggers on_remove
}

void example_tag_components() {
    std::cout << "\n=== Tag Components ===\n";
    World world;
    
    Entity e1 = world.create();
    world.add<Position>(e1, 0.0f, 0.0f);
    world.add<Player>(e1);
    
    Entity e2 = world.create();
    world.add<Position>(e2, 10.0f, 10.0f);
    
    // Query using tags
    std::cout << "Player entities:\n";
    world.for_each<Position, Player>([](Position& p, Player&) {
        std::cout << "  Player at (" << p.x << ", " << p.y << ")\n";
    });
    
    std::cout << "All entities:\n";
    world.for_each<Position>([](Position& p) {
        std::cout << "  Entity at (" << p.x << ", " << p.y << ")\n";
    });
}

void example_debug_info() {
    std::cout << "\n=== Debug Information ===\n";
    World world;
    
    // Create some entities with different archetypes
    auto batch = world.create_batch(100);
    for (size_t i = 0; i < batch.size(); ++i) {
        world.add<Position>(batch[i], 0.0f, 0.0f);
        if (i % 2 == 0) {
            world.add<Velocity>(batch[i], 1.0f, 1.0f);
        }
        if (i % 3 == 0) {
            world.add<Health>(batch[i]);
        }
    }
    
    std::cout << "Entities: " << world.get_entity_count() << "\n";
    std::cout << "Archetypes: " << world.get_archetype_count() << "\n";
    std::cout << "\n";
    world.print_memory_usage();
}

void example_move_semantics() {
    std::cout << "\n=== Move Semantics ===\n";
    
    World world1;
    world1.create_batch(10);
    std::cout << "World1 entities: " << world1.get_entity_count() << "\n";
    
    // Move construction
    World world2 = std::move(world1);
    std::cout << "World2 entities (after move): " << world2.get_entity_count() << "\n";
    
    // Move assignment
    World world3;
    world3.create_batch(5);
    std::cout << "World3 entities (before assign): " << world3.get_entity_count() << "\n";
    world3 = std::move(world2);
    std::cout << "World3 entities (after assign): " << world3.get_entity_count() << "\n";
}

void example_const_iteration() {
    std::cout << "\n=== Const Iteration ===\n";
    World world;
    
    auto entities = world.create_batch(3);
    for (size_t i = 0; i < entities.size(); ++i) {
        world.add<Position>(entities[i], float(i), float(i * 2));
    }
    
    // Non-const iteration (can modify)
    world.for_each<Position>([](Position& p) {
        p.x += 10.0f;
    });
    
    // Const iteration (read-only)
    const World& const_world = world;
    std::cout << "Positions (const iteration):\n";
    const_world.for_each<Position>([](const Position& p) {
        std::cout << "  (" << p.x << ", " << p.y << ")\n";
    });
    
    // Const chunk iteration
    const_world.for_each_chunk<Position>([](const Position* p, size_t count) {
        std::cout << "Chunk with " << count << " positions\n";
    });
}

int main() {
    std::cout << "RECS - Feature Examples\n";
    std::cout << "=======================\n";
    
    example_component_access();
    example_query_builder();
    example_batch_operations();
    example_resources();
    example_events();
    example_tag_components();
    example_debug_info();
    example_move_semantics();
    example_const_iteration();
    
    std::cout << "\n=== All Examples Completed ===\n";
    return 0;
}
