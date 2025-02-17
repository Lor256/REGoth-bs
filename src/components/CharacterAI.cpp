#include "CharacterAI.hpp"
#include <Components/BsCCamera.h>
#include <Components/BsCCharacterController.h>
#include <RTTI/RTTI_CharacterAI.hpp>
#include <Scene/BsSceneManager.h>
#include <Scene/BsSceneObject.h>
#include <animation/StateNaming.hpp>
#include <components/Character.hpp>
#include <components/GameWorld.hpp>
#include <components/StoryInformation.hpp>
#include <components/VisualCharacter.hpp>
#include <exception/Throw.hpp>
#include <log/logging.hpp>

namespace REGoth
{
  /** How fast the character can turn when not holding a weapon. (Radians/Second) */
  constexpr float TURN_SPEED_NORMAL = 0.05f;

  /** Multiplicator of how fast the character can turn while holding a weapon. */
  constexpr float TURN_SPEED_MULTIPLICATOR_WITH_WEAPON = 2.0f;

  /**
   * How far away the character can be from the camera until it should disable physics.
   * Must be larger than the range which activates physics again, ACTIVATE_PHYSICS_RANGE_METERS.
   * See https://regoth-project.github.io/REGoth-bs/content/characters.html
   *
   * TODO: Make this configurable.
   */
  constexpr float DEACTIVATE_PHYSICS_RANGE_METERS = 45.0;

  /** See DEACTIVATE_PHYSICS_RANGE_METERS */
  constexpr float ACTIVATE_PHYSICS_RANGE_METERS = 40.0;

  /** Acceleration of the Y-Axis while falling */
  constexpr float FALLING_ACCELERATION_Y = -9.81f;

  /** Constant velocity to apply downwards to keep the player on the ground */
  constexpr float DOWNWARDS_VELOCITY_WHILE_WALKING = -10.0f;

  CharacterAI::CharacterAI(const bs::HSceneObject& parent, HGameWorld world)
      : bs::Component(parent)
      , mWorld(world)
  {
    mVisual = SO()->getComponent<VisualCharacter>();

    if (mVisual.isDestroyed())
    {
      REGOTH_THROW(
          InvalidStateException,
          bs::StringUtil::format("Scene Object {0} does not have a VisualCharacter component!",
                                 SO()->getName()));
    }

    mCharacterController = SO()->getComponent<bs::CCharacterController>();

    if (mCharacterController.isDestroyed())
    {
      REGOTH_THROW(
          InvalidStateException,
          bs::StringUtil::format("Scene Object {0} does not have a CCharacterController component!",
                                 SO()->getName()));
    }
  }

  void CharacterAI::deactivatePhysics()
  {
    mIsPhysicsActive = false;
  }

  void CharacterAI::activatePhysics()
  {
    mIsPhysicsActive = true;
  }

  bool CharacterAI::shouldDisablePhysics() const
  {
    const auto& mainCamera     = bs::gSceneManager().getMainCamera();
    const auto& cameraPosition = mainCamera->getTransform().pos();
    const auto& soPosition     = SO()->getTransform().pos();

    float maxRangeSq = DEACTIVATE_PHYSICS_RANGE_METERS * DEACTIVATE_PHYSICS_RANGE_METERS;

    return cameraPosition.squaredDistance(soPosition) > maxRangeSq;
  }

  bool CharacterAI::shouldEnablePhysics() const
  {
    const auto& mainCamera     = bs::gSceneManager().getMainCamera();
    const auto& cameraPosition = mainCamera->getTransform().pos();
    const auto& soPosition     = SO()->getTransform().pos();

    float minRangeSq = ACTIVATE_PHYSICS_RANGE_METERS * ACTIVATE_PHYSICS_RANGE_METERS;

    return cameraPosition.squaredDistance(soPosition) < minRangeSq;
  }

  bool CharacterAI::isPhysicsActive() const
  {
    return mIsPhysicsActive;
  }

  void CharacterAI::handlePhysicsActivation()
  {
    if (mIsPhysicsActive)
    {
      if (shouldDisablePhysics())
      {
        deactivatePhysics();

        REGOTH_LOG(Info, Uncategorized, "[CharacterAI] Deactivate physics on {0}", SO()->getName());
      }
    }
    else
    {
      if (shouldEnablePhysics())
      {
        activatePhysics();

        REGOTH_LOG(Info, Uncategorized, "[CharacterAI]   Activate physics on {0}", SO()->getName());
      }
    }
  }

  bool CharacterAI::goForward()
  {
    bs::String anim = AnimationState::constructStateAnimationName(mWeaponMode, mWalkMode, "L");

    return tryTransitionToState("L");
  }

  bool CharacterAI::goBackward()
  {
    // Some movement states have an explict state for moving back. The default
    // running state does not.
    if (doesStateExist("BL"))
    {
      return tryTransitionToState("BL");
    }
    else
    {
      if (!isStateSwitchAllowed()) return false;

      return tryPlayTransitionAnimationTo("T_JUMPB");
    }

    return false;
  }

  bool CharacterAI::strafeLeft()
  {
    if (!isStateSwitchAllowed()) return false;

    switch (mWalkMode)
    {
      case AI::WalkMode::Run:
        return tryPlayTransitionAnimationTo("T_RUNSTRAFEL");
      case AI::WalkMode::Walk:
        return tryPlayTransitionAnimationTo("T_WALKSTRAFEL");
      case AI::WalkMode::Sneak:
        return tryPlayTransitionAnimationTo("T_SNEAKSTRAFEL");
      default:
        return false;
    }
  }

  bool CharacterAI::strafeRight()
  {
    if (!isStateSwitchAllowed()) return false;

    switch (mWalkMode)
    {
      case AI::WalkMode::Run:
        return tryPlayTransitionAnimationTo("T_RUNSTRAFER");
      case AI::WalkMode::Walk:
        return tryPlayTransitionAnimationTo("T_WALKSTRAFER");
      case AI::WalkMode::Sneak:
        return tryPlayTransitionAnimationTo("T_SNEAKSTRAFER");
      default:
        return false;
    }
  }

  bool CharacterAI::turnLeft()
  {
    if (!isTurningAllowed()) return false;

    mTurnDirection = TurnDirection::Left;

    return true;
  }

  bool CharacterAI::turnRight()
  {
    if (!isTurningAllowed()) return false;

    mTurnDirection = TurnDirection::Right;
    return true;
  }

  bool CharacterAI::stopMoving()
  {
    if (tryTransitionToState("")) return true;

    // The "STAND" state doesn't really exist but some animation reference it, like
    // the animation "T_JUMP_2_STAND".
    if (tryPlayTransitionAnimationTo("S_STAND")) return true;

    return false;
  }

  bool CharacterAI::stopTurning()
  {
    mTurnDirection = TurnDirection::None;

    return true;
  }

  bool CharacterAI::doAction()
  {
    return true;
  }

  bool CharacterAI::jump()
  {
    if (!isStateSwitchAllowed()) return false;
    if (mIsInAir) return false;

    return tryPlayTransitionAnimationTo("S_JUMP");
  }

  bool CharacterAI::tryPlayTransitionAnimationTo(const bs::String& anim)
  {
    // Cannot play animations if the character has no model yet
    if (!mVisual->hasVisual()) return false;

    bs::String playingNow = mVisual->getPlayingAnimationName();
    auto clipPlayingNow   = mVisual->findAnimationClip(playingNow);

    bs::String animToPlay = mVisual->findAnimationToTransitionTo(anim);
    auto clip             = mVisual->findAnimationClip(animToPlay);

    // Already in target anim
    if (clip == clipPlayingNow) return true;

    // If there is no clip, then the transition isn't meant to be possible.
    // That also includes the empty string.
    if (!clip)
    {
      // However, some animations refer to a special "Stand" anim, which doesn't exist
      // but rather means the current idle animation, if the character is in running
      // or walking mode.
      if (isStanding())
      {
        animToPlay = mVisual->findAnimationToTransitionTo("S_STAND", anim);
        clip       = mVisual->findAnimationClip(animToPlay);
      }
    }

    if (!clip) return false;

    if (!mVisual->isAnimationPlaying(clip))
    {
      mVisual->playAnimationClip(clip);
    }

    return true;
  }

  bool CharacterAI::tryTransitionToState(const bs::String& state)
  {
    if (!isStateSwitchAllowed()) return false;

    bs::String anim = AnimationState::constructStateAnimationName(mWeaponMode, mWalkMode, state);

    return tryPlayTransitionAnimationTo(anim);
  }

  bool CharacterAI::doesStateExist(const bs::String& state) const
  {
    bs::String anim = AnimationState::constructStateAnimationName(mWeaponMode, mWalkMode, state);

    return mVisual->findAnimationClip(anim);
  }

  bool CharacterAI::isStanding() const
  {
    bs::String currentAnimation = mVisual->getPlayingAnimationName();
    bs::String currentState     = AnimationState::getStateName(currentAnimation);

    if (currentState == "RUN") return true;

    if (currentState == "WALK") return true;

    return false;
  }

  bool CharacterAI::isStateSwitchAllowed()
  {
    mVisual                = SO()->getComponent<VisualCharacter>();
    bs::String playingAnim = mVisual->getPlayingAnimationName();

    if (playingAnim.empty()) return true;

    bs::String state = AnimationState::getStateName(playingAnim);

    // Playing some weird animation we don't know the naming scheme for?
    if (state.empty()) return false;

    if (!mVisual->isPlayingAnimationInterruptable()) return false;

    return true;
  }

  bool CharacterAI::isTurningAllowed()
  {
    // TODO: Find out when turning is allowed and implement this
    return true;
  }

  void CharacterAI::instantTurnToPosition(const bs::Vector3& position)
  {
    bs::Vector3 positionSameHeight = position;

    // Characters should stay upright (at least most of them),
    // thus modify the position as if it were straight ahead.
    positionSameHeight.y = SO()->getTransform().pos().y;

    SO()->lookAt(positionSameHeight);
  }

  bool CharacterAI::gotoPositionStraight(const bs::Vector3& position)
  {
    instantTurnToPosition(position);

    goForward();

    return isAtPosition(position);
  }

  bool CharacterAI::isAtPosition(const bs::Vector3& position)
  {
    return (SO()->getTransform().pos() - position).length() < 0.5f;
  }

  void CharacterAI::fastMove(float factor)
  {
    mVisual->setDebugAnimationSpeedFactor(factor);
  }

  void CharacterAI::fixedUpdate()
  {
    handlePhysicsActivation();

    if (!mIsPhysicsActive)
    {
      return;
    }

    if (isTurningAllowed())
    {
      handleTurning();
    }

    handleFallingAndFlying();

    bs::Vector3 rootMotion = bs::Vector3::ZERO;

    if (!mVisual->isPlayingIdleAnimation())
    {
      rootMotion = mVisual->resolveFrameRootMotion();

      // Rotate by the scene objects rotation
      rootMotion = SO()->getTransform().getRotation().rotate(rootMotion);

      // No need to multiply rootMotion by the frame delta since it is the actual movement since
      // last time we queried it. For some reason this is inverted though.
      rootMotion *= -1.0;
    }

    if (needsToUpdatePhysics(rootMotion))
    {
      const float frameDelta = bs::gTime().getFixedFrameDelta();
      bs::Vector3 velocity   = rootMotion;

      if (mIsInAir)
      {
        velocity.y += mFallingVelocity * frameDelta;
      }
      else
      {
        // Since the character controller doesn't "stick" to the ground, we apply a
        // constant downwards velocity to keep it on the ground on slopes.
        velocity.y += DOWNWARDS_VELOCITY_WHILE_WALKING * frameDelta;
      }

      auto flags = mCharacterController->move(velocity);

      // TODO: Check if the character is standing on a dynamic object, which is NOT solid ground!
      mIsStandingOnSolidGround = flags.isSet(bs::CharacterCollisionFlag::Down);

      mIsInAir = !flags.isSet(bs::CharacterCollisionFlag::Down);
    }
  }

  bool CharacterAI::needsToUpdatePhysics(const bs::Vector3& rootMotion) const
  {
    // Apply gravity while being airborne
    if (mIsInAir) return true;

    // Apply gravity in case the object below us moved
    if (!mIsStandingOnSolidGround) return true;

    // Move according to the animation system if it wants us to
    if (rootMotion.squaredLength() > 0.0f) return true;

    return false;
  }

  void CharacterAI::handleTurning()
  {
    float frameTurn = 0.0f;

    switch (mTurnDirection)
    {
      case TurnDirection::None:
        frameTurn = 0.0f;
        break;

      case TurnDirection::Left:
        frameTurn = -TURN_SPEED_NORMAL;
        break;

      case TurnDirection::Right:
        frameTurn = TURN_SPEED_NORMAL;
        break;
    }

    if (fabs(frameTurn) > 0.0001f)
    {
      SO()->rotate(bs::Vector3::UNIT_Y, bs::Radian(frameTurn));
    }
  }

  void CharacterAI::handleFallingAndFlying()
  {
    if (mVisual->isPlayingFlyingAnimation())
    {
      // Overwrite air status, in case we have not lifted off yet
      mIsInAir = true;

      // Falling velocity is taken care of by the animation system
      mFallingVelocity = 0.0f;
    }
    else if (!mIsInAir)
    {
      mFallingVelocity = 0.0f;
    }
    else
    {
      mFallingVelocity += FALLING_ACCELERATION_Y * bs::gTime().getFixedFrameDelta();
    }
  }

  void CharacterAI::teleport(const bs::String& waypoint)
  {
    bs::HSceneObject so = mWorld->findObjectByName(waypoint);

    if (!so)
    {
      // Usually we would throw here, but Gothic has some invalid waypoints inside it's scripts
      // so we would break the original games if we did that. Resort to a warning for those,
      // better than nothing, I guess.
      REGOTH_LOG(Warning, Uncategorized,
                 "[CharacterAI] Teleport failed, waypoint doesn't exist: {0}", waypoint);
      return;
    }

    SO()->setPosition(so->getTransform().pos());

    // Turn the same way the waypoint is oriented, but have the character keep looking forward
    bs::Vector3 forwardCenterd = so->getTransform().getForward();

    forwardCenterd.y = 0;
    forwardCenterd.normalize();

    SO()->setForward(forwardCenterd);
  }

  void CharacterAI::turnToNPC(bs::HSceneObject targetSO)
  {
  }

  void CharacterAI::standUp()
  {
  }

  void CharacterAI::standUpQuick()
  {
  }

  void CharacterAI::gotoWaypoint(const bs::String& waypoint)
  {
  }

  void CharacterAI::gotoFreePoint(const bs::String& freepoint)
  {
  }

  void CharacterAI::gotoNextFreePoint(const bs::String& freepoint)
  {
  }

  void CharacterAI::gotoNpc(bs::HSceneObject targetCharacterSO)
  {
  }

  void CharacterAI::startState(const bs::String& stateFunction, bs::INT32 stateBehavior,
                               const bs::String& waypoint)
  {
  }

  void CharacterAI::wait(float seconds)
  {
  }

  void CharacterAI::playAni(const bs::String& animation)
  {
  }

  bool CharacterAI::changeWalkMode(AI::WalkMode walkMode)
  {
    bs::String stateTarget = AnimationState::constructStateAnimationName(mWeaponMode, walkMode, "");

    bool wasAllowed = tryPlayTransitionAnimationTo(stateTarget);

    if (wasAllowed)
    {
      mWalkMode = walkMode;
    }

    return wasAllowed;
  }

  bool CharacterAI::changeWeaponMode(AI::WeaponMode mode)
  {
    if (!mVisual->hasVisual())
    {
      // If the model hasn't been set up yet, just make it start in the target mode.
      // This happens on the Stone-Guardians in Gothic 2, which call NPC_SetToFistMode() before
      // calling MDL_SetVisual().

      mWeaponMode = mode;

      return true;
    }
    else
    {
      // Model exists, check if the state transition is possible

      bs::String stateTarget = AnimationState::constructStateAnimationName(mode, mWalkMode, "");

      bool wasAllowed = tryPlayTransitionAnimationTo(stateTarget);

      if (wasAllowed)
      {
        mWeaponMode = mode;
      }

      if (!wasAllowed)
      {
        // FIXME: We're missing some aniAliases, for example, "T_RUN_2_SNEAK" exists,
        //        and "T_SNEAK_2_RUN" is just the same animation but in reverse. This
        //        is defined using an aniAlias, which does not seem to be implemented.
        auto c = mVisual->findAnimationClip(stateTarget);

        if (c)
        {
          mVisual->playAnimationClip(c);
          mWeaponMode = mode;
        }
      }

      return wasAllowed;
    }
  }

  void CharacterAI::tryToggleWalking()
  {
    switch (mWalkMode)
    {
      case AI::WalkMode::Run:
        changeWalkMode(AI::WalkMode::Walk);
        break;

      case AI::WalkMode::Walk:
        changeWalkMode(AI::WalkMode::Run);
        break;

      case AI::WalkMode::Sneak:
        changeWalkMode(AI::WalkMode::Run);
        break;

      default:
        // Keep current Walk-Mode
        break;
    }
  }

  void CharacterAI::tryToggleSneaking()
  {
    switch (mWalkMode)
    {
      case AI::WalkMode::Run:
        changeWalkMode(AI::WalkMode::Sneak);
        break;

      case AI::WalkMode::Walk:
        changeWalkMode(AI::WalkMode::Sneak);
        break;

      case AI::WalkMode::Sneak:
        changeWalkMode(AI::WalkMode::Run);
        break;

      default:
        // Keep current Walk-Mode
        break;
    }
  }

  void CharacterAI::tryToggleMeleeWeapon()
  {
    switch (mWeaponMode)
    {
      case AI::WeaponMode::None:
        changeWeaponMode(AI::WeaponMode::Fist);
        break;

      default:
        changeWeaponMode(AI::WeaponMode::None);
        break;
    }
  }

  void CharacterAI::stopProcessingInfos()
  {
  }

  void CharacterAI::output(bs::HSceneObject characterSO, const bs::String& svmName)
  {
  }

  void CharacterAI::processInfos()
  {
  }

  REGOTH_DEFINE_RTTI(CharacterAI);
}  // namespace REGoth
