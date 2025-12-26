#include <iostream>
#include <chrono>
#include <cmath>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "recs.h"

using namespace recs;

// Components for physics simulation
struct Position { float x, y, z; };
struct Velocity { float vx, vy, vz; };
struct Acceleration { float ax, ay, az; };
struct Mass { float m; };

// Benchmark helper
template<typename Fn>
double benchmark(const char* name, Fn&& fn, int iterations = 10) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        fn();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg = duration.count() / double(iterations);
    std::cout << name << ": " << avg << "µs (avg over " << iterations << " runs)\n";
    return avg;
}

void example_basic_parallel() {
    std::cout << "\n=== Basic Parallel Iteration ===\n";
    World world;
    
    // Create entities
    const int count = 100000;
    auto entities = world.create_batch(count);
    for (Entity e : entities) {
        world.add<Position>(e, 0.0f, 0.0f, 0.0f);
        world.add<Velocity>(e, 1.0f, 1.0f, 1.0f);
    }
    
    std::cout << "Created " << count << " entities\n";
    std::cout << "Workload: Expensive trigonometric calculations per entity\n";
    
    // Single-threaded with expensive computation
    double single_time = benchmark("Single-threaded", [&]() {
        world.for_each<Position, Velocity>([](Position& p, Velocity& v) {
            // Expensive computation: simulate complex physics
            for (int i = 0; i < 50; ++i) {
                float angle = std::atan2(p.y, p.x);
                float magnitude = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
                p.x += std::sin(angle) * v.vx * 0.001f;
                p.y += std::cos(angle) * v.vy * 0.001f;
                p.z += std::tan(angle * 0.1f) * v.vz * 0.001f;
            }
        });
    });
    
    // Parallel with OpenMP
    double parallel_time = benchmark("Parallel (OpenMP)", [&]() {
        world.parallel_for_each<Position, Velocity>([](Position& p, Velocity& v) {
            // Same expensive computation
            for (int i = 0; i < 50; ++i) {
                float angle = std::atan2(p.y, p.x);
                float magnitude = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
                p.x += std::sin(angle) * v.vx * 0.001f;
                p.y += std::cos(angle) * v.vy * 0.001f;
                p.z += std::tan(angle * 0.1f) * v.vz * 0.001f;
            }
        });
    });
    
    double speedup = single_time / parallel_time;
    std::cout << "Speedup: " << speedup << "x\n";
    if (speedup > 1.5) {
        std::cout << "✓ Good parallel scaling!\n";
    } else {
        std::cout << "⚠ Workload may be too small or memory-bound\n";
    }
}

void example_physics_simulation() {
    std::cout << "\n=== Physics Simulation (Parallel) ===\n";
    World world;
    
    const int entity_count = 50000;
    auto entities = world.create_batch(entity_count);
    
    // Initialize physics entities
    for (size_t i = 0; i < entities.size(); ++i) {
        world.add<Position>(entities[i], 
            float(i % 100), 
            float(i / 100), 
            0.0f);
        world.add<Velocity>(entities[i], 
            (i % 2 == 0 ? 1.0f : -1.0f), 
            0.0f, 
            0.0f);
        world.add<Acceleration>(entities[i], 0.0f, -9.8f, 0.0f);
        world.add<Mass>(entities[i], 1.0f + float(i % 10));
    }
    
    std::cout << "Simulating " << entity_count << " physics entities\n";
    std::cout << "Workload: Verlet integration with constraint solving\n";
    
    const float dt = 0.016f; // 60 FPS
    const int frames = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int frame = 0; frame < frames; ++frame) {
        // Apply forces (F = ma) with air resistance
        world.parallel_for_each<Velocity, Acceleration, Mass>(
            [dt](Velocity& v, Acceleration& a, Mass& m) {
                // Complex force calculation
                float speed = std::sqrt(v.vx * v.vx + v.vy * v.vy + v.vz * v.vz);
                float drag = 0.1f * speed * speed / m.m;
                
                v.vx += (a.ax - drag * v.vx) * dt / m.m;
                v.vy += (a.ay - drag * v.vy) * dt / m.m;
                v.vz += (a.az - drag * v.vz) * dt / m.m;
                
                // Add some complexity (collision response simulation)
                for (int i = 0; i < 5; ++i) {
                    float damping = 0.99f;
                    v.vx *= damping;
                    v.vy *= damping;
                    v.vz *= damping;
                }
            }
        );
        
        // Update positions with constraint solving
        world.parallel_for_each<Position, Velocity>(
            [dt](Position& p, Velocity& v) {
                // Verlet integration
                p.x += v.vx * dt;
                p.y += v.vy * dt;
                p.z += v.vz * dt;
                
                // Boundary constraints (simulate multiple iterations)
                for (int i = 0; i < 3; ++i) {
                    if (p.y < 0.0f) {
                        p.y = -p.y;
                        v.vy = -v.vy * 0.8f;
                    }
                    if (p.y > 1000.0f) {
                        p.y = 2000.0f - p.y;
                        v.vy = -v.vy * 0.8f;
                    }
                }
            }
        );
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Simulated " << frames << " frames in " << duration.count() << "ms\n";
    std::cout << "Average: " << (duration.count() * 1000.0 / frames) << "µs per frame\n";
    std::cout << "Throughput: " << (entity_count * frames / (duration.count() / 1000.0)) << " entities/sec\n";
}

void example_chunk_processing() {
    std::cout << "\n=== Chunk-Based SIMD Processing ===\n";
    World world;
    
    const int count = 100000;
    auto entities = world.create_batch(count);
    for (size_t i = 0; i < entities.size(); ++i) {
        world.add<Position>(entities[i], float(i) * 0.1f, float(i) * 0.2f, float(i) * 0.3f);
    }
    
    std::cout << "Processing " << count << " positions\n";
    std::cout << "Workload: Matrix transformations and normalization\n";
    
    // Single-threaded chunk processing with expensive computation
    double single_time = benchmark("Single-threaded chunks", [&]() {
        world.for_each_chunk<Position>([](Position* pos, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                // Expensive matrix-like transformations
                for (int iter = 0; iter < 30; ++iter) {
                    float x = pos[i].x;
                    float y = pos[i].y;
                    float z = pos[i].z;
                    
                    // Rotation and scaling
                    pos[i].x = x * std::cos(0.1f) - y * std::sin(0.1f);
                    pos[i].y = x * std::sin(0.1f) + y * std::cos(0.1f);
                    pos[i].z = z * std::cos(0.05f);
                    
                    // Normalize
                    float magnitude = std::sqrt(pos[i].x * pos[i].x + 
                                               pos[i].y * pos[i].y + 
                                               pos[i].z * pos[i].z);
                    if (magnitude > 0.0001f) {
                        pos[i].x /= magnitude;
                        pos[i].y /= magnitude;
                        pos[i].z /= magnitude;
                    }
                }
            }
        });
    });
    
    // Parallel chunk processing
    double parallel_time = benchmark("Parallel chunks (OpenMP)", [&]() {
        world.parallel_for_each_chunk<Position>([](Position* pos, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                // Same expensive computation
                for (int iter = 0; iter < 30; ++iter) {
                    float x = pos[i].x;
                    float y = pos[i].y;
                    float z = pos[i].z;
                    
                    pos[i].x = x * std::cos(0.1f) - y * std::sin(0.1f);
                    pos[i].y = x * std::sin(0.1f) + y * std::cos(0.1f);
                    pos[i].z = z * std::cos(0.05f);
                    
                    float magnitude = std::sqrt(pos[i].x * pos[i].x + 
                                               pos[i].y * pos[i].y + 
                                               pos[i].z * pos[i].z);
                    if (magnitude > 0.0001f) {
                        pos[i].x /= magnitude;
                        pos[i].y /= magnitude;
                        pos[i].z /= magnitude;
                    }
                }
            }
        });
    });
    
    double speedup = single_time / parallel_time;
    std::cout << "Speedup: " << speedup << "x\n";
    if (speedup > 2.0) {
        std::cout << "✓ Excellent parallel scaling!\n";
    } else if (speedup > 1.5) {
        std::cout << "✓ Good parallel scaling!\n";
    } else {
        std::cout << "⚠ Limited speedup - may be memory-bound\n";
    }
}

void example_multiple_systems() {
    std::cout << "\n=== Multiple Parallel Systems ===\n";
    World world;
    
    const int count = 50000;
    auto entities = world.create_batch(count);
    
    // Create entities with different component combinations
    for (size_t i = 0; i < entities.size(); ++i) {
        world.add<Position>(entities[i], float(i), 0.0f, 0.0f);
        
        if (i % 2 == 0) {
            world.add<Velocity>(entities[i], 1.0f, 0.0f, 0.0f);
        }
        
        if (i % 3 == 0) {
            world.add<Acceleration>(entities[i], 0.0f, -9.8f, 0.0f);
        }
    }
    
    std::cout << "Entities: " << world.get_entity_count() << "\n";
    std::cout << "Archetypes: " << world.get_archetype_count() << "\n\n";
    
    const float dt = 0.016f;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Run multiple systems in parallel
    for (int i = 0; i < 100; ++i) {
        // System 1: Update velocity from acceleration
        world.parallel_for_each<Velocity, Acceleration>([dt](Velocity& v, Acceleration& a) {
            v.vx += a.ax * dt;
            v.vy += a.ay * dt;
            v.vz += a.az * dt;
        });
        
        // System 2: Update position from velocity
        world.parallel_for_each<Position, Velocity>([dt](Position& p, Velocity& v) {
            p.x += v.vx * dt;
            p.y += v.vy * dt;
            p.z += v.vz * dt;
        });
        
        // System 3: Apply drag to all positions
        world.parallel_for_each<Position>([dt](Position& p) {
            p.x *= 0.99f;
            p.y *= 0.99f;
            p.z *= 0.99f;
        });
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Processed 100 frames with 3 systems in " << duration.count() << "ms\n";
}

void print_openmp_info() {
    std::cout << "\n=== OpenMP Information ===\n";
    
#ifdef _OPENMP
    #pragma omp parallel
    {
        #pragma omp single
        {
            std::cout << "OpenMP is ENABLED\n";
            std::cout << "Number of threads: " << omp_get_num_threads() << "\n";
            std::cout << "Max threads available: " << omp_get_max_threads() << "\n";
        }
    }
#else
    std::cout << "OpenMP is NOT enabled\n";
    std::cout << "Compile with -fopenmp to enable parallel processing\n";
    std::cout << "Example: g++ -std=c++17 -O3 -fopenmp -Iinclude openmp_example.cpp\n";
#endif
}

int main() {
    std::cout << "RECS - OpenMP Parallel Processing Examples\n";
    std::cout << "============================================\n";
    
    print_openmp_info();
    
    example_basic_parallel();
    example_chunk_processing();
    example_physics_simulation();
    example_multiple_systems();
    
    std::cout << "\n=== All OpenMP Examples Completed ===\n";
    
#ifdef _OPENMP
    std::cout << "Note: Performance gains depend on CPU core count and workload size.\n";
    std::cout << "      Best results with 10,000+ entities and compute-heavy operations.\n";
#else
    std::cout << "\n⚠️  OpenMP was not enabled during compilation!\n";
    std::cout << "To see parallel performance, rebuild with:\n";
    std::cout << "    g++ -std=c++17 -O3 -fopenmp -Iinclude openmp_example.cpp -o openmp_test\n";
#endif
    
    return 0;
}
