/*
 * HTTPConnectionManager.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "HTTPConnectionManager.h"
#include "HTTPConnection.hpp"
#include "ConnectionParams.hpp"
#include "Sockets.hpp"
#include "Downloader.hpp"
#include <vlc_url.h>

using namespace adaptive::http;

HTTPConnectionManager::HTTPConnectionManager    (vlc_object_t *p_object_, ConnectionFactory *factory_) :
                       p_object                 (p_object_),
                       rateObserver             (NULL)
{
    vlc_mutex_init(&lock);
    downloader = new (std::nothrow) Downloader();
    downloader->start();
    if(!factory_)
    {
        if(var_InheritBool(p_object, "adaptive-use-access"))
            factory = new (std::nothrow) StreamUrlConnectionFactory();
        else
            factory = new (std::nothrow) ConnectionFactory();
    }
    else
        factory = factory_;
}
HTTPConnectionManager::~HTTPConnectionManager   ()
{
    delete downloader;
    delete factory;
    this->closeAllConnections();
    vlc_mutex_destroy(&lock);
}

void HTTPConnectionManager::closeAllConnections      ()
{
    vlc_mutex_lock(&lock);
    releaseAllConnections();
    vlc_delete_all(this->connectionPool);
    vlc_mutex_unlock(&lock);
}

void HTTPConnectionManager::releaseAllConnections()
{
    std::vector<AbstractConnection *>::iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); ++it)
        (*it)->setUsed(false);
}

AbstractConnection * HTTPConnectionManager::reuseConnection(ConnectionParams &params)
{
    std::vector<AbstractConnection *>::const_iterator it;
    for(it = connectionPool.begin(); it != connectionPool.end(); ++it)
    {
        AbstractConnection *conn = *it;
        if(conn->canReuse(params))
            return conn;
    }
    return NULL;
}

AbstractConnection * HTTPConnectionManager::getConnection(ConnectionParams &params)
{
    if(unlikely(!factory || !downloader))
        return NULL;

    vlc_mutex_lock(&lock);
    AbstractConnection *conn = reuseConnection(params);
    if(!conn)
    {
        conn = factory->createConnection(p_object, params);

        connectionPool.push_back(conn);

        if (!conn->prepare(params))
        {
            vlc_mutex_unlock(&lock);
            return NULL;
        }
    }

    conn->setUsed(true);
    vlc_mutex_unlock(&lock);
    return conn;
}

void HTTPConnectionManager::updateDownloadRate(size_t size, mtime_t time)
{
    if(rateObserver)
        rateObserver->updateDownloadRate(size, time);
}

void HTTPConnectionManager::setDownloadRateObserver(IDownloadRateObserver *obs)
{
    rateObserver = obs;
}
