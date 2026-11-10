/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "audio/iaudiosink.h"
#include "src/core/conferenceid.h"
#include "src/core/toxpk.h"
#include "src/model/status.h"
#include "src/model/systemmessage.h"

#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QObject>

#include <memory>

class ChatManager;
class Conference;
class Core;
class Friend;
class FriendList;
class IAudioControl;
class Settings;

/**
 * @brief The calls with friends: who rings, who talks, what the chat says about a call and what
 * the user hears of it. A user interface shows the state and tells the user's choices.
 */
class CallManager : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        None,
        /** The friend calls and the user hasn't answered. */
        Incoming,
        /** The user calls and the friend hasn't answered. */
        Outgoing,
        Active,
    };

    CallManager(FriendList& friendList, ChatManager& chatManager, Settings& settings,
                IAudioControl& audio, QObject* parent = nullptr);
    ~CallManager() override;

    /**
     * Call it before ChatManager::connectToCore(): a friend who goes offline ends the call here
     * before the chat's users hear of the new status.
     */
    void connectToCore(Core& core);

    /** A phone has one earpiece: with this, a friend who calls during a call is declined. */
    void setOneCallAtATime(bool one);
    /** Without this, the friends set to be answered automatically ring like the others. */
    void setAnswersAutomatically(bool automatically);

    State state(const ToxPk& friendPk) const;
    /** What an incoming call offers, what an outgoing one asked for, what an active one has. */
    bool hasVideo(const ToxPk& friendPk) const;
    /** How long the friend and the user have talked. */
    quint32 seconds(const ToxPk& friendPk) const;

    bool start(const ToxPk& friendPk, bool video);
    bool answer(const ToxPk& friendPk, bool video);
    /** Hangs up, declines, or stops calling. */
    void end(const ToxPk& friendPk);

    /** A conference with audio has one call that its members join and leave. */
    bool joinConference(const Conference& conference);
    void leaveConference(const Conference& conference);
    bool inConferenceCall(const Conference& conference) const;

signals:
    void stateChanged(const ToxPk& friendPk, CallManager::State state);
    void conferenceCallChanged(const ConferenceId& conferenceId, bool inCall);

private:
    struct Call
    {
        State state;
        bool video;
        QElapsedTimer talking;
    };

    void onInvite(uint32_t friendId, bool video);
    void onStart(uint32_t friendId, bool video);
    void onEnd(uint32_t friendId, bool error);
    void onFriendStatusChanged(uint32_t friendId, Status::Status status);

    bool takesNoCall() const;
    void setActive(const ToxPk& friendPk, bool video);
    void addSystemMessage(const Friend& f, SystemMessageType type, const SystemMessage::Args& args);
    void play(IAudioSink::Sound newSound, bool loop);

    FriendList& friendList;
    ChatManager& chatManager;
    Settings& settings;
    IAudioControl& audio;
    Core* core = nullptr;
    QMap<ToxPk, Call> calls;
    std::unique_ptr<IAudioSink> sound;
    bool oneCallAtATime = false;
    bool answersAutomatically = true;
    QList<ConferenceId> conferenceCalls;
};
