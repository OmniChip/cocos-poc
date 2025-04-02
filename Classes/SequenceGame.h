#ifndef __HELLOWORLD_GAME_H__
#define __HELLOWORLD_GAME_H__

#include <deque>
#include <map>
#include <memory>

namespace cocos2d { class Node; }

class SequenceGame;
class GameData;

struct BandData
{
	enum Dir { DOWN = -2, LOW, LEVEL, HIGH, UP };

	GameData *const game;
	const unsigned id;
	Dir cur_dir;
	cocos2d::Node *arrow;

	BandData(GameData& game, unsigned id);
	BandData();

	void updatePitch(float ts, float value);
	void updateTime(float ts);
};

class SequenceGame
{
	friend class BandData;
	friend class GameData;

	std::map<unsigned, BandData> bands;
	std::unique_ptr<GameData> data;
	float start_ts;

	BandData& band(unsigned id) noexcept;
public:
	SequenceGame();
	~SequenceGame();

	void addBand(unsigned id, cocos2d::Node *arrow);
	void removeBand(unsigned id);
	void updateBandPitch(unsigned id, float ts, float value);
	void updateBandTime(unsigned id, float ts);
	void start();
	void update();

	auto cb() { return [this] (float d) { update(); }; }
};

#endif // __HELLOWORLD_GAME_H__
