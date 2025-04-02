/****************************************************************************
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "AppDelegate.h"
#include "BandInput.h"
#include "HelloWorldScene.h"
#include "SequenceGame.h"

#include <iostream>

// #define USE_AUDIO_ENGINE 1
// #define USE_SIMPLE_AUDIO_ENGINE 1

#if USE_AUDIO_ENGINE && USE_SIMPLE_AUDIO_ENGINE
#error "Don't use AudioEngine and SimpleAudioEngine at the same time. Please just select one in your game!"
#endif

#if USE_AUDIO_ENGINE
#include "audio/include/AudioEngine.h"
using namespace cocos2d::experimental;
#elif USE_SIMPLE_AUDIO_ENGINE
#include "audio/include/SimpleAudioEngine.h"
using namespace CocosDenshion;
#endif

USING_NS_CC;

static cocos2d::Size designResolutionSize = cocos2d::Size(480, 320);
static cocos2d::Size smallResolutionSize = cocos2d::Size(480, 320);
static cocos2d::Size mediumResolutionSize = cocos2d::Size(1024, 768);
static cocos2d::Size largeResolutionSize = cocos2d::Size(2048, 1536);

AppDelegate::AppDelegate()
{
	BandInput::getInstance();
}

AppDelegate::~AppDelegate() 
{
#if USE_AUDIO_ENGINE
    AudioEngine::end();
#elif USE_SIMPLE_AUDIO_ENGINE
    SimpleAudioEngine::end();
#endif
}

// if you want a different context, modify the value of glContextAttrs
// it will affect all platforms
void AppDelegate::initGLContextAttrs()
{
    // set OpenGL context attributes: red,green,blue,alpha,depth,stencil,multisamplesCount
    GLContextAttrs glContextAttrs = {8, 8, 8, 8, 24, 8, 0};

    GLView::setGLContextAttrs(glContextAttrs);
}

// if you want to use the package manager to install more packages,  
// don't modify or remove this function
static int register_all_packages()
{
    return 0; //flag for packages manager
}

static void onMouseEvent(EventMouse *event, EventMouse::MouseEventType evtype)
{
	Node *node = event->getCurrentTarget();
	if (!node)
		return;

	auto pt = event->getLocation();
	pt.y = Director::getInstance()->getOpenGLView()->getFrameSize().height - pt.y;

	if (node->getPosition().distanceSquared(pt) > 24*24)
		return;

	unsigned band_id = reinterpret_cast<uintptr_t>(node->getUserData());
	unsigned effect = band_id >> 24;

	band_id &= 0xFFFFFF;
	if (!effect)
		effect = 1;

	switch (evtype) {
	case EventMouse::MouseEventType::MOUSE_DOWN:
		switch (event->getMouseButton()) {
		case EventMouse::MouseButton::BUTTON_LEFT:
			BandInput::VibeAction::trigger(band_id, effect);
			break;
		case EventMouse::MouseButton::BUTTON_RIGHT:
			--effect;
			if (effect < 2)
				effect |= 0x10;
			else if (effect < 6)
				effect |= 0x20;
			else if (effect < 14)
				effect |= 0x30;
			else
				effect = (effect & 0x0F) | 0x40;

			BandInput::VibeAction::trigger(band_id, BandInput::VibeAction::bin4_effect(effect, effect >> 4));
			break;
		default:
			break;
		}
		break;

	case EventMouse::MouseEventType::MOUSE_SCROLL:
		if (event->getScrollY() == 1)
			effect = 1 + effect % 123;
		else if (event->getScrollY() == -1)
			effect = 1 + (effect + 121) % 123;
		node->setUserData(reinterpret_cast<void *>(band_id | (effect << 24)));
		break;

	default:
		break;
	}
}

template <EventMouse::MouseEventType MET>
static void onMouseEventStub(Event *e)
{
	onMouseEvent(static_cast<EventMouse *>(e), MET);
}

static void attachBandMouseListener(Node *node)
{
	EventListenerMouse *evl = EventListenerMouse::create();

	evl->onMouseDown   = onMouseEventStub<EventMouse::MouseEventType::MOUSE_DOWN>;
//	evl->onMouseUp     = onMouseEventStub<EventMouse::MouseEventType::MOUSE_UP>;
//	evl->onMouseMove   = onMouseEventStub<EventMouse::MouseEventType::MOUSE_MOVE>;
	evl->onMouseScroll = onMouseEventStub<EventMouse::MouseEventType::MOUSE_SCROLL>;

	node->getEventDispatcher()->addEventListenerWithSceneGraphPriority(evl, node);
}

bool AppDelegate::applicationDidFinishLaunching() {
    // initialize director
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    if(!glview) {
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32) || (CC_TARGET_PLATFORM == CC_PLATFORM_MAC) || (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
        glview = GLViewImpl::createWithRect("BandGame", cocos2d::Rect(0, 0, designResolutionSize.width, designResolutionSize.height));
#else
        glview = GLViewImpl::create("BandGame");
#endif
        director->setOpenGLView(glview);
    }

    // turn on display FPS
    director->setDisplayStats(true);

    // set FPS. the default value is 1.0/60 if you don't call this
    director->setAnimationInterval(1.0f / 60);

    // Set the design resolution
    glview->setDesignResolutionSize(designResolutionSize.width, designResolutionSize.height, ResolutionPolicy::NO_BORDER);
    auto frameSize = glview->getFrameSize();
    // if the frame's height is larger than the height of medium size.
    if (frameSize.height > mediumResolutionSize.height)
    {        
        director->setContentScaleFactor(MIN(largeResolutionSize.height/designResolutionSize.height, largeResolutionSize.width/designResolutionSize.width));
    }
    // if the frame's height is larger than the height of small size.
    else if (frameSize.height > smallResolutionSize.height)
    {        
        director->setContentScaleFactor(MIN(mediumResolutionSize.height/designResolutionSize.height, mediumResolutionSize.width/designResolutionSize.width));
    }
    // if the frame's height is smaller than the height of medium size.
    else
    {        
        director->setContentScaleFactor(MIN(smallResolutionSize.height/designResolutionSize.height, smallResolutionSize.width/designResolutionSize.width));
    }

    register_all_packages();

    // create a scene. it's an autorelease object
    auto scene = HelloWorld::createScene();

    scene->addChild(BandInputInjector::create());

    auto game = std::make_shared<SequenceGame>();
    auto evl = BandInput::Event::createListener([scene, game] (BandInput::Event *ev) {
	unsigned band_id = ev->getData().band_id;
//	std::cerr << "*** event for band #" << band_id << " type " << ev->getEventType() << '\n';
	auto& hw = *static_cast<HelloWorld *>(scene);
	if (ev->isBandAdded()) {
		auto *arrow = hw.addBand(band_id);
		if (!arrow)
			return;
		arrow->setUserData(reinterpret_cast<void *>(band_id));
		attachBandMouseListener(arrow);
		game->addBand(band_id, arrow);
#if 0
		auto delay = DelayTime::create(1.0f);
		Vector<FiniteTimeAction *> va(3);
		va.pushBack(delay);
		va.pushBack(BandInput::VibeAction::create(band_id, 0x4b8a018a018a4b));
		va.pushBack(delay);
		arrow->runAction(Sequence::create(va));
#endif
	} else if (ev->isBandRemoved()) {
		hw.removeBand(band_id);
		game->removeBand(band_id);
	} else if (ev->isRawType()) {
		game->updateBandTime(band_id, ev->getData().detection_ts);
	} else if (ev->isPitchType()) {
		float pitch = ev->getPitchData().pitch;
		hw.updateBandPitch(band_id, pitch);
		game->updateBandPitch(band_id, ev->getData().detection_ts, pitch);
	}
    });
    director->getEventDispatcher()->addEventListenerWithFixedPriority(evl, 1);

    auto kbevl = EventListenerKeyboard::create();
    kbevl->onKeyReleased = [game] (EventKeyboard::KeyCode key, Event *ev) {
	if (key != EventKeyboard::KeyCode::KEY_SPACE)
		return;
	game->start();
    };
    director->getEventDispatcher()->addEventListenerWithFixedPriority(kbevl, 1);

    director->getScheduler()->schedule(game->cb(), game.get(), 0, false, "game");

    // run
    director->runWithScene(scene);

    return true;
}

// This function will be called when the app is inactive. Note, when receiving a phone call it is invoked.
void AppDelegate::applicationDidEnterBackground() {
    Director::getInstance()->stopAnimation();

#if USE_AUDIO_ENGINE
    AudioEngine::pauseAll();
#elif USE_SIMPLE_AUDIO_ENGINE
    SimpleAudioEngine::getInstance()->pauseBackgroundMusic();
    SimpleAudioEngine::getInstance()->pauseAllEffects();
#endif
}

// this function will be called when the app is active again
void AppDelegate::applicationWillEnterForeground() {
    Director::getInstance()->startAnimation();

#if USE_AUDIO_ENGINE
    AudioEngine::resumeAll();
#elif USE_SIMPLE_AUDIO_ENGINE
    SimpleAudioEngine::getInstance()->resumeBackgroundMusic();
    SimpleAudioEngine::getInstance()->resumeAllEffects();
#endif
}
