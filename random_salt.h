//
// Created by Anjani Kumar <anjanik012@gmail.com> on 5/5/20.
//

#ifndef SUTO_RANDOM_SALT_H
#define SUTO_RANDOM_SALT_H

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

using namespace boost::random;

class random_salt {
private:
    static const char symbols[65];
    random_device rng;
    uniform_int_distribution<uint8_t> dist;
public:
    random_salt(): dist(0, 63){}
    std::string get_salt(uint8_t);
};


#endif //SUTO_RANDOM_SALT_H
