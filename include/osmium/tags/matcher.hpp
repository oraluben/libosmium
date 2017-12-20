#ifndef OSMIUM_TAGS_MATCHER_HPP
#define OSMIUM_TAGS_MATCHER_HPP

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

#include <osmium/osm/tag.hpp>
#include <osmium/util/string_matcher.hpp>

#include <type_traits>
#include <utility>

namespace osmium {

    /**
     * Compares a tags key (and value) against the specified StringMatchers.
     */
    class TagMatcher {

        osmium::StringMatcher m_key_matcher;
        osmium::StringMatcher m_value_matcher;
        bool m_result = true;

    public:

        /**
         * Create a default TagMatcher that matches no tags.
         */
        TagMatcher() :
            m_key_matcher(osmium::StringMatcher::always_false{}),
            m_value_matcher(osmium::StringMatcher::always_false{}) {
        }

        /**
         * Create a TagMatcher matching the key against the specified
         * StringMatcher.
         *
         * @param key_matcher StringMatcher for matching the key.
         */
        template <typename TKey, typename std::enable_if<
            std::is_convertible<TKey, osmium::StringMatcher>::value, int>::type = 0>
        explicit TagMatcher(TKey&& key_matcher) : // NOLINT clang-tidy: misc-forwarding-reference-overload (false positive due to enable_if)
            m_key_matcher(std::forward<TKey>(key_matcher)),
            m_value_matcher(osmium::StringMatcher::always_true{}) {
        }

        /**
         * Create a TagMatcher matching the key and value against the specified
         * StringMatchers.
         *
         * @param key_matcher StringMatcher for matching the key.
         * @param value_matcher StringMatcher for matching the value.
         * @param invert If set to true, invert the result of the value_matcher.
         */
        template <typename TKey, typename TValue,
            typename std::enable_if<std::is_convertible<TKey, osmium::StringMatcher>::value, int>::type = 0,
            typename std::enable_if<std::is_convertible<TValue, osmium::StringMatcher>::value, int>::type = 0>
        TagMatcher(TKey&& key_matcher, TValue&& value_matcher, bool invert = false) :
            m_key_matcher(std::forward<TKey>(key_matcher)),
            m_value_matcher(std::forward<TValue>(value_matcher)),
            m_result(!invert) {
        }

        /**
         * Match against the specified key and value.
         *
         * @returns true if the tag matches.
         */
        bool operator()(const char* key, const char* value) const noexcept {
            return m_key_matcher(key) &&
                   (m_value_matcher(value) == m_result);
        }

        /**
         * Match against the specified tag.
         *
         * @returns true if the tag matches.
         */
        bool operator()(const osmium::Tag& tag) const noexcept {
            return operator()(tag.key(), tag.value());
        }

        /**
         * Match against the specified tags.
         *
         * @returns true if any of the tags in the TagList matches.
         */
        bool operator()(const osmium::TagList& tags) const noexcept {
            for (const auto& tag : tags) {
                if (operator()(tag)) {
                    return true;
                }
            }
            return false;
        }

    }; // class TagMatcher

} // namespace osmium

#endif // OSMIUM_TAGS_MATCHER_HPP
