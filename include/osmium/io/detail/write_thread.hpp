#ifndef OSMIUM_IO_DETAIL_WRITE_THREAD_HPP
#define OSMIUM_IO_DETAIL_WRITE_THREAD_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2017 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <osmium/io/compression.hpp>
#include <osmium/io/detail/queue_util.hpp>
#include <osmium/thread/util.hpp>

#include <exception>
#include <future>
#include <memory>
#include <string>
#include <utility>

namespace osmium {

    namespace io {

        namespace detail {

            /**
             * This codes runs in its own thread, getting data from the given
             * queue, (optionally) compressing it, and writing it to the output
             * file.
             */
            class WriteThread {

                queue_wrapper<std::string> m_queue;
                std::unique_ptr<osmium::io::Compressor> m_compressor;
                std::promise<bool> m_promise;

            public:

                WriteThread(future_string_queue_type& input_queue,
                            std::unique_ptr<osmium::io::Compressor>&& compressor,
                            std::promise<bool>&& promise) :
                    m_queue(input_queue),
                    m_compressor(std::move(compressor)),
                    m_promise(std::move(promise)) {
                }

                WriteThread(const WriteThread&) = delete;
                WriteThread& operator=(const WriteThread&) = delete;

                WriteThread(WriteThread&&) = delete;
                WriteThread& operator=(WriteThread&&) = delete;

                ~WriteThread() noexcept = default;

                void operator()() {
                    osmium::thread::set_thread_name("_osmium_write");

                    try {
                        while (true) {
                            const std::string data{m_queue.pop()};
                            if (at_end_of_data(data)) {
                                break;
                            }
                            m_compressor->write(data);
                        }
                        m_compressor->close();
                        m_promise.set_value(true);
                    } catch (...) {
                        m_promise.set_exception(std::current_exception());
                        m_queue.drain();
                    }
                }

            }; // class WriteThread

        } // namespace detail

    } // namespace io

} // namespace osmium

#endif // OSMIUM_IO_DETAIL_WRITE_THREAD_HPP
