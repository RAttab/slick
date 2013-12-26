/* discovery.cpp                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 26 Dec 2013
   FreeBSD-style copyright and disclaimer apply

   Implementation of the node discovery database.
*/

#include "discovery.h"

namespace slick {


/******************************************************************************/
/* NODE LIST                                                                  */
/******************************************************************************/

bool
NodeList::
test(const Address& addr) const
{
    auto it = std::binary_search(nodes.begin(), nodes.end(), addr);
    return it != nodes.end();
}

Address
NodeList::
pickRandom(RNG& rng) const
{
    std::uniform_int_distribution<size_t> dist(0, nodes.size() - 1);
    return nodes[dist(rng)];
}

std::vector<Address>
NodeList::
pickRandom(RNG& rng, size_t count) const
{
    assert(count < nodes.size());

    std::set<Address> result;

    for (size_t i = 0; i < count; ++i)
        while (!result.insert(pickRandom(rng)).second);

    return std::vector<Address>(result.begin(), result.end());
}

} // slick
