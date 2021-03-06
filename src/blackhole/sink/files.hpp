#pragma once

#include <boost/config.hpp>

#include "blackhole/attribute.hpp"
#include "blackhole/detail/string/formatting/formatter.hpp"
#include "blackhole/detail/util/unique.hpp"
#include "blackhole/repository/factory/traits.hpp"
#include "blackhole/sink/files/backend.hpp"
#include "blackhole/sink/files/config.hpp"
#include "blackhole/sink/files/flusher.hpp"
#include "blackhole/sink/files/rotation.hpp"
#include "blackhole/sink/files/writer.hpp"
#include "blackhole/sink/thread.hpp"

namespace blackhole {

namespace sink {

template<class Backend, class Rotator>
class file_handler_t {
public:
    typedef Backend backend_type;
    typedef Rotator rotator_type;
    typedef files::config_t<rotator_type> config_type;

private:
    backend_type backend_;
    files::writer_t<backend_type> writer;
    files::flusher_t<backend_type> flusher;
    rotator_type rotator;

public:
    file_handler_t(const std::string& path, const config_type& config) :
        backend_(path),
        writer(backend_),
        flusher(config.autoflush, backend_),
        rotator(config.rotation, backend_)
    {}

    void handle(const std::string& message) {
        writer.write(message);
        flusher.flush();
        if (rotator.necessary(message)) {
            rotator.rotate();
        }
    }

    const backend_type& backend() {
        return backend_;
    }
};

template<class Backend>
class file_handler_t<Backend, null_rotator_t> {
public:
    typedef Backend backend_type;
    typedef null_rotator_t rotator_type;
    typedef files::config_t<rotator_type> config_type;

private:
    backend_type backend_;
    files::writer_t<backend_type> writer;
    files::flusher_t<backend_type> flusher;

public:
    file_handler_t(const std::string& path, const config_type& config) :
        backend_(path),
        writer(backend_),
        flusher(config.autoflush, backend_)
    {}

    void handle(const std::string& message) {
        writer.write(message);
        flusher.flush();
    }

    const backend_type& backend() {
        return backend_;
    }
};

struct substitute_attribute_t {
    const attribute::set_view_t& attributes;

    void operator()(stickystream_t& stream, const std::string& placeholder) const {
        if (auto attribute = attributes.find(placeholder)) {
            stream << attribute->value;
        } else {
            stream.rdbuf()->storage()->append(placeholder);
        }
    }
};

template<class Backend = files::boost_backend_t, class Rotator = null_rotator_t>
class files_t {
    typedef file_handler_t<Backend, Rotator> handler_type;
    typedef std::unordered_map<std::string, std::shared_ptr<handler_type>> handlers_type;

    files::config_t<Rotator> config;
    handlers_type m_handlers;
    aux::formatter_t formatter;

public:
    typedef files::config_t<Rotator> config_type;

    static const char* name() {
        return "files";
    }

    files_t(const config_type& config) :
        config(config),
        formatter(config.path)
    {}

    void consume(const std::string& message, const attribute::set_view_t& attributes) {
        auto filename = make_filename(attributes);
        auto it = m_handlers.find(filename);
        if (it == m_handlers.end()) {
            it = m_handlers.insert(it, std::make_pair(filename, std::make_shared<handler_type>(filename, config)));
        }

        const std::shared_ptr<handler_type>& handler = it->second;
        handler->handle(message);
    }

    const handlers_type& handlers() {
        return m_handlers;
    }

    std::string make_filename(const attribute::set_view_t& attributes) const {
        return formatter.execute(substitute_attribute_t { attributes });
    }
};

} // namespace sink

template<class Backend, class Watcher>
struct match_traits<sink::files_t<Backend, sink::rotator_t<Backend, Watcher>>> {
    typedef type_index_t index_type;
    typedef sink::files_t<
        Backend,
        sink::rotator_t<Backend, Watcher>
    > sink_type;

    static index_type type_index(const std::string& type, const dynamic_t& config) {
        if (type != sink_type::name()) {
            return index_type(typeid(void));
        }

        auto config_ = config.to<dynamic_t::object_t>();
        auto it = config_.find("rotation");
        if (it == config_.end()) {
            return index_type(typeid(sink::files_t<Backend>));
        }

        auto rconfig = it->second.to<dynamic_t::object_t>();

        if (rconfig.find("move") != rconfig.end()) {
            return index_type(typeid(sink::files_t<
                Backend,
                sink::rotator_t<
                    Backend,
                    sink::rotation::watcher::move_t
                >
            >));
        }

        if (rconfig.find("size") != rconfig.end()) {
            return index_type(typeid(sink::files_t<
                Backend,
                sink::rotator_t<
                    Backend,
                    sink::rotation::watcher::size_t
                >
            >));
        }

        if (rconfig.find("period") != rconfig.end()) {
            return index_type(typeid(sink::files_t<
                Backend,
                sink::rotator_t<
                    Backend,
                    sink::rotation::watcher::datetime_t<>
                >
            >));
        }

        return index_type(typeid(void));
    }
};

namespace aux {

template<class T>
struct filler;

template<class Backend, class Rotator>
struct filler<sink::files_t<Backend, Rotator>> {
    template<class Extractor, class Config>
    static void extract_to(const Extractor& ex, Config& config) {
        ex["path"].to(config.path);
        ex["autoflush"].to(config.autoflush);
    }
};

template<class Backend, class Watcher>
struct filler<sink::rotator_t<Backend, Watcher>> {
    template<class Extractor, class Config>
    static void extract_to(const Extractor& ex, Config& config) {
        ex["rotation"]["pattern"].to(config.rotation.pattern);
        ex["rotation"]["backups"].to(config.rotation.backups);
    }
};

} // namespace aux

template<class Backend>
struct factory_traits<sink::files_t<Backend>> {
    typedef sink::files_t<Backend> sink_type;
    typedef typename sink_type::config_type config_type;

    static void map_config(const aux::extractor<sink_type>& ex, config_type& config) {
        aux::filler<sink_type>::extract_to(ex, config);
    }
};

template<class Backend>
struct factory_traits<sink::files_t<Backend, sink::rotator_t<Backend, sink::rotation::watcher::move_t>>> {
    typedef sink::rotation::watcher::move_t watcher_type;
    typedef sink::rotator_t<Backend, watcher_type> rotator_type;
    typedef sink::files_t<Backend, rotator_type> sink_type;
    typedef typename sink_type::config_type config_type;

    static void map_config(const aux::extractor<sink_type>& ex, config_type& config) {
        aux::filler<sink_type>::extract_to(ex, config);
    }
};

template<class Backend>
struct factory_traits<sink::files_t<Backend, sink::rotator_t<Backend, sink::rotation::watcher::size_t>>> {
    typedef sink::rotation::watcher::size_t watcher_type;
    typedef sink::rotator_t<Backend, watcher_type> rotator_type;
    typedef sink::files_t<Backend, rotator_type> sink_type;
    typedef typename sink_type::config_type config_type;

    static void map_config(const aux::extractor<sink_type>& ex, config_type& config) {
        aux::filler<sink_type>::extract_to(ex, config);
        aux::filler<rotator_type>::extract_to(ex, config);
        ex["rotation"]["size"].to(config.rotation.watcher.size);
    }
};

template<class Backend>
struct factory_traits<sink::files_t<Backend, sink::rotator_t<Backend, sink::rotation::watcher::datetime_t<>>>> {
    typedef sink::rotation::watcher::datetime_t<> watcher_type;
    typedef sink::rotator_t<Backend, watcher_type> rotator_type;
    typedef sink::files_t<Backend, rotator_type> sink_type;
    typedef typename sink_type::config_type config_type;

    static void map_config(const aux::extractor<sink_type>& ex, config_type& config) {
        aux::filler<sink_type>::extract_to(ex, config);
        aux::filler<rotator_type>::extract_to(ex, config);
        ex["rotation"]["period"].to(config.rotation.watcher.period);
    }
};

} // namespace blackhole
