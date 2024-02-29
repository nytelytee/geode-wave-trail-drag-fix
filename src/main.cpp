#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
using namespace geode::prelude;

class $modify(PlayerObject) {

	cocos2d::CCPoint prev_position{0, 0};
	bool dont_add_point = true;
	float prev_yVelocity = -3001;

	void update(float p0) {
		if (!m_isDart) return PlayerObject::update(p0);

		// (part of the) fix for the wave interacting badly with slopes
		if (!m_isOnSlope && m_wasOnSlope && m_yVelocity != 0)
			m_waveTrail->addPoint(m_fields->prev_position);
		// exiting a straight area, put the point on the previous
		// frame's position (where the wave was while it was still on
		// the straight area)
		else if (m_yVelocity != 0 && m_fields->prev_yVelocity == 0 && !m_fields->dont_add_point)
			m_waveTrail->addPoint(m_fields->prev_position);
		// entering a straight area, put the point on the current
		// frame's position (the wave just landed on the straight area)
		else if (m_yVelocity == 0 && m_fields->prev_yVelocity != 0 && !m_fields->dont_add_point)
			m_waveTrail->addPoint(m_position);
		// when starting a new wave, a point may become stale, leading to
		// the wave getting connected to a point from the previous wave
		// this accounts for that
		else if (m_fields->dont_add_point)
			m_fields->dont_add_point = false;
		m_fields->prev_yVelocity = m_yVelocity;
		m_fields->prev_position = m_position;

		// result: straight line on straight area
		PlayerObject::update(p0);
	}
	void resetStreak() {
		m_fields->dont_add_point = true;
		PlayerObject::resetStreak();
	}
	void activateStreak() {
		m_fields->dont_add_point = true;
		PlayerObject::activateStreak();
	}
	void placeStreakPoint() {
		// (second part of the) fix for the wave interacting badly with slopes
		if (!m_isOnSlope && m_wasOnSlope) return;
		PlayerObject::placeStreakPoint();
	}
};
