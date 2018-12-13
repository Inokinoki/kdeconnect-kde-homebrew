/**
 * Copyright 2018 Simon Redman <simon@ergotech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "conversationsdbusinterface.h"
#include "interfaces/dbusinterfaces.h"
#include "interfaces/conversationmessage.h"

#include "requestconversationworker.h"

#include <QDBusConnection>

#include <core/device.h>
#include <core/kdeconnectplugin.h>

Q_LOGGING_CATEGORY(KDECONNECT_CONVERSATIONS, "kdeconnect.conversations")

ConversationsDbusInterface::ConversationsDbusInterface(KdeConnectPlugin* plugin)
    : QDBusAbstractAdaptor(const_cast<Device*>(plugin->device()))
    , m_device(plugin->device())
    , m_plugin(plugin)
    , m_lastId(0)
    , m_smsInterface(m_device->id())
{
    ConversationMessage::registerDbusType();
}

ConversationsDbusInterface::~ConversationsDbusInterface()
{
}

QVariantList ConversationsDbusInterface::activeConversations()
{
    QList<QVariant> toReturn;
    toReturn.reserve(m_conversations.size());

    for (auto it = m_conversations.cbegin(); it != m_conversations.cend(); ++it) {
        const auto& conversation = it.value().values();
        if (conversation.isEmpty()) {
            // This should really never happen because we create a conversation at the same time
            // as adding a message, but better safe than sorry
            qCWarning(KDECONNECT_CONVERSATIONS)
                    << "Conversation with ID" << it.key() << "is unexpectedly empty";
            break;
        }
        const QVariantMap& message = (*conversation.crbegin()).toVariant();
        toReturn.append(message);
    }

    return toReturn;
}

void ConversationsDbusInterface::requestConversation(const qint64& conversationID, int start, int end)
{
    RequestConversationWorker* worker = new RequestConversationWorker(conversationID, start, end, this);
    connect(worker, &RequestConversationWorker::conversationMessageRead,
            this, &ConversationsDbusInterface::conversationUpdated,
            Qt::QueuedConnection);
    worker->work();
}

void ConversationsDbusInterface::addMessages(const QList<ConversationMessage> &messages)
{
    QSet<qint32> updatedConversationIDs;

    for (const auto& message : messages) {
        const qint32& threadId = message.threadID();

        if (m_known_messages[threadId].contains(message.uID())) {
            // This message has already been processed. Don't do anything.
            continue;
        }

        updatedConversationIDs.insert(message.threadID());

        // Store the Message in the list corresponding to its thread
        bool newConversation = !m_conversations.contains(threadId);
        const auto& threadPosition = m_conversations[threadId].insert(message.date(), message);
        m_known_messages[threadId].insert(message.uID());

        // If this message was inserted at the end of the list, it is the latest message in the conversation
        bool latestMessage = threadPosition == m_conversations[threadId].end() - 1;

        // Tell the world about what just happened
        if (newConversation) {
            Q_EMIT conversationCreated(message.toVariant());
        } else if (latestMessage) {
            Q_EMIT conversationUpdated(message.toVariant());
        }
    }

    waitingForMessagesLock.lock();
    // Remove the waiting flag for all conversations which we just processed
    conversationsWaitingForMessages.subtract(updatedConversationIDs);
    waitingForMessages.wakeAll();
    waitingForMessagesLock.unlock();
}

void ConversationsDbusInterface::removeMessage(const QString& internalId)
{
    // TODO: Delete the specified message from our internal structures
}

QList<ConversationMessage> ConversationsDbusInterface::getConversation(const qint64& conversationID) const
{
    return m_conversations.value(conversationID).values();
}

void ConversationsDbusInterface::updateConversation(const qint32& conversationID)
{
    waitingForMessagesLock.lock();
    qCDebug(KDECONNECT_CONVERSATIONS) << "Requesting conversation with ID" << conversationID << "from remote";
    conversationsWaitingForMessages.insert(conversationID);
    m_smsInterface.requestConversation(conversationID);
    while (conversationsWaitingForMessages.contains(conversationID)) {
        waitingForMessages.wait(&waitingForMessagesLock);
    }
    waitingForMessagesLock.unlock();
}

void ConversationsDbusInterface::replyToConversation(const qint64& conversationID, const QString& message)
{
    const auto messagesList = m_conversations[conversationID];
    if (messagesList.isEmpty()) {
        // Since there are no messages in the conversation, we can't do anything sensible
        qCWarning(KDECONNECT_CONVERSATIONS) << "Got a conversationID for a conversation with no messages!";
        return;
    }
    // Caution:
    // This method assumes that the address of any message (in this case, whichever one pops out
    // with .first()) will be the same. This works fine for single-target SMS but might break down
    // for group MMS, etc.
    const QString& address = messagesList.first().address();
    m_smsInterface.sendSms(address, message);
}

void ConversationsDbusInterface::requestAllConversationThreads()
{
    // Prepare the list of conversations by requesting the first in every thread
    m_smsInterface.requestAllConversations();
}

QString ConversationsDbusInterface::newId()
{
    return QString::number(++m_lastId);
}
