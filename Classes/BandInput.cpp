#include "BandInput.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>

#include <linux/input.h>
#include <sys/epoll.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "band.h"
#include "vecs.h"

#define DO_CALIB 0

BandInput::BandInput() {}

static constexpr double MAG_1G = 1000000.0 / 488;
static constexpr double ROT_360 = 360000.0 / 70;
static constexpr int STABLE_ROT = 200;
static constexpr double STABLE_FORCE_DIFF = 0.15;

const std::string BandInput::Event::event_name = "band_gesture_event";

struct BandInfo;
struct VibeActionImpl;

struct BandInputImpl : public BandInput
{
	std::unique_ptr<BandManager> mgr;
	std::thread iothread;
	std::set<std::unique_ptr<BandInfo>, std::less<void>> bands;
	std::deque<std::unique_ptr<EventData>> evq;

	BandInputImpl();
	virtual ~BandInputImpl();

	virtual void checkEvents(cocos2d::EventDispatcher&);

	void on_band_found(BandDeviceLL *ll);
	void on_band_lost(BandInfo *bandinfo);

	void on_band_calibrated(BandInfo& bandinfo, std::unique_ptr<BandCalibrator> calib);

	BandInfo *find_band(unsigned id) const;

	void push_event(std::unique_ptr<EventData> ev);

	template <typename T, typename... Args>
	void push_new_event(Args&&... args)
	{
		push_event(std::make_unique<T>(std::forward<Args>(args)...));
	}
};

struct BandInfo final : public BandDevice
{
	BandInputImpl& mgr;
	unsigned id;
	std::string my_name;

	std::unique_ptr<BandCalibrator> calib;

	BandInfo(BandInputImpl&, BandDeviceLL *);

	virtual void device_initialized(const std::string& name, uint64_t ts, const DevIdData& devid) noexcept override;
	virtual void device_removed() noexcept override;
	virtual void data_received(SensorData data) noexcept override;
};

struct VibeActionImpl final : public BandInput::VibeAction
{
	VibeActionImpl(unsigned ud, uint64_t effect_);

	virtual VibeActionImpl *clone() const override;
	virtual VibeActionImpl *reverse() const override;
	virtual void update(float time) override;

	uint64_t effect;
	unsigned id;
};

namespace std {

template <typename T>
static bool operator<(const unique_ptr<T>& a, const T *b)
{
	return std::less<const T *>()(a.get(), b);
}

template <typename T>
static bool operator<(const T *a, const unique_ptr<T>& b)
{
	return std::less<const T *>()(a, b.get());
}

} /* namespace std */

static std::mutex insys_lock;
static std::unique_ptr<BandInputImpl> insys;
static unsigned next_band_id = 1;

BandInfo::BandInfo(BandInputImpl& mgr_, BandDeviceLL *bll)
	: mgr(mgr_), id(next_band_id++)
{
	if (DO_CALIB)
		calib = BandCalibrator::create();
	BandDevice::operator=(bll);
}

void BandInfo::device_initialized(const std::string& name, uint64_t ts, const DevIdData& devid) noexcept
{
	my_name = name;

	char idstr[32];
	snprintf(idstr, sizeof(idstr) - 1, "%hu/%04hX:%04hX/%hu", devid.registry, devid.vendor, devid.product, devid.version);
	idstr[sizeof(name)-1] = 0;

	std::cerr << "band #" << id << " using " << name << " (" << idstr << ") ts " << ts << "\n";

	mgr.push_new_event<BandInput::EventData>(BandInput::EventData::ADDED, id, ts);
}

void BandInfo::device_removed() noexcept
{
	if (!my_name.empty())
		mgr.push_new_event<BandInput::EventData>(BandInput::EventData::REMOVED, id, 0);
	mgr.on_band_lost(this);
}

void BandInfo::data_received(SensorData data) noexcept
{
	if (calib) {
		if (calib->process(data))
			mgr.on_band_calibrated(*this, std::move(calib));
		return;
	}

	Vec<int, 3> rot{data.v.gx, data.v.gy, data.v.gz};

	float ts = data.timestamp / 1000000.0;
	auto ev = std::make_unique<BandInput::BandRawValue>(id, ts);
	ev->ax = data.v.ax / MAG_1G;
	ev->ay = data.v.ay / MAG_1G;
	ev->az = data.v.az / MAG_1G;
	ev->gx = data.v.gx / ROT_360;
	ev->gy = data.v.gy / ROT_360;
	ev->gz = data.v.gz / ROT_360;
	mgr.push_event(std::move(ev));

	if (ANY(abs(rot) > STABLE_ROT))
		return;

	double fa = std::abs(std::hypot(data.v.ax, data.v.ay, data.v.az) / MAG_1G) - 1;
//	std::cerr << "--- fa = " << fa << '\n';
	if (abs(fa) > STABLE_FORCE_DIFF)
		return;

	float pitch = atan2(data.v.ax, std::hypot(data.v.ay, data.v.az)) * 2 / M_PI;	// 1 is up, -1 is down

	mgr.push_new_event<BandInput::GesturePitchValue>(id, ts, pitch);
}


BandInput& BandInput::getInstance()
{
	if (!insys) {
		std::unique_lock<std::mutex> lock(insys_lock);
		if (!insys)
			insys = std::make_unique<BandInputImpl>();
	}

	return *insys;
}

BandInputImpl::BandInputImpl()
	: mgr(BandManager::create())
{
	for (auto b : mgr->bands())
		on_band_found(b);

	mgr->on_new_band = std::bind(&BandInputImpl::on_band_found, this, std::placeholders::_1);

	iothread = std::thread(std::bind(&BandManager::run, std::ref(*mgr)));
}

BandInputImpl::~BandInputImpl()
{
	mgr->stop();
	iothread.join();
}

void BandInputImpl::on_band_found(BandDeviceLL *ll)
{
	try {
		bands.emplace(std::make_unique<BandInfo>(*this, ll));
	} catch (...) {
		// TODO
	}
}

void BandInputImpl::on_band_lost(BandInfo *bandinfo)
{
	bands.erase(bands.find(bandinfo));
}

void BandInputImpl::on_band_calibrated(BandInfo& band, std::unique_ptr<BandCalibrator> calib)
{
	band.adjust_zero(calib->zero_offset());
}

void BandInputImpl::checkEvents(cocos2d::EventDispatcher& evdispatch)
{
	std::deque<std::unique_ptr<EventData>> q;

	{
		std::unique_lock<std::mutex> lock(insys_lock);
		q = std::move(evq);
	}

	for (auto& gest : q)
		evdispatch.dispatchEvent(Event::create(gest.get()));
}

BandInfo *BandInputImpl::find_band(unsigned id) const
{
	// FIXME: map
	for (auto& p : bands)
		if (p->id == id)
			return p.get();
	return nullptr;
}

void BandInputImpl::push_event(std::unique_ptr<EventData> ev)
{
	std::unique_lock<std::mutex> lock(insys_lock);
	evq.emplace_back(std::move(ev));
}

BandInput::Event::Event(EventData *datap)
       	: EventCustom(event_name)
{
	setUserData(datap);
	autorelease();
}

BandInput::Event *BandInput::Event::create(EventData *datap)
{
	return new Event(datap);
}

BandInput::VibeAction *BandInput::VibeAction::create(unsigned id, uint64_t effect)
{
	return new VibeActionImpl(id, effect);
}

void BandInput::VibeAction::trigger(unsigned id, uint64_t effect)
{
	auto& in = static_cast<BandInputImpl&>(BandInput::getInstance());
	std::unique_lock<std::mutex> lock(insys_lock);
	BandInfo *band = in.find_band(id);
	if (band) {
		std::cerr << "band #" << id << " vibe " << std::hex << le64toh(htobe64(effect)) << std::dec << '\n';
		band->send_vibe(effect);
	}
}

VibeActionImpl::VibeActionImpl(unsigned id_, uint64_t effect_)
	: effect(effect_), id(id_)
{
	autorelease();
}

VibeActionImpl *VibeActionImpl::clone() const
{
	return new VibeActionImpl(id, effect);
}

VibeActionImpl *VibeActionImpl::reverse() const
{
	uint64_t ne = htobe64(le64toh(effect));
	if (ne)
		ne >>= (__builtin_ctz(ne) / 8) * 8;
	return new VibeActionImpl(id, ne);
}

void VibeActionImpl::update(float time)
{
	if (time == 1)
		trigger(id, effect);
}

BandInputInjector *BandInputInjector::create()
{
	auto p = new (std::nothrow) BandInputInjector();
	if (p)
		p->autorelease();
	p->scheduleUpdate();
	return p;
}

BandInputInjector::BandInputInjector()
{
	BandInput::getInstance();
}

bool BandInputInjector::isRunning() const
{
       return true;
}

void BandInputInjector::update(float delta)
{
	auto& bi = BandInput::getInstance();
	auto& evd = *getEventDispatcher();

	bi.checkEvents(evd);
}
