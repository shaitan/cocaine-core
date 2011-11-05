#ifndef COCAINE_DEFERRED_HPP
#define COCAINE_DEFERRED_HPP

#include <boost/enable_shared_from_this.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace engine {

class deferred_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<deferred_t>,
    public birth_control_t<deferred_t>
{
    public:
        deferred_t(driver_t* parent);

    public:
        virtual void enqueue();
        virtual void respond(zmq::message_t& chunk) = 0; 
        virtual void abort(error_code code, const std::string& error) = 0;
        
    protected:
        driver_t* m_parent;
};

class publication_t:
    public deferred_t
{
    public:
        publication_t(driver_t* parent);

    public:
        virtual void respond(zmq::message_t& chunk);
        virtual void abort(error_code code, const std::string& error);
};

}}

#endif
