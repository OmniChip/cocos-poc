#include "SequenceGame.h"
#include "BandInput.h"
#include "cocos2d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include <time.h>

USING_NS_CC;

struct GameData
{
	SequenceGame& game;
	std::random_device rd;
	std::vector<std::pair<unsigned, BandData::Dir>> sequence;
	std::map<float, std::vector<std::pair<unsigned, BandData::Dir>>> backlog;
	std::map<unsigned, float> band_ts;
	size_t current, missed;
	float start_ts;
	bool listening;

	GameData(SequenceGame& game_) : game(game_), current(0), missed(0), listening(false) {}

	void clear();
	void generate_sequence(const std::vector<unsigned>& ids, size_t n);
	void push_direction_change(unsigned id, float ts, BandData::Dir dir);
	void advance_time(unsigned id, float ts);
	bool test_sequence(unsigned id, BandData::Dir dir);
	bool in_progress() const { return current != sequence.size(); }
};

static BandData sentinel;

static float float_ts() noexcept
{
	struct timespec tv;
	if (clock_gettime(CLOCK_MONOTONIC, &tv))
		return 0;	// FIXME
	return (float)tv.tv_sec + (float)tv.tv_nsec / 1000000000;
}

SequenceGame::SequenceGame()
	: data(std::make_unique<GameData>(*this)), start_ts(0)
{
}

SequenceGame::~SequenceGame() = default;

void SequenceGame::addBand(unsigned id, Node *arrow)
{
	auto& bd = bands.emplace(std::piecewise_construct, std::make_tuple(id), std::make_tuple(std::ref(*data), id)).first->second;
	bd.arrow = arrow;
}

void SequenceGame::removeBand(unsigned id)
{
	bands.erase(id);
}

BandData& SequenceGame::band(unsigned id) noexcept
{
	auto p = bands.find(id);
	return p != bands.end() ? p->second : sentinel;
}

void SequenceGame::updateBandPitch(unsigned id, float ts, float value)
{
	band(id).updatePitch(ts, value);
}

void SequenceGame::updateBandTime(unsigned id, float ts)
{
	band(id).updateTime(ts);
}

void SequenceGame::start()
{
	if (data->in_progress())
		return;

	data->clear();

	std::vector<unsigned> ids(bands.size());
	std::transform(bands.begin(), bands.end(), ids.begin(), [] (const auto& p) { return p.second.id; });
	data->generate_sequence(ids, std::max<size_t>(3, ids.size() * 2));

	std::vector<Sequence *> vibes;
	vibes.reserve(bands.size());

	for (auto& bdp : bands) {
		unsigned band_id = bdp.second.id;
		size_t n = std::count_if(data->sequence.begin(), data->sequence.end(), [band_id] (const auto& p) { return p.first == band_id; });
		if (!n) {
			vibes.emplace_back(nullptr);
			continue;
		}

		Vector<FiniteTimeAction *> va(2 * n + 1);
		size_t last = ~(size_t)0;
		for (size_t i = 0; i < data->sequence.size(); ++i) {
			if (data->sequence[i].first != band_id)
				continue;

			int dir = (int)data->sequence[i].second;
			uint64_t effect = BandInput::VibeAction::dd5_effect(dir);
			size_t delay = i - last;

			va.pushBack(DelayTime::create(delay));
			va.pushBack(BandInput::VibeAction::create(band_id, effect));

			last = i;
		}

		va.pushBack(DelayTime::create(data->sequence.size() - last));
		vibes.emplace_back(Sequence::create(va));
	}

	size_t i = 0;
	for (auto& bdp : bands) {
		auto act = vibes[i++];
		if (act)
			bdp.second.arrow->runAction(act);
	}

	start_ts = float_ts() + data->sequence.size() + 1;
}

void SequenceGame::update()
{
	float ts = float_ts();

	if (start_ts != 0 && ts >= start_ts) {
		data->listening = true;
		start_ts = 0;
		std::cerr << "\nMOVE!\n\n";
	}
}

BandData::BandData()
	: game(nullptr), id(0), arrow(nullptr)
{
}

BandData::BandData(GameData& game_, unsigned id_)
	: game(std::addressof(game_)), id(id_), arrow(nullptr)
{
}

static const std::string dir_name[] = { "DOWN", "LOW", "LEVEL", "HIGH", "UP" };

void BandData::updatePitch(float ts, float value)
{
	if (!id)	// sentinel
		return;

	// round to nearest i/2: [-1,+1] -> [0,4]
	float vdir = std::floor((value + 1.25f) * 2.0f);	// round to nearest n/2

	float diff = (vdir - 2.0f) / 2.0f - value;
	if (std::abs(diff) > 0.15f)	// between directions
		return;

	Dir dir = (Dir)((int)vdir + DOWN);
	if (dir == cur_dir)	// unchanged
		return;

	cur_dir = dir;
	std::cerr << "--> " << id << " dir " << dir_name[dir - DOWN] << " (diff " << diff << ") ts " << ts << "\n";

	// check sequence

	if (game->listening)
		game->push_direction_change(id, ts, dir);
}

void BandData::updateTime(float ts)
{
	if (!id)
		return;

	game->advance_time(id, ts);
}

void GameData::clear()
{
	sequence.clear();
	backlog.clear();
	current = missed = 0;
	listening = false;
	start_ts = float_ts();
}

void GameData::generate_sequence(const std::vector<unsigned>& ids, size_t n)
{
	std::uniform_int_distribution<size_t> idgen(0, ids.size() - 1);
	std::uniform_int_distribution<int> dirgen(BandData::DOWN, BandData::UP);

	std::vector<bool> seen_id(ids.size(), false);
	std::decay_t<decltype(sequence.front())> prev;
	prev.first = ~0;

	sequence.resize(n);
	size_t i = 0;
	for (auto& sp : sequence) {
		size_t ididx;
		for (;;) {
			ididx = idgen(rd);
			sp.first = ids[ididx];
			sp.second = (BandData::Dir)dirgen(rd);
			if (sp == prev)
				continue;
			if (!seen_id[ididx]) {
				if (sp.second == game.band(sp.first).cur_dir)
					continue;
				seen_id[ididx] = true;
			}
			break;
		}
		prev = sp;

		std::cerr << "SEQ#" << ++i << "  " << sp.first << " " << dir_name[sp.second - BandData::DOWN] << '\n';
	}
}

void GameData::push_direction_change(unsigned id, float ts, BandData::Dir dir)
{
	backlog[ts].emplace_back(id, dir);
	advance_time(id, ts);
}

static void signal_band(unsigned id, bool ok)
{
	uint64_t effect;
	if (ok)
		effect = BandInput::VibeAction::bin4_effect(0, 1);
	else
		effect = BandInput::VibeAction::dd5_effect(0);

	BandInput::VibeAction::trigger(id, effect);
}

void GameData::advance_time(unsigned id, float ts)
{
	band_ts[id] = ts;

	if (!listening)
		return;

	ts = std::min_element(band_ts.begin(), band_ts.end(), [] (const auto& a, const auto& b) { return a.second < b.second; })->second;
	std::map<unsigned, bool> signal;

	while (!backlog.empty() && backlog.begin()->first <= ts) {
		for (const auto& banddir : backlog.begin()->second) {
			bool b = test_sequence(banddir.first, banddir.second);
			signal[banddir.first] = b;
		}
		backlog.erase(backlog.begin());
	}

	for (auto bsig : signal)
		signal_band(bsig.first, bsig.second);

	if (!listening)
		std::cerr << "\nFINISHED!\n\nlength " << sequence.size() << "  missed " << missed << "  time " << (float_ts() - start_ts) << "\n\n";
}

bool GameData::test_sequence(unsigned id, BandData::Dir dir)
{
	bool ok = sequence[current] == std::make_pair(id, dir);

	char buf[128];
	buf[127] = 0;
	snprintf(buf, sizeof(buf) - 1, "seq #%zu  %s  (%u, %d)\n", current, ok ? "accepted" : "MISSED", id, dir);
	std::cerr << buf;

	if (ok) {
		if (++current == sequence.size())
			listening = false;
	} else {
		++missed;
	}

	return ok;
}
