#ifndef OSMIUM_OSM_AREA_HPP
#define OSMIUM_OSM_AREA_HPP

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

#include <osmium/memory/collection.hpp>
#include <osmium/memory/item.hpp>
#include <osmium/memory/item_iterator.hpp>
#include <osmium/osm/box.hpp>
#include <osmium/osm/item_type.hpp>
#include <osmium/osm/node_ref_list.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/util/compatibility.hpp>

#include <cassert>
#include <cstdlib>
#include <iterator>
#include <utility>

namespace osmium {

    namespace builder {
        template <typename TDerived, typename T>
        class OSMObjectBuilder;
    } // namespace builder

    /**
     * An outer ring of an Area.
     */
    class OuterRing : public NodeRefList {

    public:

        static constexpr osmium::item_type itemtype = osmium::item_type::outer_ring;

        constexpr static bool is_compatible_to(osmium::item_type t) noexcept {
            return t == itemtype;
        }

        OuterRing():
            NodeRefList(itemtype) {
        }

    }; // class OuterRing

    static_assert(sizeof(OuterRing) % osmium::memory::align_bytes == 0, "Class osmium::OuterRing has wrong size to be aligned properly!");

    /**
     * An inner ring of an Area.
     */
    class InnerRing : public NodeRefList {

    public:

        static constexpr osmium::item_type itemtype = osmium::item_type::inner_ring;

        constexpr static bool is_compatible_to(osmium::item_type t) noexcept {
            return t == itemtype;
        }

        InnerRing():
            NodeRefList(itemtype) {
        }

    }; // class InnerRing

    static_assert(sizeof(InnerRing) % osmium::memory::align_bytes == 0, "Class osmium::InnerRing has wrong size to be aligned properly!");

    /**
     * Convert way or (multipolygon) relation id into unique area id.
     *
     * @param id Id of a way or relation
     * @param type Type of object (way or relation)
     * @returns Area id
     */
    inline osmium::object_id_type object_id_to_area_id(osmium::object_id_type id, osmium::item_type type) noexcept {
        osmium::object_id_type area_id = std::abs(id) * 2;
        if (type == osmium::item_type::relation) {
            ++area_id;
        }
        return id < 0 ? -area_id : area_id;
    }

    /**
     * Convert area id into id of the way or relation it was created from.
     *
     * @param id Area id
     * @returns Way or Relation id.
     */
    inline osmium::object_id_type area_id_to_object_id(osmium::object_id_type id) noexcept {
        return id / 2;
    }

    /**
     * An OSM area created out of a closed way or a multipolygon relation.
     */
    class Area : public OSMObject {

        template <typename TDerived, typename T>
        friend class osmium::builder::OSMObjectBuilder;

        Area() :
            OSMObject(sizeof(Area), osmium::item_type::area) {
        }

    public:

        static constexpr osmium::item_type itemtype = osmium::item_type::area;

        constexpr static bool is_compatible_to(osmium::item_type t) noexcept {
            return t == itemtype;
        }

        /**
         * Was this area created from a way? (In contrast to areas
         * created from a relation and their members.)
         *
         * Complexity: Constant.
         */
        bool from_way() const noexcept {
            return (positive_id() & 0x1) == 0;
        }

        /**
         * Return the Id of the way or relation this area was created from.
         *
         * Complexity: Constant.
         */
        osmium::object_id_type orig_id() const noexcept {
            return osmium::area_id_to_object_id(id());
        }

        /**
         * Count the number of outer and inner rings of this area.
         *
         * Complexity: Linear in the number of rings.
         *
         * @returns Pair (number outer rings, number inner rings)
         */
        std::pair<size_t, size_t> num_rings() const {
            std::pair<size_t, size_t> counter{0, 0};

            for (const auto& item : *this) {
                switch (item.type()) {
                    case osmium::item_type::outer_ring:
                        ++counter.first;
                        break;
                    case osmium::item_type::inner_ring:
                        ++counter.second;
                        break;
                    case osmium::item_type::tag_list:
                        // ignore tags
                        break;
                    case osmium::item_type::undefined:
                    case osmium::item_type::node:
                    case osmium::item_type::way:
                    case osmium::item_type::relation:
                    case osmium::item_type::area:
                    case osmium::item_type::changeset:
                    case osmium::item_type::way_node_list:
                    case osmium::item_type::relation_member_list:
                    case osmium::item_type::relation_member_list_with_full_members:
                    case osmium::item_type::changeset_discussion:
                        assert(false && "Children of Area can only be outer/inner_ring and tag_list.");
                        break;
                }
            }

            return counter;
        }

        /**
         * Check whether this area is a multipolygon, ie. whether it has more
         * than one outer ring?
         */
        bool is_multipolygon() const {
            return num_rings().first > 1;
        }

        /**
         * @deprecated Use inner_rings() instead.
         *
         * Get iterator for iterating over all inner rings in a specified outer
         * ring.
         *
         * @param it Iterator specifying outer ring.
         * @returns Iterator to first inner ring in specified outer ring.
         */
        OSMIUM_DEPRECATED osmium::memory::ItemIterator<const osmium::InnerRing> inner_ring_cbegin(const osmium::memory::ItemIterator<const osmium::OuterRing>& it) const {
            return it.cast<const osmium::InnerRing>();
        }

        /**
         * @deprecated Use inner_rings() instead.
         *
         * Get iterator for iterating over all inner rings in a specified outer
         * ring.
         *
         * @param it Iterator specifying outer ring.
         * @returns Iterator one past last inner ring in specified outer ring.
         */
        OSMIUM_DEPRECATED osmium::memory::ItemIterator<const osmium::InnerRing> inner_ring_cend(const osmium::memory::ItemIterator<const osmium::OuterRing>& it) const {
            return std::next(it).cast<const osmium::InnerRing>();
        }

        /**
         * Return an iterator range for all outer rings.
         * You can use the usual begin() and end() functions to iterate over
         * all outer rings.
         */
        osmium::memory::ItemIteratorRange<const osmium::OuterRing> outer_rings() const {
            return subitems<const osmium::OuterRing>();
        }

        /**
         * Return an iterator range for all inner rings in the given outer
         * ring.
         * You can use the usual begin() and end() functions to iterate over
         * all inner rings.
         */
        osmium::memory::ItemIteratorRange<const osmium::InnerRing> inner_rings(const osmium::OuterRing& outer) const {
            osmium::memory::ItemIteratorRange<const osmium::OuterRing> outer_range{outer.data(), next()};
            return osmium::memory::ItemIteratorRange<const osmium::InnerRing>{outer_range.cbegin().data(), std::next(outer_range.cbegin()).data()};
        }

        /**
         * Calculate the envelope of this area.
         *
         * Complexity: Linear in the number of nodes in the outer rings.
         */
        osmium::Box envelope() const noexcept {
            osmium::Box box;
            for (const auto& ring : outer_rings()) {
                box.extend(ring.envelope());
            }
            return box;
        }

    }; // class Area

    static_assert(sizeof(Area) % osmium::memory::align_bytes == 0, "Class osmium::Area has wrong size to be aligned properly!");

} // namespace osmium

#endif // OSMIUM_OSM_AREA_HPP
