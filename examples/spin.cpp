#include <cstdio>
#include <chrono>
#include <thread>

int main() {
    float y = 0;
    float yaw = 0;
    float x = 0;
    
    while (true) {
        yaw += 0.0015;

        if (yaw > 1) {
            yaw = 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        printf("%f,%f,%f\n", x, y, yaw);
    }
}