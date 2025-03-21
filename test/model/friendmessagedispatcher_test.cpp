/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "src/model/friendmessagedispatcher.h"

#include "src/core/icorefriendmessagesender.h"
#include "src/model/friend.h"
#include "src/model/message.h"

#include <QObject>
#include <QtTest/QtTest>

#include <deque>
#include <memory>
#include <set>
#include <tox/tox.h> // tox_max_message_length

class MockFriendMessageSender : public ICoreFriendMessageSender
{
public:
    bool sendAction(uint32_t friendId, const QString& action, ReceiptNum& receipt) override;

    bool sendMessage(uint32_t friendId, const QString& message, ReceiptNum& receipt) override;

    bool canSend = true;
    ReceiptNum receiptNum{0};
    size_t numSentActions = 0;
    size_t numSentMessages = 0;
};

bool MockFriendMessageSender::sendAction(uint32_t friendId, const QString& action, ReceiptNum& receipt)
{
    std::ignore = friendId;
    std::ignore = action;
    if (canSend) {
        numSentActions++;
        receipt = receiptNum;
        receiptNum.get() += 1;
    }
    return canSend;
}

bool MockFriendMessageSender::sendMessage(uint32_t friendId, const QString& message, ReceiptNum& receipt)
{
    std::ignore = friendId;
    std::ignore = message;
    if (canSend) {
        numSentMessages++;
        receipt = receiptNum;
        receiptNum.get() += 1;
    }
    return canSend;
}

class TestFriendMessageDispatcher : public QObject
{
    Q_OBJECT

public:
    TestFriendMessageDispatcher();

private slots:
    void init();
    void testSignals();
    void testMessageSending();
    void testOfflineMessages();
    void testFailedMessage();

    void onMessageSent(DispatchedMessageId id, Message message)
    {
        auto it = outgoingMessages.find(id);
        QVERIFY(it == outgoingMessages.end());
        outgoingMessages.emplace(id, std::move(message));
    }

    void onMessageComplete(DispatchedMessageId id)
    {
        auto it = outgoingMessages.find(id);
        QVERIFY(it != outgoingMessages.end());
        outgoingMessages.erase(it);
    }

    void onMessageReceived(const ToxPk& sender, Message message)
    {
        std::ignore = sender;
        receivedMessages.push_back(std::move(message));
    }

    void onMessageBroken(DispatchedMessageId id, BrokenMessageReason reason)
    {
        std::ignore = reason;
        brokenMessages.insert(id);
    }

private:
    // All unique_ptrs to make construction/init() easier to manage
    std::unique_ptr<Friend> f;
    std::unique_ptr<MockFriendMessageSender> messageSender;
    std::unique_ptr<MessageProcessor::SharedParams> sharedProcessorParams;
    std::unique_ptr<MessageProcessor> messageProcessor;
    std::unique_ptr<FriendMessageDispatcher> friendMessageDispatcher;
    std::map<DispatchedMessageId, Message> outgoingMessages;
    std::set<DispatchedMessageId> brokenMessages;
    std::deque<Message> receivedMessages;
};

TestFriendMessageDispatcher::TestFriendMessageDispatcher() = default;

/**
 * @brief Test initialization. Resets all member variables for a fresh test state
 */
void TestFriendMessageDispatcher::init()
{
    f = std::make_unique<Friend>(0, ToxPk());
    f->setStatus(Status::Status::Online);
    messageSender = std::make_unique<MockFriendMessageSender>();
    sharedProcessorParams = std::make_unique<MessageProcessor::SharedParams>(tox_max_message_length());

    messageProcessor = std::make_unique<MessageProcessor>(*sharedProcessorParams);
    friendMessageDispatcher =
        std::make_unique<FriendMessageDispatcher>(*f, *messageProcessor, *messageSender);

    connect(friendMessageDispatcher.get(), &FriendMessageDispatcher::messageSent, this,
            &TestFriendMessageDispatcher::onMessageSent);
    connect(friendMessageDispatcher.get(), &FriendMessageDispatcher::messageComplete, this,
            &TestFriendMessageDispatcher::onMessageComplete);
    connect(friendMessageDispatcher.get(), &FriendMessageDispatcher::messageReceived, this,
            &TestFriendMessageDispatcher::onMessageReceived);
    connect(friendMessageDispatcher.get(), &FriendMessageDispatcher::messageBroken, this,
            &TestFriendMessageDispatcher::onMessageBroken);

    outgoingMessages = std::map<DispatchedMessageId, Message>();
    receivedMessages = std::deque<Message>();
    brokenMessages = std::set<DispatchedMessageId>();
}

/**
 * @brief Tests that the signals emitted by the dispatcher are all emitted at the correct times
 */
void TestFriendMessageDispatcher::testSignals()
{
    auto startReceiptNum = messageSender->receiptNum;
    auto sentIds = friendMessageDispatcher->sendMessage(false, "test");
    auto endReceiptNum = messageSender->receiptNum;

    // We should have received some message ids in our callbacks
    QVERIFY(sentIds.first == sentIds.second);
    QVERIFY(outgoingMessages.contains(sentIds.first));
    QVERIFY(startReceiptNum.get() != endReceiptNum.get());
    QVERIFY(outgoingMessages.size() == 1);

    QVERIFY(outgoingMessages.begin()->second.isAction == false);
    QVERIFY(outgoingMessages.begin()->second.content == "test");

    for (auto i = startReceiptNum; i < endReceiptNum; ++i.get()) {
        friendMessageDispatcher->onReceiptReceived(i);
    }

    // If our completion ids were hooked up right this should be empty
    QVERIFY(outgoingMessages.empty());

    // If signals are emitted correctly we should have one message in our received message buffer
    QVERIFY(receivedMessages.empty());
    friendMessageDispatcher->onMessageReceived(false, "test2");

    QVERIFY(!receivedMessages.empty());
    QVERIFY(receivedMessages.front().isAction == false);
    QVERIFY(receivedMessages.front().content == "test2");
}

/**
 * @brief Tests that sent messages actually go through to core
 */
void TestFriendMessageDispatcher::testMessageSending()
{
    friendMessageDispatcher->sendMessage(false, "Test");

    QVERIFY(messageSender->numSentMessages == 1);
    QVERIFY(messageSender->numSentActions == 0);

    friendMessageDispatcher->sendMessage(true, "Test");

    QVERIFY(messageSender->numSentMessages == 1);
    QVERIFY(messageSender->numSentActions == 1);
}

/**
 * @brief Tests that messages dispatched while a friend is offline are sent later
 */
void TestFriendMessageDispatcher::testOfflineMessages()
{
    f->setStatus(Status::Status::Offline);
    auto firstReceipt = messageSender->receiptNum;

    friendMessageDispatcher->sendMessage(false, "test");
    friendMessageDispatcher->sendMessage(false, "test2");
    friendMessageDispatcher->sendMessage(true, "test3");

    QVERIFY(messageSender->numSentActions == 0);
    QVERIFY(messageSender->numSentMessages == 0);
    QVERIFY(outgoingMessages.size() == 3);

    f->setStatus(Status::Status::Online);

    QVERIFY(messageSender->numSentActions == 1);
    QVERIFY(messageSender->numSentMessages == 2);
    QVERIFY(outgoingMessages.size() == 3);

    auto lastReceipt = messageSender->receiptNum;
    for (auto i = firstReceipt; i < lastReceipt; ++i.get()) {
        friendMessageDispatcher->onReceiptReceived(i);
    }

    QVERIFY(messageSender->numSentActions == 1);
    QVERIFY(messageSender->numSentMessages == 2);
    QVERIFY(outgoingMessages.empty());
}

/**
 * @brief Tests that messages that failed to send due to toxcore are resent later
 */
void TestFriendMessageDispatcher::testFailedMessage()
{
    messageSender->canSend = false;

    friendMessageDispatcher->sendMessage(false, "test");

    QVERIFY(messageSender->numSentMessages == 0);

    messageSender->canSend = true;
    f->setStatus(Status::Status::Offline);
    f->setStatus(Status::Status::Online);

    QVERIFY(messageSender->numSentMessages == 1);
}

QTEST_GUILESS_MAIN(TestFriendMessageDispatcher)
#include "friendmessagedispatcher_test.moc"
