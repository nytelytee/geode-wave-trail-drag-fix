#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;


class $modify(WTDFPlayerObject, PlayerObject) {

  struct Fields {
  
    // why is there so much state to keep track of
    // this used to be so simple

    // a point would not have been otherwise added, but force it to be added
    bool forceAdd = true;

    // the pad has the same effect as the ring, except it has to be done 1 update later
    bool spiderPadTriggered = false;
    // the points that these 2 add are different from standard forceAdd, and they take precedence over it
    bool forceAddSpiderPad = false;
    bool forceAddSpiderRing = false;

    bool justTeleported = false;
    bool teleportedPreviouslySpiderRing = false;
    bool transitionToCollision = false;
    float portalTargetLine;
    CCPoint previousPos{-12, -12};
    CCPoint currentPos{-12, -12};
    CCPoint nextPosNoCollision{-12, -12};
    
    // slopes aren't stored in m_collidedObject for some reason
    GameObject* collidedSlope = nullptr;
    
  };

  void resetObject() {
    WTDFPlayerObject::Fields defaultFields;
    *m_fields.operator->() = defaultFields;
    m_waveTrail->reset();
    PlayerObject::resetObject();
  }
  
  bool preSlopeCollision(float p0, GameObject* p1) {
    bool value = PlayerObject::preSlopeCollision(p0, p1);
    if (!value) m_fields->collidedSlope = p1;
    return value;
  }
  
  void update(float deltaFactor) {
    m_fields->currentPos = getRealPosition();
    PlayerObject::update(deltaFactor);
    m_fields->nextPosNoCollision = getRealPosition();
    m_fields->collidedSlope = nullptr;
  }

  void postCollision(float deltaFactor) {
    PlayerObject::postCollision(deltaFactor);

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
      m_fields->forceAddSpiderPad = false;
      m_fields->spiderPadTriggered = false;
      m_fields->forceAdd = false;
      m_fields->portalTargetLine = m_isSideways ? previousPosition.y : previousPosition.x;
      addWaveTrailPoint(previousPosition);
      return;
    } else if (m_fields->forceAdd && !m_fields->forceAddSpiderRing && !m_fields->forceAddSpiderPad) {
      m_fields->forceAdd = false;
      addWaveTrailPoint(currentPosition);
      m_fields->previousPos = currentPosition;
      return;
    } else if (m_fields->spiderPadTriggered) {
      m_fields->spiderPadTriggered = false;
      m_fields->forceAddSpiderPad = true;
      m_fields->previousPos = currentPosition;
      return;
    } else if (m_fields->forceAddSpiderRing || m_fields->forceAddSpiderPad) {
      // spider orbs and pads require special care so the line looks straight
      m_fields->forceAddSpiderRing = false;
      m_fields->forceAddSpiderPad = false;
      m_fields->forceAdd = false;
      m_fields->transitionToCollision = false;
      m_fields->teleportedPreviouslySpiderRing = false;
      CCPoint pointToAdd = m_isSideways ? CCPoint{nextPosition.x, previousPosition.y} : CCPoint{previousPosition.x, nextPosition.y};
      addWaveTrailPoint(pointToAdd);
      m_fields->previousPos = pointToAdd;
      return;
    } else if (m_fields->teleportedPreviouslySpiderRing) {
      m_fields->teleportedPreviouslySpiderRing = false;
      CCPoint pointToAdd = m_isSideways ? CCPoint{nextPosition.x, m_fields->portalTargetLine} : CCPoint{m_fields->portalTargetLine, nextPosition.y};
      addWaveTrailPoint(pointToAdd);
      m_fields->previousPos = pointToAdd;
      return;
    }

    cocos2d::CCPoint currentVector = (nextPosition - currentPosition).normalize();
    cocos2d::CCPoint previousVector = (currentPosition - previousPosition).normalize();
    float crossProductMagnitude = abs(currentVector.cross(previousVector));
    
    // make the error margin inversely proportional to the delta factor
    // if 2 updates are extremely close together calculating the angle between them may lead to inaccuracies
    float errorMargin = 0.004/deltaFactor;

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
    if (crossProductMagnitude <= errorMargin) return;
    
    if (nextPosition != nextPositionNoCollision && !m_fields->transitionToCollision) {

      // if the player is supposed to land on a block between 2 updates, this calculates
      // where the player would have landed, and adds that as a point rather than the current position.
      // this algorithm is technically incorrect on slopes, but it works well enough.
      // trying to make it correct on slopes causes more problems than it solves.
      // see the unfinished 'slope' branch if you want to see my attempt at it,
      // where i ended up giving up
      
      float objectBoundMin = std::numeric_limits<float>::lowest();
      float objectBoundMax = std::numeric_limits<float>::max();
      float hitboxSizeProbably = getObjectRect().size.width;
      GameObject *object = m_collidedObject ? m_collidedObject : m_fields->collidedSlope;
      if (object) {
        objectBoundMin = object->getObjectRect().getMinX(); 
        objectBoundMax = object->getObjectRect().getMaxX(); 
      }

      if (m_isSideways) {
        if (object) {
          objectBoundMin = object->getObjectRect().getMinY(); 
          objectBoundMax = object->getObjectRect().getMaxY(); 
        }
        std::swap(nextPositionNoCollision.x, nextPositionNoCollision.y);
        std::swap(nextPosition.x, nextPosition.y);
        std::swap(currentPosition.x, currentPosition.y);
      }
      float intercept = (nextPositionNoCollision.y - currentPosition.y)/(nextPositionNoCollision.x - currentPosition.x);
      float desiredValue = (nextPosition.y - currentPosition.y + intercept * currentPosition.x)/intercept;
      // this happens if the player snapped to an object
      if (
        (desiredValue < currentPosition.x && !m_isGoingLeft) ||
        (desiredValue > currentPosition.x && m_isGoingLeft) ||
        (currentPosition.x + hitboxSizeProbably/2 < objectBoundMin && !m_isGoingLeft) ||
        (currentPosition.x - hitboxSizeProbably/2 > objectBoundMax && m_isGoingLeft)
      ) {
        // draw a vertical if the player snapped
        if (m_isSideways) addWaveTrailPoint({currentPosition.y, currentPosition.x});
        else addWaveTrailPoint(currentPosition);
        desiredValue = currentPosition.x;
      }
      CCPoint intersectionPoint{desiredValue, nextPosition.y};
      if (m_isSideways) {
        std::swap(nextPositionNoCollision.x, nextPositionNoCollision.y);
        std::swap(nextPosition.x, nextPosition.y);
        std::swap(currentPosition.x, currentPosition.y);
        std::swap(intersectionPoint.x, intersectionPoint.y);
      }
      addWaveTrailPoint(intersectionPoint);
      m_fields->previousPos = intersectionPoint;
      m_fields->transitionToCollision = true;
    } else {
      if (nextPosition == nextPositionNoCollision) m_fields->transitionToCollision = false;
      addWaveTrailPoint(currentPosition);
      m_fields->previousPos = currentPosition;
    }
    
  }

  // this will add the requested point to the wave trail, and check if it is (almost) colinear with the last 2 points
  // if it is, it will remove the last point before adding the requested one, because there is no need to have a
  // colinear duplicate
  inline void addWaveTrailPoint(CCPoint point) {
    unsigned objectCount = static_cast<unsigned>(m_waveTrail->m_pointArray->count());
    if (objectCount >= 3) {
      CCPoint point0 = static_cast<PointNode *>(m_waveTrail->m_pointArray->objectAtIndex(objectCount - 2))->m_point;
      CCPoint point1 = static_cast<PointNode *>(m_waveTrail->m_pointArray->objectAtIndex(objectCount - 1))->m_point;
      CCPoint point2 = point;
      CCPoint vector01 = (point1 - point0).normalize();
      CCPoint vector12 = (point2 - point1).normalize();
      float cpm = abs(vector12.cross(vector01));
      if (cpm <= 0.001) m_waveTrail->m_pointArray->removeObjectAtIndex(objectCount - 1);
    }
    m_waveTrail->addPoint(point);
  }
  
  // spider orb
  bool pushButton(PlayerButton button) {
    const int TOGGLE_RING = 1594;
    const int TELEPORT_RING = 3027;
    const int SPIDER_RING = 3004;
    if (!m_isDart || !m_gameLayer || LevelEditorLayer::get()) return PlayerObject::pushButton(button);
    bool willTriggerSpiderRing = false;
    
    for (unsigned i = 0; i < m_touchingRings->count(); i++) {
      RingObject *ring = static_cast<RingObject *>(m_touchingRings->objectAtIndex(i));
      switch (ring->m_objectID) {
        // these 2 seem to allow the click to reach the next ring
        case TOGGLE_RING:  // fallthrough
          if (ring->m_claimTouch) return PlayerObject::pushButton(button);
        case TELEPORT_RING:
          continue;
        case SPIDER_RING: // fallthrough
          willTriggerSpiderRing = true; // will trigger unless a toggle ring claims the touch
        default:
          if (!willTriggerSpiderRing) return PlayerObject::pushButton(button);
      }
    }
    if (willTriggerSpiderRing) {
      m_waveTrail->addPoint(m_fields->currentPos);
      m_fields->previousPos = m_fields->currentPos;
      m_fields->forceAddSpiderRing = true;
    }
    return PlayerObject::pushButton(button);
  }
  
  // spider pad
  void spiderTestJump(bool p0) {
    if (!m_isDart || !m_gameLayer || LevelEditorLayer::get() || m_fields->justTeleported) return PlayerObject::spiderTestJump(p0);
    m_waveTrail->addPoint(m_fields->currentPos);
    m_fields->previousPos = m_fields->currentPos;
    // this runs on both spider orbs and pads triggering
    // this only needs to have an effect if a pad was triggered specifically
    if (!m_fields->forceAddSpiderRing) m_fields->spiderPadTriggered = true;
    PlayerObject::spiderTestJump(p0);
  }

  void doReversePlayer(bool state) {
    m_fields->forceAdd = true;
    PlayerObject::doReversePlayer(state);
  }

  void placeStreakPoint() { if (!m_isDart || !m_gameLayer) PlayerObject::placeStreakPoint(); }

  void toggleVisibility(bool state) {
    bool needsPoint = m_isHidden;
    PlayerObject::toggleVisibility(state);
    if (state && m_isDart && needsPoint) m_fields->forceAdd = true;
  }
};

class $modify(PlayLayer) {
  void playEndAnimationToPos(CCPoint pos) {
    if (m_player1 && m_player1->m_isDart)
      m_player1->m_waveTrail->addPoint(m_player1->getRealPosition()); 
    if (m_player2 && m_player2->m_isDart)
      m_player2->m_waveTrail->addPoint(m_player2->getRealPosition());
    PlayLayer::playEndAnimationToPos(pos);
  }
};

class $modify(GJBaseGameLayer) {
  void teleportPlayer(TeleportPortalObject *portal, PlayerObject *player) {
    GJBaseGameLayer::teleportPlayer(portal, player);
    // teleport trigger passes the player as null, the original function falls back to player 1
    // this does mean that the teleport trigger does not let you teleport player 2. lol.
    // thank you hiimjustin000 for identifying the case where player can be null
    if (!player) player = m_player1;
    if (LevelEditorLayer::get() || !player->m_isDart) return;
    static_cast<WTDFPlayerObject *>(player)->m_fields->previousPos = player->getRealPosition();
    static_cast<WTDFPlayerObject *>(player)->m_fields->justTeleported = true;
  }
  

  void toggleDualMode(GameObject* portal, bool state, PlayerObject* playerTouchingPortal, bool p4) {
    if (!state && playerTouchingPortal == m_player2) {
      // player 2 will become player 1 after the original executes, so copy the fields from player 2 and add them to player 1 afterward
      WTDFPlayerObject::Fields fieldsBackup = *static_cast<WTDFPlayerObject *>(m_player2)->m_fields.operator->();
      GJBaseGameLayer::toggleDualMode(portal, state, playerTouchingPortal, p4);
      *static_cast<WTDFPlayerObject *>(m_player1)->m_fields.operator->() = fieldsBackup;
    } else {
      GJBaseGameLayer::toggleDualMode(portal, state, playerTouchingPortal, p4);
    }
  }
};
