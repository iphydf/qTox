/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "callmanager.h"

#include "audio/iaudiocontrol.h"
#include "src/core/core.h"
#include "src/core/coreav.h"
#include "src/friendlist.h"
#include "src/model/callduration.h"
#include "src/model/chathistory.h"
#include "src/model/chatmanager.h"
#include "src/model/conference.h"
#include "src/model/friend.h"
#include "src/persistence/settings.h"

#include <QDateTime>
#include <QDebug>

CallManager::CallManager(FriendList& friendList_, ChatManager& chatManager_, Settings& settings_,
                         IAudioControl& audio_, QObject* parent)
    : QObject{parent}
    , friendList{friendList_}
    , chatManager{chatManager_}
    , settings{settings_}
    , audio{audio_}
{
}

CallManager::~CallManager() = default;

void CallManager::connectToCore(Core& core_)
{
    core = &core_;
    connect(core, &Core::friendStatusChanged, this, &CallManager::onFriendStatusChanged);
    const CoreAV* av = core->getAv();
    connect(av, &CoreAV::avInvite, this, &CallManager::onInvite);
    connect(av, &CoreAV::avStart, this, &CallManager::onStart);
    connect(av, &CoreAV::avEnd, this, &CallManager::onEnd);
}

void CallManager::setOneCallAtATime(bool one)
{
    oneCallAtATime = one;
}

void CallManager::setAnswersAutomatically(bool automatically)
{
    answersAutomatically = automatically;
}

CallManager::State CallManager::state(const ToxPk& friendPk) const
{
    const auto it = calls.constFind(friendPk);
    return it == calls.cend() ? State::None : it->state;
}

bool CallManager::hasVideo(const ToxPk& friendPk) const
{
    const auto it = calls.constFind(friendPk);
    return it != calls.cend() && it->video;
}

quint32 CallManager::seconds(const ToxPk& friendPk) const
{
    const auto it = calls.constFind(friendPk);
    return it == calls.cend() || it->state != State::Active
               ? 0
               : static_cast<quint32>(it->talking.elapsed() / 1000);
}

bool CallManager::start(const ToxPk& friendPk, bool video)
{
    const Friend* f = friendList.findFriend(friendPk);
    if (core == nullptr || f == nullptr || calls.contains(friendPk) || takesNoCall()) {
        return false;
    }
    if (!core->getAv()->startCall(f->getId(), video)) {
        return false;
    }
    calls.insert(friendPk, {State::Outgoing, video, {}});
    addSystemMessage(*f, SystemMessageType::outgoingCall, {f->getDisplayedName()});
    play(IAudioSink::Sound::OutgoingCall, true);
    emit stateChanged(friendPk, State::Outgoing);
    return true;
}

bool CallManager::answer(const ToxPk& friendPk, bool video)
{
    const Friend* f = friendList.findFriend(friendPk);
    if (core == nullptr || f == nullptr || state(friendPk) != State::Incoming) {
        return false;
    }
    sound.reset();
    CoreAV* av = core->getAv();
    if (!av->answerCall(f->getId(), video)) {
        // CoreAV has dropped the call and says nothing more of it.
        calls.remove(friendPk);
        emit stateChanged(friendPk, State::None);
        return false;
    }
    // Nor does it say that the call started.
    setActive(friendPk, av->isCallVideoEnabled(f));
    return true;
}

void CallManager::end(const ToxPk& friendPk)
{
    const Friend* f = friendList.findFriend(friendPk);
    if (core == nullptr || f == nullptr || !calls.contains(friendPk)) {
        return;
    }
    // CoreAV then says that the call ended.
    core->getAv()->cancelCall(f->getId());
}

bool CallManager::joinConference(const Conference& conference)
{
    if (core == nullptr || !conference.isAvConference() || inConferenceCall(conference)
        || takesNoCall()) {
        return false;
    }
    core->getAv()->joinConferenceCall(conference);
    conferenceCalls.append(conference.getPersistentId());
    emit conferenceCallChanged(conference.getPersistentId(), true);
    return true;
}

void CallManager::leaveConference(const Conference& conference)
{
    if (core == nullptr || !conferenceCalls.removeOne(conference.getPersistentId())) {
        return;
    }
    core->getAv()->leaveConferenceCall(static_cast<int>(conference.getId()));
    emit conferenceCallChanged(conference.getPersistentId(), false);
}

bool CallManager::inConferenceCall(const Conference& conference) const
{
    return conferenceCalls.contains(conference.getPersistentId());
}

void CallManager::onInvite(uint32_t friendId, bool video)
{
    const ToxPk friendPk = friendList.id2Key(friendId);
    const Friend* f = friendList.findFriend(friendPk);
    if (f == nullptr) {
        return;
    }
    if (takesNoCall()) {
        core->getAv()->cancelCall(friendId);
        return;
    }

    calls.insert(friendPk, {State::Incoming, video, {}});
    addSystemMessage(*f, SystemMessageType::incomingCall, {f->getDisplayedName()});

    const auto wanted = video ? Settings::AutoAcceptCall::Video : Settings::AutoAcceptCall::Audio;
    if (answersAutomatically && settings.getAutoAcceptCall(friendPk).testFlag(wanted)) {
        qDebug() << "automatic call answer";
        QMetaObject::invokeMethod(core->getAv(), "answerCall", Qt::QueuedConnection,
                                  Q_ARG(uint32_t, friendId), Q_ARG(bool, video));
        setActive(friendPk, video);
        return;
    }

    play(IAudioSink::Sound::IncomingCall, true);
    emit stateChanged(friendPk, State::Incoming);
}

void CallManager::onStart(uint32_t friendId, bool video)
{
    const ToxPk friendPk = friendList.id2Key(friendId);
    if (calls.contains(friendPk)) {
        setActive(friendPk, video);
    }
}

void CallManager::onEnd(uint32_t friendId, bool error)
{
    const ToxPk friendPk = friendList.id2Key(friendId);
    const Friend* f = friendList.findFriend(friendPk);
    // A call we declined, or one that ended when the friend went offline.
    if (f == nullptr || !calls.contains(friendPk)) {
        return;
    }
    if (state(friendPk) == State::Active) {
        addSystemMessage(*f, error ? SystemMessageType::unexpectedCallEnd : SystemMessageType::callEnd,
                         {f->getDisplayedName(), secondsToDHMS(seconds(friendPk))});
    }
    calls.remove(friendPk);
    play(IAudioSink::Sound::CallEnd, false);
    emit stateChanged(friendPk, State::None);
}

void CallManager::onFriendStatusChanged(uint32_t friendId, Status::Status status)
{
    const ToxPk friendPk = friendList.id2Key(friendId);
    const Friend* f = friendList.findFriend(friendPk);
    if (Status::isOnline(status) || f == nullptr || !calls.contains(friendPk)) {
        return;
    }
    if (state(friendPk) == State::Active) {
        addSystemMessage(*f, SystemMessageType::userWentOfflineDuringCall,
                         {f->getDisplayedName(), secondsToDHMS(seconds(friendPk))});
    } else {
        addSystemMessage(*f, SystemMessageType::userWentOffline, {f->getDisplayedName()});
    }
    // First, so that onEnd() finds no call: the chat has said how this one ended.
    calls.remove(friendPk);
    core->getAv()->cancelCall(friendId);
    play(IAudioSink::Sound::CallEnd, false);
    emit stateChanged(friendPk, State::None);
}

bool CallManager::takesNoCall() const
{
    return oneCallAtATime && !(calls.isEmpty() && conferenceCalls.isEmpty());
}

void CallManager::setActive(const ToxPk& friendPk, bool video)
{
    sound.reset();
    Call& call = calls[friendPk];
    call.state = State::Active;
    call.video = video;
    call.talking.start();
    emit stateChanged(friendPk, State::Active);
}

void CallManager::addSystemMessage(const Friend& f, SystemMessageType type,
                                   const SystemMessage::Args& args)
{
    if (IChatLog* chatLog = chatManager.getFriendChatLog(f.getPublicKey())) {
        chatLog->addSystemMessage({type, QDateTime::currentDateTime(), args});
    }
}

void CallManager::play(IAudioSink::Sound newSound, bool loop)
{
    sound.reset();
    const bool busy = core->getStatus() == Status::Status::Busy;
    if (!settings.getAudioOutDevEnabled() || !settings.getNotifySound()
        || (busy && !settings.getBusySound())) {
        return;
    }
    sound = audio.makeSink();
    if (sound == nullptr) {
        qDebug() << "Failed to allocate AudioSink";
        return;
    }
    sound->connectTo_finishedPlaying(this, [this] { sound.reset(); });
    sound->playMono16Sound(newSound);
    if (loop) {
        sound->startLoop();
    }
}
