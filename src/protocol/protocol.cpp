/*
Copyright 2020 Anjani Kumar <anjanik012@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "protocol.h"

#include <iostream>
#include <algorithm>

using std::string;
using namespace boost::asio;

protocol::protocol(void *user) : steps(), m_authenticator(user) {
    steps.push({GET_FHASH, GET_FHASH});
    steps.push({GET_SALTA, GET_RSALT});
    m_socket = nullptr;
    m_user_to_auth = user;
    m_callback = nullptr;
    read_buffer.resize(200);
}

void protocol::start_auth_job() {
    bool read_status = start_read();
    if (read_status) {
        std::cout << "PROTOCOL: Waiting for message\n";
    } else {
        std::cout << "PROTOCOL: Can't read :(\n";
    }
}

bool protocol::start_read() {
    try {
        read_buffer.resize(200);
        m_socket->async_read_some(buffer(read_buffer, read_buffer.size()), boost::bind(
                &protocol::read_handler, this,
                placeholders::error,
                placeholders::bytes_transferred));
    } catch (boost::system::system_error &error) {
        return false;
    }
    return true;
}

void protocol::read_handler(const boost::system::error_code &ec, std::size_t bytes) {
    if (!ec) {
        read_buffer.erase(std::remove_if(read_buffer.begin(), read_buffer.end(),
                                         [](char c) {
                                             return !std::isalnum(c) && (c != '.') && (c != '/') && (c != '_') &&
                                                    (c != '$');
                                         }),
                          read_buffer.end());
        std::cout << "PROTOCOL: message received:-" << read_buffer << '\n';
        if (!steps.empty()) {
            auto msg_t = validate_msg();
            switch (msg_t) {
                case INVALID_TYPE:
                    std::cerr << "Unkown message type received, aborting :|\n";
                    break;
                case SALTA:
                    start_write(SALTA);
                    break;
                case RSALT:
                    start_write(RSALT);
                    break;
                case FHASH:
                    auto hash = std::string(read_buffer.begin() + GET_FHASH.length() + 1, read_buffer.end());
                    std::cout << "PROTOCOL: Final hash received:- " << hash << '\n';
                    bool auth_res = m_authenticator.check_hash(hash);
                    if (auth_res) {
                        std::cout << "PROTOCOL: Authentication complete :)\n";
                        send_auth_msg(true);
                    } else {
                        std::cerr << "PROTOCOL: Authentication failed :(\n";
                        send_auth_msg(false);
                    }
                    break;
            }
        }
    } else {
        std::cerr << "PROTOCOL: read failed :|\n";
    }
}

bool protocol::start_write(int type) {
    try {
        string reply;
        switch (type) {
            case SALTA:
                reply = "SUTO_SALTA_" + m_authenticator.get_salt() + "_" + m_authenticator.get_random_salt();
                break;
            case RSALT:
                reply = "SUTO_RSALT_" + m_authenticator.get_random_salt();
                break;
        }
        std::cout << "PROTOCOL: Sending reply:- " << reply << '\n';
        m_socket->async_write_some(buffer(reply), boost::bind(
                &protocol::write_handler, this,
                placeholders::error,
                placeholders::bytes_transferred));
    } catch (boost::system::system_error &error) {
        return false;
    }
    return true;
}

void protocol::write_handler(const boost::system::error_code &ec, std::size_t bytes) {
    if (!ec) {
        std::cout << "PROTOCOL: Messgage sent :)...Waiting for next message\n";
        start_read();
    } else {
        std::cerr << "PROTOCOL: Failed sending message :(\n";
    }
}

int protocol::validate_msg() {
    pair<string, string> step;
    steps.pop(step);
    auto f_first = read_buffer.find(step.first);
    if(f_first != string::npos) {
        if(step.first == GET_SALTA) {
            return SALTA;
        } else {
            return FHASH;
        }
    } else if(read_buffer.find(step.second) != string::npos) {
        return RSALT;
    }
    return INVALID_TYPE;
}

bool protocol::set_tcp_socket(tcp::socket *socket) {
    bool ans;
    if (socket->is_open()) {
        m_socket = socket;
        ans = true;
    } else {
        ans = false;
    }
    return ans;
}

void protocol::set_auth_completion_callback(auth_complete_callback *cb) {
    m_callback = cb;
}

void protocol::send_auth_msg(bool auth_type) {
    boost::system::error_code ec;
    if (auth_type) {
        m_socket->write_some(buffer(AUTH_SUCCESS), ec);
    } else {
        m_socket->write_some(buffer(AUTH_FAILED), ec);
    }
    if (!ec) {
        if (auth_type) {
            std::cout << "PROTOCOL: Sent " << AUTH_SUCCESS << '\n';
            m_callback->on_auth_complete();
        } else {
            std::cout << "PROTOCOL: Sent " << AUTH_FAILED << '\n';
        }
    } else {
        std::cerr << "PROTOCOL: Failed to sent auth message...something went wrong\n";
    }
    m_socket->close();
}
