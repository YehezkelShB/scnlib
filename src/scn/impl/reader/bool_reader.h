// Copyright 2017 Elias Kosunen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is a part of scnlib:
//     https://github.com/eliaskosunen/scnlib

#pragma once

#include <scn/detail/scanner.h>
#include <scn/impl/reader/common.h>

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace impl {
        struct bool_reader_base {
            enum options_type { allow_text = 1, allow_numeric = 2 };

            constexpr bool_reader_base() = default;
            constexpr bool_reader_base(unsigned opt) : m_options(opt) {}

            template <typename Range>
            scan_expected<ranges::borrowed_iterator_t<Range>> read_classic(
                Range&& range,
                bool& value) const
            {
                scan_error err{scan_error::invalid_scanned_value,
                               "Failed to read boolean"};

                if (m_options & allow_numeric) {
                    if (auto r = read_numeric(range, value)) {
                        return *r;
                    }
                    else {
                        err = r.error();
                    }
                }

                if (m_options & allow_text) {
                    if (auto r = read_textual_classic(range, value)) {
                        return *r;
                    }
                    else {
                        err = r.error();
                    }
                }

                return unexpected(err);
            }

        protected:
            template <typename Range>
            scan_expected<ranges::borrowed_iterator_t<Range>> read_numeric(
                Range&& range,
                bool& value) const
            {
                if (auto r = read_matching_code_unit(range, '0')) {
                    value = false;
                    return r;
                }
                if (auto r = read_matching_code_unit(range, '1')) {
                    value = true;
                    return r;
                }

                return unexpected_scan_error(scan_error::invalid_scanned_value,
                                             "read_numeric: No match");
            }

            template <typename Range>
            scan_expected<ranges::borrowed_iterator_t<Range>>
            read_textual_classic(Range&& range, bool& value) const
            {
                if (auto r = read_matching_string_classic(range, "true")) {
                    value = true;
                    return r;
                }
                if (auto r = read_matching_string_classic(range, "false")) {
                    value = false;
                    return r;
                }

                return unexpected_scan_error(scan_error::invalid_scanned_value,
                                             "read_textual: No match");
            }

            unsigned m_options{allow_text | allow_numeric};
        };

        template <typename CharT>
        struct bool_reader : public bool_reader_base {
            using bool_reader_base::bool_reader_base;

            template <typename Range>
            scan_expected<ranges::borrowed_iterator_t<Range>> read_localized(
                Range&& range,
                detail::locale_ref loc,
                bool& value) const
            {
                scan_error err{scan_error::invalid_scanned_value,
                               "Failed to read boolean"};

                if (m_options & allow_numeric) {
                    if (auto r = read_numeric(range, value)) {
                        return *r;
                    }
                    else {
                        err = r.error();
                    }
                }

                if (m_options & allow_text) {
                    auto stdloc = loc.get<std::locale>();
                    const auto& numpunct =
                        get_or_add_facet<std::numpunct<CharT>>(stdloc);
                    const auto truename = numpunct.truename();
                    const auto falsename = numpunct.falsename();

                    if (auto r = read_textual_custom(range, value, truename,
                                                     falsename)) {
                        return *r;
                    }
                    else {
                        err = r.error();
                    }
                }

                return unexpected(err);
            }

        protected:
            template <typename Range>
            scan_expected<ranges::borrowed_iterator_t<Range>>
            read_textual_custom(Range&& range,
                                bool& value,
                                std::basic_string_view<CharT> truename,
                                std::basic_string_view<CharT> falsename) const
            {
                const auto is_truename_shorter =
                    truename.size() <= falsename.size();
                const auto shorter =
                    std::pair{is_truename_shorter ? truename : falsename,
                              is_truename_shorter};
                const auto longer =
                    std::pair{!is_truename_shorter ? truename : falsename,
                              !is_truename_shorter};

                if (auto r = read_matching_string(range, shorter.first)) {
                    value = shorter.second;
                    return r;
                }
                if (auto r = read_matching_string(range, longer.first)) {
                    value = longer.second;
                    return r;
                }

                return unexpected_scan_error(scan_error::invalid_scanned_value,
                                             "read_textual: No match");
            }
        };

        template <typename CharT>
        class reader<bool, CharT, void>
            : public reader_base<reader<bool, CharT, void>, CharT> {
        public:
            reader() = default;

            void check_specs_impl(
                const detail::basic_format_specs<CharT>& specs,
                reader_error_handler& eh)
            {
                detail::check_bool_type_specs(specs, eh);
            }

            template <typename Range>
            scan_expected<ranges::borrowed_iterator_t<Range>> read_default(
                Range&& range,
                bool& value,
                detail::locale_ref loc) const
            {
                SCN_UNUSED(loc);

                return bool_reader<CharT>{}.read_classic(SCN_FWD(range), value);
            }

            template <typename Range>
            scan_expected<ranges::borrowed_iterator_t<Range>> read_specs(
                Range&& range,
                const detail::basic_format_specs<CharT>& specs,
                bool& value,
                detail::locale_ref loc) const
            {
                const auto rd = bool_reader<CharT>{get_options(specs)};

                if (specs.localized) {
                    return rd.read_localized(SCN_FWD(range), loc, value);
                }

                return rd.read_classic(SCN_FWD(range), value);
            }

            static constexpr unsigned get_options(
                const detail::basic_format_specs<CharT>& specs)
            {
                SCN_GCC_COMPAT_PUSH
                SCN_GCC_COMPAT_IGNORE("-Wswitch-enum")

                switch (specs.type) {
                    case detail::presentation_type::string:
                        return bool_reader_base::allow_text;

                    case detail::presentation_type::int_generic:
                        return bool_reader_base::allow_numeric;

                    default:
                        return bool_reader_base::allow_text |
                               bool_reader_base::allow_numeric;
                }

                SCN_GCC_COMPAT_POP // -Wswitch-enum
            }
        };
    }  // namespace impl

    SCN_END_NAMESPACE
}  // namespace scn
