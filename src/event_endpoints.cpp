// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#include <ampi/event_endpoints.hpp>

#include <ampi/utils/format.hpp>

#include <bit>
#include <stdexcept>

namespace ampi
{
    namespace
    {
        const auto fallback_structure_error = std::make_shared<std::string>("structure_error");
    }

    const char* structure_error::what() const noexcept
    {
        if(!what_)
            try{
                switch(reason_){
                    case reason_t::unexpected_event:
                        if(event_)
                            what_ = std::make_shared<std::string>(format(
                                "unexpected event when deserializing ",object_type_.pretty_name(),
                                ": ",*event_,", wanted ",expected_kinds_));
                        else
                            what_ = std::make_shared<std::string>(format(
                                "unexpected end of event stream when deserializing ",
                                object_type_.pretty_name(),", wanted ",expected_kinds_));
                        break;
                    case reason_t::out_of_range:
                        if(event_)
                            what_ = std::make_shared<std::string>(format(
                                "when deserializing ",object_type_.pretty_name(),
                                ", the value of event ",*event_," is out of range"));
                        else
                            what_ = std::make_shared<std::string>(format(
                                "size of container of tyoe ",object_type_.pretty_name(),
                                " is too big"));
                        break;
                    case reason_t::duplicate_key:
                        what_ = std::make_shared<std::string>(format(
                            "duplicate key when deserializing ",object_type_.pretty_name()));
                        break;
                    case reason_t::unknown_key:
                        what_ = std::make_shared<std::string>(format(
                            "unknown key ",*event_," when deserializing ",
                            object_type_.pretty_name()));
                        break;
                    default:
                        BOOST_UNREACHABLE_RETURN(nullptr);
                }
            }
            catch(...){
                what_ = fallback_structure_error;
            }
        return what_->data();
    }
}
