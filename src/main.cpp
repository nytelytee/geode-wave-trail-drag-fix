#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
using namespace geode::prelude;

cocos2d::CCPoint prev_position(0, 0);
bool dont_add_point = true;
float prev_yVelocity = -3001;

class $modify(PlayerObject) {
	void update(float p0) {
		if (m_isDart) {
			// exiting a straight area, put the point on the previous
			// frame's position (where the wave was while it was still on
			// the straight area)
			if (m_yVelocity != 0 && prev_yVelocity == 0) {
				if (dont_add_point) dont_add_point = false;
				else m_waveTrail->addPoint(prev_position);
				prev_yVelocity = m_yVelocity;
			}
			// entering a straight area, put the point on the current
			// frame's position (the wave just landed on the straight area)
			else if (m_yVelocity == 0 && prev_yVelocity != 0) {
				if (dont_add_point) dont_add_point = false;
				else m_waveTrail->addPoint(m_position);
				prev_yVelocity = m_yVelocity;
			}
			prev_position = m_position;

			// result: straight line on straight area
		}
		PlayerObject::update(p0);
	}
	void resetStreak() {
		dont_add_point = true;
		PlayerObject::resetStreak();
	}
};
