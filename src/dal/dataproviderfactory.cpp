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
 *
 *  $Id$
 */


#include "dataproviderfactory.h"

#include "mysqldataprovider.h"
#include "sqlitedataprovider.h"


namespace tmw
{
namespace dal
{


/**
 * Default constructor.
 */
DataProviderFactory::DataProviderFactory(void)
    throw()
{
    // NOOP
}


/**
 * Destructor.
 */
DataProviderFactory::~DataProviderFactory(void)
    throw()
{
    // NOOP
}


/**
 * Create a data provider.
 */
DataProvider*
DataProviderFactory::createDataProvider(void)
    throw(std::runtime_error)
{
#ifdef USE_MYSQL
    MySqlDataProvider* provider = new MySqlDataProvider;
    return provider;
#endif

#ifdef USE_SQLITE
    SqLiteDataProvider* provider = new SqLiteDataProvider;
    return provider;
#endif

    // a data provider cannot be created as the server has been compiled
    // without support for any databases.
    throw std::runtime_error("missing database backend support");
}


} // namespace dal
} // namespace tmw
