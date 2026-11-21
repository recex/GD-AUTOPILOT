#include "modules/PhysicsEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace gdautopilot {

float PhysicsEngine::rectDistance(Rect const& a, Rect const& b) noexcept {
    float dx = 0.0f;
    if (a.maxX < b.minX) dx = b.minX - a.maxX;
    else if (b.maxX < a.minX) dx = a.minX - b.maxX;

    float dy = 0.0f;
    if (a.maxY < b.minY) dy = b.minY - a.maxY;
    else if (b.maxY < a.minY) dy = a.minY - b.maxY;

    return std::sqrt(dx * dx + dy * dy);
}

float PhysicsEngine::computeSpiderTeleportY(
    PlayerState const& state,
    bool goingUp,
    std::vector<LevelObject const*> const& nearbyObjects
) const {
    float half = state.halfSize();
    float bestY = goingUp ? (state.ceilingY - half) : (state.floorY + half);

    for (auto const* obj : nearbyObjects) {
        if (!obj || obj->isDisabled) continue;
        if (obj->category != ObjectCategory::Solid && obj->category != ObjectCategory::Slope) continue;
        if (state.x + half * 0.8f < obj->hitbox.minX || state.x - half * 0.8f > obj->hitbox.maxX) continue;

        if (goingUp) {
            if (obj->hitbox.minY >= state.y + half * 0.5f) {
                bestY = std::min(bestY, obj->hitbox.minY - half);
            }
        } else {
            if (obj->hitbox.maxY <= state.y - half * 0.5f) {
                bestY = std::max(bestY, obj->hitbox.maxY + half);
            }
        }
    }

    return bestY;
}

void PhysicsEngine::applyOrbImpulse(
    PlayerState& state,
    OrbType orb,
    std::vector<LevelObject const*> const& nearbyObjects
) const {
    float gSign = state.isUpsideDown ? -1.0f : 1.0f;
    float miniScale = state.isMini() ? 0.84f : 1.0f;

    switch (orb) {
        case OrbType::Yellow:
            state.velocityY = gSign * 610.0f * miniScale;
            state.isOnGround = false;
            break;
        case OrbType::Pink:
            state.velocityY = gSign * 440.0f * miniScale;
            state.isOnGround = false;
            break;
        case OrbType::Red:
            state.velocityY = gSign * 810.0f * miniScale;
            state.isOnGround = false;
            break;
        case OrbType::Blue:
            state.isUpsideDown = !state.isUpsideDown;
            state.velocityY = (state.isUpsideDown ? 260.0f : -260.0f) * miniScale;
            state.isOnGround = false;
            break;
        case OrbType::Green:
            state.isUpsideDown = !state.isUpsideDown;
            state.velocityY = (state.isUpsideDown ? -610.0f : 610.0f) * miniScale;
            state.isOnGround = false;
            break;
        case OrbType::Black:
            state.velocityY = -gSign * 880.0f;
            state.isOnGround = false;
            break;
        case OrbType::DashGreen:
            state.isDashing = true;
            state.velocityY = 0.0f;
            state.isOnGround = false;
            break;
        case OrbType::DashPink:
            state.isUpsideDown = !state.isUpsideDown;
            state.isDashing = true;
            state.velocityY = 0.0f;
            state.isOnGround = false;
            break;
        case OrbType::Spider: {
            bool goingUp = !state.isUpsideDown;
            state.isUpsideDown = goingUp;
            state.y = computeSpiderTeleportY(state, goingUp, nearbyObjects);
            state.velocityY = 0.0f;
            state.isOnGround = true;
            break;
        }
        case OrbType::None:
            break;
    }
}

void PhysicsEngine::applyPadImpulse(
    PlayerState& state,
    PadType pad,
    std::vector<LevelObject const*> const& nearbyObjects
) const {
    float gSign = state.isUpsideDown ? -1.0f : 1.0f;
    float miniScale = state.isMini() ? 0.85f : 1.0f;

    switch (pad) {
        case PadType::Yellow:
            state.velocityY = gSign * 740.0f * miniScale;
            state.isOnGround = false;
            break;
        case PadType::Pink:
            state.velocityY = gSign * 515.0f * miniScale;
            state.isOnGround = false;
            break;
        case PadType::Red:
            state.velocityY = gSign * 940.0f * miniScale;
            state.isOnGround = false;
            break;
        case PadType::Blue:
            state.isUpsideDown = !state.isUpsideDown;
            state.velocityY = (state.isUpsideDown ? 320.0f : -320.0f) * miniScale;
            state.isOnGround = false;
            break;
        case PadType::Spider: {
            bool goingUp = !state.isUpsideDown;
            state.isUpsideDown = goingUp;
            state.y = computeSpiderTeleportY(state, goingUp, nearbyObjects);
            state.velocityY = 0.0f;
            state.isOnGround = true;
            break;
        }
        case PadType::None:
            break;
    }
}

void PhysicsEngine::applyModeKinematics(
    PlayerState& state,
    bool holdInput,
    bool justPressed,
    float dt,
    std::vector<LevelObject const*> const& nearbyObjects
) const {
    float gSign = state.isUpsideDown ? -1.0f : 1.0f;
    float gMod = std::max(state.gravityMod, 0.2f);
    float miniScale = state.isMini() ? 0.84f : 1.0f;

    // Dash ring holds player horizontally until release
    if (state.isDashing) {
        if (!holdInput) {
            state.isDashing = false;
        } else {
            state.velocityY = 0.0f;
            state.x += (state.isGoingLeft ? -state.velocityX : state.velocityX) * dt;
            state.tickTime += dt;
            return;
        }
    }

    switch (state.mode) {
        case GameMode::Cube: {
            if (holdInput && state.isOnGround) {
                state.velocityY = gSign * m_params.cubeJumpVelocity * miniScale;
                state.isOnGround = false;
            }
            state.velocityY -= gSign * m_params.baseGravity * gMod * dt;
            state.velocityY = std::clamp(state.velocityY, -900.0f, 900.0f);
            break;
        }

        case GameMode::Ship: {
            float thrust = m_params.shipThrustAccel * (state.isMini() ? 1.18f : 1.0f);
            float grav = m_params.shipGravityAccel * (state.isMini() ? 1.18f : 1.0f) * gMod;
            float accel = holdInput ? (gSign * thrust) : (-gSign * grav);
            state.velocityY += accel * dt;
            float maxV = m_params.shipMaxVelY * (state.isMini() ? 1.12f : 1.0f);
            state.velocityY = std::clamp(state.velocityY, -maxV, maxV);
            break;
        }

        case GameMode::Ball: {
            if (holdInput && state.isOnGround) {
                state.isUpsideDown = !state.isUpsideDown;
                gSign = state.isUpsideDown ? -1.0f : 1.0f;
                state.velocityY = -gSign * 145.0f;
                state.isOnGround = false;
            }
            state.velocityY -= gSign * m_params.ballGravityAccel * gMod * dt;
            state.velocityY = std::clamp(state.velocityY, -820.0f, 820.0f);
            break;
        }

        case GameMode::UFO: {
            if (justPressed) {
                state.velocityY = gSign * m_params.ufoJumpVelocity * miniScale;
                state.isOnGround = false;
            }
            state.velocityY -= gSign * m_params.ufoGravityAccel * gMod * dt;
            state.velocityY = std::clamp(state.velocityY, -740.0f, 740.0f);
            break;
        }

        case GameMode::Wave: {
            float slope = m_params.waveSlopeFactor * (state.isMini() ? 1.732f : 1.0f);
            float dir = holdInput ? gSign : -gSign;
            state.velocityY = dir * std::abs(state.velocityX) * slope;
            state.isOnGround = false;
            break;
        }

        case GameMode::Robot: {
            if (holdInput && state.isOnGround) {
                state.velocityY = gSign * m_params.robotMinJumpVel * miniScale;
                state.robotHoldTime = 0.0;
                state.isOnGround = false;
            } else if (holdInput && state.robotHoldTime < m_params.robotMaxHoldSec &&
                       ((gSign > 0.0f && state.velocityY > 0.0f) || (gSign < 0.0f && state.velocityY < 0.0f))) {
                state.velocityY += gSign * m_params.robotMaxBoostAccel * dt;
                state.robotHoldTime += dt;
            } else if (!holdInput) {
                state.robotHoldTime = m_params.robotMaxHoldSec;
            }
            state.velocityY -= gSign * m_params.baseGravity * gMod * dt;
            state.velocityY = std::clamp(state.velocityY, -900.0f, 900.0f);
            break;
        }

        case GameMode::Spider: {
            if (justPressed && state.isOnGround) {
                bool goingUp = !state.isUpsideDown;
                state.isUpsideDown = goingUp;
                state.y = computeSpiderTeleportY(state, goingUp, nearbyObjects);
                state.velocityY = 0.0f;
                state.isOnGround = true;
            } else {
                state.velocityY -= gSign * m_params.baseGravity * gMod * dt;
                state.velocityY = std::clamp(state.velocityY, -880.0f, 880.0f);
            }
            break;
        }

        case GameMode::Swing: {
            if (justPressed) {
                state.isUpsideDown = !state.isUpsideDown;
                gSign = state.isUpsideDown ? -1.0f : 1.0f;
            }
            state.velocityY -= gSign * m_params.swingGravityAccel * gMod * dt;
            float maxV = m_params.swingMaxVelY * (state.isMini() ? 1.15f : 1.0f);
            state.velocityY = std::clamp(state.velocityY, -maxV, maxV);
            break;
        }
    }

    float dx = (state.isGoingLeft ? -state.velocityX : state.velocityX) * dt;
    state.x += dx;
    state.y += state.velocityY * dt;
    state.isHolding = holdInput;
    state.tickTime += dt;
}

void PhysicsEngine::resolveObjectInteractions(
    PlayerState& state,
    bool holdInput,
    bool justPressed,
    std::vector<LevelObject const*> const& nearbyObjects,
    bool& outHitHazard,
    bool& outHitWall,
    bool& outTriggeredOrb,
    bool& outTriggeredPad,
    bool& outChangedMode,
    float& outMinHazardDist
) const {
    float half = state.halfSize();
    bool landedThisStep = false;

    // Ground & ceiling boundaries
    if (state.y - half <= state.floorY) {
        state.y = state.floorY + half;
        if (!state.isUpsideDown) {
            state.velocityY = std::max(0.0f, state.velocityY);
            landedThisStep = true;
        } else if (state.mode == GameMode::Ship || state.mode == GameMode::UFO ||
                   state.mode == GameMode::Wave || state.mode == GameMode::Swing) {
            state.velocityY = std::max(0.0f, state.velocityY);
        }
    }

    if (state.y + half >= state.ceilingY) {
        state.y = state.ceilingY - half;
        if (state.isUpsideDown) {
            state.velocityY = std::min(0.0f, state.velocityY);
            landedThisStep = true;
        } else if (state.mode == GameMode::Ship || state.mode == GameMode::UFO ||
                   state.mode == GameMode::Wave || state.mode == GameMode::Swing) {
            state.velocityY = std::min(0.0f, state.velocityY);
        } else if (state.mode == GameMode::Cube || state.mode == GameMode::Robot) {
            // Hitting ceiling hard in cube/robot when unbounded
            state.velocityY = std::min(0.0f, state.velocityY);
        }
    }

    Rect playerBox = state.hitbox();
    Rect hazardBox = state.hazardHitbox().expanded(m_params.hazardSafetyMargin, m_params.hazardSafetyMargin);

    state.inOrbRange = false;
    state.activeOrbIndex = -1;

    for (auto const* obj : nearbyObjects) {
        if (!obj || obj->isDisabled) continue;

        switch (obj->category) {
            case ObjectCategory::Hazard: {
                float dist = rectDistance(state.hazardHitbox(), obj->hitbox);
                outMinHazardDist = std::min(outMinHazardDist, dist);
                if (hazardBox.intersects(obj->hitbox)) {
                    outHitHazard = true;
                    state.isDead = true;
                    return;
                }
                break;
            }

            case ObjectCategory::Solid: {
                if (!playerBox.intersects(obj->hitbox)) break;

                // Determine overlap along X and Y
                float overlapLeft = playerBox.maxX - obj->hitbox.minX;
                float overlapBottom = playerBox.maxY - obj->hitbox.minY; // player hitting underside
                float overlapTop = obj->hitbox.maxY - playerBox.minY;    // player landing on top

                bool isVerticalLanding =
                    (!state.isUpsideDown && state.velocityY <= 25.0f && overlapTop <= 14.0f && overlapTop < overlapLeft + 4.0f);
                bool isCeilingLanding =
                    (state.isUpsideDown && state.velocityY >= -25.0f && overlapBottom <= 14.0f && overlapBottom < overlapLeft + 4.0f);

                bool isFriendlySurface =
                    (state.mode == GameMode::Ship || state.mode == GameMode::UFO || state.mode == GameMode::Swing) &&
                    (overlapTop <= 14.0f || overlapBottom <= 14.0f);

                if (state.mode == GameMode::Wave && !obj->isPassable) {
                    // Wave crashes on any solid block contact (unless D-block)
                    outHitWall = true;
                    state.isDead = true;
                    return;
                }

                if (isVerticalLanding || (isFriendlySurface && overlapTop <= overlapBottom)) {
                    state.y = obj->hitbox.maxY + half;
                    state.velocityY = std::max(0.0f, state.velocityY);
                    if (!state.isUpsideDown) landedThisStep = true;
                    playerBox = state.hitbox();
                } else if (isCeilingLanding || (isFriendlySurface && overlapBottom < overlapTop)) {
                    state.y = obj->hitbox.minY - half;
                    state.velocityY = std::min(0.0f, state.velocityY);
                    if (state.isUpsideDown) landedThisStep = true;
                    playerBox = state.hitbox();
                } else if (!obj->isPassable) {
                    // Head-on collision with solid wall -> fatal
                    outHitWall = true;
                    state.isDead = true;
                    return;
                }
                break;
            }

            case ObjectCategory::Slope: {
                if (!playerBox.intersects(obj->hitbox)) break;
                float t = std::clamp(
                    (state.x - obj->hitbox.minX) / std::max(obj->hitbox.width(), 1.0f),
                    0.0f,
                    1.0f
                );
                float surfaceY = obj->slopeUphill
                    ? (obj->hitbox.minY + t * obj->hitbox.height())
                    : (obj->hitbox.maxY - t * obj->hitbox.height());

                if (!state.isUpsideDown && state.y - half <= surfaceY + 4.0f) {
                    state.y = surfaceY + half;
                    state.velocityY = std::max(state.velocityY, obj->slopeUphill ? state.velocityX * 0.85f : 0.0f);
                    state.isOnSlope = true;
                    landedThisStep = true;
                    playerBox = state.hitbox();
                }
                break;
            }

            case ObjectCategory::Pad: {
                if (playerBox.intersects(obj->hitbox)) {
                    applyPadImpulse(state, obj->padType, nearbyObjects);
                    outTriggeredPad = true;
                    landedThisStep = false;
                }
                break;
            }

            case ObjectCategory::Orb: {
                Rect orbZone = obj->hitbox.expanded(10.0f, 10.0f);
                if (playerBox.intersects(orbZone)) {
                    state.inOrbRange = true;
                    state.activeOrbIndex = obj->uniqueIndex;
                    bool shouldActivate = justPressed || (holdInput && (obj->orbType == OrbType::DashGreen || obj->orbType == OrbType::DashPink));
                    if (shouldActivate) {
                        applyOrbImpulse(state, obj->orbType, nearbyObjects);
                        outTriggeredOrb = true;
                        landedThisStep = false;
                    }
                }
                break;
            }

            case ObjectCategory::PortalMode: {
                if (playerBox.intersects(obj->hitbox) && state.mode != obj->targetMode) {
                    state.mode = obj->targetMode;
                    outChangedMode = true;
                }
                break;
            }

            case ObjectCategory::PortalGravity: {
                if (playerBox.intersects(obj->hitbox)) {
                    if (obj->gravityDirection == +1) state.isUpsideDown = false;
                    else if (obj->gravityDirection == -1) state.isUpsideDown = true;
                    else if (obj->gravityDirection == 2) state.isUpsideDown = !state.isUpsideDown;
                }
                break;
            }

            case ObjectCategory::PortalSpeed: {
                if (playerBox.intersects(obj->hitbox)) {
                    state.speedTier = obj->targetSpeed;
                    state.velocityX = speedTierToVelocity(obj->targetSpeed);
                }
                break;
            }

            case ObjectCategory::PortalSize: {
                if (playerBox.intersects(obj->hitbox)) {
                    if (obj->setsMini) state.vehicleSize = 0.6f;
                    else if (obj->setsNormalSize) state.vehicleSize = 1.0f;
                }
                break;
            }

            default:
                break;
        }
    }

    state.isOnGround = landedThisStep;
}

void PhysicsEngine::step(
    PlayerState& state,
    bool holdInput,
    bool justPressed,
    float dt,
    std::vector<LevelObject const*> const& nearbyObjects,
    bool& outHitHazard,
    bool& outHitWall,
    bool& outTriggeredOrb,
    bool& outTriggeredPad,
    bool& outChangedMode,
    float& outMinHazardDist
) const {
    if (state.isDead) return;
    applyModeKinematics(state, holdInput, justPressed, dt, nearbyObjects);
    resolveObjectInteractions(
        state,
        holdInput,
        justPressed,
        nearbyObjects,
        outHitHazard,
        outHitWall,
        outTriggeredOrb,
        outTriggeredPad,
        outChangedMode,
        outMinHazardDist
    );
}

SimulationResult PhysicsEngine::simulateRollout(
    PlayerState const& startState,
    InputPlan const& plan,
    float durationSec,
    float stepDt,
    LevelAnalyzer const& analyzer,
    float corridorWeight
) const {
    SimulationResult res{};
    PlayerState sim = startState;
    int totalSteps = std::max(1, static_cast<int>(std::round(durationSec / std::max(stepDt, 1e-4f))));

    float maxReachX = std::abs(sim.velocityX) * durationSec * 1.35f + 120.0f;
    std::vector<LevelObject const*> windowObjs;
    windowObjs.reserve(64);
    analyzer.queryRange(sim.x - 40.0f, sim.x + maxReachX, windowObjs);

    bool prevHold = sim.isHolding && !plan.pulseOnStart;
    float accumCorridorDev = 0.0f;

    for (int i = 0; i < totalSteps; ++i) {
        bool hold = plan.initialHold;
        if (plan.switchStep >= 0 && i >= plan.switchStep) {
            if (plan.closedLoopCorridor && (sim.mode == GameMode::Ship || sim.mode == GameMode::Wave)) {
                CorridorSample target = analyzer.sampleCorridor(sim.x + std::abs(sim.velocityX) * 0.12f);
                float predictedY = sim.y + sim.velocityY * 0.16f;
                bool wantUp = predictedY < target.targetY;
                hold = sim.isUpsideDown ? !wantUp : wantUp;
            } else {
                hold = plan.secondHold;
            }
        }
        bool justPressed = (hold && !prevHold) || (i == 0 && plan.pulseOnStart && hold);
        prevHold = hold;

        bool hitHazard = false;
        bool hitWall = false;
        bool hitOrb = false;
        bool hitPad = false;
        bool changedMode = false;
        float minDist = 9999.0f;

        step(sim, hold, justPressed, stepDt, windowObjs, hitHazard, hitWall, hitOrb, hitPad, changedMode, minDist);

        res.minHazardDistance = std::min(res.minHazardDistance, minDist);
        res.triggeredOrb = res.triggeredOrb || hitOrb;
        res.triggeredPad = res.triggeredPad || hitPad;
        res.changedMode = res.changedMode || changedMode;

        if (hitHazard || hitWall || sim.isDead) {
            res.survived = false;
            res.hitHazard = hitHazard;
            res.hitWall = hitWall;
            res.stepsSurvived = i;
            break;
        }

        // Accumulate deviation from safe corridor center
        CorridorSample corr = analyzer.sampleCorridor(sim.x);
        float dev = std::abs(sim.y - corr.targetY);
        accumCorridorDev += dev;

        res.stepsSurvived = i + 1;
    }

    res.finalX = sim.x;
    res.finalY = sim.y;
    res.finalState = sim;
    res.corridorDeviation = (res.stepsSurvived > 0)
        ? (accumCorridorDev / static_cast<float>(res.stepsSurvived))
        : 999.0f;

    // Composite trajectory quality score
    float survivalRatio = static_cast<float>(res.stepsSurvived) / static_cast<float>(totalSteps);
    float clearanceBonus = std::min(res.minHazardDistance, 60.0f) * 0.45f;
    float corridorPenalty = res.corridorDeviation * 0.18f * corridorWeight;
    float orbBonus = res.triggeredOrb ? 18.0f : 0.0f;

    res.score = (res.survived ? 500.0f : 0.0f)
              + survivalRatio * 300.0f
              + clearanceBonus
              + orbBonus
              - corridorPenalty;

    return res;
}

} // namespace gdautopilot
