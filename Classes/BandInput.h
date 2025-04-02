#ifndef BANDGAME_INPUT_H_
#define BANDGAME_INPUT_H_

#include "cocos2d.h"

#include <string>

struct BandInput
{
	struct EventData
	{
		enum Type { ADDED, REMOVED, RAW, PITCH };

		Type type;
		unsigned band_id;
		float detection_ts;

		EventData(Type type_, unsigned id, float ts) : type(type_), band_id(id), detection_ts(ts) {}
       	};

	struct BandRawValue : public EventData
	{
		float ax, ay, az;	// [g]
		float gx, gy, gz;	// [360deg/s]

		BandRawValue(unsigned id, float ts) : EventData(RAW, id, ts) {}
	};

	struct GesturePitchValue : public EventData
	{
		float pitch;

		GesturePitchValue(unsigned id, float ts, float value) : EventData(PITCH, id, ts), pitch(value) {}
	};

	struct Event : public cocos2d::EventCustom
	{
		static Event *create(EventData *datap);

		EventData::Type getEventType() const { return getData().type; }
		bool isBandAdded() const { return getData().type == EventData::ADDED; }
		bool isBandRemoved() const { return getData().type == EventData::REMOVED; }
		bool isRawType() const { return getData().type == EventData::RAW; }
		bool isPitchType() const { return getData().type == EventData::PITCH; }

		EventData& getData() const { return *reinterpret_cast<EventData *>(getUserData()); }
		BandRawValue& getRawData() const { return static_cast<BandRawValue&>(getData()); }
		GesturePitchValue& getPitchData() const { return static_cast<GesturePitchValue&>(getData()); }

		static cocos2d::EventListenerCustom *createListener(std::function<void(Event *)> callback)
		{
			return cocos2d::EventListenerCustom::create(event_name, [cb=std::move(callback)] (cocos2d::EventCustom *e) {
				cb(static_cast<Event *>(e));
			});
		}
	private:
		friend class BandInput;

		static const std::string event_name;

		explicit Event(EventData *datap);
		Event(const Event& other) = delete;
	};

	struct VibeAction : public cocos2d::ActionInstant
	{
		static VibeAction *create(unsigned id, uint64_t effect);

		static void trigger(unsigned id, uint64_t effect);

		static constexpr uint64_t bin4_effect(unsigned val, size_t n)
		{
			uint64_t delay = 10;
			uint64_t effect = 0;
			if (n > 4)
				n = 4;
			for (int i = 0; i < n; ++i)
				effect |= (uint64_t)((val >> i) & 1 ? 0x4b : 0x01) << (16 * i);
			for (int i = 1; i < n; ++i)
				effect |= (delay + 0x80) << (8 + 16 * (i - 1));
			return effect;
		}

		static constexpr uint64_t dd5_effect(int v)
		{
			switch (v) {
			case -2: return bin4_effect(3, 2);
			case -1: return bin4_effect(1, 2);
			case  0: return 0x37;
			case  1: return bin4_effect(2, 2);
			case  2: return bin4_effect(0, 2);
			default: return 0;
			}
		}
	};

	static BandInput& getInstance();

	virtual void checkEvents(cocos2d::EventDispatcher&) = 0;

	virtual ~BandInput() = default;
protected:
	BandInput();
};

struct BandInputInjector : public cocos2d::Node
{
	virtual bool isRunning() const override;
	virtual void update(float delta) override;

	static BandInputInjector *create();
private:
	BandInputInjector();
};

#endif /* BANDGAME_INPUT_H_ */
