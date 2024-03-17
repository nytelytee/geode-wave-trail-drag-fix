#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(PlayerObject) {

	cocos2d::CCPoint prev_position{-12, -12};

	bool force_add = true;
	bool dont_add = false;
	
	void resetObject() {
		m_fields->force_add = true;
		PlayerObject::resetObject();
	}
	
	void postCollision(float p0) {
		PlayerObject::postCollision(p0);

		if (LevelEditorLayer::get()) return;
		if (!m_isDart) {
			m_fields->prev_position = m_position;
			return;
		}
		
		CCPoint previous_position = m_fields->prev_position;
		CCPoint current_position = m_position;
		CCPoint next_position = getRealPosition();
		
		if (m_fields->force_add) {
			m_fields->force_add = false;
			m_waveTrail->addPoint(current_position);
			m_fields->prev_position = current_position;
			return;
		}
		
		cocos2d::CCPoint current_vector = next_position - current_position;
		cocos2d::CCPoint previous_vector = current_position - previous_position;
		float cross_product_magnitude = abs(current_vector.cross(previous_vector));

		if (cross_product_magnitude > 0.01 && !m_fields->dont_add) {
			m_waveTrail->addPoint(current_position);
			// save the previous point that was a hit, for later calculation
			// rather than the current point always
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
			m_fields->prev_position = current_position;
		}
		else if (m_fields->dont_add) m_fields->dont_add = false;
	}
	
	// teleport portal
	void resetStreak() {
		m_fields->dont_add = true;
		PlayerObject::resetStreak();
	}

	void doReversePlayer(bool p0) {
		m_fields->force_add = true;
		PlayerObject::doReversePlayer(p0);
	}

	void placeStreakPoint() { if (!m_isDart || !m_gameLayer) PlayerObject::placeStreakPoint(); }

	void toggleVisibility(bool p0) {
		bool needs_point = m_isHidden;
		PlayerObject::toggleVisibility(p0);
		if (p0 && m_isDart && needs_point && !LevelEditorLayer::get()) m_fields->force_add = true;
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
