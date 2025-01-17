//
// Copyright (C) 2008 Maciej Sobczak
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOCI_CONNECTION_POOL_H_INCLUDED
#define SOCI_CONNECTION_POOL_H_INCLUDED

#include "soci-config.h"
// std
#include <cstddef>

namespace soci
{

class session;

class SOCI_DECL connection_pool
{
public:
    explicit connection_pool(std::size_t size);
    ~connection_pool();

    session & at(std::size_t pos);

    std::size_t lease();
    bool try_lease(std::size_t & pos, int timeout);
    bool try_pos_lease(std::size_t pos, int timeout);
    void give_back(std::size_t pos);

private:
    struct connection_pool_impl;
    connection_pool_impl * pimpl_;
};

}

#endif // SOCI_CONNECTION_POOL_H_INCLUDED
