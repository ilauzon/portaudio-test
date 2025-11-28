#include <cstdio>
#include <chrono>
#include <thread>

int main() {
    float y = 0;
    float yaw = 0;
    float x = -1;
    float addAmount = 0.003;
    
    while (true) {
        x += addAmount;

        if (x > 1 || x < -1) {
            addAmount = -addAmount;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        printf("%f,%f,%f\n", x, y, yaw);
    }
}