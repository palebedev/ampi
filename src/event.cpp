// Copyright 2021 Pavel A. Lebedev
// Licensed under the Apache License, Version 2.0.
// (See accompanying file LICENSE.txt or copy at
//  http://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#include <ampi/event.hpp>

#include <boost/io/ios_state.hpp>

namespace ampi
{
    std::ostream& operator<<(std::ostream& stream,object_kind kind)
    {
        static const char* names[] = {
            "null",
            "bool",
            "unsigned_int",
            "signed_int",
            "float",
            "double",
            "map",
            "sequence",
            "binary",
            "extension",
            "string",
            "timestamp",
            "data_buffer"
        };
        return stream << names[static_cast<uint8_t>(kind)];
    }

    std::ostream& operator<<(std::ostream& stream,object_kind_set oks)
    {
        if(oks.set_){
            bool first = true;
            for(unsigned x = oks.set_;x;){
                stream << (first?'[':',');
                int pos = std::countr_zero(x);
                stream << static_cast<object_kind>(pos);
                first = false;
                x ^= 1u<<pos;
            }
        }else
            stream << '[';
        return stream << ']';
    }

    std::ostream& operator<<(std::ostream& stream,timestamp_t ts)
    {
        // FIXME: no operator<< for std::chrono::sys_time in libc++.
        //        We add subsecond display to the standard.
        auto dp = std::chrono::floor<std::chrono::days>(ts);
        std::chrono::year_month_day ymd{dp};
        // When we're at the lowest representable date, the cast from days
        // to nanoseconds in subtraction below overflows.
        // Add a day to both to avoid this.
        if(dp<std::chrono::time_point_cast<std::chrono::days>(timestamp_t::min())){
            dp += std::chrono::days(1);
            ts += std::chrono::days(1);
        }
        std::chrono::hh_mm_ss hms{ts-dp};
        boost::io::ios_fill_saver ifs{stream};
        stream << std::setfill('0')
               << std::setw(4) << int(ymd.year()) << '-'
               << std::setw(2) << unsigned(ymd.month()) << '-'
               << std::setw(2) << unsigned(ymd.day()) << ' '
               << std::setw(2) << hms.hours().count() << ':'
               << std::setw(2) << hms.minutes().count() << ':'
               << std::setw(2) << hms.seconds().count();
        if(auto c = hms.subseconds().count())
            stream << '.' << std::setw(9) << std::setfill('0') << c;
        return stream;
    }

    namespace
    {
        struct event_visitor
        {
            std::ostream& stream_;

            void operator()(auto x)
            {
                stream_ << x;
            }

            void operator()(sequence_header sh)
            {
                stream_ << sh.size;
            }

            template<typename T>
                requires std::is_same_v<T,map_header>||
                         std::is_same_v<T,sequence_header>||
                         std::is_same_v<T,binary_header>||
                         std::is_same_v<T,string_header>
            void operator()(T h)
            {
                stream_ << h.size;
            }

            void operator()(extension_header eh)
            {
                stream_ << ':' << eh.type << ':' << eh.size;
            }
        };
    }

    std::ostream& operator<<(std::ostream& stream,const event& e)
    {
        stream << e.kind();
        if(e.kind()!=object_kind::null){
            stream << ':';
            visit(event_visitor{stream},e.v_);
        }
        return stream;
    }
}
