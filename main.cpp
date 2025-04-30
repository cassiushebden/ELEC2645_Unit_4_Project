#include "mbed.h"
#include "Joystick.h"
#include "N5110.h"
#include "power.h"
#include "StartupScreen.h"
#include <cmath>
#include <vector>
#include <cstdio>



// LCD (IO, Ser_TX, Ser_RX, MOSI, SCLK, PWM)
N5110 lcd(PC_7, PA_9, PB_10, PB_5, PB_3, PA_10);

// Joystick (Y, X)
Joystick joystick(PC_3, PC_2);

PwmOut buzzer(PA_8); // buzzer

// Button to shoot
DigitalIn shootButton(PB_4);

// LEDs to display number of balls remaining (using specified pins)
DigitalOut led1(PC_0);  // PC0
DigitalOut led2(PC_1);  // PC1
DigitalOut led3(PB_0);  // PB0
DigitalOut led4(PA_4);  // PA4


I2C i2c(PB_9, PB_8);  // Only define once in main.cpp
// Constants
#define TABLE_WIDTH 84
#define TABLE_HEIGHT 48
#define BALL_RADIUS 2
#define MAX_BALLS 5
#define CUE_LENGTH 20  // Double the length of the cue stick
#define POCKET_RADIUS 2
#define INITIAL_BALLS 5
#define ANGLE_CHANGE_RATE 0.1f  // Rate at which angle changes for fine control

// Define missing math constants
#define M_PI 3.14159265358979323846
#define M_PI_2 (M_PI / 2.0)
#define M_PI_4 (M_PI / 4.0)




// Pocket positions
struct Pocket {
    int x, y;
};

const Pocket pockets[6] = {
    {2, 2},
    {42, 2},
    {82, 2},
    {2, 46},
    {42, 46},
    {82, 46}
};

// Ball structure
struct Ball {
    float x, y;
    float vx = 0, vy = 0;
    bool active = true;
    bool isCueBall = false;
};

std::vector<Ball> balls;
float cueAngle = 0.0f;
bool shotTaken = false;
bool shotInProgress = false;
bool gameOver = false;
int ballsRemaining = 0;

// Update LED display based on balls remaining
// New implementation: All LEDs on at start, turn off sequentially as balls are potted
void updateLEDs() {
    // Turn on/off LEDs based on ballsRemaining
    // For 4 balls: all 4 LEDs on
    // For 3 balls: 3 LEDs on (led4 off)
    // For 2 balls: 2 LEDs on (led3, led4 off)
    // For 1 ball: 1 LED on (led2, led3, led4 off)
    // For 0 balls: all LEDs off
    
    led1 = (ballsRemaining >= 1) ? 1 : 0;
    led2 = (ballsRemaining >= 2) ? 1 : 0;
    led3 = (ballsRemaining >= 3) ? 1 : 0;
    led4 = (ballsRemaining >= 4) ? 1 : 0;
}

void playTone(float frequency, int duration_ms) {
    buzzer.period(1.0f / frequency);  // Set frequency
    buzzer.write(0.5f);               // 50% duty cycle for audible tone
    ThisThread::sleep_for(duration_ms);
    buzzer.write(0.0f);               // Turn off buzzer
}

void initGame() {
    balls.clear();
    gameOver = false;

    // Cue ball at 1/4th position
    balls.push_back({TABLE_WIDTH / 4.0f, TABLE_HEIGHT / 2.0f, 0, 0, true, true});

    // Rack of 4 balls in triangle at 3/4 position
    float startX = TABLE_WIDTH * 3.0f / 4.0f;
    float startY = TABLE_HEIGHT / 2.0f;
    balls.push_back({startX, startY});
    balls.push_back({startX + 4, startY - 3});
    balls.push_back({startX + 4, startY + 3});
    balls.push_back({startX + 8, startY});
    
    ballsRemaining = INITIAL_BALLS - 1; // Exclude cue ball
    updateLEDs();
    
    printf("New game started! Balls remaining: %d\n", ballsRemaining);
}

bool checkPocketCollision(Ball &ball) {
    for (int i = 0; i < 6; i++) {
        float dx = ball.x - pockets[i].x;
        float dy = ball.y - pockets[i].y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance < POCKET_RADIUS + BALL_RADIUS) {
            return true; // Ball is in pocket
        }
    }
    return false;
}

// New function for fine cue angle control using analog joystick values
void updateCueAngleFine() {
    // Get joystick coordinates
    Vector2D coord = joystick.get_coord();
    
    // coord.x is the X axis, coord.y is the Y axis
    float x = -coord.x;
    float y = coord.y;

    // Only adjust if joystick is moved enough (to avoid drift)
    if (fabs(x) > 0.1 || fabs(y) > 0.1) {
        // Convert joystick position to angle
        float targetAngle = atan2(-y, x);  // Negate y because screen coordinates are inverted
        
        // Gradually move toward the target angle for smoother control
        float angleDiff = targetAngle - cueAngle;
        
        // Handle angle wrapping around +/- PI
        if (angleDiff > M_PI) angleDiff -= 2 * M_PI;
        if (angleDiff < -M_PI) angleDiff += 2 * M_PI;
        
        cueAngle += angleDiff * ANGLE_CHANGE_RATE;
        
        // Keep angle in range -PI to PI
        if (cueAngle > M_PI) cueAngle -= 2 * M_PI;
        if (cueAngle < -M_PI) cueAngle += 2 * M_PI;
    }
}


void updateBalls() {
    for (auto &ball : balls) {
        if (!ball.active) continue;

        ball.x += ball.vx;
        ball.y += ball.vy;

        ball.vx *= 0.98;
        ball.vy *= 0.98;

        if (fabs(ball.vx) < 0.05f) ball.vx = 0;
        if (fabs(ball.vy) < 0.05f) ball.vy = 0;

        if (ball.x - BALL_RADIUS < 1 || ball.x + BALL_RADIUS > TABLE_WIDTH - 1)
            ball.vx *= -1;
        if (ball.y - BALL_RADIUS < 1 || ball.y + BALL_RADIUS > TABLE_HEIGHT - 1)
            ball.vy *= -1;
            
        // Check if ball is potted
        if (checkPocketCollision(ball)) {
            if (ball.isCueBall) {
                // Reset cue ball if potted
                ball.x = TABLE_WIDTH / 4.0f;
                ball.y = TABLE_HEIGHT / 2.0f;
                ball.vx = 0;
                ball.vy = 0;
                printf("Cue ball potted! Resetting position.\n");
            } else {
                // Remove normal ball if potted
                ball.active = false;
                ballsRemaining--;
                updateLEDs();
                printf("Ball potted! Balls remaining: %d\n", ballsRemaining);
                playTone(880.0f, 150);  // A5 for scoring
                // Check if game is over (all non-cue balls potted)
                if (ballsRemaining <= 0) {
                    gameOver = true;
                    printf("GAME OVER! All balls potted. Press shoot button to restart.\n");
                    playTone(440.0f, 300);  // A4
                    playTone(349.23f, 300); // F4
                    playTone(261.63f, 600); // C4
                }
            }
        }
    }

    // Ball collision handling
    for (size_t i = 0; i < balls.size(); ++i) {
        for (size_t j = i + 1; j < balls.size(); ++j) {
            Ball &a = balls[i];
            Ball &b = balls[j];
            
            if (!a.active || !b.active) continue;
            
            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float dist = sqrt(dx * dx + dy * dy);
            if (dist < BALL_RADIUS * 2 && dist > 0) {
                float nx = dx / dist;
                float ny = dy / dist;
                float p = 2 * (a.vx * nx + a.vy * ny - b.vx * nx - b.vy * ny) / 2;
                a.vx -= p * nx;
                a.vy -= p * ny;
                b.vx += p * nx;
                b.vy += p * ny;
            }
        }
    }
}

void shoot() {
    if (!shotTaken && !gameOver && shootButton.read() == 0) {
        // Read the accelerometer Y-axis value
        int16_t accelY = read_accel_y();  
        
        // Map the accelerometer value to power (0 to 100%)
        int16_t power = map_to_power(accelY);  

        // Adjust the shot velocity based on the power (you can scale this factor)
        float shotVelocity = 2.0f * (power / 100.0f);  // Scale the power to velocity range

        // Apply the shot velocity to the ball's velocity components
        balls[0].vx = shotVelocity * cos(cueAngle);
        balls[0].vy = shotVelocity * sin(cueAngle);

        // Set flags for shot progress
        shotTaken = true;
        shotInProgress = true;

        printf("Shot taken! Power: %d%%, Velocity: (%.2f, %.2f)\n", power, balls[0].vx, balls[0].vy);
    }
}

bool allBallsStopped() {
    for (auto &ball : balls) {
        if (ball.active && (fabs(ball.vx) >= 0.05f || fabs(ball.vy) >= 0.05f)) {
            return false;
        }
    }
    return true;
}

void resetForNextShot() {
    // Check if all balls have stopped moving, reset shot
    if (shotInProgress && allBallsStopped()) {
        shotInProgress = false;
        shotTaken = false;
        printf("Ready for next shot. Use joystick to aim, button to shoot.\n");
    }
}

void drawGameOver() {
    lcd.printString("GAME OVER", 10, 2);
    lcd.printString("ALL BALLS", 10, 3);
    lcd.printString("POTTED!", 15, 4);
}

// In your draw() function
void draw() {
    lcd.clear();
    lcd.drawRect(0, 0, TABLE_WIDTH, TABLE_HEIGHT, FILL_TRANSPARENT);

    // Draw pockets
    for (int i = 0; i < 6; i++) {
        lcd.drawCircle(pockets[i].x, pockets[i].y, POCKET_RADIUS, FILL_BLACK);
    }

    if (gameOver) {
        drawGameOver();
    } else {
        // Cue stick
        if (!shotInProgress) {
            Ball cueBall = balls[0];
            float cueStartX = cueBall.x - cos(cueAngle) * CUE_LENGTH;
            float cueStartY = cueBall.y - sin(cueAngle) * CUE_LENGTH;
            lcd.drawLine(cueStartX, cueStartY, cueBall.x, cueBall.y, FILL_BLACK);
        }

        // Draw the cue ball outline
        lcd.drawCircle((int)balls[0].x, (int)balls[0].y, BALL_RADIUS, FILL_TRANSPARENT);  // No fill, just outline

        // Draw other balls
        for (auto &ball : balls) {
            if (ball.active && !ball.isCueBall) {  // Don't draw the cue ball again
                lcd.drawCircle((int)ball.x, (int)ball.y, BALL_RADIUS, FILL_BLACK);  // Filled balls
            }
        }
    }

    lcd.refresh();
}




int main() {
    lcd.init(LPH7366_1);
    lcd.setContrast(0.55);
    lcd.setBrightness(0.5);
    joystick.init();
    shootButton.mode(PullUp);

    // Show startup screen
    StartupScreen startup(lcd);
    startup.show();

    // Wait for button press to start the game
    while (shootButton.read() == 1) {
        ThisThread::sleep_for(100ms);
    }

    // Small delay and clear screen after button press
    ThisThread::sleep_for(300ms);  // debounce delay
    lcd.clear();                   // clear startup screen
    lcd.refresh();                 // push the clear to screen

    // Game messages and setup
    printf("Pool Game Starting...\n");
    printf("Use joystick to aim, button to shoot.\n");
   
    playTone(523.25, 200);  // C5 note
    playTone(659.25, 200);  // E5
    playTone(783.99, 200);  // G5

    initGame();

    while (1) {
        if (!gameOver) {
            updateCueAngleFine();
            shoot();
            updateBalls();
            resetForNextShot();
        } else if (shootButton.read() == 0) {
            printf("Restarting game...\n");
            initGame();
            ThisThread::sleep_for(300ms); // debounce
        }

        draw();  // this will now run right after startup
        ThisThread::sleep_for(30ms);
    }
}
