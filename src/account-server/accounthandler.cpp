/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */

#include "configuration.h"
#include "point.h"
#include "account-server/accounthandler.hpp"
#include "account-server/account.hpp"
#include "account-server/accountclient.hpp"
#include "account-server/serverhandler.hpp"
#include "account-server/storage.hpp"
#include "chat-server/chathandler.hpp"
#include "net/connectionhandler.hpp"
#include "net/messagein.hpp"
#include "net/messageout.hpp"
#include "net/netcomputer.hpp"
#include "utils/logger.h"
#include "utils/stringfilter.h"

bool
AccountHandler::startListen(enet_uint16 port)
{
    LOG_INFO("Account handler started:");
    return ConnectionHandler::startListen(port);
}

NetComputer*
AccountHandler::computerConnected(ENetPeer *peer)
{
    return new AccountClient(peer);
}

void
AccountHandler::computerDisconnected(NetComputer *comp)
{
    delete comp;
}

/**
 * Generic interface convention for getting a message and sending it to the
 * correct subroutines. Account handler takes care of determining the
 * current step in the account process, be it creation, setup, or login.
 */
void
AccountHandler::processMessage(NetComputer *comp, MessageIn &message)
{
    AccountClient &computer = *static_cast< AccountClient * >(comp);

    Storage &store = Storage::instance("tmw");

#if defined (SQLITE_SUPPORT)
    // Reopen the db in this thread for sqlite, to avoid
    // Library Call out of sequence problem due to thread safe.
    store.setUser(config.getValue("dbuser", ""));
    store.setPassword(config.getValue("dbpass", ""));
    store.close();
    store.open();
#endif

    MessageOut result;

    switch (message.getId())
    {
        case PAMSG_LOGIN:
            handleLoginMessage(computer, message);
            break;

        case PAMSG_LOGOUT:
            handleLogoutMessage(computer);
            break;

        case PAMSG_REGISTER:
            handleRegisterMessage(computer, message);
            break;

        case PAMSG_UNREGISTER:
            handleUnregisterMessage(computer, message);
            break;

        case PAMSG_EMAIL_CHANGE:
            {
                result.writeShort(APMSG_EMAIL_CHANGE_RESPONSE);

                if (computer.getAccount().get() == NULL) {
                    result.writeByte(ERRMSG_NO_LOGIN);
                    break;
                }

                std::string email = message.readString();
                if (!stringFilter->isEmailValid(email))
                {
                    result.writeByte(ERRMSG_INVALID_ARGUMENT);
                }
                else if (stringFilter->findDoubleQuotes(email))
                {
                    result.writeByte(ERRMSG_INVALID_ARGUMENT);
                }
                else if (store.doesEmailAddressExist(email))
                {
                    result.writeByte(EMAILCHG_EXISTS_EMAIL);
                }
                else
                {
                    computer.getAccount()->setEmail(email);
                    result.writeByte(ERRMSG_OK);
                }
            }
            break;

        case PAMSG_EMAIL_GET:
            {
                result.writeShort(APMSG_EMAIL_GET_RESPONSE);
                if (computer.getAccount().get() == NULL) {
                    result.writeByte(ERRMSG_NO_LOGIN);
                    break;
                }
                else
                {
                    result.writeByte(ERRMSG_OK);
                    result.writeString(computer.getAccount()->getEmail());
                }
            }
            break;

        case PAMSG_PASSWORD_CHANGE:
            handlePasswordChangeMessage(computer, message);
            break;

        case PAMSG_CHAR_CREATE:
            handleCharacterCreateMessage(computer, message);
            break;

        case PAMSG_CHAR_SELECT:
            {
                result.writeShort(APMSG_CHAR_SELECT_RESPONSE);

                if (computer.getAccount().get() == NULL)
                {
                    result.writeByte(ERRMSG_NO_LOGIN);
                    break; // not logged in
                }

                unsigned char charNum = message.readByte();

                Players &chars = computer.getAccount()->getCharacters();
                // Character ID = 0 to Number of Characters - 1.
                if (charNum >= chars.size()) {
                    // invalid char selection
                    result.writeByte(ERRMSG_INVALID_ARGUMENT);
                    break;
                }

                std::string address;
                short port;
                if (!serverHandler->getGameServerFromMap(chars[charNum]->getMap(), address, port))
                {
                    result.writeByte(ERRMSG_FAILURE);
                    LOG_ERROR("Character Selection: No game server for the map.");
                    break;
                }

                // set character
                computer.setCharacter(chars[charNum]);
                PlayerPtr selectedChar = computer.getCharacter();
                result.writeByte(ERRMSG_OK);

                LOG_DEBUG(selectedChar->getName() << " is trying to enter the servers.");

                std::string magic_token(32, ' ');
                for (int i = 0; i < 32; ++i) {
                    magic_token[i] =
                        1 + (int) (127 * (rand() / (RAND_MAX + 1.0)));
                }
                result.writeString(magic_token, 32);
                result.writeString(address);
                result.writeShort(port);
                // TODO: get correct address and port for the chat server
                result.writeString(config.getValue("accountServerAddress", "localhost"));
                result.writeShort(int(config.getValue("accountServerPort", DEFAULT_SERVER_PORT)) + 2);

                serverHandler->registerGameClient(magic_token, selectedChar);
                registerChatClient(magic_token, selectedChar->getName(),
                                   AL_NORMAL);
            }
            break;

        case PAMSG_CHAR_DELETE:
            {
                result.writeShort(APMSG_CHAR_DELETE_RESPONSE);

                if (computer.getAccount().get() == NULL)
                {
                    result.writeByte(ERRMSG_NO_LOGIN);
                    break; // not logged in
                }

                unsigned char charNum = message.readByte();

                Players &chars = computer.getAccount()->getCharacters();
                // Character ID = 0 to Number of Characters - 1.
                if (charNum >= chars.size()) {
                    // invalid char selection
                    result.writeByte(ERRMSG_INVALID_ARGUMENT);
                    break;
                }

                // Delete the character
                // if the character to delete is the current character, get off of it in
                // memory.
                if ( computer.getCharacter().get() != NULL )
                {
                    if ( computer.getCharacter()->getName() == chars[charNum].get()->getName() )
                    {
                        computer.unsetCharacter();
                    }
                }

                std::string deletedCharacter = chars[charNum].get()->getName();
                computer.getAccount()->delCharacter(deletedCharacter);
                store.flush(computer.getAccount());
                LOG_INFO(deletedCharacter << ": Character deleted...");
                result.writeByte(ERRMSG_OK);

            }
            break;

        default:
            LOG_WARN("Invalid message type");
            result.writeShort(XXMSG_INVALID);
            break;
    }

    // return result
    if (result.getLength() > 0)
        computer.send(result);
}

void
AccountHandler::handleLoginMessage(AccountClient &computer, MessageIn &msg)
{
    unsigned long clientVersion = msg.readLong();
    std::string username = msg.readString();
    std::string password = msg.readString();

    MessageOut reply(APMSG_LOGIN_RESPONSE);

    if (clientVersion < config.getValue("clientVersion", 0))
    {
        reply.writeByte(LOGIN_INVALID_VERSION);
    }
    if (stringFilter->findDoubleQuotes(username))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    if (computer.getAccount().get() != NULL) {
        reply.writeByte(ERRMSG_FAILURE);
    }
    if (getClientNumber() >= MAX_CLIENTS )
    {
        reply.writeByte(LOGIN_SERVER_FULL);
    }
    else
    {
        // Check if the account exists
        Storage &store = Storage::instance("tmw");
        AccountPtr acc = store.getAccount(username);

        if (!acc.get() || acc->getPassword() != password)
        {
            reply.writeByte(ERRMSG_INVALID_ARGUMENT);
        }
        else
        {
            // Associate account with connection
            computer.setAccount(acc);

            reply.writeByte(ERRMSG_OK);
            computer.send(reply);

            // Return information about available characters
            Players &chars = computer.getAccount()->getCharacters();

            // Send characters list
            for (unsigned int i = 0; i < chars.size(); i++)
            {
                MessageOut charInfo(APMSG_CHAR_INFO);
                charInfo.writeByte(i); // Slot
                charInfo.writeString(chars[i]->getName());
                charInfo.writeByte((unsigned char) chars[i]->getGender());
                charInfo.writeByte(chars[i]->getHairStyle());
                charInfo.writeByte(chars[i]->getHairColor());
                charInfo.writeByte(chars[i]->getLevel());
                charInfo.writeShort(chars[i]->getMoney());
                for (int j = 0; j < NB_RSTAT; ++j)
                        charInfo.writeShort(chars[i]->getRawStat(j));
                computer.send(charInfo);
            }
            return;
        }
    }

    computer.send(reply);
}

void
AccountHandler::handleLogoutMessage(AccountClient &computer)
{
    MessageOut reply(APMSG_LOGOUT_RESPONSE);

    if (computer.getAccount().get() == NULL)
    {
        reply.writeByte(ERRMSG_NO_LOGIN);
    }
    else
    {
        computer.unsetAccount();
        reply.writeByte(ERRMSG_OK);
    }

    computer.send(reply);
}

void
AccountHandler::handleRegisterMessage(AccountClient &computer, MessageIn &msg)
{
    unsigned long clientVersion = msg.readLong();
    std::string username = msg.readString();
    std::string password = msg.readString();
    std::string email = msg.readString();

    MessageOut reply(APMSG_REGISTER_RESPONSE);

    if (clientVersion < config.getValue("clientVersion", 0))
    {
        reply.writeByte(REGISTER_INVALID_VERSION);
    }
    else if (stringFilter->findDoubleQuotes(username))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else if (stringFilter->findDoubleQuotes(email))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else if ((username.length() < MIN_LOGIN_LENGTH) ||
            (username.length() > MAX_LOGIN_LENGTH))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else if ((password.length() < MIN_PASSWORD_LENGTH) ||
            (password.length() > MAX_PASSWORD_LENGTH))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else if (!stringFilter->isEmailValid(email))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    // Checking if the Name is slang's free.
    else if (!stringFilter->filterContent(username))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else
    {
        Storage &store = Storage::instance("tmw");
        AccountPtr accPtr = store.getAccount(username);

        // Check whether the account already exists.
        if (accPtr.get())
        {
            reply.writeByte(REGISTER_EXISTS_USERNAME);
        }
        // Find out whether the email is already in use.
        else if (store.doesEmailAddressExist(email))
        {
            reply.writeByte(REGISTER_EXISTS_EMAIL);
        }
        else
        {
            AccountPtr acc(new Account(username, password, email));
            store.addAccount(acc);
            reply.writeByte(ERRMSG_OK);
        }
    }

    computer.send(reply);
}

void
AccountHandler::handleUnregisterMessage(AccountClient &computer,
                                        MessageIn &msg)
{
    std::string username = msg.readString();
    std::string password = msg.readString();

    MessageOut reply(APMSG_UNREGISTER_RESPONSE);

    if (stringFilter->findDoubleQuotes(username))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else
    {
        // See if the account exists
        Storage &store = Storage::instance("tmw");
        AccountPtr accPtr = store.getAccount(username);

        if (!accPtr.get() || accPtr->getPassword() != password)
        {
            reply.writeByte(ERRMSG_INVALID_ARGUMENT);
        }
        else
        {
            // If the account to delete is the current account we're logged in.
            // Get out of it in memory.
            if (computer.getAccount().get() != NULL )
            {
                if (computer.getAccount()->getName() == username)
                {
                    computer.unsetAccount();
                }
            }

            // Delete account and associated characters
            LOG_INFO("Farewell " << username << " ...");
            store.delAccount(accPtr);
            reply.writeByte(ERRMSG_OK);
        }
    }

    computer.send(reply);
}

void
AccountHandler::handlePasswordChangeMessage(AccountClient &computer,
                                            MessageIn &msg)
{
    std::string oldPassword = msg.readString();
    std::string newPassword = msg.readString();

    MessageOut reply(APMSG_PASSWORD_CHANGE_RESPONSE);

    if (computer.getAccount().get() == NULL)
    {
        reply.writeByte(ERRMSG_NO_LOGIN);
    }
    else if (newPassword.length() < MIN_PASSWORD_LENGTH ||
            newPassword.length() > MAX_PASSWORD_LENGTH)
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else if (stringFilter->findDoubleQuotes(newPassword))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else if (oldPassword != computer.getAccount()->getPassword())
    {
        reply.writeByte(ERRMSG_FAILURE);
    }
    else
    {
        computer.getAccount()->setPassword(newPassword);
        reply.writeByte(ERRMSG_OK);
    }

    computer.send(reply);
}

void
AccountHandler::handleCharacterCreateMessage(AccountClient &computer,
                                             MessageIn &msg)
{
    std::string name = msg.readString();
    int hairStyle = msg.readByte();
    int hairColor = msg.readByte();
    int gender = msg.readByte();

    MessageOut reply(APMSG_CHAR_CREATE_RESPONSE);

    if (computer.getAccount().get() == NULL) {
        reply.writeByte(ERRMSG_NO_LOGIN);
    }
    else if (!stringFilter->filterContent(name))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else if (stringFilter->findDoubleQuotes(name))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else if (hairStyle < 0 || hairStyle > MAX_HAIRSTYLE_VALUE)
    {
        reply.writeByte(CREATE_INVALID_HAIRSTYLE);
    }
    else if (hairColor < 0 || hairColor > MAX_HAIRCOLOR_VALUE)
    {
        reply.writeByte(CREATE_INVALID_HAIRCOLOR);
    }
    else if (gender < 0 || gender > MAX_GENDER_VALUE)
    {
        reply.writeByte(CREATE_INVALID_GENDER);
    }
    else if ((name.length() < MIN_CHARACTER_LENGTH) ||
             (name.length() > MAX_CHARACTER_LENGTH))
    {
        reply.writeByte(ERRMSG_INVALID_ARGUMENT);
    }
    else
    {
        Storage &store = Storage::instance("tmw");
        if (store.doesCharacterNameExist(name))
        {
            reply.writeByte(CREATE_EXISTS_NAME);
            computer.send(reply);
            return;
        }

        // A player shouldn't have more than MAX_OF_CHARACTERS characters.
        Players &chars = computer.getAccount()->getCharacters();
        if (chars.size() >= MAX_OF_CHARACTERS)
        {
            reply.writeByte(CREATE_TOO_MUCH_CHARACTERS);
            computer.send(reply);
            return;
        }

        // LATER_ON: Add race, face and maybe special attributes.

        // Customization of player's stats...
        RawStatistics rawStats;
        for (int i = 0; i < NB_RSTAT; ++i)
            rawStats.stats[i] = msg.readShort();

        // We see if the difference between the lowest stat and the highest
        // isn't too big.
        unsigned short lowestStat = 0;
        unsigned short highestStat = 0;
        unsigned int totalStats = 0;
        bool validNonZeroRawStats = true;
        for (int i = 0; i < NB_RSTAT; ++i)
        {
            unsigned short stat = rawStats.stats[i];
            // For good total stat check.
            totalStats = totalStats + stat;

            // For checking if all stats are at least > 0
            if (stat <= 0) validNonZeroRawStats = false;
            if (lowestStat == 0 || lowestStat > stat) lowestStat = stat;
            if (highestStat == 0 || highestStat < stat) highestStat = stat;
        }

        if (totalStats > POINTS_TO_DISTRIBUTES_AT_LVL1)
        {
            reply.writeByte(CREATE_RAW_STATS_TOO_HIGH);
        }
        else if (totalStats < POINTS_TO_DISTRIBUTES_AT_LVL1)
        {
            reply.writeByte(CREATE_RAW_STATS_TOO_LOW);
        }
        else if ((highestStat - lowestStat) > (signed) MAX_DIFF_BETWEEN_STATS)
        {
            reply.writeByte(CREATE_RAW_STATS_INVALID_DIFF);
        }
        else if (!validNonZeroRawStats)
        {
            reply.writeByte(CREATE_RAW_STATS_EQUAL_TO_ZERO);
        }
        else
        {
            PlayerPtr newCharacter(new PlayerData(name));
            for (int i = 0; i < NB_RSTAT; ++i)
                newCharacter->setRawStat(i, rawStats.stats[i]);
            newCharacter->setMoney(0);
            newCharacter->setLevel(1);
            newCharacter->setGender(gender);
            newCharacter->setHairStyle(hairStyle);
            newCharacter->setHairColor(hairColor);
            newCharacter->setMap((int) config.getValue("defaultMap", 1));
            Point startingPos = { (int) config.getValue("startX", 0),
                                  (int) config.getValue("startY", 0) };
            newCharacter->setPos(startingPos);
            computer.getAccount()->addCharacter(newCharacter);

            LOG_INFO("Character " << name << " was created for "
                     << computer.getAccount()->getName() << "'s account.");

            store.flush(computer.getAccount()); // flush changes
            reply.writeByte(ERRMSG_OK);
            computer.send(reply);

            // Send new characters infos back to client
            MessageOut charInfo(APMSG_CHAR_INFO);
            int slot = chars.size() - 1;
            charInfo.writeByte(slot);
            charInfo.writeString(chars[slot]->getName());
            charInfo.writeByte((unsigned char) chars[slot]->getGender());
            charInfo.writeByte(chars[slot]->getHairStyle());
            charInfo.writeByte(chars[slot]->getHairColor());
            charInfo.writeByte(chars[slot]->getLevel());
            charInfo.writeShort(chars[slot]->getMoney());
            for (int j = 0; j < NB_RSTAT; ++j)
                charInfo.writeShort(chars[slot]->getRawStat(j));
            computer.send(charInfo);
            return;
        }
    }

    computer.send(reply);
}