#include <iostream>
#include <cmath>
#include <cassert>

#include "recs.h"

using namespace recs;

// ------------------------------------------------------------
// Test components
// ------------------------------------------------------------
struct Position {
    float x, y;
};

struct Velocity {
    float vx, vy;
};

struct Health {
    int hp;
};

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static void banner(const char* name) {
    std::cout << "\n=== " << name << " ===\n";
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main() {
    World world;

    banner("Entity creation");
    Entity e1 = world.create();
    Entity e2 = world.create();

    assert(world.alive(e1));
    assert(world.alive(e2));

    std::cout << "Created entities: "
              << e1.id << ", " << e2.id << "\n";

    banner("Add components");
    world.add<Position, Velocity>(e1);
    world.add<Position>(e2);

    banner("Initialize components");
    world.for_each<Position>([](Position& p) {
        p.x = 0.0f;
        p.y = 0.0f;
    });

    world.for_each<Velocity>([](Velocity& v) {
        v.vx = 1.0f;
        v.vy = 0.5f;
    });

    banner("Iterate (Position + Velocity)");
    world.for_each<Position, Velocity>([](Position& p, Velocity& v) {
        p.x += v.vx;
        p.y += v.vy;
        std::cout << "Moved to (" << p.x << ", " << p.y << ")\n";
    });

    banner("Add / remove components (archetype migration)");
    world.add<Health>(e1);
    world.remove<Velocity>(e1);

    world.for_each<Position>([](Position& p) {
        std::cout << "Position still valid: "
                  << p.x << ", " << p.y << "\n";
    });

    banner("Chunk iteration (SIMD-style)");
    world.for_each_chunk<Position>([](Position* p, size_t count) {
        std::cout << "Chunk size: " << count << "\n";
        for (size_t i = 0; i < count; ++i) {
            p[i].x += 10.0f;
            p[i].y += 10.0f;
        }
    });

    world.for_each<Position>([](Position& p) {
        std::cout << "After chunk update: "
                  << p.x << ", " << p.y << "\n";
    });

    banner("Entity destruction & generation safety");
    uint32_t old_id = e2.id;
    uint32_t old_gen = e2.generation;

    world.destroy(e2);
    assert(!world.alive(e2));

    Entity e3 = world.create();
    std::cout << "Reused ID: " << e3.id
              << " generation: " << e3.generation << "\n";

    assert(e3.id == old_id);
    assert(e3.generation != old_gen);

    banner("Stress test (many entities)");
    constexpr int N = 100000;

    for (int i = 0; i < N; ++i) {
        Entity e = world.create();
        world.add<Position, Velocity>(e);
    }

    world.for_each_chunk<Position, Velocity>(
        [](Position* p, Velocity* v, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                p[i].x += v[i].vx;
                p[i].y += v[i].vy;
            }
        }
    );

    std::cout << "Updated " << N << " entities\n";

    banner("All tests completed");
    return 0;
}
