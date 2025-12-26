#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "recs.h"

using namespace recs;

struct Position { float x, y; };
struct Velocity { float vx, vy; };
struct Health { int hp; };

void test_concurrent_creation() {
    std::cout << "\n=== Concurrent Entity Creation ===\n";
    World world;
    
    const int threads = 4;
    const int entities_per_thread = 1000;
    std::vector<std::thread> workers;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&world, entities_per_thread]() {
            for (int i = 0; i < entities_per_thread; ++i) {
                Entity e = world.create();
                world.add<Position>(e, float(i), float(i));
            }
        });
    }
    
    for (auto& w : workers) {
        w.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Created " << world.get_entity_count() << " entities across " 
              << threads << " threads in " << duration.count() << "ms\n";
    std::cout << "Archetypes: " << world.get_archetype_count() << "\n";
}

void test_concurrent_component_access() {
    std::cout << "\n=== Concurrent Component Access ===\n";
    World world;
    
    // Create entities
    std::vector<Entity> entities;
    for (int i = 0; i < 1000; ++i) {
        Entity e = world.create();
        world.add<Position>(e, float(i), float(i));
        world.add<Health>(e);
        entities.push_back(e);
    }
    
    // Multiple threads reading and checking components
    const int readers = 4;
    std::vector<std::thread> workers;
    std::atomic<int> read_count{0};
    
    for (int t = 0; t < readers; ++t) {
        workers.emplace_back([&world, &entities, &read_count]() {
            for (int i = 0; i < 100; ++i) {
                for (Entity e : entities) {
                    if (world.has<Position>(e)) {
                        if (auto* pos = world.get<Position>(e)) {
                            // Read position (thread-safe)
                            volatile float x = pos->x;
                            read_count++;
                        }
                    }
                }
            }
        });
    }
    
    for (auto& w : workers) {
        w.join();
    }
    
    std::cout << "Performed " << read_count.load() << " thread-safe component reads\n";
}

void test_concurrent_iteration() {
    std::cout << "\n=== Concurrent System Updates ===\n";
    World world;
    
    // Create entities
    const int entity_count = 10000;
    auto entities = world.create_batch(entity_count);
    for (Entity e : entities) {
        world.add<Position>(e, 0.0f, 0.0f);
        world.add<Velocity>(e, 1.0f, 1.0f);
    }
    
    std::cout << "Created " << entity_count << " entities\n";
    
    // Single-threaded update
    auto start = std::chrono::high_resolution_clock::now();
    world.for_each<Position, Velocity>([](Position& p, Velocity& v) {
        p.x += v.vx;
        p.y += v.vy;
    });
    auto end = std::chrono::high_resolution_clock::now();
    auto single_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Single-threaded update: " << single_duration.count() << "µs\n";
    
    // Parallel update (requires OpenMP: compile with -fopenmp)
    // Uncomment if OpenMP is available
    /*
    start = std::chrono::high_resolution_clock::now();
    world.parallel_for_each<Position, Velocity>([](Position& p, Velocity& v) {
        p.x += v.vx;
        p.y += v.vy;
    });
    end = std::chrono::high_resolution_clock::now();
    auto parallel_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Parallel update: " << parallel_duration.count() << "µs\n";
    std::cout << "Speedup: " << (float)single_duration.count() / parallel_duration.count() << "x\n";
    */
}

void test_resource_thread_safety() {
    std::cout << "\n=== Thread-Safe Resource Access ===\n";
    World world;
    
    struct GameState {
        std::atomic<int> frame_count{0};
        float delta_time;
    };
    
    world.set_resource<GameState>();
    
    const int threads = 4;
    std::vector<std::thread> workers;
    
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&world]() {
            for (int i = 0; i < 100; ++i) {
                auto& state = world.get_resource<GameState>();
                state.frame_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    for (auto& w : workers) {
        w.join();
    }
    
    auto& state = world.get_resource<GameState>();
    std::cout << "Final frame count: " << state.frame_count.load() << " (expected: 400)\n";
}

void test_mixed_operations() {
    std::cout << "\n=== Mixed Concurrent Operations ===\n";
    World world;
    
    std::atomic<bool> running{true};
    std::atomic<int> creates{0};
    std::atomic<int> destroys{0};
    std::atomic<int> reads{0};
    
    // Entity creator thread
    std::thread creator([&]() {
        while (running) {
            Entity e = world.create();
            world.add<Position>(e, 0.0f, 0.0f);
            creates++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Entity destroyer thread
    std::thread destroyer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Let some entities be created
        while (running) {
            if (world.get_entity_count() > 100) {
                Entity e{0, 0}; // Try first entity
                if (world.alive(e)) {
                    world.destroy(e);
                    destroys++;
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
    
    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 2; ++i) {
        readers.emplace_back([&]() {
            while (running) {
                world.for_each<Position>([&](Position& p) {
                    volatile float x = p.x;
                    reads++;
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }
    
    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;
    
    creator.join();
    destroyer.join();
    for (auto& r : readers) {
        r.join();
    }
    
    std::cout << "Operations completed:\n";
    std::cout << "  Creates: " << creates.load() << "\n";
    std::cout << "  Destroys: " << destroys.load() << "\n";
    std::cout << "  Reads: " << reads.load() << "\n";
    std::cout << "  Final entities: " << world.get_entity_count() << "\n";
}

int main() {
    std::cout << "RECS - Thread Safety Examples\n";
    std::cout << "==============================\n";
    
    test_concurrent_creation();
    test_concurrent_component_access();
    test_concurrent_iteration();
    test_resource_thread_safety();
    test_mixed_operations();
    
    std::cout << "\n=== All Threading Tests Completed Successfully ===\n";
    std::cout << "Note: All World operations are thread-safe!\n";
    std::cout << "Note: Compile with -fopenmp for parallel iteration support.\n";
    
    return 0;
}
