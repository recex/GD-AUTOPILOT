# GD AUTOPILOT

**GD AUTOPILOT** is a modular, real-time AI level completion bot built natively in C++23 for Geometry Dash using the Geode SDK.

## Key Features

- **Full 8-Mode Support**: Dedicated controllers and physics models for **Cube, Ship, Ball, UFO, Wave, Robot, Spider, and Swing**.
- **240Hz Physics Engine**: Predicts trajectories across gravity flips, speed changes (0.5x–4x), mini portals, slopes, pads, and orbs.
- **Level Analyzer**: Classifies level geometry, builds spatial lookup tables, identifies obstacle patterns, and computes safe corridors.
- **Real-Time AI Engine**: Combines multi-branch physics rollout simulation, corridor guidance, pattern heuristics, and adaptive learning memory.
- **Adaptive Learning System**: Remembers failure zones per level, adjusts timing biases across attempts, and reuses strategies that worked.
- **Native Geode Control Suite**: In-game 6-tab popup menu (`AI PLAYER`, `LEVEL ANALYZER`, `PHYSICS ENGINE`, `AI SETTINGS`, `STATISTICS`, `DEBUG CONSOLE`) plus live HUD overlay.
