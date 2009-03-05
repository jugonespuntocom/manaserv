/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World  is free software; you can redistribute  it and/or modify it
 *  under the terms of the GNU General  Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or any later version.
 *
 *  The Mana  World is  distributed in  the hope  that it  will be  useful, but
 *  WITHOUT ANY WARRANTY; without even  the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *  more details.
 *
 *  You should  have received a  copy of the  GNU General Public  License along
 *  with The Mana  World; if not, write to the  Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _TMWSERV_CHARACTER_HPP_
#define _TMWSERV_CHARACTER_HPP_

#include <map>
#include <string>
#include <vector>

#include "common/inventorydata.hpp"
#include "game-server/being.hpp"

class BuySell;
class GameClient;
class MessageIn;
class MessageOut;
class Point;
class Trade;

struct Special
{
    Special(int needed)
    {
        currentMana = 0;
        neededMana = needed;
    }
    int currentMana;
    int neededMana;
};

/**
 * The representation of a player's character in the game world.
 */
class Character : public Being
{
    public:

        /**
         * Utility constructor for creating a Character from a received
         * characterdata message.
         */
        Character(MessageIn &msg);

        ~Character();

        /**
         * recalculates the level when necessary and calls Being::update
         */
        void update();

        /**
         * Perform actions.
         */
        void perform();

        /**
         * makes the character respawn
         */
        void respawn();

        /**
         * makes the character perform a special action
         * when it is allowed to do so
         */
        void useSpecial(int id);

        /**
         * Allows a character to perform a special action
         */
        void giveSpecial(int id);

        /**
         * Gets client computer.
         */
        GameClient *getClient() const
        { return mClient; }

        /**
         * Sets client computer.
         */
        void setClient(GameClient *c)
        { mClient = c; }

        /**
         * Gets a reference on the possessions.
         */
        Possessions const &getPossessions() const
        { return mPossessions; }

        /**
         * Gets a reference on the possessions.
         */
        Possessions &getPossessions()
        { return mPossessions; }

        /**
         * Gets the Trade object the character is involved in.
         */
        Trade *getTrading() const;

        /**
         * Sets the Trade object the character is involved in.
         * Cancels other transactions.
         */
        void setTrading(Trade *t);

        /**
         * Gets the BuySell object the character is involved in.
         */
        BuySell *getBuySell() const;

        /**
         * Sets the trade object the character is involved in.
         * Cancels other transactions.
         */
        void setBuySell(BuySell *t);

        /**
         * Cancels current transaction.
         */
        void cancelTransaction();

        /**
         * Gets transaction status of the character.
         */
        bool isBusy() const
        { return mTransaction != TRANS_NONE; }

        /*
         * Character data:
         * Get and set methods
         */

        /** Gets the database id of the character. */
        int
        getDatabaseID() const
        { return mDatabaseID; }

        /** Sets the database id of the character. */
        void
        setDatabaseID(int id)
        { mDatabaseID = id; }

        /** Gets the gender of the character (male or female). */
        int
        getGender() const
        { return mGender; }

        /** Sets the gender of the character (male or female). */
        void
        setGender(int gender)
        { mGender = gender; }

        /** Gets the hairstyle of the character. */
        int
        getHairStyle() const
        { return mHairStyle; }

        /** Sets the hairstyle of the character. */
        void
        setHairStyle(int style)
        { mHairStyle = style; }

        /** Gets the haircolor of the character. */
        int
        getHairColor() const
        { return mHairColor; }

        /** Sets the haircolor of the character. */
        void
        setHairColor(int color)
        { mHairColor = color; }

        /** Gets the level of the character. */
        int
        getLevel() const
        { return mLevel; }

        /** Sets the level of the character. */
        void
        setLevel(int level)
        { mLevel = level; }

        /** Gets the account level of the user. */
        int getAccountLevel() const
        { return mAccountLevel; }

        /** Sets the account level of the user. */
        void setAccountLevel(int l)
        { mAccountLevel = l; }

        /** Gets the party id of the character */
        int getParty() const
        { return mParty; }

        /** Sets the party id of the character */
        void setParty(int party)
        { mParty = party; }

        /**
         * Sends a message that informs the client about attribute
         * modified since last call.
         */
        void sendStatus();

        /**
         * Gets the ID of the map that the character is on.
         * For serialization purpose only.
         */
        int getMapId() const;

        /**
         * Sets the ID of the map that the character is on.
         * For serialization purpose only.
         */
        void setMapId(int);

        /**
         * Updates base Being attributes.
         */
        void modifiedAttribute(int);

        /**
         * Calls all the "disconnected" listener.
         */
        void disconnected();

        /**
         * Associative array containing all the quest variables known by the
         * server.
         */
        std::map< std::string, std::string > questCache;

        /**
         * Gives a skill a specific amount of exp and checks if a levelup
         * occured.
         */
        void receiveExperience(size_t skill, int experience);

        /**
         * Gets total accumulated exp for skill
         */
        int getExperience(int skill) const
        { return mExperience[skill]; }

        /**
         * Sets total accumulated exp for skill
         */
        void setExperience(int skill, int value)
        { mExperience[skill] = 0; receiveExperience(skill + CHAR_SKILL_BEGIN , value) ; }

        /**
         * Shortcut to get being's health
         */
        int getHealth() const
        { return getModifiedAttribute(CHAR_ATTR_VITALITY); }

        /**
         * Returns the exp needed to reach a specific skill level
         */
        static int expForLevel(int level);

        /**
         * Tries to use a character point to increase a
         * basic attribute
         */
        AttribmodResponseCode useCharacterPoint(size_t attribute);

        /**
         * Tries to use a correction point to reduce a
         * basic attribute and regain a character point
         */
        AttribmodResponseCode useCorrectionPoint(size_t attribute);

        void setCharacterPoints(int points)
        { mCharacterPoints = points; }

        int getCharacterPoints() const
        { return mCharacterPoints; }

        void setCorrectionPoints(int points)
        { mCorrectionPoints = points; }

        int getCorrectionPoints() const
        { return mCorrectionPoints; }

        /**
         * Gets the way the object is blocked by other things on the map
         */
        virtual unsigned char getWalkMask() const
        { return 0x82; } // blocked by walls and monsters ( bin 1000 0010)

    private:
        Character(Character const &);
        Character &operator=(Character const &);

        static const float EXPCURVE_EXPONENT;
        static const float EXPCURVE_FACTOR;
        static const float LEVEL_SKILL_PRECEDENCE_FACTOR; // I am taking suggestions for a better name
        static const int CHARPOINTS_PER_LEVELUP = 5;
        static const int CORRECTIONPOINTS_PER_LEVELUP = 2;
        static const int CORRECTIONPOINTS_MAX = 10;

        static const AttackZone UNARMED_ATTACK_ZONE;

        /**
         * Advances the character by one level;
         */
        void levelup();

        /**
         * Marks attribute as recently modified.
         */
        void flagAttribute(int);

        /**
         * Returns the exp needed for next skill levelup
         */
        int getExpNeeded(size_t skill);

        /**
         * Returns the exp collected on this skill level
         */
        int getExpGot(size_t skill);

        /**
         * Recalculates the character level
         */
        void recalculateLevel();

        enum TransactionType
        { TRANS_NONE, TRANS_TRADE, TRANS_BUYSELL };

        GameClient *mClient;   /**< Client computer. */
        /** Handler of the transaction the character is involved in. */
        void *mTransactionHandler;

        Possessions mPossessions;    /**< Possesssions of the character. */

        /** Attributes modified since last update. */
        std::set<size_t> mModifiedAttributes;
        std::set<size_t> mModifiedExperience;

        std::vector<unsigned int> mExperience; /**< experience collected for each skill.*/

        std::map<int, Special*> mSpecials;

        int mDatabaseID;             /**< Character's database ID. */
        unsigned char mGender;       /**< Gender of the character. */
        unsigned char mHairStyle;    /**< Hair Style of the character. */
        unsigned char mHairColor;    /**< Hair Color of the character. */
        int mLevel;                  /**< Level of the character. */
        int mLevelProgress;          /**< progress to next level in percent */
        int mCharacterPoints;        /**< unused attribute points that can be distributed */
        int mCorrectionPoints;       /**< unused attribute correction points */
        bool mUpdateLevelProgress;   /**< flag raised when percent to next level changed */
        bool mRecalculateLevel;      /**< flag raised when the character level might have increased */
        unsigned char mAccountLevel; /**< Account level of the user. */
        int mParty;                  /**< Party id of the character */
        TransactionType mTransaction; /**< Trade/buy/sell action the character is involved in. */

    protected:
        /**
         * Gets the way the object blocks pathfinding for other objects
         */
        virtual Map::BlockType getBlockType() const
        { return Map::BLOCKTYPE_CHARACTER; }
};

#endif // _TMWSERV_CHARACTER_HPP_
