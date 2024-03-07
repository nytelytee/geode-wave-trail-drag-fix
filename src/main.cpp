#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(WaveTrailFixPlayerObject, PlayerObject) {

	cocos2d::CCPoint prev_position{-12, -12};
	cocos2d::CCPoint current_position{-12, -12};

	bool force_add = true;
	bool dont_add = false;

	void resetObject() {
		m_fields->force_add = true;
		PlayerObject::resetObject();
	}

	void update(float p0) {
		m_fields->current_position = getRealPosition();
		PlayerObject::update(p0);
	}

	void postCollision(float p0) {
		PlayerObject::postCollision(p0);
		if (!m_isDart) {
			m_fields->prev_position = m_fields->current_position;
			return;
		}

		CCPoint next_position = getRealPosition();
		
		if (m_fields->force_add) {
			m_fields->force_add = false;
			m_waveTrail->addPoint(m_fields->current_position);
			return;
		}
		
		cocos2d::CCPoint current_vector = next_position - m_fields->current_position;
		cocos2d::CCPoint previous_vector = m_fields->current_position - m_fields->prev_position;
		float cross_product_magnitude = abs(current_vector.cross(previous_vector));

		if (cross_product_magnitude > 0.01 && !m_fields->dont_add)
			m_waveTrail->addPoint(m_fields->current_position);
		else if (m_fields->dont_add) m_fields->dont_add = false;

		m_fields->prev_position = m_fields->current_position;
	}
	
	void resetStreak() {
		m_fields->dont_add = true;
		PlayerObject::resetStreak();
	}

	void placeStreakPoint() { if (!m_isDart || !m_gameLayer) PlayerObject::placeStreakPoint(); }
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
