#ifndef OSMIUM_AREA_MULTIPOLYGON_MANAGER_HPP
#define OSMIUM_AREA_MULTIPOLYGON_MANAGER_HPP

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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <osmium/area/stats.hpp>
#include <osmium/handler.hpp>
#include <osmium/handler/check_order.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/memory/callback_buffer.hpp>
#include <osmium/osm/item_type.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/tag.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/relations/manager_util.hpp>
#include <osmium/relations/members_database.hpp>
#include <osmium/relations/relations_database.hpp>
#include <osmium/storage/item_stash.hpp>
#include <osmium/tags/taglist.hpp>
#include <osmium/tags/tags_filter.hpp>

namespace osmium {

    /**
     * @brief Code related to the building of areas (multipolygons) from relations.
     */
    namespace area {

        /**
         * This class collects all data needed for creating areas from
         * relations tagged with type=multipolygon or type=boundary.
         * Most of its functionality is derived from the parent class
         * osmium::relations::Collector.
         *
         * The actual assembling of the areas is done by the assembler
         * class given as template argument.
         *
         * @tparam TAssembler Multipolygon Assembler class.
         * @pre The Ids of all objects must be unique in the input data.
         */
        template <typename TAssembler>
        class MultipolygonManager : public osmium::handler::Handler {

            using assembler_config_type = typename TAssembler::config_type;
            const assembler_config_type m_assembler_config;

            // All relations and members we are interested in will be kept
            // in here.
            osmium::ItemStash m_stash;

            /// Database of all relations we are interested in
            relations::RelationsDatabase m_relations_db;

            /// Database of all members we are interested in
            relations::MembersDatabase<osmium::Way> m_members_db;

            osmium::memory::CallbackBuffer m_output;

            area_stats m_stats;

            osmium::TagsFilter m_filter;

            using handler_pass2 = relations::SecondPassHandlerWithCheckOrder<MultipolygonManager>;
            handler_pass2 m_handler_pass2;

            void remove_members(const osmium::Relation& relation) {
                for (const auto& member : relation.members()) {
                    if (member.ref() != 0) {
                        assert(member.type() == osmium::item_type::way);
                        m_members_db.remove(member.ref(), relation.id());
                    }
                }
            }

            /**
             * We are interested in all relations tagged with type=multipolygon
             * or type=boundary with at least one way member.
             */
            bool keep_relation(const osmium::Relation& relation) const {
                const char* type = relation.tags().get_value_by_key("type");

                // ignore relations without "type" tag
                if (type == nullptr) {
                    return false;
                }

                if (((!std::strcmp(type, "multipolygon")) || (!std::strcmp(type, "boundary"))) && osmium::tags::match_any_of(relation.tags(), m_filter)) {
                    return std::any_of(relation.members().begin(), relation.members().end(), [](const RelationMember& member) {
                        return member.type() == osmium::item_type::way;
                    });
                }

                return false;
            }

            void assemble_way(const osmium::Way& way) {
                // you need at least 4 nodes to make up a polygon
                if (way.nodes().size() <= 3) {
                    return;
                }

                try {
                    if (!way.nodes().front().location() || !way.nodes().back().location()) {
                        throw osmium::invalid_location("invalid location");
                    }
                    if (way.ends_have_same_location()) {
                        if (way.tags().has_tag("area", "no")) {
                            return;
                        }

                        if (osmium::tags::match_none_of(way.tags(), m_filter)) {
                            return;
                        }

                        TAssembler assembler{m_assembler_config};
                        assembler(way, m_output.buffer());
                        m_stats += assembler.stats();
                        m_output.possibly_flush();
                    }
                } catch (const osmium::invalid_location&) {
                    // XXX ignore
                }
            }

            /**
             * This is called when a relation is complete, ie. all members
             * were found in the input. It will build the area using the
             * assembler.
             */
            void complete_relation(const osmium::Relation& relation) {
                std::vector<const osmium::Way*> ways;
                ways.reserve(relation.members().size());
                for (const auto& member : relation.members()) {
                    if (member.ref() != 0) {
                        ways.push_back(&m_members_db.get(member.ref()));
                    }
                }

                try {
                    TAssembler assembler{m_assembler_config};
                    assembler(relation, ways, m_output.buffer());
                    m_stats += assembler.stats();
                    m_output.possibly_flush();
                } catch (const osmium::invalid_location&) {
                    // XXX ignore
                }
            }

        public:

            /**
             * Construct a MultipolygonManager.
             *
             * @param assembler_config The configuration that will be given to
             *                         any newly constructed area assembler.
             * @param filter An optional filter specifying what tags are
             *               needed on closed ways or multipolygon relations
             *               to build the area.
             */
            explicit MultipolygonManager(const assembler_config_type& assembler_config, const osmium::TagsFilter& filter = osmium::TagsFilter{true}) :
                m_assembler_config(assembler_config),
                m_stash(),
                m_relations_db(m_stash),
                m_members_db(m_stash, m_relations_db),
                m_filter(filter),
                m_handler_pass2(*this) {
            }

            /// Access the internal RelationsDatabase.
            osmium::relations::RelationsDatabase& relations_db() noexcept {
                return m_relations_db;
            }

            /// Access the internal MembersDatabase.
            osmium::relations::MembersDatabase<osmium::Way>& members_db() noexcept {
                return m_members_db;
            }

            /**
             * Return reference to second pass handler.
             */
            handler_pass2& handler(const std::function<void(osmium::memory::Buffer&&)>& callback = nullptr) {
                m_output.set_callback(callback);
                return m_handler_pass2;
            }

            /**
             * Access the aggregated statistics generated by the assemblers
             * called from the manager.
             */
            const area_stats& stats() const noexcept {
                return m_stats;
            }

            /**
             * This function overwrites the one in the Handler class. It is
             * usually called on the first pass when reading an OSM file. It
             * will ask keep_relation() to decide whether to keep this
             * relation or not. If the relation should be kept, it will
             * remember the relation and note down interest in the members.
             */
            void relation(const osmium::Relation& relation) {
                if (keep_relation(relation)) {
                    auto rel_handle = m_relations_db.add(relation);

                    int n = 0;
                    for (auto& member : rel_handle->members()) {
                        if (member.type() == osmium::item_type::way) {
                            m_members_db.track(rel_handle, member.ref(), n);
                        } else {
                            member.set_ref(0); // set member id to zero to indicate we are not interested
                        }
                        ++n;
                    }
                }
            }

            /**
             * Sort the members database. This has to be called between the
             * first and second pass.
             */
            void prepare() {
                m_members_db.prepare();
            }

            /**
             * This function is called for each way from the second pass
             * handler. If the way is needed by some relation, it will be
             * stored in the members database. It will also build an area
             * for the way if possible.
             */
            void member_way(const osmium::Way& way) {
                m_members_db.add(way, [this](relations::RelationHandle& rel_handle) {
                    complete_relation(*rel_handle);
                    remove_members(*rel_handle);
                    rel_handle.remove();
                });
                assemble_way(way);
            }

            /**
             * Flush the output buffer. This is called by the second pass
             * handler after all members are read.
             */
            void flush_output() {
                m_output.flush();
            }

            /**
             * Return the contents of the ouput buffer.
             */
            osmium::memory::Buffer read() {
                return m_output.read();
            }

            /**
             * Return the memory used by different components of the manager.
             */
            relations::relations_manager_memory_usage used_memory() const noexcept {
                return {
                    m_relations_db.used_memory(),
                    m_members_db.used_memory(),
                    m_stash.used_memory()
                };
            }

        }; // class MultipolygonManager

    } // namespace area

} // namespace osmium

#endif // OSMIUM_AREA_MULTIPOLYGON_MANAGER_HPP
