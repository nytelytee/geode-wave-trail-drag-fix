#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(WTDFPlayerObject, PlayerObject) {
  
  // why is there so much state to keep track of
  // this used to be so simple
  bool forceAdd = true;
  bool dontAdd = false;
  bool forceAddSpiderRing = false;
  bool justTeleported = false;
  bool teleportedPreviouslySpiderRing = false;
  bool transitionToCollision = false;
  bool inputChanged = false;
  float portalTargetLine;
  CCPoint previousPos{-12, -12};
  CCPoint currentPos{-12, -12};
  CCPoint nextPosNoCollision{-12, -12};

  void resetObject() {
    m_waveTrail->reset();
    PlayerObject::resetObject();
    m_fields->forceAdd = true;
  }
  
  void activateStreak() {
    PlayerObject::activateStreak();
    m_fields->dontAdd = true;
  }

  void update(float p0) {
    // i used to just use m_position as the current position, but apparently that only gets updated once per frame
    // this became really apparent once Click Between Frames came out, so i guess i am going back to doing it this way
    // i would have had to do this anyway to get nextPosNoCollision, so yeah
    m_fields->currentPos = getRealPosition();
    PlayerObject::update(p0);
    m_fields->nextPosNoCollision = getRealPosition();
  }

  void postCollision(float p0) {
    PlayerObject::postCollision(p0);

    if (LevelEditorLayer::get() || !m_gameLayer) return;
    
    if (!m_isDart || m_isHidden) {
      m_fields->previousPos = m_fields->currentPos;
      return;
    }
    
    CCPoint previousPosition = m_fields->previousPos;
    CCPoint currentPosition = m_fields->currentPos;
    CCPoint nextPositionNoCollision = m_fields->nextPosNoCollision;
    CCPoint nextPosition = getRealPosition();

    if (m_fields->justTeleported) {
      // if we just teleported, we set a streak point to the portal's position and save its location
      // we need to place a new point if we are clicking a spider orb and teleporting at the same time
      // as well
      m_fields->justTeleported = false;
      m_fields->teleportedPreviouslySpiderRing = m_fields->forceAddSpiderRing;
      m_fields->forceAddSpiderRing = false;
      m_fields->forceAdd = false;
      m_fields->inputChanged = false;
      m_fields->portalTargetLine = m_isSideways ? previousPosition.y : previousPosition.x;
      m_waveTrail->addPoint(previousPosition);
      return;
    } else if (m_fields->forceAdd && !m_fields->forceAddSpiderRing) {
      m_fields->forceAdd = false;
      m_fields->inputChanged = false;
      m_waveTrail->addPoint(currentPosition);
      m_fields->previousPos = currentPosition;
      return;
    } else if (m_fields->forceAddSpiderRing) {
      // spider orb require special care so the line looks straight
      m_fields->forceAddSpiderRing = false;
      m_fields->forceAdd = false;
      m_fields->inputChanged = false;
      m_fields->transitionToCollision = false;
      m_fields->teleportedPreviouslySpiderRing = false;
      CCPoint pointToAdd = m_isSideways ? CCPoint{nextPosition.x, previousPosition.y} : CCPoint{previousPosition.x, nextPosition.y};
      m_waveTrail->addPoint(pointToAdd);
      m_fields->previousPos = pointToAdd;
      return;
    } else if (m_fields->teleportedPreviouslySpiderRing) {
      m_fields->teleportedPreviouslySpiderRing = false;
      m_fields->inputChanged = false;
      CCPoint pointToAdd = m_isSideways ? CCPoint{nextPosition.x, m_fields->portalTargetLine} : CCPoint{m_fields->portalTargetLine, nextPosition.y};
      m_waveTrail->addPoint(pointToAdd);
      m_fields->previousPos = pointToAdd;
      return;
    }
    cocos2d::CCPoint currentVector = (nextPosition - currentPosition).normalize();
    cocos2d::CCPoint previousVector = (currentPosition - previousPosition).normalize();
    float crossProductMagnitude = abs(currentVector.cross(previousVector));

    // save the current point as prevPoint only if it is placed as a streak point
    // this makes it so that even smooth paths where
    // any 3 consecutive points may look like a straight line
    // still get marked for new point addition, because
    // we are no longer checking for 3 consecutive points
    // this accounts for the wave going in very changing smooth curves
    // but one issue is that the cumulative error eventually gets large enough
    // that a point is placed even when moving in a constant direction
    // i don't think that can be recreated unless you use the zoom trigger to
    // zoom the game out up to a point where you can't even notice that extra point anymore though
    // basically, this trades off accounting for one type of cumulative error
    // (slowly moving smooth transitions not being detected as actually changing moving)
    // over another (the an unchanged direction being detected as a change in direction over a long period of time)

    if (crossProductMagnitude > 0.01 && !m_fields->dontAdd) {
      if (nextPosition != nextPositionNoCollision && !m_fields->transitionToCollision) {
        if (m_isSideways) {
          std::swap(nextPositionNoCollision.x, nextPositionNoCollision.y);
          std::swap(nextPosition.x, nextPosition.y);
          std::swap(currentPosition.x, currentPosition.y);
        }
        float intercept = (nextPositionNoCollision.y - currentPosition.y)/(nextPositionNoCollision.x - currentPosition.x);
        float desiredValue = (nextPosition.y - currentPosition.y + intercept * currentPosition.x)/intercept;
        if ((desiredValue < currentPosition.x && !m_isGoingLeft) || (desiredValue > currentPosition.x && m_isGoingLeft))
          desiredValue = currentPosition.x;
        CCPoint intersectionPoint{desiredValue, nextPosition.y};
        if (m_isSideways) {
          std::swap(nextPositionNoCollision.x, nextPositionNoCollision.y);
          std::swap(nextPosition.x, nextPosition.y);
          std::swap(currentPosition.x, currentPosition.y);
          std::swap(intersectionPoint.x, intersectionPoint.y);
        }
        m_waveTrail->addPoint(intersectionPoint);
        m_fields->previousPos = intersectionPoint;
        m_fields->transitionToCollision = true;
      } else {
        if (nextPosition == nextPositionNoCollision) m_fields->transitionToCollision = false;
        m_waveTrail->addPoint(currentPosition);
        m_fields->previousPos = currentPosition;
        // removes the last point if the input changed, and it's really close to the previous, and the angle changed sufficiently (higher margin of error allowed than the outer check)
        // for some reason, Click Between Frames causes an extra point to get added that isn't *quite* at the same angle as the original
        size_t objectCount = m_waveTrail->m_pointArray->count();
        if (m_fields->inputChanged && objectCount >= 3) {
          m_fields->inputChanged = false;
          CCPoint point0 = static_cast<PointNode *>(m_waveTrail->m_pointArray->objectAtIndex(objectCount - 3))->m_point;
          CCPoint point1 = static_cast<PointNode *>(m_waveTrail->m_pointArray->objectAtIndex(objectCount - 2))->m_point;
          CCPoint point2 = static_cast<PointNode *>(m_waveTrail->m_pointArray->objectAtIndex(objectCount - 1))->m_point;
          CCPoint vector01 = (point1 - point0).normalize();
          CCPoint vector12 = (point2 - point1).normalize();
          float cpm = abs(vector12.cross(vector01));
          if ((cpm <= 0.1 && point2.getDistanceSq(point1) <= 9))
            m_waveTrail->m_pointArray->removeObjectAtIndex(objectCount - 2);
        } else if (m_fields->inputChanged) m_fields->inputChanged = false;
      }
    } else if (m_fields->dontAdd) {
      if (nextPosition == nextPositionNoCollision) m_fields->transitionToCollision = false;
      m_fields->dontAdd = false;
      m_fields->previousPos = currentPosition;
      return;
    }
  }
   
  void releaseButton(PlayerButton p0) {
    m_fields->inputChanged = true;
    PlayerObject::releaseButton(p0);
  }

  // spider orb
  void pushButton(PlayerButton p0) {
    m_fields->inputChanged = true;
    const int TOGGLE_RING = 1594;
    const int TELEPORT_RING = 3027;
    const int SPIDER_RING = 3004;
    if (!m_isDart || !m_gameLayer || LevelEditorLayer::get()) return PlayerObject::pushButton(p0);
    bool willTriggerSpiderRing = false;
    
    for (size_t i = 0; i < m_touchingRings->count(); i++) {
      RingObject *ring = static_cast<RingObject *>(m_touchingRings->objectAtIndex(i));
      switch (ring->m_objectID) {
        // these 2 seem to allow the click to reach the next ring
        case TOGGLE_RING:  // fallthrough
          if (ring->m_claimTouch) return PlayerObject::pushButton(p0);
        case TELEPORT_RING:
          continue;
        case SPIDER_RING: // fallthrough
          willTriggerSpiderRing = true; // will trigger unless a toggle ring claims the touch
        default:
          if (!willTriggerSpiderRing) return PlayerObject::pushButton(p0);
      }
    }
    if (willTriggerSpiderRing) {
      m_waveTrail->addPoint(m_fields->currentPos);
      m_fields->previousPos = m_fields->currentPos;
      m_fields->forceAddSpiderRing = true;
    }
    PlayerObject::pushButton(p0);
  }

  void doReversePlayer(bool p0) {
    m_fields->forceAdd = true;
    PlayerObject::doReversePlayer(p0);
  }

  void placeStreakPoint() { if (!m_isDart || !m_gameLayer) PlayerObject::placeStreakPoint(); }

  void toggleVisibility(bool p0) {
    bool needsPoint = m_isHidden;
    PlayerObject::toggleVisibility(p0);
    if (p0 && m_isDart && needsPoint) m_fields->forceAdd = true;
  }
};

class $modify(PlayLayer) {
  void playEndAnimationToPos(CCPoint p0) {
    if (m_player1 && m_player1->m_isDart)
      m_player1->m_waveTrail->addPoint(m_player1->m_position); 
    if (m_player2 && m_player2->m_isDart)
      m_player2->m_waveTrail->addPoint(m_player2->m_position); 
    PlayLayer::playEndAnimationToPos(p0);
  }
};

// teleport portal
class $modify(GJBaseGameLayer) {
  void teleportPlayer(TeleportPortalObject *portal, PlayerObject *player) {
    GJBaseGameLayer::teleportPlayer(portal, player);
    // no idea why player can be null, but it happened in one level and thus caused a crash
    // it was a platformer level, if anyone has an explanation, let me know
    if (LevelEditorLayer::get() || !player || !player->m_isDart) return;
    CCPoint targetPos = getPortalTargetPos(portal, getPortalTarget(portal), player);
    static_cast<WTDFPlayerObject *>(player)->m_fields->previousPos = targetPos;
    static_cast<WTDFPlayerObject *>(player)->m_fields->justTeleported = true;
    static_cast<WTDFPlayerObject *>(player)->m_fields->dontAdd = true;
  }
};
