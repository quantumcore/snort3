//--------------------------------------------------------------------------
// Copyright (C) 2014-2016 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------
// ips_ack.cc author Russ Combs <rucombs@cisco.com>

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <ctype.h>

#include "main/snort_types.h"
#include "main/snort_debug.h"
#include "detection/treenodes.h"
#include "protocols/packet.h"
#include "parser/parser.h"
#include "utils/util.h"
#include "profiler/profiler.h"
#include "hash/sfhashfcn.h"
#include "detection/detection_defines.h"
#include "framework/ips_option.h"
#include "framework/parameter.h"
#include "framework/module.h"
#include "framework/range.h"
#include "protocols/tcp.h"

#define s_name "ack"

#define s_help \
    "rule option to match on TCP ack numbers"

static THREAD_LOCAL ProfileStats tcpAckPerfStats;

class TcpAckOption : public IpsOption
{
public:
    TcpAckOption(const RangeCheck& c) :
        IpsOption(s_name)
    { config = c; }

    uint32_t hash() const override;
    bool operator==(const IpsOption&) const override;

    int eval(Cursor&, Packet*) override;

private:
    RangeCheck config;
};

//-------------------------------------------------------------------------
// api methods
//-------------------------------------------------------------------------

uint32_t TcpAckOption::hash() const
{
    uint32_t a,b,c;

    a = config.op;
    b = config.min;
    c = config.max;

    mix_str(a,b,c,get_name());
    finalize(a,b,c);

    return c;
}

bool TcpAckOption::operator==(const IpsOption& ips) const
{
    if ( strcmp(s_name, ips.get_name()) )
        return false;

    TcpAckOption& rhs = (TcpAckOption&)ips;
    return ( config == rhs.config );
}

int TcpAckOption::eval(Cursor&, Packet* p)
{
    Profile profile(tcpAckPerfStats);

    if ( p->ptrs.tcph && config.eval(p->ptrs.tcph->th_ack) )
        return DETECTION_OPTION_MATCH;

    return DETECTION_OPTION_NO_MATCH;
}

//-------------------------------------------------------------------------
// module
//-------------------------------------------------------------------------

static const Parameter s_params[] =
{
    { "~range", Parameter::PT_STRING, nullptr, nullptr,
      "check if tcp ack value is 'value | min<>max | <max | >min'" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

class AckModule : public Module
{
public:
    AckModule() : Module(s_name, s_help, s_params) { }

    bool begin(const char*, int, SnortConfig*) override;
    bool set(const char*, Value&, SnortConfig*) override;

    ProfileStats* get_profile() const override
    { return &tcpAckPerfStats; }

    RangeCheck data;
};

bool AckModule::begin(const char*, int, SnortConfig*)
{
    data.init();
    return true;
}

bool AckModule::set(const char*, Value& v, SnortConfig*)
{
    if ( !v.is("~range") )
        return false;

    return data.parse(v.get_string());
}

//-------------------------------------------------------------------------
// api methods
//-------------------------------------------------------------------------

static Module* mod_ctor()
{
    return new AckModule;
}

static void mod_dtor(Module* m)
{
    delete m;
}

static IpsOption* ack_ctor(Module* p, OptTreeNode*)
{
    AckModule* m = (AckModule*)p;
    return new TcpAckOption(m->data);
}

static void ack_dtor(IpsOption* p)
{
    delete p;
}

static const IpsApi ack_api =
{
    {
        PT_IPS_OPTION,
        sizeof(IpsApi),
        IPSAPI_VERSION,
        0,
        API_RESERVED,
        API_OPTIONS,
        s_name,
        s_help,
        mod_ctor,
        mod_dtor
    },
    OPT_TYPE_DETECTION,
    1, PROTO_BIT__TCP,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    ack_ctor,
    ack_dtor,
    nullptr
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &ack_api.base,
    nullptr
};
#else
const BaseApi* ips_ack = &ack_api.base;
#endif

